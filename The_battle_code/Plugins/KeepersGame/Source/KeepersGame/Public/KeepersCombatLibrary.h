#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "KeepersCombatLibrary.generated.h"

/**
 * Combat rules of Keepers of the Lands that sit on top of the Blueprint combat system.
 */
UCLASS()
class KEEPERSGAME_API UKeepersCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Orders BP_CombatSystem.UnitsInCombat by TeamIndex (stable), so NextUnit plays every unit of
	 * team 0 (the player), then every unit of team 1, and so on: a player phase, then an enemy phase.
	 */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Combat", meta = (DefaultToSelf = "CombatSystem"))
	static void SortUnitsInCombatByTeam(UObject* CombatSystem);

	/**
	 * Enemy intents, called from BP_UnitAi's AiFlow graph.
	 *
	 * SaveIntent: the AI reached its casting spot; instead of attacking now it remembers
	 * AiSpellCasting, marks the tiles it will hit, and ends its turn.
	 */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Intents", meta = (DefaultToSelf = "UnitAi"))
	static void SaveIntent(UObject* UnitAi);

	/**
	 * At the start of the AI's turn: if it has an intent, writes it back into AiSpellCasting
	 * (cast from wherever the unit stands now) and returns true so the AI attacks right away.
	 */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Intents", meta = (DefaultToSelf = "UnitAi"))
	static bool RestoreIntent(UObject* UnitAi);

	/**
	 * After the AI attacked: refills its AP and per-turn cast counts so it can plan its next intent
	 * in the same turn. Nothing extra is cast; the refill is only used for planning.
	 */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Intents", meta = (DefaultToSelf = "UnitAi"))
	static void PrepareToPlanNextIntent(UObject* UnitAi);

	/** Removes an AI's intent and its tile markers. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Intents", meta = (DefaultToSelf = "UnitAi"))
	static void ClearIntent(UObject* UnitAi);
};
