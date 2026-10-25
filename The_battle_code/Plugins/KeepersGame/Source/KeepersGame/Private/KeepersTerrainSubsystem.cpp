#include "KeepersTerrainSubsystem.h"

#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "KeepersReflection.h"
#include "KeepersBarks.h"
#include "KeepersPlantActor.h"
#include "KeepersRockActor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersTerrain, Log, All);

namespace KeepersTerrain
{
	static const FString RaiseRockAnimation = TEXT("BP_SpellAnim_RaiseRock");
	/** Spell animations whose hit pushes the target. */
	static const TArray<FString> PushAnimations = { TEXT("BP_SpellAnim_Gust"), TEXT("BP_SpellAnim_ShieldBash") };
	static const FString SproutAnimation = TEXT("BP_SpellAnim_Sprout");
	static const FString RootsAnimation = TEXT("BP_SpellAnim_Roots");
	static const FString CombatSystemClass = TEXT("BP_CombatSystem_C");

	/** E_TileType values. */
	static constexpr uint8 ObstacleTileType = 2;
	static constexpr uint8 ChasmTileType = 5; // FlyingUnitsOnly
	static constexpr uint8 ForestTileType = 3; // DoubleCost

	static constexpr int32 BumpDamage = 1;

	/** Seconds after which a terrain effect applies even if its animation has not finished. */
	static constexpr float AnimationTimeout = 3.f;

	/** BP_Grid.GridTiles[Index] (S_TileData), or nullptr. */
	static uint8* FindTile(UObject* Grid, const FIntPoint& Index, const UScriptStruct** OutStruct = nullptr)
	{
		FMapProperty* TilesProperty = Grid ? CastField<FMapProperty>(KeepersReflection::FindMember(Grid->GetClass(), TEXT("GridTiles"))) : nullptr;
		const FStructProperty* TileProperty = TilesProperty ? CastField<FStructProperty>(TilesProperty->ValueProp) : nullptr;
		if (!TileProperty)
		{
			return nullptr;
		}
		if (OutStruct)
		{
			*OutStruct = TileProperty->Struct;
		}
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

	static UObject* GetUnitOnTile(UObject* Grid, const FIntPoint& Index)
	{
		const UScriptStruct* TileStruct = nullptr;
		uint8* Tile = FindTile(Grid, Index, &TileStruct);
		FProperty* UnitProperty = nullptr;
		void* UnitPtr = Tile ? KeepersReflection::FindValuePtr(TileStruct, Tile, TEXT("UnitOnTile"), &UnitProperty) : nullptr;
		const FObjectPropertyBase* UnitObject = CastField<FObjectPropertyBase>(UnitProperty);
		return UnitObject && UnitPtr ? UnitObject->GetObjectPropertyValue(UnitPtr) : nullptr;
	}

	static bool GetTileType(UObject* Grid, const FIntPoint& Index, uint8& OutType)
	{
		const UScriptStruct* TileStruct = nullptr;
		uint8* Tile = FindTile(Grid, Index, &TileStruct);
		return Tile && KeepersReflection::GetEnumByte(TileStruct, Tile, TEXT("Type"), OutType);
	}

	static bool GetTileLocation(UObject* Grid, const FIntPoint& Index, FVector& OutLocation)
	{
		const UScriptStruct* TileStruct = nullptr;
		uint8* Tile = FindTile(Grid, Index, &TileStruct);
		const void* Transform = Tile ? KeepersReflection::FindValuePtr(TileStruct, Tile, TEXT("Transform")) : nullptr;
		if (!Transform)
		{
			return false;
		}
		OutLocation = static_cast<const FTransform*>(Transform)->GetLocation();
		return true;
	}

	/** BP_Unit.UnitData.Stats.ValidTileTypes contains Type. */
	static bool CanStandOn(UObject* Unit, uint8 Type)
	{
		FProperty* DataProperty = nullptr;
		void* Data = KeepersReflection::FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitData"), &DataProperty);
		const FStructProperty* DataStruct = CastField<FStructProperty>(DataProperty);
		FProperty* StatsProperty = nullptr;
		void* Stats = DataStruct ? KeepersReflection::FindValuePtr(DataStruct->Struct, Data, TEXT("Stats"), &StatsProperty) : nullptr;
		const FStructProperty* StatsStruct = CastField<FStructProperty>(StatsProperty);
		FProperty* TypesProperty = nullptr;
		void* Types = StatsStruct ? KeepersReflection::FindValuePtr(StatsStruct->Struct, Stats, TEXT("ValidTileTypes"), &TypesProperty) : nullptr;
		FArrayProperty* TypesArray = CastField<FArrayProperty>(TypesProperty);
		if (!TypesArray)
		{
			return Type != ObstacleTileType && Type != ChasmTileType;
		}
		FScriptArrayHelper Helper(TypesArray, Types);
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			if (*Helper.GetRawPtr(Index) == Type)
			{
				return true;
			}
		}
		return false;
	}

	static FVector GetTileSize(UObject* Grid)
	{
		if (const void* SizePtr = Grid ? KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTileSize")) : nullptr)
		{
			return *static_cast<const FVector*>(SizePtr);
		}
		return FVector(200.f, 200.f, 100.f);
	}

	static UObject* FindCombatSystem(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == CombatSystemClass)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

void UKeepersTerrainSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UKeepersTerrainSubsystem::OnActorSpawned));
}

void UKeepersTerrainSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Super::Deinitialize();
}

bool UKeepersTerrainSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersTerrainSubsystem::OnActorSpawned(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	const FString ClassName = Actor->GetClass()->GetName();
	if (ClassName.StartsWith(KeepersTerrain::RaiseRockAnimation))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::RaiseRock);
	}
	else if (KeepersTerrain::PushAnimations.ContainsByPredicate([&ClassName](const FString& Prefix) { return ClassName.StartsWith(Prefix); }))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Push);
	}
	else if (ClassName.StartsWith(KeepersTerrain::SproutAnimation))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Sprout);
	}
	else if (ClassName.StartsWith(KeepersTerrain::RootsAnimation))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Roots);
	}
	else
	{
		return;
	}
	// The animation destroys itself after hitting its tiles; the effect follows the hit.
	Actor->OnDestroyed.AddDynamic(this, &UKeepersTerrainSubsystem::OnTerrainAnimationDestroyed);

	// Some animations never complete (BP_SpellAnim_WindGust with a single target), so do not wait forever.
	TWeakObjectPtr<AActor> WeakAnimation(Actor);
	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakAnimation]()
	{
		if (AActor* Animation = WeakAnimation.Get())
		{
			Animation->OnDestroyed.RemoveDynamic(this, &UKeepersTerrainSubsystem::OnTerrainAnimationDestroyed);
			OnTerrainAnimationDestroyed(Animation);
		}
	}), KeepersTerrain::AnimationTimeout, false);
}

void UKeepersTerrainSubsystem::OnTerrainAnimationDestroyed(AActor* Animation)
{
	EKeepersTerrainEffect Effect;
	if (!PendingEffects.RemoveAndCopyValue(Animation, Effect))
	{
		return;
	}
	UObject* Grid = KeepersReflection::GetObject(Animation, TEXT("Grid"));
	FIntPoint Origin, Target;
	KeepersReflection::GetIntPoint(Animation->GetClass(), Animation, TEXT("OriginIndex"), Origin);
	if (!Grid || !KeepersReflection::GetIntPoint(Animation->GetClass(), Animation, TEXT("TargetIndex"), Target))
	{
		return;
	}

	switch (Effect)
	{
	case EKeepersTerrainEffect::RaiseRock:
		if (RaiseRock(Grid, Target))
		{
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Origin)), EKeepersBarkSituation::RaisesRock, 0.8f);
		}
		break;
	case EKeepersTerrainEffect::Push:
		PushUnit(KeepersTerrain::FindCombatSystem(GetWorld()), Origin, Target);
		break;
	case EKeepersTerrainEffect::Roots:
		if (UObject* Rooted = KeepersTerrain::GetUnitOnTile(Grid, Target))
		{
			KeepersReflection::FCall SetMp(Rooted, TEXT("SetCurrentMp"));
			SetMp.Set<int32>(TEXT("Mp"), 0);
			SetMp.Set<bool>(TEXT("ShowNumber"), false);
			SetMp.Call();
		}
		break;
	case EKeepersTerrainEffect::Sprout:
		if (GrowForest(Grid, Target))
		{
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Origin)), EKeepersBarkSituation::GrowsForest, 0.6f);
		}
		break;
	}
}

bool UKeepersTerrainSubsystem::SetTileType(UObject* Grid, const FIntPoint& Index, uint8 Type, FTransform& OutTransform)
{
	const UScriptStruct* TileStruct = nullptr;
	const uint8* Tile = KeepersTerrain::FindTile(Grid, Index, &TileStruct);
	if (!Tile)
	{
		return false;
	}

	// Work on a copy: AddGridTile replaces the tile's data in the map.
	TArray<uint8> Data;
	Data.SetNumZeroed(TileStruct->GetStructureSize());
	TileStruct->InitializeStruct(Data.GetData());
	TileStruct->CopyScriptStruct(Data.GetData(), Tile);

	FProperty* TypeProperty = nullptr;
	void* TypePtr = KeepersReflection::FindValuePtr(TileStruct, Data.GetData(), TEXT("Type"), &TypeProperty);
	if (FByteProperty* ByteType = CastField<FByteProperty>(TypeProperty))
	{
		ByteType->SetPropertyValue(TypePtr, Type);
	}

	OutTransform = FTransform::Identity;
	if (const void* TransformPtr = KeepersReflection::FindValuePtr(TileStruct, Data.GetData(), TEXT("Transform")))
	{
		OutTransform = *static_cast<const FTransform*>(TransformPtr);
	}

	KeepersReflection::FCall AddTile(Grid, TEXT("AddGridTile"));
	AddTile.SetRaw(TEXT("Data"), Data.GetData());
	AddTile.Call();
	TileStruct->DestroyStruct(Data.GetData());
	return true;
}

bool UKeepersTerrainSubsystem::RaiseRock(UObject* Grid, FIntPoint Index, int32 Levels)
{
	if (KeepersTerrain::GetUnitOnTile(Grid, Index))
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("RaiseRock: tile (%d,%d) is occupied"), Index.X, Index.Y);
		return false;
	}
	FTransform TileTransform;
	if (!SetTileType(Grid, Index, KeepersTerrain::ObstacleTileType, TileTransform))
	{
		return false;
	}

	const FVector TileSize = KeepersTerrain::GetTileSize(Grid);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKeepersRockActor* Rock = GetWorld()->SpawnActor<AKeepersRockActor>(TileTransform.GetLocation(), TileTransform.Rotator(), Params);
	if (Rock)
	{
		Rock->SetActorScale3D(FVector(TileSize.X / 200.f * 0.95f, TileSize.Y / 200.f * 0.95f, 1.f));
		Rock->Rise(TileTransform.GetLocation(), TileTransform.GetLocation().Z + TileSize.Z * Levels);
		TerrainProps.Add(Rock);
	}

	UE_LOG(LogKeepersTerrain, Log, TEXT("RaiseRock at (%d,%d)"), Index.X, Index.Y);
	return true;
}

bool UKeepersTerrainSubsystem::GrowForest(UObject* Grid, FIntPoint Index)
{
	uint8 Type = 0;
	if (!KeepersTerrain::GetTileType(Grid, Index, Type) || Type == KeepersTerrain::ObstacleTileType || Type == KeepersTerrain::ChasmTileType)
	{
		return false;
	}
	const bool bOccupied = KeepersTerrain::GetUnitOnTile(Grid, Index) != nullptr;
	FTransform TileTransform;
	if (!SetTileType(Grid, Index, KeepersTerrain::ForestTileType, TileTransform))
	{
		return false;
	}

	static const TCHAR* Trees[] = {
		TEXT("/Game/Art/Meshes/SM_Tree_01.SM_Tree_01"), TEXT("/Game/Art/Meshes/SM_Tree_02.SM_Tree_02"), TEXT("/Game/Art/Meshes/SM_Tree_03.SM_Tree_03") };
	static const TCHAR* Bushes[] = {
		TEXT("/Game/Art/Meshes/SM_Bush_01.SM_Bush_01"), TEXT("/Game/Art/Meshes/SM_Bush_03.SM_Bush_03") };
	// Stable choice per tile so a forest looks varied but does not flicker between casts.
	const uint32 Pick = GetTypeHash(Index);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, bOccupied ? Bushes[Pick % UE_ARRAY_COUNT(Bushes)] : Trees[Pick % UE_ARRAY_COUNT(Trees)]);

	const FVector TileSize = KeepersTerrain::GetTileSize(Grid);
	const FRotator Yaw(0.f, static_cast<float>(Pick % 360), 0.f);
	FVector Location = TileTransform.GetLocation();
	if (bOccupied)
	{
		Location += Yaw.Vector() * TileSize.X * 0.3f; // beside the unit, not inside it
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AKeepersPlantActor* Plant = GetWorld()->SpawnActor<AKeepersPlantActor>(Location, Yaw, Params))
	{
		Plant->Grow(Mesh, TileSize.X / 200.f * (bOccupied ? 0.9f : 0.8f), !bOccupied);
		TerrainProps.Add(Plant);
	}

	UE_LOG(LogKeepersTerrain, Log, TEXT("GrowForest at (%d,%d)%s"), Index.X, Index.Y, bOccupied ? TEXT(" (bush)") : TEXT(""));
	return true;
}

bool UKeepersTerrainSubsystem::PushUnit(UObject* CombatSystem, FIntPoint Origin, FIntPoint Target)
{
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	UObject* Unit = KeepersTerrain::GetUnitOnTile(Grid, Target);
	if (!Unit || Origin == Target)
	{
		return false;
	}

	FVector OriginLocation, TargetLocation;
	if (!KeepersTerrain::GetTileLocation(Grid, Origin, OriginLocation) || !KeepersTerrain::GetTileLocation(Grid, Target, TargetLocation))
	{
		return false;
	}
	const FVector PushDirection = (TargetLocation - OriginLocation).GetSafeNormal2D();

	// The neighbour of Target that lies furthest along the push direction (works for every grid shape).
	const FVector TileSize = KeepersTerrain::GetTileSize(Grid);
	TOptional<FIntPoint> Destination;
	float BestAlignment = 0.5f;
	for (int32 DX = -2; DX <= 2; ++DX)
	{
		for (int32 DY = -2; DY <= 2; ++DY)
		{
			const FIntPoint Candidate(Target.X + DX, Target.Y + DY);
			FVector CandidateLocation;
			if (Candidate == Target || !KeepersTerrain::GetTileLocation(Grid, Candidate, CandidateLocation))
			{
				continue;
			}
			const FVector Offset = CandidateLocation - TargetLocation;
			const float Distance = Offset.Size2D();
			if (Distance > TileSize.X * 1.2f)
			{
				continue; // not adjacent
			}
			const float Alignment = FVector::DotProduct(Offset.GetSafeNormal2D(), PushDirection);
			if (Alignment > BestAlignment)
			{
				BestAlignment = Alignment;
				Destination = Candidate;
			}
		}
	}

	if (!Destination.IsSet())
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s hits the edge"), *Unit->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, KeepersTerrain::BumpDamage);
		return true;
	}

	uint8 DestinationType = 0;
	KeepersTerrain::GetTileType(Grid, Destination.GetValue(), DestinationType);
	if (UObject* Blocker = KeepersTerrain::GetUnitOnTile(Grid, Destination.GetValue()))
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s bumps into %s"), *Unit->GetName(), *Blocker->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, KeepersTerrain::BumpDamage);
		DamageUnit(CombatSystem, Blocker, KeepersTerrain::BumpDamage);
		return true;
	}
	if (DestinationType == KeepersTerrain::ObstacleTileType)
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s hits a rock"), *Unit->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, KeepersTerrain::BumpDamage);
		return true;
	}

	KeepersReflection::FCall Move(CombatSystem, TEXT("SetUnitIndexOnGrid"));
	Move.Set<UObject*>(TEXT("Unit"), Unit);
	Move.Set<FIntPoint>(TEXT("Index"), Destination.GetValue());
	Move.Set<bool>(TEXT("Force"), true);
	Move.Set<bool>(TEXT("SnapLocation"), true);
	Move.Call();

	if (!KeepersTerrain::CanStandOn(Unit, DestinationType))
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s falls"), *Unit->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Falls);
		DamageUnit(CombatSystem, Unit, 9999);
	}
	else
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s to (%d,%d)"), *Unit->GetName(), Destination->X, Destination->Y);
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Pushed, 0.7f);
	}
	return true;
}

void UKeepersTerrainSubsystem::DamageUnit(UObject* CombatSystem, UObject* Unit, int32 Amount)
{
	KeepersReflection::FCall Modify(Unit, TEXT("ModifyCurrentHp"));
	Modify.Set<int32>(TEXT("Modifier"), -Amount);
	Modify.Set<bool>(TEXT("ShowNumber"), true);
	Modify.Call();

	bool bAlive = true;
	KeepersReflection::FCall IsAlive(Unit, TEXT("IsAlive"));
	if (IsAlive.Call())
	{
		IsAlive.Get<bool>(TEXT("IsAlive"), bAlive);
	}
	if (!bAlive && CombatSystem)
	{
		KeepersReflection::FCall Death(CombatSystem, TEXT("ProcessUnitDeath"));
		Death.Set<UObject*>(TEXT("Unit"), Unit);
		Death.Call();
	}
}
