#include "KeepersSettings.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace KeepersSettings
{
	static const TCHAR* Section = TEXT("Keepers");

	float DefaultOf(const TCHAR* Key)
	{
		const FString Name = Key;
		if (Name == InvertZoom || Name == EdgeScroll || Name == ShowFps)
		{
			return 0.f;
		}
		return 1.f;
	}

	float Get(const TCHAR* Key)
	{
		float Value = DefaultOf(Key);
		GConfig->GetFloat(Section, Key, Value, GGameUserSettingsIni);
		return Value;
	}

	void Set(const TCHAR* Key, float Value)
	{
		GConfig->SetFloat(Section, Key, Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	FIntPoint GetWantedResolution()
	{
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		FIntPoint Wanted = Settings ? Settings->GetScreenResolution() : FIntPoint(1920, 1080);
		int32 X = 0;
		int32 Y = 0;
		if (GConfig->GetInt(Section, TEXT("WantedResX"), X, GGameUserSettingsIni) && GConfig->GetInt(Section, TEXT("WantedResY"), Y, GGameUserSettingsIni) && X > 0 && Y > 0)
		{
			Wanted = FIntPoint(X, Y);
		}
		return Wanted;
	}

	void ApplyScreen(FIntPoint Wanted, EWindowMode::Type Mode)
	{
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (!Settings)
		{
			return;
		}
		const FIntPoint Desktop = Settings->GetDesktopResolution();
		GConfig->SetInt(Section, TEXT("WantedResX"), Wanted.X, GGameUserSettingsIni);
		GConfig->SetInt(Section, TEXT("WantedResY"), Wanted.Y, GGameUserSettingsIni);
		FIntPoint Window = Wanted;
		float Percent = 100.f;
		if (Mode == EWindowMode::WindowedFullscreen && Desktop.X > 0)
		{
			// The window covers the desktop; the chosen size is how sharp the picture inside it is.
			Window = Desktop;
			Percent = FMath::Clamp(100.f * Wanted.Y / Desktop.Y, 50.f, 100.f);
		}
		else if (Mode == EWindowMode::Windowed && Desktop.X > 0)
		{
			// A window larger than the screen hangs off it and the clicks land beside the buttons.
			const float Fit = FMath::Min(1.f, FMath::Min((Desktop.X - 40.f) / Wanted.X, (Desktop.Y - 90.f) / Wanted.Y));
			Window = FIntPoint(FMath::RoundToInt(Wanted.X * Fit), FMath::RoundToInt(Wanted.Y * Fit));
		}
		Settings->SetFullscreenMode(Mode);
		Settings->SetScreenResolution(Window);
		Settings->SetResolutionScaleValueEx(Percent);
		Settings->ApplyResolutionSettings(false);
		Settings->ApplySettings(false);
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
		UE_LOG(LogTemp, Log, TEXT("Keepers screen: mode %d, window %dx%d, picture %dx%d (%.0f%%)"), static_cast<int32>(Mode), Window.X, Window.Y, Wanted.X, Wanted.Y, Percent);
	}

	void SanitizeScreen()
	{
		static bool bDone = false;
		UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (bDone || !Settings || GIsEditor)
		{
			return;
		}
		bDone = true;
		const FIntPoint Desktop = Settings->GetDesktopResolution();
		const FIntPoint Screen = Settings->GetScreenResolution();
		const EWindowMode::Type Mode = Settings->GetFullscreenMode();
		const bool bOffScreen = Mode == EWindowMode::Windowed && (Screen.X > Desktop.X || Screen.Y > Desktop.Y);
		const bool bBorderlessMismatch = Mode == EWindowMode::WindowedFullscreen && Screen != Desktop;
		if (Desktop.X > 0 && (bOffScreen || bBorderlessMismatch))
		{
			ApplyScreen(GetWantedResolution(), Mode);
		}
	}

	const TArray<FKeyBind>& KeyBinds()
	{
		static const TArray<FKeyBind> Binds =
		{
			{ TEXT("CamForward"), TEXT("Камера вперёд"), EKeys::W, TEXT("InputAxis_Forward"), 1.f },
			{ TEXT("CamBack"), TEXT("Камера назад"), EKeys::S, TEXT("InputAxis_Forward"), -1.f },
			{ TEXT("CamLeft"), TEXT("Камера влево"), EKeys::A, TEXT("InputAxis_Right"), -1.f },
			{ TEXT("CamRight"), TEXT("Камера вправо"), EKeys::D, TEXT("InputAxis_Right"), 1.f },
			{ TEXT("RotLeft"), TEXT("Поворот влево"), EKeys::Q, TEXT("InputAction_RotationLeft"), 0.f },
			{ TEXT("RotRight"), TEXT("Поворот вправо"), EKeys::E, TEXT("InputAction_RotationRight"), 0.f },
			{ TEXT("Pause"), TEXT("Пауза"), EKeys::P, TEXT(""), 0.f },
			{ TEXT("Ability1"), TEXT("Способность 1"), EKeys::One, TEXT(""), 0.f },
			{ TEXT("Ability2"), TEXT("Способность 2"), EKeys::Two, TEXT(""), 0.f },
			{ TEXT("Ability3"), TEXT("Способность 3"), EKeys::Three, TEXT(""), 0.f },
			{ TEXT("Ability4"), TEXT("Способность 4"), EKeys::Four, TEXT(""), 0.f },
			{ TEXT("Ability5"), TEXT("Способность 5"), EKeys::Five, TEXT(""), 0.f },
			{ TEXT("Ability6"), TEXT("Способность 6"), EKeys::Six, TEXT(""), 0.f },
			{ TEXT("BattleSpeed"), TEXT("Скорость боя x1 / x3"), EKeys::Tab, TEXT(""), 0.f },
		};
		return Binds;
	}

	static const FKeyBind* FindBind(const TCHAR* Id)
	{
		return KeyBinds().FindByPredicate([Id](const FKeyBind& Bind) { return FCString::Strcmp(Bind.Id, Id) == 0; });
	}

	FKey GetKey(const TCHAR* Id)
	{
		const FKeyBind* Bind = FindBind(Id);
		FString Name;
		if (GConfig->GetString(Section, *FString::Printf(TEXT("Key_%s"), Id), Name, GGameUserSettingsIni))
		{
			const FKey Key(*Name);
			if (Key.IsValid())
			{
				return Key;
			}
		}
		return Bind ? Bind->Default : EKeys::Invalid;
	}

	void SetKey(const TCHAR* Id, const FKey& Key)
	{
		GConfig->SetString(Section, *FString::Printf(TEXT("Key_%s"), Id), *Key.GetFName().ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void ResetKeys()
	{
		for (const FKeyBind& Bind : KeyBinds())
		{
			GConfig->RemoveKey(Section, *FString::Printf(TEXT("Key_%s"), Bind.Id), GGameUserSettingsIni);
		}
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void ApplyKeys(const UObject* WorldContext)
	{
		UInputSettings* Input = UInputSettings::GetInputSettings();
		if (!Input)
		{
			return;
		}
		// Each bound control replaces its default key in the camera's mappings; the arrows stay as a spare.
		for (const FKeyBind& Bind : KeyBinds())
		{
			if (!*Bind.Mapping)
			{
				continue;
			}
			const FName Mapping(Bind.Mapping);
			const FKey Key = GetKey(Bind.Id);
			if (Bind.Scale != 0.f)
			{
				TArray<FInputAxisKeyMapping> Existing;
				Input->GetAxisMappingByName(Mapping, Existing);
				for (const FInputAxisKeyMapping& Old : Existing)
				{
					const bool bArrow = Old.Key == EKeys::Up || Old.Key == EKeys::Down || Old.Key == EKeys::Left || Old.Key == EKeys::Right;
					if (Old.Scale == Bind.Scale && !bArrow)
					{
						Input->RemoveAxisMapping(Old, false);
					}
				}
				Input->AddAxisMapping(FInputAxisKeyMapping(Mapping, Key, Bind.Scale), false);
			}
			else
			{
				TArray<FInputActionKeyMapping> Existing;
				Input->GetActionMappingByName(Mapping, Existing);
				for (const FInputActionKeyMapping& Old : Existing)
				{
					Input->RemoveActionMapping(Old, false);
				}
				Input->AddActionMapping(FInputActionKeyMapping(Mapping, Key), false);
			}
		}
		Input->ForceRebuildKeymaps();
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr)
		{
			if (Player->PlayerInput)
			{
				Player->PlayerInput->ForceRebuildingKeyMaps(true);
			}
		}
	}

	float GetVolume()
	{
		return FMath::Clamp(Get(Volume), 0.f, 1.f);
	}

	void SetVolume(const UObject* WorldContext, float InVolume)
	{
		Set(Volume, FMath::Clamp(InVolume, 0.f, 1.f));
		ApplyVolume(WorldContext);
	}

	void ApplyVolume(const UObject* WorldContext)
	{
		static USoundMix* Mix = nullptr;
		if (!Mix)
		{
			Mix = NewObject<USoundMix>(GetTransientPackage(), TEXT("KeepersVolumeMix"));
			Mix->AddToRoot();
		}
		if (!WorldContext)
		{
			return;
		}
		auto Apply = [WorldContext](USoundClass* Class, float Value)
		{
			if (Class)
			{
				// The mix keeps raw pointers to its classes: they must outlive every level change.
				if (!Class->IsRooted())
				{
					Class->AddToRoot();
				}
				// Class volumes multiply down the hierarchy, so each class only sets its own.
				UGameplayStatics::SetSoundMixClassOverride(WorldContext, Mix, Class, FMath::Clamp(Value, 0.f, 1.f), 1.f, 0.f, false);
			}
		};
		// Every sound without its own class plays through the project's default class (Master); music,
		// ambience, effects and interface have their own classes under it.
		Apply(Cast<USoundClass>(GetDefault<UAudioSettings>()->DefaultSoundClassName.TryLoad()), GetVolume());
		Apply(LoadObject<USoundClass>(nullptr, TEXT("/Game/Keepers/Audio/SC_Music.SC_Music")), Get(MusicVolume));
		Apply(LoadObject<USoundClass>(nullptr, TEXT("/Game/Keepers/Audio/SC_Ambient.SC_Ambient")), Get(AmbientVolume));
		Apply(LoadObject<USoundClass>(nullptr, TEXT("/Game/Keepers/Audio/SC_Effects.SC_Effects")), Get(EffectsVolume));
		Apply(LoadObject<USoundClass>(nullptr, TEXT("/Game/Keepers/Audio/SC_UI.SC_UI")), Get(UiVolume));
		UGameplayStatics::PushSoundMixModifier(WorldContext, Mix);
	}
}
