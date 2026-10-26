"""The story in the campaign: region scenes on the map, the finale and three endings, peaceful choices, the Rogue
from the tavern."""
import os
os.chdir(r'C:/ProjectsUE/Turn-based-strategy/The_battle_code/Plugins/KeepersGame/Source/KeepersGame')


def patch(p, pairs):
    s = open(p, 'rb').read().decode('utf-8-sig')
    crlf = '\r\n' in s
    s = s.replace('\r\n', '\n')
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='\r\n' if crlf else '\n').write(s)


patch('Public/KeepersCampaign.h', [
('''	/** Region of the run (KeepersRegions), from 0. */
	UPROPERTY(SaveGame)
	int32 Region = 0;
};''', '''	/** Region of the run (KeepersRegions), from 0. */
	UPROPERTY(SaveGame)
	int32 Region = 0;

	/** Peaceful choices towards the chickens: three open the pact ending. */
	UPROPERTY(SaveGame)
	int32 Peace = 0;
};'''),
('''	/** The map shows the region's introduction once after a new region begins. */''', '''	int32 GetPeace() const { return Save ? Save->Peace : 0; }
	void AddPeace();

	/** The map shows the region's introduction once after a new region begins. */'''),
])
patch('Private/KeepersCampaign.cpp', [
('''	GenerateMap(FMath::Rand());
	bActive = true;
	WriteSave();''', '''	GenerateMap(FMath::Rand());
	bActive = true;
	bRegionIntroPending = true;
	WriteSave();'''),
('''bool UKeepersCampaignSubsystem::IsFinalRegion() const''', '''void UKeepersCampaignSubsystem::AddPeace()
{
	if (Save)
	{
		++Save->Peace;
		WriteSave();
		UE_LOG(LogKeepersCampaign, Log, TEXT("Peace with the chickens: %d"), Save->Peace);
	}
}

bool UKeepersCampaignSubsystem::IsFinalRegion() const'''),
])

patch('Private/KeepersEvents.cpp', [
('''		Fight,
		FightMercenaries
	};''', '''		Fight,
		FightMercenaries,
		/** Glory and one more peaceful choice towards the chickens. */
		Peace,
		/** The Rogue joins the roster. */
		Rogue
	};'''),
('''					{ TEXT("Приютить"), 0, 100, EEffect::Glory, 10,''', '''					{ TEXT("Приютить"), 0, 100, EEffect::Peace, 10,'''),
('''					{ TEXT("Послушать слухи"), 0, 100, EEffect::Glory, 6, TEXT("Говорят, Великий Петух боится сквозняков. И что слизни делятся, если их ударить. Это вы и так знали.") },''',
 '''					{ TEXT("Послушать слухи"), 0, 100, EEffect::Glory, 6, TEXT("Говорят, Великий Петух боится сквозняков. И что слизни делятся, если их ударить. Это вы и так знали.") },
					{ TEXT("Угостить незнакомца в углу (10 зол.)"), 10, 100, EEffect::Rogue, 0, TEXT("Незнакомец в капюшоне оказался Плутом. «Камень? Я его просто нёс. Случайно. Возьмите меня с собой — я знаю, где куры прячут остальное».") },'''),
('''					{ TEXT("Помочь починить"), 0, 100, EEffect::Glory, 10,''', '''					{ TEXT("Помочь починить"), 0, 100, EEffect::Peace, 10,'''),
('''					{ TEXT("Освободить курицу"), 0, 100, EEffect::Glory, 8,''', '''					{ TEXT("Освободить курицу"), 0, 100, EEffect::Peace, 8,'''),
('''					{ TEXT("Дать взятку (15 зол.)"), 15, 100, EEffect::None, 0,''', '''					{ TEXT("Дать взятку (15 зол.)"), 15, 100, EEffect::Peace, 0,'''),
('''		case EEffect::Glory:
			if (UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(Context))
			{
				Party->AddGlory(Amount);
			}
			return Amount < 0 ? FString::Printf(TEXT("−%d славы"), -Amount) : FString::Printf(TEXT("+%d славы"), Amount);''', '''		case EEffect::Glory:
			if (UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(Context))
			{
				Party->AddGlory(Amount);
			}
			return Amount < 0 ? FString::Printf(TEXT("−%d славы"), -Amount) : FString::Printf(TEXT("+%d славы"), Amount);
		case EEffect::Peace:
		{
			Campaign->AddPeace();
			if (UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(Context))
			{
				Party->AddGlory(Amount);
			}
			const FString Glory = Amount > 0 ? FString::Printf(TEXT("+%d славы · "), Amount) : FString();
			return Glory + FString::Printf(TEXT("мир с курами: %d"), Campaign->GetPeace());
		}
		case EEffect::Rogue:
			if (UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(Context))
			{
				if (Party->UnlockHero(TEXT("Rogue")))
				{
					return TEXT("Плут открыт: возьмите его в отряд на экране «Отряд»");
				}
				Party->AddGlory(10);
			}
			return TEXT("Плут уже с вами: +10 славы");'''),
('''			{ EEffect::FightMercenaries, TEXT("Fight") },
		};''', '''			{ EEffect::FightMercenaries, TEXT("Fight") },
			{ EEffect::Peace, TEXT("Charm") },
			{ EEffect::Rogue, TEXT("Backstab") },
		};'''),
])

patch('Public/KeepersCampaignUI.h', [
('''	/** The camp node: smith, merchant, rest. */
	void ShowCamp();''', '''	/** The camp node: smith, merchant, rest. */
	void ShowCamp();

	/** A scene of the story; OnFinished runs when it closes. */
	class UKeepersDialogWidget* ShowDialog(TArray<struct FKeepersLine> Lines, TFunction<void()> OnFinished = nullptr);
	/** After the last boss: the choice of the ending. */
	void ShowEndings();'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''#include "KeepersRegions.h"''', '''#include "KeepersRegions.h"
#include "KeepersStory.h"'''),
('''	Screen = CreateWidget<UKeepersCampaignMapWidget>(Player, UKeepersCampaignMapWidget::StaticClass());
	Screen->AddToViewport(10);
	SetUiInput(Screen);''', '''	Screen = CreateWidget<UKeepersCampaignMapWidget>(Player, UKeepersCampaignMapWidget::StaticClass());
	Screen->AddToViewport(10);
	SetUiInput(Screen);
	// A new region begins with its scene.
	if (Campaign->bRegionIntroPending)
	{
		Campaign->bRegionIntroPending = false;
		TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
		ShowDialog(KeepersStory::RegionIntro(Campaign->GetRegion()), [WeakThis]()
		{
			if (WeakThis.IsValid() && WeakThis->Screen)
			{
				WeakThis->SetUiInput(WeakThis->Screen);
			}
		});
	}'''),
('''void UKeepersCampaignWorldSubsystem::ShowMap()''', '''UKeepersDialogWidget* UKeepersCampaignWorldSubsystem::ShowDialog(TArray<FKeepersLine> Lines, TFunction<void()> OnFinished)
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	UKeepersDialogWidget* Dialog = Player ? CreateWidget<UKeepersDialogWidget>(Player, UKeepersDialogWidget::StaticClass()) : nullptr;
	if (Dialog)
	{
		Dialog->Setup(MoveTemp(Lines), MoveTemp(OnFinished));
		Dialog->AddToViewport(60);
		SetUiInput(Dialog);
	}
	return Dialog;
}

void UKeepersCampaignWorldSubsystem::ShowEndings()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (!Campaign)
	{
		return;
	}
	using KeepersStory::EEnding;
	TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
	TWeakObjectPtr<UKeepersCampaignSubsystem> WeakCampaign(Campaign);
	struct FEndingCard
	{
		EEnding Ending;
		const TCHAR* Description;
		const TCHAR* Icon;
		FLinearColor Color;
	};
	static const FEndingCard Cards[] =
	{
		{ EEnding::Return, TEXT("Камни вернутся в святилище, земли успокоятся. Так велит долг Хранителей."), TEXT("Leaf"), FLinearColor(0.2f, 0.4f, 0.3f) },
		{ EEnding::Keep, TEXT("Власть над землями — в ваших руках. Что может пойти не так?"), TEXT("Relic"), FLinearColor(0.35f, 0.12f, 0.3f) },
		{ EEnding::Pact, TEXT("Поделить камни с курами и жить в мире."), TEXT("Charm"), FLinearColor(0.5f, 0.38f, 0.12f) },
	};
	TArray<UKeepersChoiceWidget::FOption> Options;
	for (const FEndingCard& Card : Cards)
	{
		const bool bOpen = Card.Ending != EEnding::Pact || Campaign->GetPeace() >= KeepersStory::PeaceNeeded;
		UKeepersChoiceWidget::FOption Option;
		Option.Title = KeepersStory::EndingName(Card.Ending);
		Option.Description = FText::FromString(bOpen ? FString(Card.Description)
			: FString::Printf(TEXT("Куры вам не верят: нужно %d мирных решения, а было %d."), KeepersStory::PeaceNeeded, Campaign->GetPeace()));
		Option.Color = bOpen ? Card.Color : FLinearColor(0.18f, 0.18f, 0.2f);
		Option.Image = KeepersCards::Card(Card.Icon);
		const EEnding Ending = Card.Ending;
		Option.OnPicked = [WeakThis, WeakCampaign, Ending, bOpen]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (!bOpen)
			{
				WeakThis->ShowEndings();
				return;
			}
			UE_LOG(LogKeepersCampaignUI, Log, TEXT("Ending: %s"), *KeepersStory::EndingName(Ending).ToString());
			WeakThis->ShowDialog(KeepersStory::Ending(Ending), [WeakCampaign]()
			{
				if (WeakCampaign.IsValid())
				{
					WeakCampaign->CompleteCurrentNode();
				}
			});
		};
		Options.Add(MoveTemp(Option));
	}
	ShowChoice(FText::FromString(TEXT("Камни земель")), FText::FromString(TEXT("Шесть камней в руках Хранителей. Как с ними поступить?")), MoveTemp(Options));
}

void UKeepersCampaignWorldSubsystem::ShowMap()'''),
('''		const FKeepersRegion& Region = Campaign->GetRegionDef();
		const bool bFinal = Campaign->IsFinalRegion();''', '''		const FKeepersRegion& Region = Campaign->GetRegionDef();
		const bool bFinal = Campaign->IsFinalRegion();
		// The last boss: the finale and the choice of the ending.
		if (bFinal)
		{
			TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
			ShowDialog(KeepersStory::Finale(), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowEndings(); } });
			return;
		}'''),
('''	else if (Action == TEXT("close"))
	{''', '''	else if (Action == TEXT("dialog"))
	{
		Self->ShowDialog(KeepersStory::RegionIntro(Index));
	}
	else if (Action == TEXT("finale"))
	{
		TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakSelf(Self);
		Self->ShowDialog(KeepersStory::Finale(), [WeakSelf]() { if (WeakSelf.IsValid()) { WeakSelf->ShowEndings(); } });
	}
	else if (Action == TEXT("next"))
	{
		for (TObjectIterator<UKeepersDialogWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport())
			{
				It->Advance();
				break;
			}
		}
	}
	else if (Action == TEXT("close"))
	{'''),
])
print('ok')
