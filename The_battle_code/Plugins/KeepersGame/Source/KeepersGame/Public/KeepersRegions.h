#pragma once

#include "CoreMinimal.h"
#include "KeepersSurfaces.h"

/**
 * A region of the campaign: its own enemies, a rule of the land and a boss. The run goes through them in order;
 * beating a region's boss opens the next one on a new map, with the squad's gold, relics and levels kept.
 */
struct FKeepersRegion
{
	FName Id;
	FText Name;
	/** "регион II". */
	FText Numeral;
	/** What awaits there (the map's caption and the loading screen). */
	FText Intro;
	/** Said when the boss falls. */
	FText Liberated;

	/** Ice or marsh patches in every battle. */
	EKeepersSurface Surface = EKeepersSurface::None;
	/** Forest cells grown on the field at the start of a battle. */
	int32 Forests = 0;
	/** Rocks (barricades) raised on the field at the start of a battle. */
	int32 Barricades = 0;
	/** The land shifts: every second round a rock rises, a forest grows and a prop crumbles. */
	bool bShifting = false;

	/** Enemies (E_UnitType) who join the battles, the elite battles and the boss. */
	TArray<uint8> Band;
	TArray<uint8> EliteBand;
	TArray<uint8> BossBand;

	/** The boss: a unit of this type, bigger and tougher; or a totem (an enemy turned into it). */
	FText BossName;
	uint8 BossType = 3;
	float BossScale = 1.6f;
	int32 BossHp = 20;
	bool bBossTotem = false;
	/** Every round the boss's land closes in: forest grows next to a hero. */
	bool bBossGrowsForest = false;

	/** The hero opened when the region is cleared. */
	FName Unlocks;
};

namespace KeepersRegions
{
	KEEPERSGAME_API const TArray<FKeepersRegion>& All();
	/** The region by number, clamped to the list. */
	KEEPERSGAME_API const FKeepersRegion& Get(int32 Index);
}
