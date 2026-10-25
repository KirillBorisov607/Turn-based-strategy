#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "KeepersAiLibrary.generated.h"

/**
 * Tactics for the enemies' Blueprint AI. BP_UnitAi scores every (spell, casting spot, target) by the damage
 * it deals; AdjustPlanScore adds what the spot and the target mean on the battlefield.
 */
UCLASS()
class KEEPERSGAME_API UKeepersAiLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Called from BP_UnitAi.CalculateSpellScoreAndUpdateSpellCasting with the plan's damage score.
	 * Only real attacks (score above zero) are adjusted:
	 *  - a spot on the edge of water, a pit or a cliff is risky: heroes push units off (walkers only);
	 *  - high ground over the target is better;
	 *  - a tile another enemy already threatens is worth less: the threats spread over the heroes;
	 *  - ranged units keep away from the heroes; a badly wounded unit keeps away too;
	 *  - a hero that can be finished off is worth more.
	 */
	UFUNCTION(BlueprintPure, Category = "Keepers|AI")
	static double AdjustPlanScore(UObject* Unit, FIntPoint Origin, FIntPoint Target, double BaseScore);
};
