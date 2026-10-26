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
	Roots,
	// The Mage.
	Fireball,
	IcePath,
	ChainLightning,
	// The Rogue.
	Dash,
	Backstab,
	SmokeBomb,
	// The Barbarian.
	Crush,
	Throw,
	Rage,
	// Enemies.
	Boulder
};

/**
 * Terrain and push effects of the Keepers' abilities (and of the enemies': Charge works as Dash, RamStrike pushes,
 * a catapult's Boulder stays on an empty target cell as a rock).
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
 *  - BP_SpellAnim_Fireball: forest on the target and around it burns down (plain ground again).
 *  - BP_SpellAnim_IcePath: water on the target and the next cell along the cast freezes into walkable ice.
 *  - BP_SpellAnim_ChainLightning: the bolt jumps to up to two more foes within two cells (3 damage each).
 *  - BP_SpellAnim_Dash: the Rogue lands on the empty target cell; foes next to the landing take 2.
 *  - BP_SpellAnim_Backstab: a foe flanked by another of the Rogue's allies takes the hit twice.
 *  - BP_SpellAnim_SmokeBomb: smoke on the target and around it blocks sight for two rounds.
 *  - BP_SpellAnim_Crush: rocks and trees raised in battle on the target and around it are smashed.
 *  - BP_SpellAnim_Throw: the target flies over the Barbarian to the cell behind it (water drowns it,
 *    a rock or a unit hurts both).
 *  - BP_SpellAnim_Rage: the Barbarian (who paid health) gets its action points back and +1 movement.
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

	/** Was this tile turned into forest by Sprout? */
	bool IsForest(const FIntPoint& Index) const { return ForestTiles.Contains(Index); }

	/** Pushes the unit on Target one tile away from Origin. Returns false if there was no unit. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Terrain")
	bool PushUnit(UObject* CombatSystem, FIntPoint Origin, FIntPoint Target);

	/** Sets a tile's E_TileType through BP_Grid.AddGridTile; returns the tile transform. */
	static bool SetTileType(UObject* Grid, const FIntPoint& Index, uint8 Type, FTransform& OutTransform);

	/** BP_Grid and BP_Unit helpers for the other battle systems. */
	static UObject* UnitOnTile(UObject* Grid, const FIntPoint& Index);
	static bool TileType(UObject* Grid, const FIntPoint& Index, uint8& OutType);
	static bool TileLocation(UObject* Grid, const FIntPoint& Index, FVector& OutLocation);
	/** The neighbour of Index furthest along Direction. */
	static TOptional<FIntPoint> NextAlong(UObject* Grid, const FIntPoint& Index, const FVector& Direction);
	static bool CanUnitStandOn(UObject* Unit, uint8 Type);
	static UObject* CombatSystemOf(UWorld* World);

	/** Damages a unit and lets the combat system process its death. */
	static void DamageUnit(UObject* CombatSystem, UObject* Unit, int32 Amount);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnActorSpawned(AActor* Actor);

	UFUNCTION()
	void OnTerrainAnimationDestroyed(AActor* Animation);

	/** Moves a unit by force to Destination (none: the edge): bumps, falls into water, or lands. */
	bool ForcedMove(UObject* CombatSystem, UObject* Grid, UObject* Unit, const TOptional<FIntPoint>& Destination, int32 ExtraDamage);

	/** Effects of the Mage's, the Rogue's and the Barbarian's abilities. */
	void ApplyHeroAbility(EKeepersTerrainEffect Effect, UObject* Grid, const FIntPoint& Origin, const FIntPoint& Target);
	/** Removes a rock or a tree raised in battle on a cell; the cell becomes plain ground. */
	bool ClearProp(UObject* Grid, const FIntPoint& Index);

	FDelegateHandle SpawnHandle;

	TMap<TWeakObjectPtr<AActor>, EKeepersTerrainEffect> PendingEffects;

	TSet<FIntPoint> ForestTiles;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> TerrainProps;
};
