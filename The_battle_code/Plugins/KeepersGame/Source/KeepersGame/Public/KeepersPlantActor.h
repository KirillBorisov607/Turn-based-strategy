#pragma once

#include "GameFramework/Actor.h"
#include "KeepersPlantActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * A tree or bush grown by the Druid. Pops out of the ground with a small overshoot.
 * Trees block line of sight (the LineOfSight trace channel); bushes do not.
 */
UCLASS(NotPlaceable)
class KEEPERSGAME_API AKeepersPlantActor : public AActor
{
	GENERATED_BODY()

public:
	AKeepersPlantActor();

	void Grow(UStaticMesh* Mesh, float TargetScale, bool bBlocksLineOfSight);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Plant")
	float GrowDuration = 0.5f;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	float FinalScale = 1.f;
	float Elapsed = 0.f;
};
