#pragma once

#include "StructUtils/InstancedStruct.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersIntentSubsystem.generated.h"

class AKeepersIntentMarkers;

/** What one enemy will do at the start of its next turn. */
USTRUCT()
struct FKeepersIntent
{
	GENERATED_BODY()

	/** Copy of BP_UnitAi.AiSpellCasting (S_UnitAiSpellCastingData). */
	UPROPERTY()
	FInstancedStruct SpellCasting;

	/** Tiles the attack will hit and their world locations, for the markers. */
	UPROPERTY()
	TArray<FIntPoint> AttackedIndexes;

	UPROPERTY()
	TArray<FVector> AttackedLocations;
};

/**
 * Keeps every enemy intent in the world and shows the threatened tiles.
 * Intents of units that die or leave combat are dropped automatically.
 */
UCLASS()
class KEEPERSGAME_API UKeepersIntentSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	void SetIntent(UObject* UnitAi, FKeepersIntent&& Intent);
	const FKeepersIntent* FindIntent(const UObject* UnitAi) const;
	void RemoveIntent(const UObject* UnitAi);

	/** All tiles currently threatened by an enemy. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Intents")
	TArray<FIntPoint> GetThreatenedIndexes() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void RefreshMarkers();

	/** Keyed by BP_UnitAi; the intents hold no object references, so they need no GC tracking. */
	TMap<TWeakObjectPtr<UObject>, FKeepersIntent> Intents;

	UPROPERTY()
	TObjectPtr<AKeepersIntentMarkers> Markers;

	bool bMarkersDirty = false;
};
