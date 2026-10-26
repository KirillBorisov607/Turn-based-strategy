"""Wounded heroes and recruits: a hero who falls in a won battle sits the next one out; the camp hires a recruit
with a funny name for the slot or pays a healer."""
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
('''	/** Abilities learned on level-ups, as "Hero:Spell". */
	UPROPERTY(SaveGame)
	TArray<FName> LearnedSpells;
};''', '''	/** Abilities learned on level-ups, as "Hero:Spell". */
	UPROPERTY(SaveGame)
	TArray<FName> LearnedSpells;

	/** Heroes who fell in the last won battle: they sit the next battle out. */
	UPROPERTY(SaveGame)
	TArray<FName> Wounded;

	/** Recruits hired at a camp: the wounded hero they stand in for, their unit type and name. */
	UPROPERTY(SaveGame)
	TArray<FName> RecruitFor;

	UPROPERTY(SaveGame)
	TArray<FName> RecruitBase;

	UPROPERTY(SaveGame)
	TArray<FString> RecruitNames;
};'''),
('''	/** Gives the next rank for free (events); false when maxed. */''', '''	/** Wounded heroes sit the next battle out, unless a healer at a camp helps. */
	bool IsWounded(FName Hero) const { return Save && Save->Wounded.Contains(Hero); }
	const TArray<FName>& GetWounded() const { return Save->Wounded; }
	/** After a won battle: those who sat out are well again, the fallen are wounded. */
	void ResolveWounds(const TArray<FName>& SatOut, const TArray<FName>& Fallen);
	static int32 HealerCost() { return 30; }
	static int32 RecruitCost() { return 15; }
	/** The camp's healer: the hero is well at once. */
	bool HealWound(FName Hero);
	/** A recruit stands in for a wounded hero in the next battle; returns the recruit's name. */
	FString HireRecruit(FName WoundedHero);
	/** The recruit standing in for a hero: false when there is none. */
	bool GetRecruit(FName WoundedHero, FName& OutBase, FString& OutName) const;

	/** Gives the next rank for free (events); false when maxed. */'''),
])

patch('Private/KeepersCampaign.cpp', [
('''void UKeepersCampaignSubsystem::LearnSpell(FName Hero, FName Spell)''', '''void UKeepersCampaignSubsystem::ResolveWounds(const TArray<FName>& SatOut, const TArray<FName>& Fallen)
{
	if (!Save)
	{
		return;
	}
	for (const FName& Hero : SatOut)
	{
		Save->Wounded.Remove(Hero);
		const int32 Recruit = Save->RecruitFor.IndexOfByKey(Hero);
		if (Recruit != INDEX_NONE)
		{
			Save->RecruitFor.RemoveAt(Recruit);
			Save->RecruitBase.RemoveAt(Recruit);
			Save->RecruitNames.RemoveAt(Recruit);
		}
	}
	for (const FName& Hero : Fallen)
	{
		Save->Wounded.AddUnique(Hero);
	}
	WriteSave();
	UE_LOG(LogKeepersCampaign, Log, TEXT("Wounds: %d heroes back, %d wounded"), SatOut.Num(), Fallen.Num());
}

bool UKeepersCampaignSubsystem::HealWound(FName Hero)
{
	if (!IsWounded(Hero) || !SpendGold(HealerCost()))
	{
		return false;
	}
	Save->Wounded.Remove(Hero);
	ResolveWounds({ Hero }, {});
	return true;
}

FString UKeepersCampaignSubsystem::HireRecruit(FName WoundedHero)
{
	FName Base;
	FString Name;
	if (!IsWounded(WoundedHero) || GetRecruit(WoundedHero, Base, Name) || !SpendGold(RecruitCost()))
	{
		return Name;
	}
	static const TCHAR* First[] = { TEXT("Гриша"), TEXT("Федот"), TEXT("Тимоха"), TEXT("Савва"), TEXT("Ерёма"), TEXT("Прохор"),
		TEXT("Кузьма"), TEXT("Фрося"), TEXT("Глаша"), TEXT("Дуня"), TEXT("Марфа"), TEXT("Луша") };
	static const TCHAR* Nick[] = { TEXT("Два Щита"), TEXT("Без Сапога"), TEXT("Курощуп"), TEXT("Громкий Чих"), TEXT("Кривой Лук"),
		TEXT("Последний Шанс"), TEXT("Ленивый Молот"), TEXT("Сырная Борода"), TEXT("Тихий Топот"), TEXT("Гроза Огородов") };
	static const TCHAR* Bases[] = { TEXT("Warrior"), TEXT("Ranger"), TEXT("Priest") };
	Name = FString::Printf(TEXT("%s %s"), First[FMath::RandRange(0, UE_ARRAY_COUNT(First) - 1)], Nick[FMath::RandRange(0, UE_ARRAY_COUNT(Nick) - 1)]);
	Base = Bases[FMath::RandRange(0, UE_ARRAY_COUNT(Bases) - 1)];
	Save->RecruitFor.Add(WoundedHero);
	Save->RecruitBase.Add(Base);
	Save->RecruitNames.Add(Name);
	WriteSave();
	UE_LOG(LogKeepersCampaign, Log, TEXT("Recruit %s (%s) stands in for %s"), *Name, *Base.ToString(), *WoundedHero.ToString());
	return Name;
}

bool UKeepersCampaignSubsystem::GetRecruit(FName WoundedHero, FName& OutBase, FString& OutName) const
{
	const int32 Index = Save ? Save->RecruitFor.IndexOfByKey(WoundedHero) : INDEX_NONE;
	if (Index == INDEX_NONE || !Save->RecruitBase.IsValidIndex(Index) || !Save->RecruitNames.IsValidIndex(Index))
	{
		return false;
	}
	OutBase = Save->RecruitBase[Index];
	OutName = Save->RecruitNames[Index];
	return true;
}

void UKeepersCampaignSubsystem::LearnSpell(FName Hero, FName Spell)'''),
])

# The battle: wounded heroes sit out, recruits take their place.
patch('Public/KeepersPartyBattle.h', [
('''	/** Makes a battle unit another type: E_UnitType and the default unit data of that type. */
	static bool ChangeUnitType(UObject* Unit, uint8 UnitType);
};''', '''	/** Makes a battle unit another type: E_UnitType and the default unit data of that type. */
	static bool ChangeUnitType(UObject* Unit, uint8 UnitType);

	/** A recruit's funny name, empty for everyone else. */
	static FText NicknameOf(const UObject* Unit);
	static bool IsRecruit(const UObject* Unit) { return !NicknameOf(Unit).IsEmpty(); }

	/** Heroes who sit this battle out with wounds. */
	TArray<FName> SatOut;

private:
	TMap<TWeakObjectPtr<const UObject>, FText> Nicknames;
};'''),
])
patch('Private/KeepersPartyBattle.cpp', [
('''	// Abilities learned on the run's level-ups; the units build their ability lists on BeginPlay.
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(&InWorld);
	if (Campaign && Campaign->IsActive() && Campaign->IsInBattleNode())
	{''', '''	// Abilities learned on the run's level-ups; the units build their ability lists on BeginPlay.
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(&InWorld);
	if (Campaign && Campaign->IsActive() && Campaign->IsInBattleNode())
	{
		// Wounded heroes sit out: a recruit takes the place, or the place stays empty (one hero always fights).
		int32 Fighting = Teams[0].Num();
		for (AActor* Unit : Teams[0])
		{
			const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit));
			if (!Def || Def->bEnemy || !Campaign->IsWounded(Def->Id))
			{
				continue;
			}
			FName Base;
			FString Name;
			const FKeepersHeroDef* BaseDef = Campaign->GetRecruit(Def->Id, Base, Name) ? KeepersHeroes::Find(Base) : nullptr;
			if (BaseDef && ChangeUnitType(Unit, static_cast<uint8>(BaseDef->UnitType)))
			{
				Nicknames.Add(Unit, FText::FromString(Name));
				SatOut.Add(Def->Id);
				UE_LOG(LogKeepersPartyBattle, Log, TEXT("%s is wounded: recruit %s fights instead"), *Def->Id.ToString(), *Name);
			}
			else if (Fighting > 1)
			{
				--Fighting;
				SatOut.Add(Def->Id);
				Unit->Destroy();
				UE_LOG(LogKeepersPartyBattle, Log, TEXT("%s is wounded and sits this battle out"), *Def->Id.ToString());
			}
		}'''),
('''			const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit));
			for (const FName& Spell : Def && !Def->bEnemy ? Campaign->GetLearnedSpells(Def->Id) : TArray<FName>())''',
 '''			const FKeepersHeroDef* Def = IsValid(Unit) && !IsRecruit(Unit) ? KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit)) : nullptr;
			for (const FName& Spell : Def && !Def->bEnemy ? Campaign->GetLearnedSpells(Def->Id) : TArray<FName>())'''),
('''void UKeepersPartyBattleSubsystem::OnWorldBeginPlay(UWorld& InWorld)''', '''FText UKeepersPartyBattleSubsystem::NicknameOf(const UObject* Unit)
{
	const UWorld* World = Unit ? Unit->GetWorld() : nullptr;
	const UKeepersPartyBattleSubsystem* Self = World ? World->GetSubsystem<UKeepersPartyBattleSubsystem>() : nullptr;
	const FText* Name = Self ? Self->Nicknames.Find(Unit) : nullptr;
	return Name ? *Name : FText::GetEmpty();
}

void UKeepersPartyBattleSubsystem::OnWorldBeginPlay(UWorld& InWorld)'''),
])

patch('Private/KeepersUnitPanels.cpp', [
('''		const FString English = Byte->Enum->GetDisplayNameTextByValue(Byte->GetPropertyValue(Ptr)).ToString();
		const FText Local = UKeepersBattleHudSubsystem::LocalUnitName(English);''', '''		const FText Nickname = UKeepersPartyBattleSubsystem::NicknameOf(Unit);
		if (!Nickname.IsEmpty())
		{
			return Nickname;
		}
		const FString English = Byte->Enum->GetDisplayNameTextByValue(Byte->GetPropertyValue(Ptr)).ToString();
		const FText Local = UKeepersBattleHudSubsystem::LocalUnitName(English);'''),
('''#include "KeepersHeroes.h"''', '''#include "KeepersHeroes.h"
#include "KeepersPartyBattle.h"'''),
])

# Victory: the fallen are wounded, those who sat out come back; recruits do not grow.
patch('Public/KeepersCampaignUI.h', [
('''	bool bArtifactsApplied = false;''', '''	bool bArtifactsApplied = false;

	/** Heroes on the field when the battle began (recruits not counted). */
	TMap<TWeakObjectPtr<UObject>, FName> BattleHeroes;'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''		// Relics for one hero or for all.
		const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Hero));
		const FName Id = Def ? Def->Id : NAME_None;''', '''		// Relics for one hero or for all; a recruit has only the relics for everyone.
		const bool bRecruit = UKeepersPartyBattleSubsystem::IsRecruit(Hero);
		const FKeepersHeroDef* Def = bRecruit ? nullptr : KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Hero));
		const FName Id = Def ? Def->Id : NAME_None;
		if (Def)
		{
			BattleHeroes.Add(Hero, Id);
		}'''),
('''	if (Campaign->IsBossNode(Campaign->GetCurrentNode()))
	{
		UKeepersChoiceWidget::FOption Done;''', '''	// The fallen are wounded and sit the next battle out; those who sat this one out are well again.
	TArray<FName> Fallen;
	for (const TPair<TWeakObjectPtr<UObject>, FName>& Entry : BattleHeroes)
	{
		int32 Hp = 0;
		if (!Entry.Key.IsValid() || (KeepersReflection::GetUnitStat(Entry.Key.Get(), TEXT("HP_Current"), Hp) && Hp <= 0))
		{
			Fallen.Add(Entry.Value);
		}
	}
	const UKeepersPartyBattleSubsystem* PartyBattle = GetWorld()->GetSubsystem<UKeepersPartyBattleSubsystem>();
	Campaign->ResolveWounds(PartyBattle ? PartyBattle->SatOut : TArray<FName>(), Fallen);

	if (Campaign->IsBossNode(Campaign->GetCurrentNode()))
	{
		UKeepersChoiceWidget::FOption Done;'''),
('''	ShowLevelUp();
}

void UKeepersCampaignWorldSubsystem::ShowLevelUp()''', '''	if (Fallen.Num() > 0)
	{
		FString Names;
		for (const FName& Hero : Fallen)
		{
			const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + (Def ? Def->Name.ToString() : Hero.ToString());
		}
		TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
		UKeepersChoiceWidget::FOption Next;
		Next.Title = FText::FromString(TEXT("Дальше"));
		Next.Description = FText::FromString(TEXT("На привале лекарь поставит на ноги, а за раненого может выйти новобранец."));
		Next.Color = FLinearColor(0.35f, 0.15f, 0.12f);
		Next.Image = KeepersCards::Card(TEXT("Hp"));
		Next.OnPicked = [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowLevelUp(); } };
		Options.Add(MoveTemp(Next));
		ShowChoice(FText::FromString(TEXT("Победа, но не без потерь")),
			FText::FromString(FString::Printf(TEXT("Ранены: %s. Следующий бой они пропустят."), *Names)), MoveTemp(Options));
		return;
	}
	ShowLevelUp();
}

void UKeepersCampaignWorldSubsystem::ShowLevelUp()'''),
('''#include "KeepersHeroes.h"''', '''#include "KeepersHeroes.h"
#include "KeepersPartyBattle.h"'''),
])

# The camp: an infirmary for the wounded.
patch('Public/KeepersCamp.h', [
('''	UPROPERTY()
	TObjectPtr<UHorizontalBox> Merchant;''', '''	UPROPERTY()
	TObjectPtr<UHorizontalBox> Merchant;

	/** Wounded heroes: a healer or a recruit. */
	UPROPERTY()
	TObjectPtr<UHorizontalBox> Infirmary;

	UPROPERTY()
	TObjectPtr<class UBorder> InfirmaryTitle;'''),
('''	void TestBuy(int32 Offer);''', '''	void TestBuy(int32 Offer);
	/** Test hook: heal (or hire a recruit for) the wounded hero Index. */
	void TestInfirmary(int32 Index, bool bRecruit);'''),
('''	void Buy(int32 Offer);''', '''	void Buy(int32 Offer);
	void Heal(FName Hero);
	void Hire(FName Hero);'''),
])
patch('Private/KeepersCamp.cpp', [
('''		Column->AddChildToVerticalBox(KeepersCamp::SectionTitle(WidgetTree, TEXT("Торговец")))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));''',
 '''		InfirmaryTitle = KeepersCamp::SectionTitle(WidgetTree, TEXT("Лазарет"));
		Column->AddChildToVerticalBox(InfirmaryTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		Infirmary = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Infirmary)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));

		Column->AddChildToVerticalBox(KeepersCamp::SectionTitle(WidgetTree, TEXT("Торговец")))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));'''),
('''	// The merchant: artifacts for gold.
	Merchant->ClearChildren();''', '''	// The infirmary: the wounded get a healer, or a recruit stands in for them.
	Infirmary->ClearChildren();
	const TArray<FName> Wounded = Campaign->GetWounded();
	InfirmaryTitle->SetVisibility(Wounded.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Infirmary->SetVisibility(Wounded.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (const FName& Hero : Wounded)
	{
		const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 12.f, FLinearColor(0.8f, 0.3f, 0.25f, 0.6f)));
		Card->SetPadding(FMargin(14.f));
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Lines->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, KeepersCamp::T(FString::Printf(TEXT("%s ранен"), Def ? *Def->Name.ToString() : *Hero.ToString())), KeepersStyle::Heading(22)));
		FName Base;
		FString RecruitName;
		const bool bHasRecruit = Campaign->GetRecruit(Hero, Base, RecruitName);
		const FKeepersHeroDef* BaseDef = bHasRecruit ? KeepersHeroes::Find(Base) : nullptr;
		Lines->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, KeepersCamp::T(bHasRecruit
			? FString::Printf(TEXT("Вместо него выйдет новобранец %s (%s)."), *RecruitName, BaseDef ? *BaseDef->Name.ToString().ToLower() : TEXT(""))
			: FString(TEXT("Пропустит следующий бой."))), KeepersStyle::Body(13), KeepersStyle::TextDim, true))->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));
		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UKeepersFnButton* Heal = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		Heal->Setup(KeepersCamp::T(FString::Printf(TEXT("Лекарь · %d"), UKeepersCampaignSubsystem::HealerCost())), UKeepersFnButton::EKind::Secondary, 14,
			[WeakThis, Hero]() { if (WeakThis.IsValid()) { WeakThis->Heal(Hero); } }, 150.f);
		Heal->SetButtonEnabled(Gold >= UKeepersCampaignSubsystem::HealerCost());
		Buttons->AddChildToHorizontalBox(Heal)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		UKeepersFnButton* Hire = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		Hire->Setup(KeepersCamp::T(bHasRecruit ? TEXT("Нанят") : FString::Printf(TEXT("Новобранец · %d"), UKeepersCampaignSubsystem::RecruitCost())), UKeepersFnButton::EKind::Secondary, 14,
			[WeakThis, Hero]() { if (WeakThis.IsValid()) { WeakThis->Hire(Hero); } }, 170.f);
		Hire->SetButtonEnabled(!bHasRecruit && Gold >= UKeepersCampaignSubsystem::RecruitCost());
		Buttons->AddChildToHorizontalBox(Hire);
		Lines->AddChildToVerticalBox(Buttons);
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetWidthOverride(360.f);
		CardSize->AddChild(Lines);
		Card->SetContent(CardSize);
		Infirmary->AddChildToHorizontalBox(Card)->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
	}

	// The merchant: artifacts for gold.
	Merchant->ClearChildren();'''),
('''void UKeepersCampWidget::TestBuy(int32 Offer)
{
	Buy(Offer);
}''', '''void UKeepersCampWidget::TestBuy(int32 Offer)
{
	Buy(Offer);
}

void UKeepersCampWidget::Heal(FName Hero)
{
	if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this))
	{
		Campaign->HealWound(Hero);
		Refresh();
	}
}

void UKeepersCampWidget::Hire(FName Hero)
{
	if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this))
	{
		Campaign->HireRecruit(Hero);
		Refresh();
	}
}

void UKeepersCampWidget::TestInfirmary(int32 Index, bool bRecruit)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (Campaign && Campaign->GetWounded().IsValidIndex(Index))
	{
		const FName Hero = Campaign->GetWounded()[Index];
		bRecruit ? Hire(Hero) : Heal(Hero);
	}
}'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''	else if (Self->Camp && Action == TEXT("buy"))''', '''	else if (Self->Camp && (Action == TEXT("heal") || Action == TEXT("recruit")))
	{
		Self->Camp->TestInfirmary(Index, Action == TEXT("recruit"));
	}
	else if (Self->Camp && Action == TEXT("buy"))'''),
])
patch('Private/KeepersDirector.cpp', [
('''	else if (Step.Action == TEXT("learn") && Campaign && Args.Num() >= 2)''', '''	else if (Step.Action == TEXT("wound") && Campaign && Args.Num() >= 1)
	{
		Campaign->ResolveWounds({}, { FName(*Args[0]) });
	}
	else if (Step.Action == TEXT("learn") && Campaign && Args.Num() >= 2)'''),
])
print('ok')
