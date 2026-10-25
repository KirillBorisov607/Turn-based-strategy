#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "KeepersCreatureSubsystem.generated.h"

/**
 * Enemy traits that make each creature play differently.
 *
 * Slime:
 *  - leaves a slime puddle on every Normal tile it leaves (DoubleCost: slow to cross);
 *  - splits into two small slimes when it dies (small slimes do not split again).
 *    The last slime of a battle does not split: the battle is already won.
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

private:
	void LeavePuddle(UObject* Grid, const FIntPoint& Index);
	void Split(UObject* CombatSystem, AActor* Slime, const FIntPoint& Index);

	/** Last known tile of every living slime. */
	TMap<TWeakObjectPtr<AActor>, FIntPoint> SlimeTiles;

	/** Slimes already seen alive (to detect the moment they die). */
	TSet<TWeakObjectPtr<AActor>> LivingSlimes;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Puddles;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> PuddleMaterial;

	TWeakObjectPtr<UObject> CachedCombatSystem;
};
