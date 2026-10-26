"""The Mage, the Rogue and the Barbarian join the battles: unit types, unlocking, the party in battle."""
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
('''	/** Opened outfits as "Hero:Index"; the first outfit of every hero is always open. */
	UPROPERTY(SaveGame)
	TArray<FName> UnlockedLooks;
};''', '''	/** Opened outfits as "Hero:Index"; the first outfit of every hero is always open. */
	UPROPERTY(SaveGame)
	TArray<FName> UnlockedLooks;

	/** Heroes opened beyond the first three (the Mage, the Rogue, the Barbarian). */
	UPROPERTY(SaveGame)
	TArray<FName> UnlockedHeroes;
};'''),
('''	/** Whether the hero can be taken into the squad now. */
	bool IsUnlocked(FName Hero) const;''', '''	/** Whether the hero can be taken into the squad now. */
	bool IsUnlocked(FName Hero) const;

	/** Opens a hero for good (a region cleared, an event); returns true the first time. */
	bool UnlockHero(FName Hero);'''),
])
patch('Private/KeepersHeroes.cpp', [
('''bool UKeepersPartySubsystem::IsUnlocked(FName Hero) const
{
	// Heroes need a battle unit; the others open with the campaign's later regions.
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	return Def && Def->UnitType != INDEX_NONE;
}''', '''bool UKeepersPartySubsystem::IsUnlocked(FName Hero) const
{
	// The first three are open from the start; the others open in the campaign (Unlock tells how).
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	return Def && Def->UnitType != INDEX_NONE && (Def->Unlock.IsEmpty() || Save->UnlockedHeroes.Contains(Hero));
}

bool UKeepersPartySubsystem::UnlockHero(FName Hero)
{
	if (!KeepersHeroes::Find(Hero) || Save->UnlockedHeroes.Contains(Hero))
	{
		return false;
	}
	Save->UnlockedHeroes.Add(Hero);
	WriteSave();
	return true;
}'''),
('''		Mage.Unlock = T(TEXT("Пройдите Солёные болота"));''', '''		Mage.Unlock = T(TEXT("Пройдите Солёные болота"));
		Mage.UnitType = 9;'''),
('''		Rogue.Unlock = T(TEXT("Событие «Таверна»"));''', '''		Rogue.Unlock = T(TEXT("Событие «Таверна»"));
		Rogue.UnitType = 10;'''),
('''		Barbarian.Unlock = T(TEXT("Победа на «Жаре 1»"));''', '''		Barbarian.Unlock = T(TEXT("Пройдите Ледяной перевал"));
		Barbarian.UnitType = 11;'''),
('''		Brute.Spells = { TEXT("SwordSlash") };''', '''		Brute.Spells = { TEXT("SwordSlash"), TEXT("ShieldBash") };'''),
('''			S(TEXT("Fireball"), TEXT("Огненный шар"), TEXT("Взрыв по области. Поджигает лес."), TEXT("4–6 урона · область"), P::Cast),
			S(TEXT("IcePath"), TEXT("Ледяная дорожка"), TEXT("Вода под целью замерзает, по льду скользят."), TEXT("3 клетки в линию"), P::Cast),
			S(TEXT("ChainLightning"), TEXT("Цепная молния"), TEXT("Молния перескакивает между врагами."), TEXT("3 урона · до 3 целей"), P::Cast),
			S(TEXT("Dash"), TEXT("Рывок"), TEXT("Пробегает сквозь врага на клетку за ним."), TEXT("до 3 клеток")),
			S(TEXT("Backstab"), TEXT("Удар в спину"), TEXT("Сзади бьёт вдвое сильнее."), TEXT("3–4 урона · ×2 сзади")),
			S(TEXT("SmokeBomb"), TEXT("Дымовая шашка"), TEXT("Дым закрывает обзор на два хода."), TEXT("область 2 клетки"), P::Cast),
			S(TEXT("Crush"), TEXT("Сокрушить"), TEXT("Ломает скалы и деревья вместе с врагами рядом."), TEXT("5–7 урона · рядом")),
			S(TEXT("Throw"), TEXT("Бросок врага"), TEXT("Хватает соседа и бросает через себя."), TEXT("2 урона + бросок")),
			S(TEXT("Rage"), TEXT("Ярость"), TEXT("Чем меньше здоровья, тем сильнее удары."), TEXT("+1 урон за каждые 10 потерянных HP"), P::Cast),''', '''			S(TEXT("Fireball"), TEXT("Огненный шар"), TEXT("Взрыв по клетке и вокруг неё. Лес в огне сгорает дотла."), TEXT("3–5 урона · область · 2–4 клетки · перезарядка 2"), P::Cast),
			S(TEXT("IcePath"), TEXT("Ледяная дорожка"), TEXT("Вода под целью и за ней замерзает: по льду можно пройти."), TEXT("1–2 урона · −1 ОП · 1–3 клетки по прямой"), P::Cast),
			S(TEXT("ChainLightning"), TEXT("Цепная молния"), TEXT("Молния перескакивает ещё на двух врагов рядом с целью."), TEXT("3 урона · до 3 целей · 2–4 клетки"), P::Cast),
			S(TEXT("Dash"), TEXT("Рывок"), TEXT("Прыжок на пустую клетку; враги рядом с местом приземления получают удар."), TEXT("2 урона рядом · 2–3 клетки по прямой")),
			S(TEXT("Backstab"), TEXT("Удар в спину"), TEXT("Если рядом с врагом стоит союзник, удар вдвое сильнее."), TEXT("4 урона · ×2 в окружении · рядом")),
			S(TEXT("SmokeBomb"), TEXT("Дымовая шашка"), TEXT("Дым на клетке и вокруг закрывает обзор на два хода."), TEXT("1–3 клетки · перезарядка 3"), P::Cast),
			S(TEXT("Crush"), TEXT("Сокрушить"), TEXT("Мощный удар; скалы и деревья вокруг цели — в щепки."), TEXT("5–7 урона · рядом")),
			S(TEXT("Throw"), TEXT("Бросок врага"), TEXT("Хватает соседа и бросает через себя: в воду — насмерть."), TEXT("2 урона + бросок · рядом")),
			S(TEXT("Rage"), TEXT("Ярость"), TEXT("Платит здоровьем и сразу действует ещё раз, +1 к движению."), TEXT("−4 HP · раз за ход · перезарядка 3"), P::Cast),'''),
])
print('ok')
