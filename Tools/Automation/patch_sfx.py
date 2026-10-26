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
('''	/** Spawns a burst at a place; also for other code (abilities, objectives). */''', '''	/** Plays a sound of /Game/Keepers/Audio/SFX ("Hit" -> SFX_Hit), with a little variation of pitch. */
	static void Sound(const UObject* WorldContext, const TCHAR* Name, float Volume = 1.f);

	/** Spawns a burst at a place; also for other code (abilities, objectives). */'''),
])
patch('Private/KeepersJuice.cpp', [
('''#include "KeepersReflection.h"''', '''#include "Kismet/GameplayStatics.h"
#include "KeepersReflection.h"
#include "Sound/SoundBase.h"'''),
('''void UKeepersJuiceSubsystem::Burst(''', '''void UKeepersJuiceSubsystem::Sound(const UObject* WorldContext, const TCHAR* Name, float Volume)
{
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Keepers/Audio/SFX/SFX_%s.SFX_%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Sound && WorldContext && WorldContext->GetWorld())
	{
		UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume, FMath::FRandRange(0.92f, 1.08f));
	}
}

void UKeepersJuiceSubsystem::Burst('''),
('''			Burst(World, Chest, AKeepersBurst::EKind::Hit, KeepersJuice::ColorOf(Unit, Team), FMath::Clamp(8 - Change * 2, 10, 24));
			if (Hp <= 0)
			{''', '''			Burst(World, Chest, AKeepersBurst::EKind::Hit, KeepersJuice::ColorOf(Unit, Team), FMath::Clamp(8 - Change * 2, 10, 24));
			Sound(World, Unit->GetName().Contains(TEXT("Slime")) ? TEXT("Slime") : TEXT("Hit"), FMath::Clamp(0.6f - Change * 0.05f, 0.6f, 1.f));
			if (Hp <= 0)
			{
				Sound(World, TEXT("Puff"), 0.8f);'''),
('''			Burst(World, Chest, AKeepersBurst::EKind::Heal, FLinearColor(0.4f, 1.f, 0.45f), FMath::Clamp(4 + Change, 6, 16));''', '''			Burst(World, Chest, AKeepersBurst::EKind::Heal, FLinearColor(0.4f, 1.f, 0.45f), FMath::Clamp(4 + Change, 6, 16));
			Sound(World, TEXT("Heal"), 0.7f);'''),
])
patch('Private/KeepersTerrainSubsystem.cpp', [
('''				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Puff, FLinearColor(0.62f, 0.55f, 0.45f), 14, 0.9f);''', '''				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Puff, FLinearColor(0.62f, 0.55f, 0.45f), 14, 0.9f);
				UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));'''),
('''				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 30.f), AKeepersBurst::EKind::Hit, FLinearColor(0.42f, 0.27f, 0.12f), 14);''', '''				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 30.f), AKeepersBurst::EKind::Hit, FLinearColor(0.42f, 0.27f, 0.12f), 14);
				UKeepersJuiceSubsystem::Sound(this, TEXT("Leaves"), 0.8f);'''),
('''				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 40.f), AKeepersBurst::EKind::Heal, FLinearColor(0.35f, 0.85f, 0.25f), 16, 1.2f);''', '''				UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 40.f), AKeepersBurst::EKind::Heal, FLinearColor(0.35f, 0.85f, 0.25f), 16, 1.2f);
				UKeepersJuiceSubsystem::Sound(this, TEXT("Leaves"));'''),
('''		UKeepersJuiceSubsystem::Burst(GetWorld(), Pushed->GetActorLocation(), AKeepersBurst::EKind::Puff, FLinearColor(0.85f, 0.92f, 1.f), 10, 0.7f);''', '''		UKeepersJuiceSubsystem::Burst(GetWorld(), Pushed->GetActorLocation(), AKeepersBurst::EKind::Puff, FLinearColor(0.85f, 0.92f, 1.f), 10, 0.7f);
		UKeepersJuiceSubsystem::Sound(this, TEXT("Gust"));'''),
('''			UKeepersJuiceSubsystem::Burst(GetWorld(), Splash, AKeepersBurst::EKind::Hit, FLinearColor(0.35f, 0.65f, 1.f), 20, 1.1f);''', '''			UKeepersJuiceSubsystem::Burst(GetWorld(), Splash, AKeepersBurst::EKind::Hit, FLinearColor(0.35f, 0.65f, 1.f), 20, 1.1f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"));'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''		// The choice is made: its panel closes (the callback may open another one, or a level).''', '''		UKeepersJuiceSubsystem::Sound(this, TEXT("Card"));
		// The choice is made: its panel closes (the callback may open another one, or a level).'''),
('''	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	const FString Name = Def ? Def->Name.ToString() : Hero.ToString();
	TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);''', '''	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	const FString Name = Def ? Def->Name.ToString() : Hero.ToString();
	UKeepersJuiceSubsystem::Sound(GetWorld(), TEXT("LevelUp"));
	TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);'''),
('''	if (Winner == 0)
	{
		ShowChoice(FText::FromString(TEXT("Победа!")),''', '''	UKeepersJuiceSubsystem::Sound(GetWorld(), Winner == 0 ? TEXT("Victory") : TEXT("Defeat"));
	if (Winner == 0)
	{
		ShowChoice(FText::FromString(TEXT("Победа!")),'''),
('''	TArray<UKeepersChoiceWidget::FOption> Options;

	if (Winner != 0)
	{''', '''	TArray<UKeepersChoiceWidget::FOption> Options;
	UKeepersJuiceSubsystem::Sound(GetWorld(), Winner == 0 ? TEXT("Victory") : TEXT("Defeat"));

	if (Winner != 0)
	{'''),
('''#include "KeepersLoadingScreen.h"''', '''#include "KeepersJuice.h"
#include "KeepersLoadingScreen.h"'''),
])
patch('Private/KeepersEvents.cpp', [
('''			Campaign->AddGold(Amount);
			return FString::Printf(TEXT("+%d золота"), Amount);''', '''			Campaign->AddGold(Amount);
			UKeepersJuiceSubsystem::Sound(Context, TEXT("Coin"));
			return FString::Printf(TEXT("+%d золота"), Amount);'''),
('''#include "KeepersHeroes.h"
''', '''#include "KeepersHeroes.h"
#include "KeepersJuice.h"
'''),
])
print('ok')
