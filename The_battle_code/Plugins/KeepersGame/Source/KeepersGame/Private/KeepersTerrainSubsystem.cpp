#include "KeepersTerrainSubsystem.h"

#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "KeepersReflection.h"
#include "KeepersBarks.h"
#include "KeepersCampaign.h"
#include "KeepersJuice.h"
#include "KeepersPlantActor.h"
#include "KeepersRockActor.h"
#include "KeepersSmokeActor.h"
#include "KeepersSurfaces.h"
#include "Algo/Find.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersTerrain, Log, All);

namespace KeepersTerrain
{
	static const FString RaiseRockAnimation = TEXT("BP_SpellAnim_RaiseRock");
	/** Spell animations whose hit pushes the target. */
	static const TArray<FString> PushAnimations = { TEXT("BP_SpellAnim_Gust"), TEXT("BP_SpellAnim_ShieldBash"), TEXT("BP_SpellAnim_RamStrike") };
	static const FString SproutAnimation = TEXT("BP_SpellAnim_Sprout");
	static const FString RootsAnimation = TEXT("BP_SpellAnim_Roots");
	/** Animations of the heroes' newer abilities and their effects. */
	static const TPair<const TCHAR*, EKeepersTerrainEffect> HeroAnimations[] =
	{
		{ TEXT("BP_SpellAnim_Fireball"), EKeepersTerrainEffect::Fireball },
		{ TEXT("BP_SpellAnim_IcePath"), EKeepersTerrainEffect::IcePath },
		{ TEXT("BP_SpellAnim_ChainLightning"), EKeepersTerrainEffect::ChainLightning },
		{ TEXT("BP_SpellAnim_Dash"), EKeepersTerrainEffect::Dash },
		{ TEXT("BP_SpellAnim_Backstab"), EKeepersTerrainEffect::Backstab },
		{ TEXT("BP_SpellAnim_SmokeBomb"), EKeepersTerrainEffect::SmokeBomb },
		{ TEXT("BP_SpellAnim_Crush"), EKeepersTerrainEffect::Crush },
		{ TEXT("BP_SpellAnim_Throw"), EKeepersTerrainEffect::Throw },
		{ TEXT("BP_SpellAnim_Rage"), EKeepersTerrainEffect::Rage },
		{ TEXT("BP_SpellAnim_Charge"), EKeepersTerrainEffect::Dash },
		{ TEXT("BP_SpellAnim_Boulder"), EKeepersTerrainEffect::Boulder },
	};
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

	static int32 TeamOf(const UObject* Unit)
	{
		int32 Team = 0;
		if (Unit)
		{
			KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		}
		return Team;
	}

	static FVector GetTileSize(UObject* Grid);

	/** Cells next to Index (any grid shape): within 1.2 cell sizes. */
	static TArray<FIntPoint> Adjacent(UObject* Grid, const FIntPoint& Index, float Range = 1.2f)
	{
		TArray<FIntPoint> Result;
		FVector Center;
		if (!GetTileLocation(Grid, Index, Center))
		{
			return Result;
		}
		const float Size = GetTileSize(Grid).X;
		const int32 Reach = FMath::CeilToInt(Range * 2.f);
		for (int32 DX = -Reach; DX <= Reach; ++DX)
		{
			for (int32 DY = -Reach; DY <= Reach; ++DY)
			{
				const FIntPoint Candidate(Index.X + DX, Index.Y + DY);
				FVector Location;
				if (Candidate != Index && GetTileLocation(Grid, Candidate, Location) && (Location - Center).Size2D() <= Size * Range)
				{
					Result.Add(Candidate);
				}
			}
		}
		return Result;
	}

	/** The neighbour of Index that lies furthest along Direction (alignment above 0.5), if any. */
	static TOptional<FIntPoint> NeighbourAlong(UObject* Grid, const FIntPoint& Index, const FVector& Direction)
	{
		FVector Center;
		TOptional<FIntPoint> Best;
		if (!GetTileLocation(Grid, Index, Center))
		{
			return Best;
		}
		float BestAlignment = 0.5f;
		for (const FIntPoint& Candidate : Adjacent(Grid, Index))
		{
			FVector Location;
			GetTileLocation(Grid, Candidate, Location);
			const float Alignment = FVector::DotProduct((Location - Center).GetSafeNormal2D(), Direction);
			if (Alignment > BestAlignment)
			{
				BestAlignment = Alignment;
				Best = Candidate;
			}
		}
		return Best;
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

UObject* UKeepersTerrainSubsystem::UnitOnTile(UObject* Grid, const FIntPoint& Index)
{
	return KeepersTerrain::GetUnitOnTile(Grid, Index);
}

bool UKeepersTerrainSubsystem::TileType(UObject* Grid, const FIntPoint& Index, uint8& OutType)
{
	return KeepersTerrain::GetTileType(Grid, Index, OutType);
}

bool UKeepersTerrainSubsystem::TileLocation(UObject* Grid, const FIntPoint& Index, FVector& OutLocation)
{
	return KeepersTerrain::GetTileLocation(Grid, Index, OutLocation);
}

TOptional<FIntPoint> UKeepersTerrainSubsystem::NextAlong(UObject* Grid, const FIntPoint& Index, const FVector& Direction)
{
	return KeepersTerrain::NeighbourAlong(Grid, Index, Direction.GetSafeNormal2D());
}

bool UKeepersTerrainSubsystem::CanUnitStandOn(UObject* Unit, uint8 Type)
{
	return Unit && KeepersTerrain::CanStandOn(Unit, Type);
}

UObject* UKeepersTerrainSubsystem::CombatSystemOf(UWorld* World)
{
	return KeepersTerrain::FindCombatSystem(World);
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
	else if (const TPair<const TCHAR*, EKeepersTerrainEffect>* Hero = Algo::FindByPredicate(KeepersTerrain::HeroAnimations,
		[&ClassName](const TPair<const TCHAR*, EKeepersTerrainEffect>& Entry) { return ClassName.StartsWith(Entry.Key); }))
	{
		PendingEffects.Add(Actor, Hero->Value);
	}
	else if (ClassName.StartsWith(TEXT("BP_SpellAnim_")))
	{
		// Any other blow: the world may still react (a tree by the target).
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Blow);
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

	// What the eye sees on the target tile: dust of a rock, leaves of a sprout, splinters of roots.
	FVector TargetLocation;
	const bool bHasLocation = KeepersTerrain::GetTileLocation(Grid, Target, TargetLocation);
	switch (Effect)
	{
	case EKeepersTerrainEffect::RaiseRock:
		if (RaiseRock(Grid, Target, UKeepersCampaignSubsystem::HasArtifact(this, TEXT("AncestorStone")) ? 3 : 2))
		{
			if (bHasLocation)
			{
				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Puff, FLinearColor(0.62f, 0.55f, 0.45f), 14, 0.9f);
				UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit, FLinearColor(0.5f, 0.5f, 0.52f), 12);
			}
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Origin)), EKeepersBarkSituation::RaisesRock, 0.8f);
			const UObject* Caster = KeepersTerrain::GetUnitOnTile(Grid, Origin);
			TryRockfall(Grid, Target, Caster ? KeepersTerrain::TeamOf(Caster) : 0);
		}
		break;
	case EKeepersTerrainEffect::Push:
		FellTree(Grid, Origin, Target);
		PushUnit(KeepersTerrain::FindCombatSystem(GetWorld()), Origin, Target);
		break;
	case EKeepersTerrainEffect::Blow:
		FellTree(Grid, Origin, Target);
		break;
	case EKeepersTerrainEffect::Roots:
		if (UObject* Rooted = KeepersTerrain::GetUnitOnTile(Grid, Target))
		{
			if (bHasLocation)
			{
				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 30.f), AKeepersBurst::EKind::Hit, FLinearColor(0.42f, 0.27f, 0.12f), 14);
				UKeepersJuiceSubsystem::Sound(this, TEXT("Leaves"), 0.8f);
			}
			KeepersReflection::FCall SetMp(Rooted, TEXT("SetCurrentMp"));
			SetMp.Set<int32>(TEXT("Mp"), 0);
			SetMp.Set<bool>(TEXT("ShowNumber"), false);
			SetMp.Call();
		}
		break;
	case EKeepersTerrainEffect::Fireball:
	case EKeepersTerrainEffect::IcePath:
	case EKeepersTerrainEffect::ChainLightning:
	case EKeepersTerrainEffect::Dash:
	case EKeepersTerrainEffect::Backstab:
	case EKeepersTerrainEffect::SmokeBomb:
	case EKeepersTerrainEffect::Crush:
	case EKeepersTerrainEffect::Throw:
	case EKeepersTerrainEffect::Rage:
		if (Effect == EKeepersTerrainEffect::Backstab)
		{
			FellTree(Grid, Origin, Target);
		}
		ApplyHeroAbility(Effect, Grid, Origin, Target);
		break;
	case EKeepersTerrainEffect::Boulder:
		// The boulder stays where it fell when the cell was empty.
		if (!KeepersTerrain::GetUnitOnTile(Grid, Target) && RaiseRock(Grid, Target, 1) && bHasLocation)
		{
			UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Puff, FLinearColor(0.62f, 0.55f, 0.45f), 16, 1.f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
		}
		break;
	case EKeepersTerrainEffect::Sprout:
		if (GrowForest(Grid, Target))
		{
			if (bHasLocation)
			{
				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 40.f), AKeepersBurst::EKind::Heal, FLinearColor(0.35f, 0.85f, 0.25f), 16, 1.2f);
				UKeepersJuiceSubsystem::Sound(this, TEXT("Leaves"));
				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 80.f), AKeepersBurst::EKind::Hit, FLinearColor(0.2f, 0.6f, 0.15f), 10);
			}
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
	RockTiles.Add(Index);

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
	UObject* Occupant = KeepersTerrain::GetUnitOnTile(Grid, Index);
	const bool bOccupied = Occupant != nullptr;
	FTransform TileTransform;
	if (!SetTileType(Grid, Index, KeepersTerrain::ForestTileType, TileTransform))
	{
		return false;
	}
	ForestTiles.Add(Index);
	if (Occupant && UKeepersCampaignSubsystem::HasArtifact(this, TEXT("OakHeart")))
	{
		int32 Team = -1;
		KeepersReflection::GetInt(Occupant->GetClass(), Occupant, TEXT("TeamIndex"), Team);
		if (Team == 0)
		{
			KeepersReflection::FCall Heal(Occupant, TEXT("ModifyCurrentHp"));
			Heal.Set<int32>(TEXT("Modifier"), 3);
			Heal.Set<bool>(TEXT("ShowNumber"), true);
			Heal.Call();
		}
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
	const TOptional<FIntPoint> Destination = KeepersTerrain::NeighbourAlong(Grid, Target, PushDirection);
	return ForcedMove(CombatSystem, Grid, Unit, Destination, UKeepersCampaignSubsystem::HasArtifact(this, TEXT("StormFeather")) ? 1 : 0);
}

bool UKeepersTerrainSubsystem::ForcedMove(UObject* CombatSystem, UObject* Grid, UObject* Unit, const TOptional<FIntPoint>& Destination, int32 FeatherBonus)
{
	const int32 BumpDamage = KeepersTerrain::BumpDamage + FeatherBonus;
	if (SlideDepth == 0)
	{
		if (const AActor* Start = Cast<AActor>(Unit))
		{
			LastPushFrom = Start->GetActorLocation();
		}
	}
	if (!Destination.IsSet())
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s hits the edge"), *Unit->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, BumpDamage);
		return true;
	}

	uint8 DestinationType = 0;
	KeepersTerrain::GetTileType(Grid, Destination.GetValue(), DestinationType);
	if (UObject* Blocker = KeepersTerrain::GetUnitOnTile(Grid, Destination.GetValue()))
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s bumps into %s"), *Unit->GetName(), *Blocker->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, BumpDamage);
		DamageUnit(CombatSystem, Blocker, BumpDamage);
		return true;
	}
	if (DestinationType == KeepersTerrain::ObstacleTileType)
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s hits a rock"), *Unit->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, BumpDamage + (UKeepersCampaignSubsystem::HasArtifact(this, TEXT("AncestorStone")) ? 1 : 0));
		// A rock raised in battle is loose: the blow sends it rolling.
		FVector UnitLocation;
		FVector RockLocation;
		if (const AActor* Pushed = Cast<AActor>(Unit))
		{
			if (KeepersTerrain::GetTileLocation(Grid, Destination.GetValue(), RockLocation))
			{
				UnitLocation = Pushed->GetActorLocation();
				RollRock(CombatSystem, Grid, Destination.GetValue(), (RockLocation - UnitLocation).GetSafeNormal2D());
			}
		}
		return true;
	}

	// A gust: a puff of air where the unit stood.
	if (const AActor* Pushed = Cast<AActor>(Unit))
	{
		UKeepersJuiceSubsystem::Burst(GetWorld(), Pushed->GetActorLocation(), AKeepersBurst::EKind::Puff, FLinearColor(0.85f, 0.92f, 1.f), 10, 0.7f);
		UKeepersJuiceSubsystem::Sound(this, TEXT("Gust"));
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
		// A splash where it fell.
		FVector Splash;
		if (KeepersTerrain::GetTileLocation(Grid, Destination.GetValue(), Splash))
		{
			UKeepersJuiceSubsystem::Burst(GetWorld(), Splash, AKeepersBurst::EKind::Hit, FLinearColor(0.35f, 0.65f, 1.f), 20, 1.1f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"));
		}
		DamageUnit(CombatSystem, Unit, 9999);
	}
	else
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s to (%d,%d)"), *Unit->GetName(), Destination->X, Destination->Y);
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Pushed, 0.7f);
		if (FeatherBonus > 0)
		{
			DamageUnit(CombatSystem, Unit, FeatherBonus);
		}
		const UKeepersSurfaceSubsystem* Surfaces = UKeepersSurfaceSubsystem::Get(this);
		const EKeepersSurface Surface = Surfaces ? Surfaces->GetSurface(Destination.GetValue()) : EKeepersSurface::None;
		if (Surface == EKeepersSurface::Ice && KeepersTerrain::TeamOf(Unit) >= 0 && SlideDepth < 3)
		{
			// On ice the push carries on.
			FVector Here;
			const AActor* Pushed = Cast<AActor>(Unit);
			if (Pushed && KeepersTerrain::GetTileLocation(Grid, Destination.GetValue(), Here))
			{
				const FVector Direction = (Here - LastPushFrom).GetSafeNormal2D();
				if (const TOptional<FIntPoint> Next = KeepersTerrain::NeighbourAlong(Grid, Destination.GetValue(), Direction))
				{
					UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Slides, 0.8f);
					++SlideDepth;
					LastPushFrom = Here;
					ForcedMove(CombatSystem, Grid, Unit, Next, 0);
					--SlideDepth;
				}
			}
		}
		else if (Surface == EKeepersSurface::Marsh)
		{
			// The marsh holds whoever lands in it: no steps next turn.
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::StuckInMarsh, 0.8f);
			KeepersReflection::FCall SetMp(Unit, TEXT("SetCurrentMp"));
			SetMp.Set<int32>(TEXT("Mp"), 0);
			SetMp.Set<bool>(TEXT("ShowNumber"), false);
			SetMp.Call();
		}
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

void UKeepersTerrainSubsystem::TestInteraction(const UObject* WorldContext, const FString& Kind)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UKeepersTerrainSubsystem* Self = World ? World->GetSubsystem<UKeepersTerrainSubsystem>() : nullptr;
	UObject* CombatSystem = World ? KeepersTerrain::FindCombatSystem(World) : nullptr;
	UObject* Grid = CombatSystem ? KeepersReflection::GetObject(CombatSystem, TEXT("Grid")) : nullptr;
	if (!Self || !Grid)
	{
		return;
	}
	// An enemy with a free cell beside it and a free cell beyond.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		FIntPoint Foe;
		if (It->GetClass()->GetName() != TEXT("BP_Unit_C") || KeepersTerrain::TeamOf(*It) == 0 || !KeepersReflection::GetIntPoint(It->GetClass(), *It, TEXT("Index"), Foe))
		{
			continue;
		}
		FVector FoeLocation;
		KeepersTerrain::GetTileLocation(Grid, Foe, FoeLocation);
		for (const FIntPoint& Side : KeepersTerrain::Adjacent(Grid, Foe))
		{
			uint8 Type = 0;
			FVector SideLocation;
			if (KeepersTerrain::GetUnitOnTile(Grid, Side) || !KeepersTerrain::GetTileType(Grid, Side, Type) || Type != 1 || !KeepersTerrain::GetTileLocation(Grid, Side, SideLocation))
			{
				continue;
			}
			UE_LOG(LogKeepersTerrain, Log, TEXT("Test %s at (%d,%d) by (%d,%d)"), *Kind, Foe.X, Foe.Y, Side.X, Side.Y);
			if (Kind == TEXT("tree"))
			{
				Self->GrowForest(Grid, Foe);
				Self->FellTree(Grid, Side, Foe);
			}
			else if (Kind == TEXT("rock"))
			{
				Self->RaiseRock(Grid, Side, 2);
				Self->RollRock(CombatSystem, Grid, Side, (FoeLocation - SideLocation).GetSafeNormal2D());
			}
			else if (Kind == TEXT("rockfall"))
			{
				Self->RaiseRock(Grid, Side, 2);
				Self->TryRockfall(Grid, Side, 0);
			}
			return;
		}
	}
}

AActor* UKeepersTerrainSubsystem::PropAt(UObject* Grid, const FIntPoint& Index, bool bTree) const
{
	FVector Location;
	if (!KeepersTerrain::GetTileLocation(Grid, Index, Location))
	{
		return nullptr;
	}
	const float Reach = KeepersTerrain::GetTileSize(Grid).X * 0.45f;
	for (AActor* Prop : TerrainProps)
	{
		if (Prop && (bTree ? Prop->IsA<AKeepersPlantActor>() : Prop->IsA<AKeepersRockActor>()) && FVector::Dist2D(Prop->GetActorLocation(), Location) < Reach)
		{
			return Prop;
		}
	}
	return nullptr;
}

void UKeepersTerrainSubsystem::FellTree(UObject* Grid, const FIntPoint& Origin, const FIntPoint& Target)
{
	// Only blows from next to the target; the tree must stand on the target's cell.
	FVector OriginLocation;
	FVector TargetLocation;
	const float Step = KeepersTerrain::GetTileSize(Grid).X;
	if (!KeepersTerrain::GetTileLocation(Grid, Origin, OriginLocation) || !KeepersTerrain::GetTileLocation(Grid, Target, TargetLocation)
		|| FVector::Dist2D(OriginLocation, TargetLocation) > Step * 1.3f || !PropAt(Grid, Target, true))
	{
		return;
	}
	UObject* CombatSystem = KeepersTerrain::FindCombatSystem(GetWorld());
	const FVector Direction = (TargetLocation - OriginLocation).GetSafeNormal2D();
	UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 120.f), AKeepersBurst::EKind::Hit, FLinearColor(0.3f, 0.6f, 0.2f), 20, 1.3f);
	UKeepersJuiceSubsystem::Sound(this, TEXT("Leaves"));
	ClearProp(Grid, Target);
	UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Origin)), EKeepersBarkSituation::TreeFalls);
	// It falls away from the blow: whoever stands there is hit, the trunk becomes a thicket.
	if (const TOptional<FIntPoint> Behind = KeepersTerrain::NeighbourAlong(Grid, Target, Direction))
	{
		FVector BehindLocation;
		if (KeepersTerrain::GetTileLocation(Grid, Behind.GetValue(), BehindLocation))
		{
			UKeepersJuiceSubsystem::Burst(GetWorld(), BehindLocation + FVector(0.f, 0.f, 30.f), AKeepersBurst::EKind::Puff, FLinearColor(0.45f, 0.35f, 0.2f), 14, 1.1f);
		}
		if (UObject* Under = KeepersTerrain::GetUnitOnTile(Grid, Behind.GetValue()))
		{
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Under), EKeepersBarkSituation::Bumped);
			DamageUnit(CombatSystem, Under, 3);
		}
		GrowForest(Grid, Behind.GetValue());
	}
	UE_LOG(LogKeepersTerrain, Log, TEXT("A tree falls at (%d,%d)"), Target.X, Target.Y);
}

bool UKeepersTerrainSubsystem::RollRock(UObject* CombatSystem, UObject* Grid, const FIntPoint& Rock, const FVector& Direction)
{
	if (!PropAt(Grid, Rock, false))
	{
		return false;
	}
	const TOptional<FIntPoint> Next = KeepersTerrain::NeighbourAlong(Grid, Rock, Direction);
	uint8 Type = 0;
	if (!Next || !KeepersTerrain::GetTileType(Grid, Next.GetValue(), Type) || Type == KeepersTerrain::ObstacleTileType)
	{
		return false;
	}
	FVector From;
	FVector To;
	KeepersTerrain::GetTileLocation(Grid, Rock, From);
	KeepersTerrain::GetTileLocation(Grid, Next.GetValue(), To);
	ClearProp(Grid, Rock);
	UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Rock)), EKeepersBarkSituation::RockRolls);
	UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
	if (UObject* Under = KeepersTerrain::GetUnitOnTile(Grid, Next.GetValue()))
	{
		// It rolls over a unit and breaks.
		UKeepersJuiceSubsystem::Burst(GetWorld(), To + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit, FLinearColor(0.5f, 0.5f, 0.52f), 22, 1.3f);
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Under), EKeepersBarkSituation::Rockfall);
		DamageUnit(CombatSystem, Under, 4);
	}
	else if (Type == KeepersTerrain::ChasmTileType)
	{
		// Into the water or the pit: gone with a splash.
		UKeepersJuiceSubsystem::Burst(GetWorld(), To, AKeepersBurst::EKind::Hit, FLinearColor(0.35f, 0.65f, 1.f), 20, 1.1f);
		UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"));
	}
	else
	{
		RaiseRock(Grid, Next.GetValue(), 1);
	}
	UE_LOG(LogKeepersTerrain, Log, TEXT("A rock rolls from (%d,%d) to (%d,%d)"), Rock.X, Rock.Y, Next->X, Next->Y);
	return true;
}

void UKeepersTerrainSubsystem::TryRockfall(UObject* Grid, const FIntPoint& Rock, int32 CasterTeam)
{
	FVector Top;
	if (!KeepersTerrain::GetTileLocation(Grid, Rock, Top))
	{
		return;
	}
	const float Step = KeepersTerrain::GetTileSize(Grid).Z;
	UObject* CombatSystem = KeepersTerrain::FindCombatSystem(GetWorld());
	for (const FIntPoint& Below : KeepersTerrain::Adjacent(Grid, Rock))
	{
		FVector Location;
		UObject* Foe = KeepersTerrain::GetUnitOnTile(Grid, Below);
		if (Foe && KeepersTerrain::TeamOf(Foe) != CasterTeam && KeepersTerrain::GetTileLocation(Grid, Below, Location) && Top.Z - Location.Z >= Step * 1.5f)
		{
			// The rock was raised on the edge: it topples onto the foe below.
			ClearProp(Grid, Rock);
			UKeepersJuiceSubsystem::Burst(GetWorld(), Location + FVector(0.f, 0.f, 80.f), AKeepersBurst::EKind::Hit, FLinearColor(0.55f, 0.5f, 0.45f), 26, 1.4f);
			UKeepersJuiceSubsystem::Burst(GetWorld(), Location, AKeepersBurst::EKind::Puff, FLinearColor(0.62f, 0.55f, 0.45f), 18, 1.2f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Foe), EKeepersBarkSituation::Rockfall);
			DamageUnit(CombatSystem, Foe, 5);
			UE_LOG(LogKeepersTerrain, Log, TEXT("Rockfall from (%d,%d) onto (%d,%d)"), Rock.X, Rock.Y, Below.X, Below.Y);
			return;
		}
	}
}

void UKeepersTerrainSubsystem::MeltIce(UObject* Grid, const TArray<FIntPoint>& Area)
{
	UObject* CombatSystem = KeepersTerrain::FindCombatSystem(GetWorld());
	for (const FIntPoint& Index : Area)
	{
		if (!IceTiles.Contains(Index))
		{
			continue;
		}
		// The plate goes, the water comes back.
		FVector Location;
		KeepersTerrain::GetTileLocation(Grid, Index, Location);
		for (int32 I = TerrainProps.Num() - 1; I >= 0; --I)
		{
			AActor* Prop = TerrainProps[I];
			if (Prop && Prop->IsA<AStaticMeshActor>() && FVector::Dist2D(Prop->GetActorLocation(), Location) < 30.f)
			{
				Prop->Destroy();
				TerrainProps.RemoveAt(I);
			}
		}
		FTransform Unused;
		SetTileType(Grid, Index, KeepersTerrain::ChasmTileType, Unused);
		IceTiles.Remove(Index);
		UKeepersJuiceSubsystem::Burst(GetWorld(), Location + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Puff, FLinearColor(0.85f, 0.92f, 1.f), 14, 1.f);
		UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"), 0.7f);
		if (UObject* Unit = KeepersTerrain::GetUnitOnTile(Grid, Index))
		{
			if (!KeepersTerrain::CanStandOn(Unit, KeepersTerrain::ChasmTileType))
			{
				UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::IceMelts);
				DamageUnit(CombatSystem, Unit, 9999);
			}
		}
		UE_LOG(LogKeepersTerrain, Log, TEXT("Ice melts at (%d,%d)"), Index.X, Index.Y);
	}
}

bool UKeepersTerrainSubsystem::FreezeWater(UObject* Grid, const FIntPoint& Index)
{
	uint8 Type = 0;
	FTransform Tile;
	if (!KeepersTerrain::GetTileType(Grid, Index, Type) || Type != KeepersTerrain::ChasmTileType || !SetTileType(Grid, Index, 1, Tile))
	{
		return false;
	}
	const float Size = KeepersTerrain::GetTileSize(Grid).X;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AStaticMeshActor* Ice = GetWorld()->SpawnActor<AStaticMeshActor>(Tile.GetLocation() + FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, Params))
	{
		Ice->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Ice->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Ice->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ice->SetActorScale3D(FVector(Size / 100.f * 0.48f, Size / 100.f * 0.48f, 0.08f));
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), Ice))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.9f, 1.f));
			Ice->GetStaticMeshComponent()->SetMaterial(0, Material);
		}
		TerrainProps.Add(Ice);
	}
	IceTiles.Add(Index);
	// Frozen water is ice: who stops on it slides.
	if (UKeepersSurfaceSubsystem* Surfaces = UKeepersSurfaceSubsystem::Get(this))
	{
		Surfaces->Paint(Index, EKeepersSurface::Ice, false);
	}
	return true;
}

bool UKeepersTerrainSubsystem::ClearProp(UObject* Grid, const FIntPoint& Index)
{
	FVector Location;
	if (!KeepersTerrain::GetTileLocation(Grid, Index, Location))
	{
		return false;
	}
	const float Reach = KeepersTerrain::GetTileSize(Grid).X * 0.45f;
	bool bCleared = false;
	for (int32 I = TerrainProps.Num() - 1; I >= 0; --I)
	{
		AActor* Prop = TerrainProps[I];
		const bool bRockOrTree = Prop && (Prop->IsA<AKeepersRockActor>() || Prop->IsA<AKeepersPlantActor>());
		if (bRockOrTree && FVector::Dist2D(Prop->GetActorLocation(), Location) < Reach)
		{
			UKeepersJuiceSubsystem::Burst(GetWorld(), Prop->GetActorLocation() + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit,
				Prop->IsA<AKeepersRockActor>() ? FLinearColor(0.5f, 0.5f, 0.52f) : FLinearColor(0.25f, 0.55f, 0.2f), 16, 1.2f);
			Prop->Destroy();
			TerrainProps.RemoveAt(I);
			bCleared = true;
		}
	}
	if (bCleared || ForestTiles.Contains(Index))
	{
		FTransform Unused;
		SetTileType(Grid, Index, 1, Unused);
		ForestTiles.Remove(Index);
		RockTiles.Remove(Index);
		bCleared = true;
	}
	return bCleared;
}

void UKeepersTerrainSubsystem::ApplyHeroAbility(EKeepersTerrainEffect Effect, UObject* Grid, const FIntPoint& Origin, const FIntPoint& Target)
{
	UObject* CombatSystem = KeepersTerrain::FindCombatSystem(GetWorld());
	UObject* Caster = KeepersTerrain::GetUnitOnTile(Grid, Origin);
	UObject* Victim = KeepersTerrain::GetUnitOnTile(Grid, Target);
	const int32 Team = KeepersTerrain::TeamOf(Caster);
	FVector TargetLocation = FVector::ZeroVector;
	FVector OriginLocation = FVector::ZeroVector;
	KeepersTerrain::GetTileLocation(Grid, Target, TargetLocation);
	KeepersTerrain::GetTileLocation(Grid, Origin, OriginLocation);
	TArray<FIntPoint> Area = KeepersTerrain::Adjacent(Grid, Target);
	Area.Insert(Target, 0);
	UWorld* World = GetWorld();
	UE_LOG(LogKeepersTerrain, Log, TEXT("Hero ability %d from (%d,%d) at (%d,%d)"), static_cast<int32>(Effect), Origin.X, Origin.Y, Target.X, Target.Y);

	switch (Effect)
	{
	case EKeepersTerrainEffect::Fireball:
		// The blast; forest in it burns to the ground.
		UKeepersJuiceSubsystem::Burst(World, TargetLocation + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit, FLinearColor(1.f, 0.45f, 0.1f), 24, 1.3f);
		UKeepersJuiceSubsystem::Burst(World, TargetLocation + FVector(0.f, 0.f, 30.f), AKeepersBurst::EKind::Puff, FLinearColor(0.3f, 0.28f, 0.26f), 12, 1.f);
		UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"), 0.8f);
		for (const FIntPoint& Index : Area)
		{
			if (ForestTiles.Contains(Index))
			{
				ClearProp(Grid, Index);
			}
		}
		MeltIce(Grid, Area);
		break;
	case EKeepersTerrainEffect::IcePath:
	{
		// The target and the next cell along the cast freeze.
		TArray<FIntPoint> Path = { Target };
		if (const TOptional<FIntPoint> Next = KeepersTerrain::NeighbourAlong(Grid, Target, (TargetLocation - OriginLocation).GetSafeNormal2D()))
		{
			Path.Add(Next.GetValue());
		}
		for (const FIntPoint& Index : Path)
		{
			FVector Location;
			if (FreezeWater(Grid, Index) && KeepersTerrain::GetTileLocation(Grid, Index, Location))
			{
				UKeepersJuiceSubsystem::Burst(World, Location + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Hit, FLinearColor(0.8f, 0.95f, 1.f), 14);
				UE_LOG(LogKeepersTerrain, Log, TEXT("IcePath freezes (%d,%d)"), Index.X, Index.Y);
			}
		}
		UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"), 0.6f);
		break;
	}
	case EKeepersTerrainEffect::ChainLightning:
	{
		// The bolt jumps to the two nearest foes within two cells of the target.
		TArray<TPair<float, UObject*>> Foes;
		for (const FIntPoint& Index : KeepersTerrain::Adjacent(Grid, Target, 2.3f))
		{
			UObject* Unit = KeepersTerrain::GetUnitOnTile(Grid, Index);
			FVector Location;
			if (Unit && Unit != Victim && KeepersTerrain::TeamOf(Unit) != Team && KeepersTerrain::GetTileLocation(Grid, Index, Location))
			{
				Foes.Emplace(FVector::Dist2D(Location, TargetLocation), Unit);
			}
		}
		Foes.Sort([](const TPair<float, UObject*>& A, const TPair<float, UObject*>& B) { return A.Key < B.Key; });
		UKeepersJuiceSubsystem::Burst(World, TargetLocation + FVector(0.f, 0.f, 80.f), AKeepersBurst::EKind::Hit, FLinearColor(1.f, 0.95f, 0.5f), 16);
		for (int32 I = 0; I < FMath::Min(Foes.Num(), 2); ++I)
		{
			if (const AActor* Struck = Cast<AActor>(Foes[I].Value))
			{
				UKeepersJuiceSubsystem::Burst(World, Struck->GetActorLocation() + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit, FLinearColor(0.7f, 0.85f, 1.f), 14);
			}
			DamageUnit(CombatSystem, Foes[I].Value, 3);
		}
		// Water carries it: foes standing in the marsh near the strike take 2 more.
		if (const UKeepersSurfaceSubsystem* Surfaces = UKeepersSurfaceSubsystem::Get(this))
		{
			for (const FIntPoint& Index : KeepersTerrain::Adjacent(Grid, Target, 2.3f))
			{
				UObject* Wet = KeepersTerrain::GetUnitOnTile(Grid, Index);
				if (Wet && KeepersTerrain::TeamOf(Wet) != Team && Surfaces->GetSurface(Index) == EKeepersSurface::Marsh)
				{
					UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Wet), EKeepersBarkSituation::Shocked);
					DamageUnit(CombatSystem, Wet, 2);
				}
			}
		}
		UKeepersJuiceSubsystem::Sound(this, TEXT("Hit"));
		break;
	}
	case EKeepersTerrainEffect::Dash:
	{
		uint8 Type = 0;
		if (!Caster || Victim || !KeepersTerrain::GetTileType(Grid, Target, Type) || !KeepersTerrain::CanStandOn(Caster, Type))
		{
			break;
		}
		if (const AActor* Runner = Cast<AActor>(Caster))
		{
			UKeepersJuiceSubsystem::Burst(World, Runner->GetActorLocation(), AKeepersBurst::EKind::Puff, FLinearColor(0.8f, 0.78f, 0.72f), 10, 0.7f);
		}
		KeepersReflection::FCall Move(CombatSystem, TEXT("SetUnitIndexOnGrid"));
		Move.Set<UObject*>(TEXT("Unit"), Caster);
		Move.Set<FIntPoint>(TEXT("Index"), Target);
		Move.Set<bool>(TEXT("Force"), true);
		Move.Set<bool>(TEXT("SnapLocation"), true);
		Move.Call();
		UKeepersJuiceSubsystem::Sound(this, TEXT("Gust"));
		// Foes next to the landing get a slash on the way.
		for (const FIntPoint& Index : KeepersTerrain::Adjacent(Grid, Target))
		{
			UObject* Unit = KeepersTerrain::GetUnitOnTile(Grid, Index);
			if (Unit && KeepersTerrain::TeamOf(Unit) != Team)
			{
				DamageUnit(CombatSystem, Unit, 2);
			}
		}
		UE_LOG(LogKeepersTerrain, Log, TEXT("Dash to (%d,%d)"), Target.X, Target.Y);
		break;
	}
	case EKeepersTerrainEffect::Backstab:
	{
		if (!Victim || KeepersTerrain::TeamOf(Victim) == Team)
		{
			break;
		}
		// Flanked: another of the Rogue's side stands next to the victim.
		for (const FIntPoint& Index : KeepersTerrain::Adjacent(Grid, Target))
		{
			UObject* Ally = KeepersTerrain::GetUnitOnTile(Grid, Index);
			if (Ally && Ally != Caster && KeepersTerrain::TeamOf(Ally) == Team)
			{
				UKeepersJuiceSubsystem::Burst(World, TargetLocation + FVector(0.f, 0.f, 80.f), AKeepersBurst::EKind::Hit, FLinearColor(0.85f, 0.1f, 0.15f), 18);
				DamageUnit(CombatSystem, Victim, 4);
				UE_LOG(LogKeepersTerrain, Log, TEXT("Backstab: flanked, double hit"));
				break;
			}
		}
		break;
	}
	case EKeepersTerrainEffect::SmokeBomb:
		for (const FIntPoint& Index : Area)
		{
			FVector Location;
			if (KeepersTerrain::GetTileLocation(Grid, Index, Location))
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AKeepersSmokeActor* Smoke = World->SpawnActor<AKeepersSmokeActor>(Location, FRotator::ZeroRotator, Params))
				{
					Smoke->Setup(KeepersTerrain::GetTileSize(Grid).X, 2);
				}
			}
		}
		UKeepersJuiceSubsystem::Sound(this, TEXT("Puff"));
		break;
	case EKeepersTerrainEffect::Crush:
		for (const FIntPoint& Index : Area)
		{
			ClearProp(Grid, Index);
		}
		UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
		break;
	case EKeepersTerrainEffect::Throw:
		if (Victim && Caster && Victim != Caster)
		{
			// Over the Barbarian's head to the cell behind it.
			ForcedMove(CombatSystem, Grid, Victim, KeepersTerrain::NeighbourAlong(Grid, Origin, (OriginLocation - TargetLocation).GetSafeNormal2D()), 0);
		}
		break;
	case EKeepersTerrainEffect::Rage:
		if (Caster)
		{
			int32 ApMax = 3;
			KeepersReflection::GetUnitStat(Caster, TEXT("AP_Max"), ApMax);
			KeepersReflection::FCall Ap(Caster, TEXT("SetCurrentAp"));
			Ap.Set<int32>(TEXT("Ap"), ApMax);
			Ap.Set<bool>(TEXT("ShowNumber"), true);
			Ap.Call();
			KeepersReflection::FCall Mp(Caster, TEXT("ModifyCurrentMp"));
			Mp.Set<int32>(TEXT("Modifier"), 1);
			Mp.Set<bool>(TEXT("ShowNumber"), true);
			Mp.Call();
			UKeepersJuiceSubsystem::Burst(World, OriginLocation + FVector(0.f, 0.f, 90.f), AKeepersBurst::EKind::Puff, FLinearColor(0.9f, 0.15f, 0.1f), 12, 0.9f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Hit"));
		}
		break;
	default:
		break;
	}
}
