#pragma once

#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersBattleScenery.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Land around a battle field, so the field is an island in a lake and not a board hanging in the sky:
 * a lake right under the field's edges, hilly shores of the same low-poly hexagons, woods, a few
 * buildings of the Toony Tiny RTS Set on the hills, a ground plane to the horizon and a light haze.
 */
UCLASS(NotPlaceable)
class KEEPERSGAME_API AKeepersBattleScenery : public AActor
{
	GENERATED_BODY()

public:
	AKeepersBattleScenery();

	/** Builds everything around a field centred at Center with the given radius; WaterZ is the lake level. */
	void Build(const FVector& Center, float FieldRadius, float WaterZ, int32 Seed);

private:
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material);
	UInstancedStaticMeshComponent* AddInstanced(UStaticMesh* Mesh, UMaterialInterface* Material);
};

/** Puts AKeepersBattleScenery into battle levels and the campaign map and hides the old flat backdrop plates. */
UCLASS()
class KEEPERSGAME_API UKeepersBattleScenerySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Surrounds the level's field with scenery (once per world); the main menu calls it after its island streams in. */
	static void SurroundField(UWorld& World, int32 Retries = 30);
};
