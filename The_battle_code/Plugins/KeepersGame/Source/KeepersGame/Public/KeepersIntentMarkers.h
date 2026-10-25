#pragma once

#include "GameFramework/Actor.h"
#include "KeepersIntentMarkers.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Red pulsing discs on the tiles enemies will hit next turn.
 * Spawned and filled by UKeepersIntentSubsystem.
 */
UCLASS(NotPlaceable, Transient)
class KEEPERSGAME_API AKeepersIntentMarkers : public AActor
{
	GENERATED_BODY()

public:
	AKeepersIntentMarkers();

	/** Replaces all markers; a location hit by several enemies gets one marker. */
	void SetMarkerLocations(const TArray<FVector>& Locations);

	virtual void Tick(float DeltaSeconds) override;

	/** Disc size relative to a 200-unit tile. */
	UPROPERTY(EditAnywhere, Category = "Intents")
	float DiscScale = 1.5f;

	/** Height above the tile surface. */
	UPROPERTY(EditAnywhere, Category = "Intents")
	float HeightOffset = 8.f;

	UPROPERTY(EditAnywhere, Category = "Intents")
	FLinearColor Color = FLinearColor(0.9f, 0.08f, 0.05f);

	/** Pulses per second. */
	UPROPERTY(EditAnywhere, Category = "Intents")
	float PulseSpeed = 1.5f;

private:
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Discs;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Material;

	TArray<FVector> MarkerLocations;
	float Time = 0.f;
};
