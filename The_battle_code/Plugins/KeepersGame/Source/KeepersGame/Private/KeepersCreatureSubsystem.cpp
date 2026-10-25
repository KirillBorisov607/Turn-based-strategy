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
	for (const TPair<FIntPoint, FVector>& Spot : Spots)
	{
		AActor* Small = GetWorld()->SpawnActorDeferred<AActor>(Slime->GetClass(), FTransform(Spot.Value + FVector(0.f, 0.f, 100.f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Small)
		{
			continue;
		}
		if (FByteProperty* TypeProperty = CastField<FByteProperty>(KeepersReflection::FindMember(Small->GetClass(), TEXT("UnitType"))))
		{
			TypeProperty->SetPropertyValue_InContainer(Small, KeepersCreatures::SlimeType);
		}
		Small->Tags.Add(KeepersCreatures::SmallSlimeTag);
		UGameplayStatics::FinishSpawningActor(Small, FTransform(Spot.Value + FVector(0.f, 0.f, 100.f)));
		Small->SetActorScale3D(FVector(0.65f));

		KeepersReflection::FCall Add(CombatSystem, TEXT("AddUnitInCombat"));
		Add.Set<UObject*>(TEXT("Unit"), Small);
		Add.Set<FIntPoint>(TEXT("Index"), Spot.Key);
		Add.Set<int32>(TEXT("TeamIndex"), SlimeTeam);
		Add.Set<bool>(TEXT("UseAi"), true);
		Add.Call();
		KeepersReflection::FCall(Small, TEXT("CombatStarted")).Call();
		if (UObject* Ai = KeepersReflection::GetObject(Small, TEXT("Ai")))
		{
			KeepersReflection::FCall(Ai, TEXT("AiCombatStarted")).Call();
		}
		KeepersReflection::FCall SetHp(Small, TEXT("SetCurrentHp"));
		SetHp.Set<int32>(TEXT("Hp"), SmallSlimeHp);
		SetHp.Set<bool>(TEXT("ShowNumber"), false);
		SetHp.Call();
		UE_LOG(LogKeepersCreatures, Log, TEXT("Slime split: small slime at (%d,%d)"), Spot.Key.X, Spot.Key.Y);
	}
}

TStatId UKeepersCreatureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersCreatureSubsystem, STATGROUP_Tickables);
}

bool UKeepersCreatureSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
