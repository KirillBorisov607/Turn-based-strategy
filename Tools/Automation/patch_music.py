import os
os.chdir(r'C:/ProjectsUE/Turn-based-strategy/The_battle_code/Plugins/KeepersGame/Source/KeepersGame')


def patch(p, pairs):
    s = open(p, 'rb').read().decode('utf-8-sig')
    crlf = '\r\n' in s
    s = s.replace('\r\n', '\n')
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:70])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='\r\n' if crlf else '\n').write(s)


patch('Public/KeepersJuice.h', [
('''class UStaticMeshComponent;''', '''class UStaticMeshComponent;
class UAudioComponent;'''),
(''' * sparks in the colour of the one hit, green motes of a heal, a puff when a unit falls.
 */''', ''' * sparks in the colour of the one hit, green motes of a heal, a puff when a unit falls.
 * Plays the battle music while the fight is on.
 */'''),
('''private:
	TMap<TWeakObjectPtr<AActor>, int32> Health;''', '''private:
	void UpdateMusic();

	UPROPERTY()
	TObjectPtr<UAudioComponent> Music;
	bool bMusicFading = false;
	TMap<TWeakObjectPtr<AActor>, int32> Health;'''),
])
patch('Private/KeepersJuice.cpp', [
('''#include "Components/StaticMeshComponent.h"''', '''#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"'''),
('''void UKeepersJuiceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();''', '''void UKeepersJuiceSubsystem::UpdateMusic()
{
	UObject* Combat = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("BP_CombatSystem_C"))
		{
			Combat = *It;
			break;
		}
	}
	bool bInCombat = false;
	bool bFinishing = false;
	if (Combat)
	{
		KeepersReflection::GetBool(Combat->GetClass(), Combat, TEXT("IsInCombat"), bInCombat);
		KeepersReflection::GetBool(Combat->GetClass(), Combat, TEXT("IsFinishingCombat"), bFinishing);
	}
	if (bInCombat && !bFinishing && !Music)
	{
		// The battle theme (looped, Music volume of the settings), faded in.
		if (USoundBase* Theme = LoadObject<USoundBase>(nullptr, TEXT("/Game/Keepers/Audio/Music/MUS_Battle.MUS_Battle"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Music = UGameplayStatics::SpawnSound2D(this, Theme, 0.4f, 1.f, 0.f, nullptr, false, false);
			if (Music)
			{
				Music->FadeIn(2.5f, 0.4f);
			}
		}
	}
	if (Music && bFinishing && !bMusicFading)
	{
		bMusicFading = true;
		Music->FadeOut(2.f, 0.f);
	}
}

void UKeepersJuiceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	UpdateMusic();'''),
])
print('ok')
