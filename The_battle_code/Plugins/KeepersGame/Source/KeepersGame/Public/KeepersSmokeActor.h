#pragma once

#include "GameFramework/Actor.h"
#include "KeepersSmokeActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * A cloud of the Rogue's smoke bomb over one cell: it blocks line of sight (LineOfSight channel only) and
 * clears after two rounds of the heroes.
 */
UCLASS(NotPlaceable)
class KEEPERSGAME_API AKeepersSmokeActor : public AActor
{
	GENERATED_BODY()

public:
	AKeepersSmokeActor();

	void Setup(float CellSize, int32 InRounds);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Puffs;
	TArray<float> BaseScales;

	TWeakObjectPtr<UObject> CombatSystem;
	int32 RoundsLeft = 2;
	int32 LastTeam = INDEX_NONE;
	float Age = 0.f;
	float Fade = 1.f;
	bool bClearing = false;
};
