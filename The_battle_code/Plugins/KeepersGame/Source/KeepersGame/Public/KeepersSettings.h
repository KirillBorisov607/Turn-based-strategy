#pragma once

#include "CoreMinimal.h"

/**
 * Player settings that UGameUserSettings does not cover, saved in GameUserSettings.ini ([Keepers]):
 * volumes per sound class, camera speeds, edge scrolling, battle speed, barks, the FPS counter.
 */
namespace KeepersSettings
{
	/** Keys. Volumes are 0..1; speeds are factors (1 = as designed); switches are 0 or 1. */
	inline const TCHAR* Volume = TEXT("Volume");
	inline const TCHAR* MusicVolume = TEXT("MusicVolume");
	inline const TCHAR* AmbientVolume = TEXT("AmbientVolume");
	inline const TCHAR* EffectsVolume = TEXT("EffectsVolume");
	inline const TCHAR* UiVolume = TEXT("UiVolume");
	inline const TCHAR* CameraSpeed = TEXT("CameraSpeed");
	inline const TCHAR* RotateSpeed = TEXT("RotateSpeed");
	inline const TCHAR* ZoomSpeed = TEXT("ZoomSpeed");
	inline const TCHAR* InvertZoom = TEXT("InvertZoom");
	inline const TCHAR* EdgeScroll = TEXT("EdgeScroll");
	inline const TCHAR* BattleSpeed = TEXT("BattleSpeed");
	inline const TCHAR* Barks = TEXT("Barks");
	inline const TCHAR* ShowFps = TEXT("ShowFps");

	/** The saved value of a key, or its default. */
	KEEPERSGAME_API float Get(const TCHAR* Key);
	KEEPERSGAME_API float DefaultOf(const TCHAR* Key);
	KEEPERSGAME_API void Set(const TCHAR* Key, float Value);

	/** Master volume, 0..1, 1 by default. */
	KEEPERSGAME_API float GetVolume();

	KEEPERSGAME_API void SetVolume(const UObject* WorldContext, float Volume);

	/** Applies the saved volumes to the world's audio (call when a level starts or a volume changes). */
	KEEPERSGAME_API void ApplyVolume(const UObject* WorldContext);
}
