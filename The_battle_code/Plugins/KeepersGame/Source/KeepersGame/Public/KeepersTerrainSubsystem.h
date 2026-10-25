#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "KeepersTerrainSubsystem.generated.h"

class AKeepersRockActor;

/**
 * Terrain changes made by the Keepers' abilities.
 *
 * Spells stay data driven: a spell's Animation class (in DT_SpellData) is a child of BP_SpellAnimation,
 * and this subsystem reacts when one of the terrain animations is spawned by BP_CombatSystem.CastSpell:
 *  - BP_SpellAnim_RaiseRock: the target tile becomes a stone column (Obstacle).
 */
UCLASS()
class KEEPERSGAME_API UKeepersTerrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Turns an empty tile into an Obstacle and raises a stone column on it. Returns false if the tile is taken. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Terrain")
	bool RaiseRock(UObject* Grid, FIntPoint Index, int32 Levels = 2);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnActorSpawned(AActor* Actor);

	FDelegateHandle SpawnHandle;

	UPROPERTY()
	TArray<TObjectPtr<AKeepersRockActor>> Rocks;
};
