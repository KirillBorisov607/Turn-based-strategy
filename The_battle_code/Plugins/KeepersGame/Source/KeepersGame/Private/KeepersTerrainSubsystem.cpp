#include "KeepersTerrainSubsystem.h"

#include "Engine/World.h"
#include "KeepersReflection.h"
#include "KeepersRockActor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersTerrain, Log, All);

namespace KeepersTerrain
{
	static const FString RaiseRockAnimation = TEXT("BP_SpellAnim_RaiseRock");

	/** E_TileType::Obstacle (NewEnumerator2). */
	static constexpr uint8 ObstacleTileType = 2;

	/** Delay so the column rises when the spell animation reaches the tile. */
	static constexpr float RaiseDelay = 0.35f;
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
	if (!Actor || !Actor->GetClass()->GetName().StartsWith(KeepersTerrain::RaiseRockAnimation))
	{
		return;
	}

	// Exposed-on-spawn variables are set after the spawn callback, so read them on the next frames.
	TWeakObjectPtr<AActor> WeakAnimation(Actor);
	FTimerHandle Timer;
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakAnimation]()
	{
		AActor* Animation = WeakAnimation.Get();
		if (!Animation)
		{
			return;
		}
		UObject* Grid = KeepersReflection::GetObject(Animation, TEXT("Grid"));
		FIntPoint Target;
		if (Grid && KeepersReflection::GetIntPoint(Animation->GetClass(), Animation, TEXT("TargetIndex"), Target))
		{
			RaiseRock(Grid, Target);
		}
	}), KeepersTerrain::RaiseDelay, false);
}

bool UKeepersTerrainSubsystem::RaiseRock(UObject* Grid, FIntPoint Index, int32 Levels)
{
	if (!Grid)
	{
		return false;
	}

	FMapProperty* TilesProperty = CastField<FMapProperty>(KeepersReflection::FindMember(Grid->GetClass(), TEXT("GridTiles")));
	const FStructProperty* TileProperty = TilesProperty ? CastField<FStructProperty>(TilesProperty->ValueProp) : nullptr;
	if (!TileProperty)
	{
		return false;
	}
	const UScriptStruct* TileStruct = TileProperty->Struct;

	FScriptMapHelper Tiles(TilesProperty, TilesProperty->ContainerPtrToValuePtr<void>(Grid));
	const uint8* Tile = nullptr;
	for (FScriptMapHelper::FIterator It(Tiles); It; ++It)
	{
		if (*reinterpret_cast<const FIntPoint*>(Tiles.GetKeyPtr(It)) == Index)
		{
			Tile = Tiles.GetValuePtr(It);
			break;
		}
	}
	if (!Tile)
	{
		return false;
	}

	// Work on a copy: AddGridTile replaces the tile's data in the map.
	TArray<uint8> Data;
	Data.SetNumZeroed(TileStruct->GetStructureSize());
	TileStruct->InitializeStruct(Data.GetData());
	TileStruct->CopyScriptStruct(Data.GetData(), Tile);

	FProperty* UnitProperty = nullptr;
	void* UnitPtr = KeepersReflection::FindValuePtr(TileStruct, Data.GetData(), TEXT("UnitOnTile"), &UnitProperty);
	const FObjectPropertyBase* UnitObject = CastField<FObjectPropertyBase>(UnitProperty);
	if (UnitObject && UnitObject->GetObjectPropertyValue(UnitPtr))
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("RaiseRock: tile (%d,%d) is occupied"), Index.X, Index.Y);
		TileStruct->DestroyStruct(Data.GetData());
		return false;
	}

	FProperty* TypeProperty = nullptr;
	void* TypePtr = KeepersReflection::FindValuePtr(TileStruct, Data.GetData(), TEXT("Type"), &TypeProperty);
	if (FByteProperty* ByteType = CastField<FByteProperty>(TypeProperty))
	{
		ByteType->SetPropertyValue(TypePtr, KeepersTerrain::ObstacleTileType);
	}

	FTransform TileTransform = FTransform::Identity;
	if (const void* TransformPtr = KeepersReflection::FindValuePtr(TileStruct, Data.GetData(), TEXT("Transform")))
	{
		TileTransform = *static_cast<const FTransform*>(TransformPtr);
	}

	KeepersReflection::FCall AddTile(Grid, TEXT("AddGridTile"));
	AddTile.SetRaw(TEXT("Data"), Data.GetData());
	AddTile.Call();
	TileStruct->DestroyStruct(Data.GetData());

	FVector TileSize(200.f, 200.f, 100.f);
	if (const void* SizePtr = KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTileSize")))
	{
		TileSize = *static_cast<const FVector*>(SizePtr);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKeepersRockActor* Rock = GetWorld()->SpawnActor<AKeepersRockActor>(TileTransform.GetLocation(), TileTransform.Rotator(), Params);
	if (Rock)
	{
		Rock->SetActorScale3D(FVector(TileSize.X / 200.f * 0.95f, TileSize.Y / 200.f * 0.95f, 1.f));
		Rock->Rise(TileTransform.GetLocation(), TileTransform.GetLocation().Z + TileSize.Z * Levels);
		Rocks.Add(Rock);
	}

	UE_LOG(LogKeepersTerrain, Log, TEXT("RaiseRock at (%d,%d)"), Index.X, Index.Y);
	return true;
}
