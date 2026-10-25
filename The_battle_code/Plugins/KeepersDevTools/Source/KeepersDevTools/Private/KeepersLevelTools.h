#pragma once

#include "CoreMinimal.h"

/** The level kit's commands without dialogs, for scripts and tests. */
namespace KeepersLevelTools
{
	/** Creates a battle level from the template and adds it to the game; returns its package, empty on failure. */
	FString CreateLevel(const FString& Title);
	/** Checks the open level; returns the report shown by "Проверить уровень". */
	FString CheckReport();
}
