#pragma once

#include "GameFramework/Actor.h"
#include "KeepersRockActor.generated.h"

class UStaticMeshComponent;

/**
 * A stone column raised by the Guardian. Rises out of the ground when spawned.
 * Blocks line of sight (the LineOfSight trace channel) like a wall.
 */
UCLASS(NotPlaceable)
class KEEPERSGAME_API AKeepersRockActor : public AActor
{
	GENERATED_BODY()

public:
	AKeepersRockActor();

	/** Starts the rise animation: the top goes from the tile surface to TargetTopZ. */
	void Rise(const FVector& TileSurface, float TargetTopZ);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Rock")
	float RiseDuration = 0.45f;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	float StartZ = 0.f;
	float EndZ = 0.f;
	float Elapsed = 0.f;
	bool bRising = false;
};
