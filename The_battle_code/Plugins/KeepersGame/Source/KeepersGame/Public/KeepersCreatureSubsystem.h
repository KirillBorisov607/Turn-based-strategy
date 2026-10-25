#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "KeepersCreatureSubsystem.generated.h"

/**
 * Enemy traits that make each creature play differently.
 *
 * Slime:
 *  - leaves a slime puddle on every Normal tile it leaves (DoubleCost: slow to cross);
 *  - splits into two small slimes when it dies (small slimes do not split again). Small slimes have
 *    no AP: they cannot attack, they crawl, block tiles and leave puddles.
 *    The last slime of a battle does not split: the battle is already won.
 *
 * Chicken (enemy):
 *  - every ChickenCallEvery rounds one enemy chicken calls for help: a slime appears next to it
 *    (ChickenMaxCalls per battle).
 */
UCLASS()
class KEEPERSGAME_API UKeepersCreatureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** HP of a slime born from a split. */
	static constexpr int32 SmallSlimeHp = 3;

	static constexpr int32 ChickenCallEvery = 2;
	static constexpr int32 ChickenMaxCalls = 3;

private:
	void LeavePuddle(UObject* Grid, const FIntPoint& Index);
	void Split(UObject* CombatSystem, AActor* Slime, const FIntPoint& Index);
	void OnRoundStarted(UObject* CombatSystem);

	/** Spawns a unit of UnitType next to Index for Team and adds it to the combat. */
	AActor* SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag, bool bCanAct = true);

	/** Last known tile of every living slime. */
	TMap<TWeakObjectPtr<AActor>, FIntPoint> SlimeTiles;

	/** Slimes already seen alive (to detect the moment they die). */
	TSet<TWeakObjectPtr<AActor>> LivingSlimes;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Puddles;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> PuddleMaterial;

	TWeakObjectPtr<UObject> CachedCombatSystem;

	/** Chicken calls made this battle. */
	int32 ChickenCalls = 0;

	int32 Round = 0;
	int32 LastTeam = INDEX_NONE;
};
