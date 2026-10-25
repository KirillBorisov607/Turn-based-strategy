#pragma once

#include "CoreMinimal.h"

/** Player settings that UGameUserSettings does not cover (master volume), saved in GameUserSettings.ini. */
namespace KeepersSettings
{
	/** 0..1, 1 by default. */
	KEEPERSGAME_API float GetVolume();

	KEEPERSGAME_API void SetVolume(const UObject* WorldContext, float Volume);

	/** Applies the saved volume to the world's audio (call when a level starts). */
	KEEPERSGAME_API void ApplyVolume(const UObject* WorldContext);
}
