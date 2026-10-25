#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "KeepersTerrainSubsystem.generated.h"

class AKeepersRockActor;

/** What a terrain spell does once its animation has hit. */
enum class EKeepersTerrainEffect : uint8
{
	RaiseRock,
	Push,
	Sprout,
	Roots
};

/**
 * Terrain and push effects of the Keepers' abilities.
 *
 * Spells stay data driven: a spell's Animation class (in DT_SpellData) is a child of BP_SpellAnimation.
 * When BP_CombatSystem.CastSpell spawns one of the animations below, this subsystem waits until the
 * animation has hit (it destroys itself) and then applies the effect at its TargetIndex:
 *  - BP_SpellAnim_RaiseRock: the empty target tile becomes a stone column (Obstacle).
 *  - BP_SpellAnim_Gust, BP_SpellAnim_ShieldBash: the unit on the target tile is pushed one tile away
 *    from the caster.
 *    Blocked by a rock, a unit or the edge: 1 damage (and 1 to the unit it bumps into).
 *    Pushed over a chasm it cannot stand on: it falls and dies.
 *  - BP_SpellAnim_Sprout: the target tile becomes forest (DoubleCost). An empty tile grows a tree that
 *    blocks line of sight; an occupied tile grows a bush.
 *  - BP_SpellAnim_Roots: the target's MP (already cut by the spell) is set to exactly 0 for its next turn.
 */
UCLASS()
class KEEPERSGAME_API UKeepersTerrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Turns an empty tile into an Obstacle and raises a stone column on it. Returns false if the tile is taken. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Terrain")
	bool RaiseRock(UObject* Grid, FIntPoint Index, int32 Levels = 2);

	/** Turns a tile into forest and grows a tree (or a bush under a unit). Returns false on rocks and chasms. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Terrain")
	bool GrowForest(UObject* Grid, FIntPoint Index);

	/** Pushes the unit on Target one tile away from Origin. Returns false if there was no unit. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Terrain")
	bool PushUnit(UObject* CombatSystem, FIntPoint Origin, FIntPoint Target);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnActorSpawned(AActor* Actor);

	UFUNCTION()
	void OnTerrainAnimationDestroyed(AActor* Animation);

	/** Sets a tile's E_TileType through BP_Grid.AddGridTile; returns the tile transform. */
	static bool SetTileType(UObject* Grid, const FIntPoint& Index, uint8 Type, FTransform& OutTransform);

	/** Damages a unit and lets the combat system process its death. */
	static void DamageUnit(UObject* CombatSystem, UObject* Unit, int32 Amount);

	FDelegateHandle SpawnHandle;

	TMap<TWeakObjectPtr<AActor>, EKeepersTerrainEffect> PendingEffects;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> TerrainProps;
};
