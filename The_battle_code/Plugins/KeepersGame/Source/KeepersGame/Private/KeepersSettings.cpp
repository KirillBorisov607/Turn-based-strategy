#include "KeepersSettings.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace KeepersSettings
{
	static const TCHAR* Section = TEXT("Keepers");

	float GetVolume()
	{
		float Volume = 1.f;
		GConfig->GetFloat(Section, TEXT("Volume"), Volume, GGameUserSettingsIni);
		return FMath::Clamp(Volume, 0.f, 1.f);
	}

	void SetVolume(const UObject* WorldContext, float Volume)
	{
		GConfig->SetFloat(Section, TEXT("Volume"), FMath::Clamp(Volume, 0.f, 1.f), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		ApplyVolume(WorldContext);
	}

	void ApplyVolume(const UObject* WorldContext)
	{
		// Every sound without its own class plays through the project's default class (Master).
		USoundClass* Master = Cast<USoundClass>(GetDefault<UAudioSettings>()->DefaultSoundClassName.TryLoad());
		static USoundMix* Mix = nullptr;
		if (!Mix)
		{
			Mix = NewObject<USoundMix>(GetTransientPackage(), TEXT("KeepersVolumeMix"));
			Mix->AddToRoot();
		}
		if (!Master || !WorldContext)
		{
			return;
		}
		UGameplayStatics::SetSoundMixClassOverride(WorldContext, Mix, Master, GetVolume(), 1.f, 0.f, true);
		UGameplayStatics::PushSoundMixModifier(WorldContext, Mix);
	}
}
