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


patch('Private/KeepersAiLibrary.cpp', [
('''#include "KeepersIntentSubsystem.h"''', '''#include "KeepersBattleObjective.h"
#include "KeepersIntentSubsystem.h"'''),
('''	const int32 Team = TeamOf(Unit);
	double Bonus = 0.0;
''', '''	const int32 Team = TeamOf(Unit);
	double Bonus = 0.0;
	// The battle's objective changes what the enemies want (only the enemies of the heroes read it).
	const AKeepersBattleObjective* Objective = Team != 0 ? AKeepersBattleObjective::Find(Unit->GetWorld()) : nullptr;
	// Against the clock the enemies rush: no time to keep safe.
	const bool bRush = Objective && Objective->IsTimed();
	if (bRush)
	{
		Bonus += 1.0;
	}
'''),
('''		Bonus -= 0.6 * FMath::Min(Edges, 3);''', '''		Bonus -= (bRush ? 0.3 : 0.6) * FMath::Min(Edges, 3);'''),
('''	const bool bWounded = Hp * 3 < HpMax;''', '''	const bool bWounded = !bRush && Hp * 3 < HpMax;'''),
('''	return BaseScore + Bonus;''', '''	if (Objective)
	{
		// The unit the heroes protect is the prey.
		if (Victim && Victim == Objective->GetProtected())
		{
			Bonus += 3.0;
		}
		// The beacon is guarded: spots near it are worth more.
		FIntPoint Beacon;
		if (Objective->GetBeacon(Beacon))
		{
			Bonus += 0.6 * FMath::Max(0, 3 - StepsBetween(Map.bHex, Origin, Beacon));
		}
		// The leader keeps back; the others stand by it.
		if (const AActor* Leader = Objective->GetLeader())
		{
			FIntPoint LeaderIndex;
			if (Leader == Unit)
			{
				Bonus += Nearest < 99 ? 0.4 * FMath::Min(Nearest - 1, 3) : 0.0;
			}
			else if (KeepersReflection::GetIntPoint(Leader->GetClass(), Leader, TEXT("Index"), LeaderIndex) && StepsBetween(Map.bHex, Origin, LeaderIndex) <= 2)
			{
				Bonus += 0.8;
			}
		}
	}
	return BaseScore + Bonus;'''),
])
print('ok')
