#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "KeepersPartyBattle.generated.h"

/**
 * The squad the player picked goes into battle. Battle levels place the Warrior, the Ranger and the Priest;
 * before anything begins to play, every hero unit whose hero is not in the squad becomes one of the squad's
 * heroes that the level does not have (its E_UnitType and unit data from DT_DefaultUnitDataPerType).
 * The tutorial keeps its own heroes.
 */
UCLASS()
class KEEPERSGAME_API UKeepersPartyBattleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Makes a battle unit another type: E_UnitType and the default unit data of that type. */
	static bool ChangeUnitType(UObject* Unit, uint8 UnitType);
};
