#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "KeepersBarks.h"
#include "KeepersReactions.generated.h"

/** Lines for the reactions, by unit type and situation (heroes, people of the chickens, slimes, chickens, bats). */
namespace KeepersReactions
{
	/** Lines for a unit type in a situation; empty when there are none. */
	KEEPERSGAME_API const TArray<const TCHAR*>& Lines(uint8 UnitType, EKeepersBarkSituation Situation);
	/** How many lines the reactions know (for the docs and tests). */
	KEEPERSGAME_API int32 Count();
}

/**
 * Units react to what happens in battle: a hero hit by a friend complains and the friend apologises, a kill gets
 * a boast, a fallen friend a cry, low health a worried word, healing a thank-you; the battle's start and its end
 * get a line too. Health is watched every frame; the unit whose turn it is counts as the one who struck.
 * A few seconds pass between lines so the field does not turn into a market.
 */
UCLASS()
class KEEPERSGAME_API UKeepersReactionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	/** A line if the unit and the field have been quiet long enough. */
	bool Say(AActor* Unit, EKeepersBarkSituation Situation, float Chance = 1.f, bool bUrgent = false);

	struct FWatched
	{
		int32 Hp = 0;
		int32 HpMax = 1;
		bool bAlive = true;
		bool bWarnedLow = false;
		int32 Team = 0;
	};
	TMap<TWeakObjectPtr<AActor>, FWatched> Watched;
	TMap<TWeakObjectPtr<AActor>, float> LastSaid;
	float Clock = 0.f;
	float LastAny = -10.f;
	bool bStarted = false;
	bool bEnded = false;

	UPROPERTY()
	TObjectPtr<UObject> CombatSystem;
};
