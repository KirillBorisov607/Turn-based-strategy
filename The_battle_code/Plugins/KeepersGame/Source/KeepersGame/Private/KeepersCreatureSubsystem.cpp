#include "KeepersCreatureSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "KeepersBarks.h"
#include "KeepersReflection.h"
#include "KeepersTerrainSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersCreatures, Log, All);

namespace KeepersCreatures
{
	static constexpr uint8 SlimeType = 3;   // E_UnitType::Slime
	static constexpr uint8 ChickenType = 4; // E_UnitType::Chicken
	static constexpr uint8 NormalTile = 1;  // E_TileType::Normal
	static constexpr uint8 SlimeTile = 3;   // E_TileType::DoubleCost
	static const FName SmallSlimeTag = TEXT("SmallSlime");

	static uint8 UnitType(const UObject* Unit)
	{
		FProperty* Property = nullptr;
		void* Ptr = KeepersReflection::FindValuePtr(Unit->GetClass(), const_cast<UObject*>(Unit), TEXT("UnitType"), &Property);
		const FByteProperty* Byte = CastField<FByteProperty>(Property);
		return Byte ? Byte->GetPropertyValue(Ptr) : 255;
	}

	static bool IsAlive(UObject* Unit)
	{
		bool bAlive = false;
		KeepersReflection::FCall Call(Unit, TEXT("IsAlive"));
		if (Call.Call())
		{
			Call.Get<bool>(TEXT("IsAlive"), bAlive);
		}
		return bAlive;
	}

	static TArray<UObject*> UnitsInCombat(UObject* CombatSystem)
	{
		TArray<UObject*> Units;
		FArrayProperty* Property = CombatSystem ? CastField<FArrayProperty>(KeepersReflection::FindMember(CombatSystem->GetClass(), TEXT("UnitsInCombat"))) : nullptr;
		FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
		if (Inner)
		{
			FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(CombatSystem));
			for (int32 I = 0; I < Helper.Num(); ++I)
			{
				Units.Add(Inner->GetObjectPropertyValue(Helper.GetRawPtr(I)));
			}
		}
		return Units;
	}

	/** Tile data helpers (the grid is a Blueprint: BP_Grid.GridTiles). */
	static uint8* FindTile(UObject* Grid, const FIntPoint& Index, const UScriptStruct** OutStruct)
	{
		FMapProperty* TilesProperty = Grid ? CastField<FMapProperty>(KeepersReflection::FindMember(Grid->GetClass(), TEXT("GridTiles"))) : nullptr;
		const FStructProperty* TileProperty = TilesProperty ? CastField<FStructProperty>(TilesProperty->ValueProp) : nullptr;
		if (!TileProperty)
		{
			return nullptr;
		}
		*OutStruct = TileProperty->Struct;
		FScriptMapHelper Tiles(TilesProperty, TilesProperty->ContainerPtrToValuePtr<void>(Grid));
		for (FScriptMapHelper::FIterator It(Tiles); It; ++It)
		{
			if (*reinterpret_cast<const FIntPoint*>(Tiles.GetKeyPtr(It)) == Index)
			{
				return Tiles.GetValuePtr(It);
			}
		}
		return nullptr;
	}

	static bool IsFreeWalkable(UObject* Grid, const FIntPoint& Index, FVector& OutLocation)
	{
		const UScriptStruct* TileStruct = nullptr;
		uint8* Tile = FindTile(Grid, Index, &TileStruct);
		if (!Tile)
		{
			return false;
		}
		uint8 Type = 0;
		KeepersReflection::GetEnumByte(TileStruct, Tile, TEXT("Type"), Type);
		if (Type != 1 && Type != 3 && Type != 4) // Normal, DoubleCost, TripleCost
		{
			return false;
		}
		FProperty* UnitProperty = nullptr;
		void* UnitPtr = KeepersReflection::FindValuePtr(TileStruct, Tile, TEXT("UnitOnTile"), &UnitProperty);
		const FObjectPropertyBase* UnitObject = CastField<FObjectPropertyBase>(UnitProperty);
		if (UnitObject && UnitObject->GetObjectPropertyValue(UnitPtr))
		{
			return false;
		}
		const void* Transform = KeepersReflection::FindValuePtr(TileStruct, Tile, TEXT("Transform"));
		OutLocation = Transform ? static_cast<const FTransform*>(Transform)->GetLocation() : FVector::ZeroVector;
		return true;
	}

	/** Writes an int in BP_Unit.UnitData.Stats (AP_Max, MP_Max, ...). */
	static void SetStat(UObject* Unit, const FString& Name, int32 Value)
	{
		FProperty* DataProperty = nullptr;
		void* Data = KeepersReflection::FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitData"), &DataProperty);
		const FStructProperty* DataStruct = CastField<FStructProperty>(DataProperty);
		FProperty* StatsProperty = nullptr;
		void* Stats = DataStruct ? KeepersReflection::FindValuePtr(DataStruct->Struct, Data, TEXT("Stats"), &StatsProperty) : nullptr;
		const FStructProperty* StatsStruct = CastField<FStructProperty>(StatsProperty);
		FProperty* StatProperty = nullptr;
		void* Stat = StatsStruct ? KeepersReflection::FindValuePtr(StatsStruct->Struct, Stats, Name, &StatProperty) : nullptr;
		if (FIntProperty* IntProperty = CastField<FIntProperty>(StatProperty))
		{
			IntProperty->SetPropertyValue(Stat, Value);
		}
	}

	static UObject* FindCombatSystem(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_CombatSystem_C"))
			{
				return *It;
			}
		}
		return nullptr;
	}
}

void UKeepersCreatureSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UObject* CombatSystem = CachedCombatSystem.Get();
	if (!CombatSystem)
	{
		CombatSystem = KeepersCreatures::FindCombatSystem(GetWorld());
		CachedCombatSystem = CombatSystem;
	}
	bool bInCombat = false;
	bool bFinishing = false;
	if (CombatSystem)
	{
		KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
		KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);
	}
	if (!bInCombat || bFinishing)
	{
		return;
	}
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));

	// A round starts each time play returns to team 0.
	if (const UObject* Current = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit")))
	{
		int32 Team = INDEX_NONE;
		KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), Team);
		if (Team == 0 && LastTeam != 0)
		{
			++Round;
			OnRoundStarted(CombatSystem);
		}
		LastTeam = Team;
	}

	for (UObject* UnitObject : KeepersCreatures::UnitsInCombat(CombatSystem))
	{
		AActor* Unit = Cast<AActor>(UnitObject);
		if (!Unit || KeepersCreatures::UnitType(Unit) != KeepersCreatures::SlimeType)
		{
			continue;
		}
		FIntPoint Index;
		KeepersReflection::GetIntPoint(Unit->GetClass(), Unit, TEXT("Index"), Index);
		const bool bAlive = KeepersCreatures::IsAlive(Unit);

		if (bAlive)
		{
			LivingSlimes.Add(Unit);
			if (const FIntPoint* Previous = SlimeTiles.Find(Unit))
			{
				if (*Previous != Index && Previous->X > -900)
				{
					LeavePuddle(Grid, *Previous);
				}
			}
			SlimeTiles.Add(Unit, Index);
		}
		else if (LivingSlimes.Remove(Unit) > 0)
		{
			const FIntPoint* LastTile = SlimeTiles.Find(Unit);
			if (!Unit->ActorHasTag(KeepersCreatures::SmallSlimeTag))
			{
				Split(CombatSystem, Unit, LastTile ? *LastTile : Index);
			}
			SlimeTiles.Remove(Unit);
		}
	}
}

void UKeepersCreatureSubsystem::LeavePuddle(UObject* Grid, const FIntPoint& Index)
{
	const UScriptStruct* TileStruct = nullptr;
	uint8* Tile = KeepersCreatures::FindTile(Grid, Index, &TileStruct);
	uint8 Type = 0;
	if (!Tile || !KeepersReflection::GetEnumByte(TileStruct, Tile, TEXT("Type"), Type) || Type != KeepersCreatures::NormalTile)
	{
		return;
	}

	FTransform TileTransform;
	if (!UKeepersTerrainSubsystem::SetTileType(Grid, Index, KeepersCreatures::SlimeTile, TileTransform))
	{
		return;
	}

	static UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!PuddleMaterial)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			PuddleMaterial = UMaterialInstanceDynamic::Create(Base, this);
			PuddleMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 1.f, 0.05f));
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location = TileTransform.GetLocation() + FVector(0.f, 0.f, 2.f);
	if (AStaticMeshActor* Puddle = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Params))
	{
		Puddle->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* Mesh = Puddle->GetStaticMeshComponent();
		Mesh->SetStaticMesh(Disc);
		Mesh->SetMaterial(0, PuddleMaterial);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Puddle->SetActorScale3D(FVector(FMath::FRandRange(1.1f, 1.4f), FMath::FRandRange(0.9f, 1.2f), 0.02f));
		Puddles.Add(Puddle);
	}
	UE_LOG(LogKeepersCreatures, Log, TEXT("Slime puddle at (%d,%d)"), Index.X, Index.Y);
}

void UKeepersCreatureSubsystem::Split(UObject* CombatSystem, AActor* Slime, const FIntPoint& Index)
{
	// The last enemy standing ends the battle; do not split then.
	int32 SlimeTeam = 0;
	KeepersReflection::GetInt(Slime->GetClass(), Slime, TEXT("TeamIndex"), SlimeTeam);
	int32 AliveAllies = 0;
	for (UObject* Unit : KeepersCreatures::UnitsInCombat(CombatSystem))
	{
		int32 Team = -1;
		if (Unit && Unit != Slime && KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team) && Team == SlimeTeam && KeepersCreatures::IsAlive(Unit))
		{
			++AliveAllies;
		}
	}
	if (AliveAllies == 0)
	{
		return;
	}

	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	TArray<TPair<FIntPoint, FVector>> Spots;
	for (int32 DX = -1; DX <= 1 && Spots.Num() < 2; ++DX)
	{
		for (int32 DY = -1; DY <= 1 && Spots.Num() < 2; ++DY)
		{
			const FIntPoint Candidate(Index.X + DX, Index.Y + DY);
			FVector Location;
			if (Candidate != Index && KeepersCreatures::IsFreeWalkable(Grid, Candidate, Location))
			{
				Spots.Emplace(Candidate, Location);
			}
		}
	}

	UKeepersBarkLibrary::BarkAbout(Slime, EKeepersBarkSituation::Falls);
	for (int32 Piece = 0; Piece < 2; ++Piece)
	{
		if (AActor* Small = SpawnUnitNear(CombatSystem, Slime->GetClass(), KeepersCreatures::SlimeType, Index, SlimeTeam, SmallSlimeHp, 0.65f, KeepersCreatures::SmallSlimeTag, false))
		{
			UE_LOG(LogKeepersCreatures, Log, TEXT("Slime split: %s"), *Small->GetName());
		}
	}
}

void UKeepersCreatureSubsystem::OnRoundStarted(UObject* CombatSystem)
{
	if (Round <= 1 || (Round - 1) % ChickenCallEvery != 0)
	{
		return;
	}
	if (ChickenCalls >= ChickenMaxCalls)
	{
		return;
	}
	TArray<AActor*> Chickens;
	for (UObject* UnitObject : KeepersCreatures::UnitsInCombat(CombatSystem))
	{
		AActor* Chicken = Cast<AActor>(UnitObject);
		int32 Team = 0;
		if (Chicken && KeepersCreatures::UnitType(Chicken) == KeepersCreatures::ChickenType && KeepersCreatures::IsAlive(Chicken)
			&& KeepersReflection::GetInt(Chicken->GetClass(), Chicken, TEXT("TeamIndex"), Team) && Team != 0)
		{
			Chickens.Add(Chicken);
		}
	}
	if (Chickens.Num() == 0)
	{
		return;
	}
	AActor* Chicken = Chickens[FMath::RandRange(0, Chickens.Num() - 1)];
	int32 Team = 0;
	KeepersReflection::GetInt(Chicken->GetClass(), Chicken, TEXT("TeamIndex"), Team);
	FIntPoint Index;
	KeepersReflection::GetIntPoint(Chicken->GetClass(), Chicken, TEXT("Index"), Index);
	if (AActor* Help = SpawnUnitNear(CombatSystem, Chicken->GetClass(), KeepersCreatures::SlimeType, Index, Team, 0, 1.f, NAME_None))
	{
		++ChickenCalls;
		UKeepersBarkLibrary::Bark(Chicken, FText::FromString(TEXT("Ко-ко-КО! На помощь!")));
		UE_LOG(LogKeepersCreatures, Log, TEXT("%s called %s"), *Chicken->GetName(), *Help->GetName());
	}
}

AActor* UKeepersCreatureSubsystem::SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag, bool bCanAct)
{
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	FIntPoint Spot;
	FVector SpotLocation;
	bool bFound = false;
	for (int32 Ring = 1; Ring <= 2 && !bFound; ++Ring)
	{
		for (int32 DX = -Ring; DX <= Ring && !bFound; ++DX)
		{
			for (int32 DY = -Ring; DY <= Ring && !bFound; ++DY)
			{
				const FIntPoint Candidate(Index.X + DX, Index.Y + DY);
				if (Candidate != Index && KeepersCreatures::IsFreeWalkable(Grid, Candidate, SpotLocation))
				{
					Spot = Candidate;
					bFound = true;
				}
			}
		}
	}
	if (!bFound || !UnitClass)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(SpotLocation + FVector(0.f, 0.f, 100.f));
	AActor* Unit = GetWorld()->SpawnActorDeferred<AActor>(UnitClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Unit)
	{
		return nullptr;
	}
	if (FByteProperty* TypeProperty = CastField<FByteProperty>(KeepersReflection::FindMember(Unit->GetClass(), TEXT("UnitType"))))
	{
		TypeProperty->SetPropertyValue_InContainer(Unit, UnitType);
	}
	if (!Tag.IsNone())
	{
		Unit->Tags.Add(Tag);
	}
	UGameplayStatics::FinishSpawningActor(Unit, SpawnTransform);
	Unit->SetActorScale3D(FVector(Scale));

	KeepersReflection::FCall Add(CombatSystem, TEXT("AddUnitInCombat"));
	Add.Set<UObject*>(TEXT("Unit"), Unit);
	Add.Set<FIntPoint>(TEXT("Index"), Spot);
	Add.Set<int32>(TEXT("TeamIndex"), Team);
	Add.Set<bool>(TEXT("UseAi"), true);
	Add.Call();
	KeepersReflection::FCall(Unit, TEXT("CombatStarted")).Call();
	if (UObject* Ai = KeepersReflection::GetObject(Unit, TEXT("Ai")))
	{
		KeepersReflection::FCall(Ai, TEXT("AiCombatStarted")).Call();
	}
	if (!bCanAct)
	{
		KeepersCreatures::SetStat(Unit, TEXT("AP_Max"), 0);
		KeepersCreatures::SetStat(Unit, TEXT("AP_Current"), 0);
	}
	if (Hp > 0)
	{
		KeepersReflection::FCall SetHp(Unit, TEXT("SetCurrentHp"));
		SetHp.Set<int32>(TEXT("Hp"), Hp);
		SetHp.Set<bool>(TEXT("ShowNumber"), false);
		SetHp.Call();
	}
	return Unit;
}

TStatId UKeepersCreatureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersCreatureSubsystem, STATGROUP_Tickables);
}

bool UKeepersCreatureSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
