"""Quick battles end with the game's own victory / defeat panel: play again or back to the menu."""
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


patch('Public/KeepersTutorial.h', [
('''	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;''', '''	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	bool IsTutorialActive() const { return bActive; }'''),
])
patch('Public/KeepersBattleObjective.h', [
('''	static EKeepersObjectiveKind TakeRequest();''', '''	static EKeepersObjectiveKind TakeRequest();
	/** The objective given to the current battle (to play it again). */
	static EKeepersObjectiveKind LastGiven;'''),
])
patch('Private/KeepersBattleObjective.cpp', [
('''EKeepersObjectiveKind AKeepersBattleObjective::TakeRequest()''', '''EKeepersObjectiveKind AKeepersBattleObjective::LastGiven = EKeepersObjectiveKind::None;

EKeepersObjectiveKind AKeepersBattleObjective::TakeRequest()'''),
('''	const EKeepersObjectiveKind Kind = AKeepersBattleObjective::TakeRequest();
	if (Kind == EKeepersObjectiveKind::None || !KeepersObjective::FindCombatSystem(&InWorld))''', '''	const EKeepersObjectiveKind Kind = AKeepersBattleObjective::TakeRequest();
	AKeepersBattleObjective::LastGiven = Kind;
	if (Kind == EKeepersObjectiveKind::None || !KeepersObjective::FindCombatSystem(&InWorld))'''),
])
patch('Public/KeepersCampaignUI.h', [
('''	void TickBattle();''', '''	void TickBattle();
	/** A battle outside the campaign: its own result panel instead of the old end screen. */
	void TickQuickBattle();
	bool bQuickResolved = false;
	float QuickDelay = 0.f;
	/** The quick battle had mercenaries: they come again on a replay. */
	bool bQuickMercenaries = false;'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (!Campaign || !Campaign->IsInBattleNode() || bBattleResolved)
	{
		return;
	}''', '''	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (Campaign && !Campaign->IsInBattleNode())
	{
		TickQuickBattle();
		return;
	}
	if (!Campaign || bBattleResolved)
	{
		return;
	}'''),
('''void UKeepersCampaignWorldSubsystem::ApplyArtifactsAtBattleStart(UObject* CombatSystem)''', '''void UKeepersCampaignWorldSubsystem::TickQuickBattle()
{
	UObject* CombatSystem = bQuickResolved ? nullptr : KeepersUI::FindCombatSystem(GetWorld());
	const UKeepersTutorialSubsystem* Tutorial = GetWorld()->GetSubsystem<UKeepersTutorialSubsystem>();
	if (!CombatSystem || (Tutorial && Tutorial->IsTutorialActive()))
	{
		return;
	}
	bool bFinishing = false;
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);
	if (!bFinishing)
	{
		return;
	}
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->GetClass()->GetName() == TEXT("W_CombatEndScreen_C"))
		{
			It->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	QuickDelay += GetWorld()->GetDeltaSeconds();
	if (QuickDelay < 0.8f)
	{
		return;
	}
	bQuickResolved = true;
	int32 Winner = INDEX_NONE;
	KeepersReflection::GetInt(CombatSystem->GetClass(), CombatSystem, TEXT("TeamIndexThatWonLastCombat"), Winner);
	if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
	{
		Player->SetShowMouseCursor(true);
	}

	// Play the same battle again (with its objective and mercenaries), or go back to the menu.
	const FName Level(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));
	const EKeepersObjectiveKind Objective = AKeepersBattleObjective::LastGiven;
	const bool bMercenaries = bQuickMercenaries;
	TWeakObjectPtr<UWorld> WeakWorld(GetWorld());
	TArray<UKeepersChoiceWidget::FOption> Options;
	UKeepersChoiceWidget::FOption Again;
	Again.Title = FText::FromString(TEXT("Ещё раз"));
	Again.Description = FText::FromString(TEXT("Тот же бой с начала."));
	Again.Color = FLinearColor(0.2f, 0.35f, 0.25f);
	Again.Image = KeepersCards::Card(TEXT("Fight"));
	Again.OnPicked = [WeakWorld, Level, Objective, bMercenaries]()
	{
		if (UWorld* World = WeakWorld.Get())
		{
			AKeepersBattleObjective::RequestForNextBattle(Objective);
			UKeepersCampaignSubsystem::SetMercenaryBattle(World, bMercenaries);
			KeepersLoading::OpenLevel(World, Level);
		}
	};
	Options.Add(MoveTemp(Again));
	UKeepersChoiceWidget::FOption Menu;
	Menu.Title = FText::FromString(TEXT("В меню"));
	Menu.Description = FText::FromString(TEXT("К костру."));
	Menu.Color = FLinearColor(0.3f, 0.25f, 0.1f);
	Menu.Image = KeepersCards::Card(TEXT("Leave"));
	Menu.OnPicked = [WeakWorld]()
	{
		if (UWorld* World = WeakWorld.Get())
		{
			KeepersLoading::OpenLevel(World, UKeepersCampaignSubsystem::MainMenuLevel);
		}
	};
	Options.Add(MoveTemp(Menu));
	if (Winner == 0)
	{
		ShowChoice(FText::FromString(TEXT("Победа!")), FText::FromString(TEXT("Поле за Хранителями. Куры будут рассказывать об этом бое ещё долго.")), MoveTemp(Options));
	}
	else
	{
		ShowChoice(FText::FromString(TEXT("Поражение")), FText::FromString(KeepersUI::DefeatLine(CombatSystem)), MoveTemp(Options));
	}
}

void UKeepersCampaignWorldSubsystem::ApplyArtifactsAtBattleStart(UObject* CombatSystem)'''),
('''	if (Campaign->bMercenaryQuickBattle)
	{
		Campaign->bMercenaryQuickBattle = false;''', '''	if (Campaign->bMercenaryQuickBattle)
	{
		Campaign->bMercenaryQuickBattle = false;
		bQuickMercenaries = true;'''),
])
print('ok')
