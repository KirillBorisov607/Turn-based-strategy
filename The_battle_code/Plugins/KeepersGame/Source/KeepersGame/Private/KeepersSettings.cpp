#include "KeepersSettings.h"

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
