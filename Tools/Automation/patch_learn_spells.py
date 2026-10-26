"""Heroes learn new abilities on level-ups 3 and 5: learnable spells per hero, the save, the level-up cards, the battle."""
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


patch('Public/KeepersHeroes.h', [
('''	TArray<FName> Spells;
	TArray<FKeepersLook> Looks;''', '''	TArray<FName> Spells;
	/** Abilities the hero can learn on the run's level-ups (UKeepersCampaignSubsystem::IsSpellLevel). */
	TArray<FName> LearnableSpells;
	TArray<FKeepersLook> Looks;'''),
])
patch('Private/KeepersHeroes.cpp', [
('''		Warrior.Spells = { TEXT("SwordSlash"), TEXT("RaiseRock"), TEXT("ShieldBash") };''',
 '''		Warrior.Spells = { TEXT("SwordSlash"), TEXT("RaiseRock"), TEXT("ShieldBash") };
		Warrior.LearnableSpells = { TEXT("Throw"), TEXT("Crush"), TEXT("Rage") };'''),
('''		Ranger.Spells = { TEXT("LongRangeArrow"), TEXT("Gust"), TEXT("ShortRangeArrow") };''',
 '''		Ranger.Spells = { TEXT("LongRangeArrow"), TEXT("Gust"), TEXT("ShortRangeArrow") };
		Ranger.LearnableSpells = { TEXT("Dash"), TEXT("SmokeBomb"), TEXT("ChainLightning") };'''),
('''		Priest.Spells = { TEXT("Heal"), TEXT("Sprout"), TEXT("Roots") };''',
 '''		Priest.Spells = { TEXT("Heal"), TEXT("Sprout"), TEXT("Roots") };
		Priest.LearnableSpells = { TEXT("IcePath"), TEXT("RaiseRock"), TEXT("SmokeBomb") };'''),
('''		Mage.Spells = { TEXT("Fireball"), TEXT("IcePath"), TEXT("ChainLightning") };''',
 '''		Mage.Spells = { TEXT("Fireball"), TEXT("IcePath"), TEXT("ChainLightning") };
		Mage.LearnableSpells = { TEXT("Gust"), TEXT("RaiseRock"), TEXT("Heal") };'''),
('''		Rogue.Spells = { TEXT("Dash"), TEXT("Backstab"), TEXT("SmokeBomb") };''',
 '''		Rogue.Spells = { TEXT("Dash"), TEXT("Backstab"), TEXT("SmokeBomb") };
		Rogue.LearnableSpells = { TEXT("ShortRangeArrow"), TEXT("Throw"), TEXT("Roots") };'''),
('''		Barbarian.Spells = { TEXT("Crush"), TEXT("Throw"), TEXT("Rage") };''',
 '''		Barbarian.Spells = { TEXT("Crush"), TEXT("Throw"), TEXT("Rage") };
		Barbarian.LearnableSpells = { TEXT("ShieldBash"), TEXT("RaiseRock"), TEXT("Dash") };'''),
])

patch('Public/KeepersReflection.h', [
('''	KEEPERSGAME_API bool AddUnitStat(UObject* Unit, const FString& Stat, int32 Delta);''',
 '''	KEEPERSGAME_API bool AddUnitStat(UObject* Unit, const FString& Stat, int32 Delta);

	/** Adds an E_Spell entry (by name, "Fireball") to BP_Unit.UnitData.Stats.AvailableSpells unless it is there. */
	KEEPERSGAME_API bool AddUnitSpell(UObject* Unit, const FString& Spell);'''),
])
patch('Private/KeepersReflection.cpp', [
('''	bool SetObject(UObject* Object, const FString& Name, UObject* Value)''',
 '''	bool AddUnitSpell(UObject* Unit, const FString& Spell)
	{
		FProperty* DataProperty = nullptr;
		void* Data = Unit ? FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitData"), &DataProperty) : nullptr;
		const FStructProperty* DataStruct = CastField<FStructProperty>(DataProperty);
		FProperty* StatsProperty = nullptr;
		void* Stats = DataStruct ? FindValuePtr(DataStruct->Struct, Data, TEXT("Stats"), &StatsProperty) : nullptr;
		const FStructProperty* StatsStruct = CastField<FStructProperty>(StatsProperty);
		FProperty* SpellsProperty = nullptr;
		void* Spells = StatsStruct ? FindValuePtr(StatsStruct->Struct, Stats, TEXT("AvailableSpells"), &SpellsProperty) : nullptr;
		const FArrayProperty* Array = CastField<FArrayProperty>(SpellsProperty);
		const FByteProperty* Byte = Array ? CastField<FByteProperty>(Array->Inner) : nullptr;
		const FEnumProperty* EnumProperty = Array ? CastField<FEnumProperty>(Array->Inner) : nullptr;
		const UEnum* Enum = Byte ? Byte->Enum.Get() : EnumProperty ? EnumProperty->GetEnum() : nullptr;
		if (!Enum || !Spells)
		{
			return false;
		}
		int64 Value = INDEX_NONE;
		for (int32 I = 0; I < Enum->NumEnums() - 1; ++I)
		{
			if (Enum->GetDisplayNameTextByIndex(I).ToString() == Spell)
			{
				Value = Enum->GetValueByIndex(I);
			}
		}
		if (Value == INDEX_NONE)
		{
			return false;
		}
		FScriptArrayHelper Helper(Array, Spells);
		for (int32 I = 0; I < Helper.Num(); ++I)
		{
			if (*Helper.GetRawPtr(I) == static_cast<uint8>(Value))
			{
				return false;
			}
		}
		*Helper.GetRawPtr(Helper.AddValue()) = static_cast<uint8>(Value);
		return true;
	}

	bool SetObject(UObject* Object, const FString& Name, UObject* Value)'''),
])

patch('Public/KeepersCampaign.h', [
('''	UPROPERTY(SaveGame)
	TMap<FName, int32> GrowthAction;
};''', '''	UPROPERTY(SaveGame)
	TMap<FName, int32> GrowthAction;

	/** Abilities learned on level-ups, as "Hero:Spell". */
	UPROPERTY(SaveGame)
	TArray<FName> LearnedSpells;
};'''),
('''	void LevelUp(FName Hero, EKeepersGrowth Growth);''', '''	void LevelUp(FName Hero, EKeepersGrowth Growth);

	/** Levels on which a hero learns an ability instead of growing. */
	static bool IsSpellLevel(int32 Level) { return Level == 3 || Level == 5; }
	/** Learnable abilities the hero has not learned yet. */
	TArray<FName> GetSpellsToLearn(FName Hero) const;
	TArray<FName> GetLearnedSpells(FName Hero) const;
	/** A level-up that teaches an ability. */
	void LearnSpell(FName Hero, FName Spell);'''),
])
patch('Private/KeepersCampaign.cpp', [
('''	return 1 + GetGrowth(Hero, EKeepersGrowth::Hp) + GetGrowth(Hero, EKeepersGrowth::Move) + GetGrowth(Hero, EKeepersGrowth::Action);
}''', '''	return 1 + GetGrowth(Hero, EKeepersGrowth::Hp) + GetGrowth(Hero, EKeepersGrowth::Move) + GetGrowth(Hero, EKeepersGrowth::Action) + GetLearnedSpells(Hero).Num();
}

TArray<FName> UKeepersCampaignSubsystem::GetLearnedSpells(FName Hero) const
{
	TArray<FName> Spells;
	const FString Prefix = Hero.ToString() + TEXT(":");
	for (const FName& Entry : Save ? Save->LearnedSpells : TArray<FName>())
	{
		const FString Text = Entry.ToString();
		if (Text.StartsWith(Prefix))
		{
			Spells.Add(FName(*Text.RightChop(Prefix.Len())));
		}
	}
	return Spells;
}

TArray<FName> UKeepersCampaignSubsystem::GetSpellsToLearn(FName Hero) const
{
	TArray<FName> Spells;
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	const TArray<FName> Learned = GetLearnedSpells(Hero);
	for (const FName& Spell : Def ? Def->LearnableSpells : TArray<FName>())
	{
		if (!Learned.Contains(Spell) && !Def->Spells.Contains(Spell))
		{
			Spells.Add(Spell);
		}
	}
	return Spells;
}

void UKeepersCampaignSubsystem::LearnSpell(FName Hero, FName Spell)
{
	if (!Save || GetHeroLevel(Hero) >= MaxHeroLevel || GetLearnedSpells(Hero).Contains(Spell))
	{
		return;
	}
	Save->LearnedSpells.Add(FName(*FString::Printf(TEXT("%s:%s"), *Hero.ToString(), *Spell.ToString())));
	WriteSave();
	UE_LOG(LogKeepersCampaign, Log, TEXT("%s learns %s and grows to level %d"), *Hero.ToString(), *Spell.ToString(), GetHeroLevel(Hero));
}'''),
])

patch('Public/KeepersCampaignUI.h', [
('''	/** A hero's portrait (T_Portrait_<Hero>). */''', '''	/** The picture of an ability: its card medallion, else its spell icon. */
	KEEPERSGAME_API UTexture2D* SpellIcon(FName Spell);
	/** A hero's portrait (T_Portrait_<Hero>). */'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''UTexture2D* KeepersCards::Portrait(FName Hero)''', '''UTexture2D* KeepersCards::SpellIcon(FName Spell)
{
	const FString Name = Spell.ToString();
	if (UTexture2D* Medallion = Card(*Name))
	{
		return Medallion;
	}
	return Icon(Spell == TEXT("Gust") ? TEXT("Icon_WindGust") : *(TEXT("Icon_") + Name));
}

UTexture2D* KeepersCards::Portrait(FName Hero)'''),
('''		// Growth of the hero in this run.
		AddHp(Campaign->GetGrowth(Id, EKeepersGrowth::Hp) * 5);''', '''		// Growth of the hero in this run.
		for (const FName& Spell : Campaign->GetLearnedSpells(Id))
		{
			KeepersReflection::AddUnitSpell(Hero, Spell.ToString());
		}
		AddHp(Campaign->GetGrowth(Id, EKeepersGrowth::Hp) * 5);'''),
('''	TArray<UKeepersChoiceWidget::FOption> Options;
	for (const FGrowthCard& Card : Cards)
	{''', '''	TArray<UKeepersChoiceWidget::FOption> Options;
	// Levels 3 and 5 teach an ability: one of the hero's learnable ones, or toughness instead.
	const int32 NextLevel = Campaign->GetHeroLevel(Hero) + 1;
	const TArray<FName> ToLearn = Campaign->GetSpellsToLearn(Hero);
	const bool bLearns = UKeepersCampaignSubsystem::IsSpellLevel(NextLevel) && ToLearn.Num() > 0;
	if (bLearns)
	{
		for (int32 I = 0; I < FMath::Min(ToLearn.Num(), 2); ++I)
		{
			const FName Spell = ToLearn[I];
			const FKeepersSpellInfo* Info = KeepersHeroes::FindSpell(Spell);
			UKeepersChoiceWidget::FOption Option;
			Option.Title = Info ? Info->Name : FText::FromName(Spell);
			Option.Description = Info ? FText::FromString(Info->Description.ToString() + TEXT("\\n") + Info->Numbers.ToString()) : FText::GetEmpty();
			Option.Color = FLinearColor(0.32f, 0.2f, 0.45f);
			Option.Image = KeepersCards::SpellIcon(Spell);
			Option.OnPicked = [WeakThis, WeakCampaign, Hero, Spell]()
			{
				if (WeakCampaign.IsValid())
				{
					WeakCampaign->LearnSpell(Hero, Spell);
				}
				if (WeakThis.IsValid())
				{
					WeakThis->ShowVictoryRelics();
				}
			};
			Options.Add(MoveTemp(Option));
		}
	}
	for (const FGrowthCard& Card : Cards)
	{
		if (bLearns && Card.Growth != EKeepersGrowth::Hp)
		{
			continue;
		}'''),
('''	ShowChoice(FText::FromString(FString::Printf(TEXT("%s — уровень %d"), *Name, Campaign->GetHeroLevel(Hero) + 1)),
		FText::FromString(FString::Printf(TEXT("Бой не прошёл даром. Чему %s научился? Сейчас: +%d здоровья, +%d движения, +%d действия."), *Name,
			Campaign->GetGrowth(Hero, EKeepersGrowth::Hp) * 5, Campaign->GetGrowth(Hero, EKeepersGrowth::Move), Campaign->GetGrowth(Hero, EKeepersGrowth::Action))),''',
 '''	const FString Grown = FString::Printf(TEXT("Сейчас: +%d здоровья, +%d движения, +%d действия."),
		Campaign->GetGrowth(Hero, EKeepersGrowth::Hp) * 5, Campaign->GetGrowth(Hero, EKeepersGrowth::Move), Campaign->GetGrowth(Hero, EKeepersGrowth::Action));
	ShowChoice(FText::FromString(FString::Printf(TEXT("%s — уровень %d%s"), *Name, NextLevel, bLearns ? TEXT(": новое умение") : TEXT(""))),
		FText::FromString(bLearns ? FString::Printf(TEXT("Опыт открывает новое умение. Какое возьмёт %s? %s"), *Name, *Grown)
			: FString::Printf(TEXT("Бой не прошёл даром. Чему %s научился? %s"), *Name, *Grown)),'''),
])
print('ok')
