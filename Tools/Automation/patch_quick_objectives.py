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


patch('Private/KeepersMainMenu.cpp', [
('''		const TCHAR* Level;
		const TCHAR* Name;
		const TCHAR* Description;
	};''', '''		const TCHAR* Level;
		const TCHAR* Name;
		const TCHAR* Description;
		/** The objective the battle gets (the level's own objective wins). */
		EKeepersObjectiveKind Objective = EKeepersObjectiveKind::None;
	};'''),
('''		{ TEXT("L_Combat_FairDuel"), TEXT("Честная дуэль"),''', '''		{ TEXT("L_Combat_FairDuel"), TEXT("Прорыв к маяку"), TEXT("Доведите хотя бы одного героя до маяка на той стороне. Враги будут его стеречь."), EKeepersObjectiveKind::Reach },
		{ TEXT("L_Slice_Hex"), TEXT("Охота на вожака"), TEXT("Наёмники кур с вожаком во главе. Свалите вожака — остальные разбегутся. Он держится позади."), EKeepersObjectiveKind::Leader },
		{ TEXT("L_Combat_SoSlimy"), TEXT("Осада"), TEXT("Продержитесь четыре хода. Слизни знают, что время на вашей стороне, и лезут напролом."), EKeepersObjectiveKind::Survive },
		{ TEXT("L_Combat_FairDuel"), TEXT("Честная дуэль"),'''),
('''			const bool bMercenaries = FCString::Strcmp(Battle.Name, TEXT("Наёмники кур")) == 0;
			AddItem(Battle.Name, Battle.Description, [WeakThis, Level, bMercenaries]()
			{
				if (WeakThis.IsValid())
				{''', '''			const bool bMercenaries = FCString::Strcmp(Battle.Name, TEXT("Наёмники кур")) == 0 || Battle.Objective == EKeepersObjectiveKind::Leader;
			const EKeepersObjectiveKind Objective = Battle.Objective;
			AddItem(Battle.Name, Battle.Description, [WeakThis, Level, bMercenaries, Objective]()
			{
				if (WeakThis.IsValid())
				{
					AKeepersBattleObjective::RequestForNextBattle(Objective);'''),
('''#include "KeepersLevelKit.h"
''', '''#include "KeepersBattleObjective.h"
#include "KeepersLevelKit.h"
'''),
])
patch('Private/KeepersDirector.cpp', [
('''	Fps = FMath::Clamp(Fps, 5.f, 60.f);''', '''	Fps = FMath::Clamp(Fps, 5.f, 60.f);
	// The objective of the first battle: -KeepersObjective=reach|leader|survive.
	FString Goal;
	if (FParse::Value(CommandLine, TEXT("-KeepersObjective="), Goal))
	{
		AKeepersBattleObjective::RequestForNextBattle(Goal == TEXT("reach") ? EKeepersObjectiveKind::Reach
			: Goal == TEXT("leader") ? EKeepersObjectiveKind::Leader : Goal == TEXT("survive") ? EKeepersObjectiveKind::Survive : EKeepersObjectiveKind::None);
		if (Goal == TEXT("leader"))
		{
			UKeepersCampaignSubsystem::SetMercenaryBattle(GetGameInstance()->GetWorld(), true);
		}
	}'''),
('''#include "KeepersCampaign.h"
''', '''#include "KeepersBattleObjective.h"
#include "KeepersCampaign.h"
'''),
])
print('ok')
