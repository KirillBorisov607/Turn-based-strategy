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


patch('Public/KeepersAiLibrary.h', [
('''	 *  - a hero that can be finished off is worth more.
	 */
	UFUNCTION(BlueprintPure, Category = "Keepers|AI")
	static double AdjustPlanScore(UObject* Unit, FIntPoint Origin, FIntPoint Target, double BaseScore);''', '''	 *  - a hero that can be finished off is worth more;
	 *  - a shield bash that throws a hero into water or off a cliff is the best blow of all.
	 * Spell is the plan's E_Spell.
	 */
	UFUNCTION(BlueprintPure, Category = "Keepers|AI")
	static double AdjustPlanScore(UObject* Unit, uint8 Spell, FIntPoint Origin, FIntPoint Target, double BaseScore);'''),
])
patch('Private/KeepersAiLibrary.cpp', [
('''double UKeepersAiLibrary::AdjustPlanScore(UObject* Unit, FIntPoint Origin, FIntPoint Target, double BaseScore)''', '''double UKeepersAiLibrary::AdjustPlanScore(UObject* Unit, uint8 Spell, FIntPoint Origin, FIntPoint Target, double BaseScore)'''),
('''	if (Objective)
	{
		// The unit the heroes protect is the prey.''', '''	// A shield bash pushes the target one cell away from the striker: into water or off a cliff it is a kill.
	static uint8 ShieldBash = MAX_uint8;
	if (ShieldBash == MAX_uint8)
	{
		ShieldBash = MAX_uint8 - 1;
		if (const UEnum* Spells = LoadObject<UEnum>(nullptr, TEXT("/Game/Blueprints/Core/Combat/Spells/Utilities/E_Spell.E_Spell")))
		{
			for (int32 I = 0; I < Spells->NumEnums(); ++I)
			{
				if (Spells->GetDisplayNameTextByIndex(I).ToString() == TEXT("ShieldBash"))
				{
					ShieldBash = static_cast<uint8>(Spells->GetValueByIndex(I));
				}
			}
		}
	}
	if (Spell == ShieldBash && Victim && TeamOf(Victim) != Team && At && !CanFly(Victim))
	{
		const FIntPoint Landing = Target + (Target - Origin);
		const FTile* To = Map.Tiles.Find(Landing);
		if (!To || To->Type == 0 || To->Type == 5 || At->Z - To->Z > Map.Step * 1.5f)
		{
			Bonus += 6.0;
		}
		else if (To->Unit || To->Type == 2)
		{
			Bonus += 1.0;
		}
	}
	if (Objective)
	{
		// The unit the heroes protect is the prey.'''),
])
print('ok')
