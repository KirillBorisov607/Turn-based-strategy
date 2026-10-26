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


patch('Public/KeepersCampaign.h', [
('''	/** Battle level for Battle/Elite/Boss nodes. */
	UPROPERTY(BlueprintReadOnly, SaveGame)
	FName Level;
};''', '''	/** Battle level for Battle/Elite/Boss nodes. */
	UPROPERTY(BlueprintReadOnly, SaveGame)
	FName Level;

	/** EKeepersObjectiveKind the battle gets if its level has no objective of its own. */
	UPROPERTY(BlueprintReadOnly, SaveGame)
	uint8 Objective = 0;
};'''),
])
patch('Private/KeepersCampaign.cpp', [
('''			case EKeepersNodeType::Battle:
				Node.Level = Battles[Random.RandRange(0, Battles.Num() - 1)];
				break;
			case EKeepersNodeType::Elite:
				Node.Level = Elites[Random.RandRange(0, Elites.Num() - 1)];
				break;''', '''			case EKeepersNodeType::Battle:
			{
				Node.Level = Battles[Random.RandRange(0, Battles.Num() - 1)];
				// Not every battle is "defeat them all": a beacon to reach, a leader to fell, a siege to hold.
				const float Goal = Random.FRand();
				Node.Objective = static_cast<uint8>(Goal < 0.25f ? EKeepersObjectiveKind::Reach
					: Goal < 0.45f ? EKeepersObjectiveKind::Leader
					: Goal < 0.6f ? EKeepersObjectiveKind::Survive : EKeepersObjectiveKind::None);
				break;
			}
			case EKeepersNodeType::Elite:
				Node.Level = Elites[Random.RandRange(0, Elites.Num() - 1)];
				Node.Objective = static_cast<uint8>(Random.FRand() < 0.5f ? EKeepersObjectiveKind::Leader : EKeepersObjectiveKind::None);
				break;'''),
('''void UKeepersCampaignSubsystem::OpenLevel(FName Level) const
{''', '''void UKeepersCampaignSubsystem::OpenLevel(FName Level) const
{
	// The battle of the current node takes its objective along.
	if (Save && Save->Nodes.IsValidIndex(Save->CurrentNode) && Save->Nodes[Save->CurrentNode].Level == Level)
	{
		AKeepersBattleObjective::RequestForNextBattle(static_cast<EKeepersObjectiveKind>(Save->Nodes[Save->CurrentNode].Objective));
	}'''),
('''#include "KeepersLevelKit.h"
''', '''#include "KeepersBattleObjective.h"
#include "KeepersLevelKit.h"
'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''			const FText Label = KeepersUI::NodeName(Node.Type);''', '''			// The battle's objective shows on its node.
			static const TCHAR* Goals[] = { TEXT(""), TEXT(" · маяк"), TEXT(" · вожак"), TEXT(" · осада") };
			const FText Label = FText::FromString(KeepersUI::NodeName(Node.Type).ToString() + Goals[FMath::Min<int32>(Node.Objective, 3)]);'''),
])
print('ok')
