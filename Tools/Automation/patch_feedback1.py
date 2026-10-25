"""Feedback fixes: end the whole player turn, menu buttons, old end screen, defeat text, objectives in every battle."""
import os

ROOT = r"C:\ProjectsUE\Turn-based-strategy\The_battle_code\Plugins\KeepersGame\Source\KeepersGame"


def patch(rel, old, new):
    path = os.path.join(ROOT, rel)
    text = open(path, encoding="utf-8-sig").read()
    if new in text:
        return
    assert old in text, f"{rel}: anchor not found: {old[:80]}"
    open(path, "w", encoding="utf-8-sig").write(text.replace(old, new, 1))


# --- End the whole player turn ---------------------------------------------------------------------------------
patch("Public/KeepersCombatLibrary.h", """	/**
	 * Heroes act in any order""", """	/**
	 * "End turn" button: ends the turn of every hero that has not played yet this phase, so the enemies
	 * play next. Stops at a unit controlled by the AI (it plays by itself).
	 */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Combat", meta = (DefaultToSelf = "CombatSystem"))
	static void EndPlayerTurn(UObject* CombatSystem);

	/**
	 * Heroes act in any order""")
patch("Private/KeepersCombatLibrary.cpp", """bool UKeepersCombatLibrary::TrySwitchToUnitAt(UObject* Action, FIntPoint Index)""", """void UKeepersCombatLibrary::EndPlayerTurn(UObject* CombatSystem)
{
	const UObject* First = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
	if (!First)
	{
		return;
	}
	int32 Team = INDEX_NONE;
	KeepersReflection::GetInt(First->GetClass(), First, TEXT("TeamIndex"), Team);
	for (int32 Guard = 0; Guard < 64; ++Guard)
	{
		KeepersReflection::FCall(CombatSystem, TEXT("NextUnit")).Call();
		const UObject* Next = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
		int32 NextTeam = INDEX_NONE;
		if (Next)
		{
			KeepersReflection::GetInt(Next->GetClass(), Next, TEXT("TeamIndex"), NextTeam);
		}
		if (!Next || Next == First || NextTeam != Team || KeepersReflection::GetObject(Next, TEXT("Ai")))
		{
			break;
		}
	}
}

bool UKeepersCombatLibrary::TrySwitchToUnitAt(UObject* Action, FIntPoint Index)""")

# --- Creature spawner public (the protect objective spawns its chicken) ----------------------------------------
patch("Public/KeepersCreatureSubsystem.h", """	/** Spawns a unit of UnitType next to Index for Team and adds it to the combat. */
	AActor* SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag, bool bCanAct = true);

""", "")
patch("Public/KeepersCreatureSubsystem.h", """	static constexpr int32 ChickenMaxCalls = 3;
""", """	static constexpr int32 ChickenMaxCalls = 3;

	/** Spawns a unit of UnitType next to Index for Team and adds it to the combat. */
	AActor* SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag, bool bCanAct = true);
""")

# --- Campaign UI -----------------------------------------------------------------------------------------------
patch("Public/KeepersCampaignUI.h", """	UFUNCTION()
	void OnContinueClicked();""", """	UFUNCTION()
	void OnContinueClicked();

	UFUNCTION()
	void OnTutorialClicked();

	/** Every battle shows a goal: a campaign battle gets one picked by its node, others "defeat all". */
	void EnsureObjective();

	bool bObjectiveChecked = false;""")
patch("Private/KeepersCampaignUI.cpp", """	if (!bMapShown && World->GetOutermost()->GetName().Contains(TEXT("L_CampaignMap")))
	{
		ShowMap();
	}
	TickBattle();""", """	if (!bMapShown && World->GetOutermost()->GetName().Contains(TEXT("L_CampaignMap")))
	{
		ShowMap();
	}
	if (!bObjectiveChecked)
	{
		EnsureObjective();
	}
	TickBattle();""")
# Menu: Continue and Tutorial in the row of the Quit button
patch("Private/KeepersCampaignUI.cpp", """	auto AddButton = [&](const TCHAR* Label, int32 Row, int32 Column, bool bContinue)""", """	int32 BottomRow = 3;
	if (const UWidget* Quit = Menu->WidgetTree->FindWidget(TEXT("Button_Quit")))
	{
		if (const UGridSlot* QuitSlot = Cast<UGridSlot>(Quit->Slot))
		{
			BottomRow = QuitSlot->GetRow();
		}
	}

	auto AddButton = [&](const TCHAR* Label, int32 Row, int32 Column, int32 Action)""")
patch("Private/KeepersCampaignUI.cpp", """		if (bContinue)
		{
			Button->OnClicked.AddDynamic(this, &UKeepersCampaignWorldSubsystem::OnContinueClicked);
		}
		else
		{
			Button->OnClicked.AddDynamic(this, &UKeepersCampaignWorldSubsystem::OnCampaignClicked);
		}
	};
	AddButton(TEXT("Кампания"), 0, 1, false);
	if (UKeepersCampaignSubsystem::HasSavedCampaign())
	{
		AddButton(TEXT("Продолжить"), 3, 0, true);
	}""", """		switch (Action)
		{
		case 0: Button->OnClicked.AddDynamic(this, &UKeepersCampaignWorldSubsystem::OnCampaignClicked); break;
		case 1: Button->OnClicked.AddDynamic(this, &UKeepersCampaignWorldSubsystem::OnContinueClicked); break;
		default: Button->OnClicked.AddDynamic(this, &UKeepersCampaignWorldSubsystem::OnTutorialClicked); break;
		}
	};
	AddButton(TEXT("Кампания"), 0, 1, 0);
	if (UKeepersCampaignSubsystem::HasSavedCampaign())
	{
		AddButton(TEXT("Продолжить"), BottomRow, 0, 1);
	}
	AddButton(TEXT("Обучение"), BottomRow, 2, 2);""")
patch("Private/KeepersCampaignUI.cpp", """void UKeepersCampaignWorldSubsystem::ShowMap()""", """void UKeepersCampaignWorldSubsystem::OnTutorialClicked()
{
	UKeepersTutorialSubsystem::StartTutorial(GetWorld());
}

void UKeepersCampaignWorldSubsystem::EnsureObjective()
{
	UObject* CombatSystem = KeepersUI::FindCombatSystem(GetWorld());
	bool bInCombat = false;
	if (CombatSystem)
	{
		KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
	}
	if (!bInCombat || !KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit")))
	{
		return;
	}
	bObjectiveChecked = true;
	for (TActorIterator<AKeepersBattleObjective> It(GetWorld()); It; ++It)
	{
		return; // the level has its own goal
	}

	// Campaign battles get a goal picked by their node; others defeat every enemy.
	int32 Variant = 0;
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (Campaign && Campaign->IsInBattleNode() && Campaign->GetNodes()[Campaign->GetCurrentNode()].Type == EKeepersNodeType::Battle)
	{
		Variant = Campaign->GetCurrentNode() % 3;
	}

	AKeepersBattleObjective* Objective = GetWorld()->SpawnActorDeferred<AKeepersBattleObjective>(AKeepersBattleObjective::StaticClass(), FTransform::Identity);
	if (!Objective)
	{
		return;
	}
	if (Variant == 1)
	{
		Objective->SurviveTurns = 5;
	}
	else if (Variant == 2)
	{
		// A friendly chicken joins the heroes and must survive.
		TArray<UObject*> Heroes = KeepersUI::Heroes(CombatSystem);
		UKeepersCreatureSubsystem* Creatures = GetWorld()->GetSubsystem<UKeepersCreatureSubsystem>();
		if (Heroes.Num() > 0 && Creatures)
		{
			FIntPoint Index;
			KeepersReflection::GetIntPoint(Heroes[0]->GetClass(), Heroes[0], TEXT("Index"), Index);
			if (AActor* Chicken = Creatures->SpawnUnitNear(CombatSystem, Heroes[0]->GetClass(), 4, Index, 0, 0, 1.f, NAME_None))
			{
				Objective->ProtectedUnit = Chicken;
				Objective->ProtectedName = FText::FromString(TEXT("курицу Пеструшку"));
				Objective->SurviveTurns = 4;
				UKeepersBarkLibrary::Bark(Chicken, FText::FromString(TEXT("Ко! Защитите меня, Хранители!")));
			}
		}
	}
	Objective->FinishSpawning(FTransform::Identity);
}

void UKeepersCampaignWorldSubsystem::ShowMap()""")
patch("Private/KeepersCampaignUI.cpp", """#include "KeepersCampaign.h\"""", """#include "KeepersBarks.h"
#include "KeepersBattleObjective.h"
#include "KeepersCampaign.h"
#include "KeepersCreatureSubsystem.h"
#include "KeepersTutorial.h\"""")
# Hide the old end screen in campaign battles and show ours sooner
patch("Private/KeepersCampaignUI.cpp", """	if (bFinishing)
	{
		// Let the last blow land before the result screen.
		ResultDelay += GetWorld()->GetDeltaSeconds();
		if (ResultDelay > 1.5f)""", """	if (bFinishing)
	{
		// The campaign shows its own result screen instead of the old one.
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->GetClass()->GetName() == TEXT("W_CombatEndScreen_C"))
			{
				It->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		// Let the last blow land before the result screen.
		ResultDelay += GetWorld()->GetDeltaSeconds();
		if (ResultDelay > 0.8f)""")
# Defeat text that matches the enemies
patch("Private/KeepersCampaignUI.cpp", """		ShowChoice(FText::FromString(TEXT("Хранители отступили...")),
			FText::FromString(TEXT("Слизни празднуют. Куры делают вид, что так и задумывали.")), MoveTemp(Options));""", """		ShowChoice(FText::FromString(TEXT("Хранители отступили...")), FText::FromString(DefeatLine(CombatSystem)), MoveTemp(Options));""")
patch("Private/KeepersCampaignUI.cpp", """// ---------------------------------------------------------------------------------------------------------------------

void UKeepersOptionButton::Setup(""", """namespace KeepersUI
{
	/** One line about the winners, built from the enemy types in the battle. */
	static FString DefeatLine(UObject* CombatSystem)
	{
		TSet<uint8> Types;
		FArrayProperty* Property = CombatSystem ? CastField<FArrayProperty>(KeepersReflection::FindMember(CombatSystem->GetClass(), TEXT("UnitsInCombat"))) : nullptr;
		FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
		if (Inner)
		{
			FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(CombatSystem));
			for (int32 I = 0; I < Helper.Num(); ++I)
			{
				UObject* Unit = Inner->GetObjectPropertyValue(Helper.GetRawPtr(I));
				int32 Team = 0;
				FProperty* TypeProperty = nullptr;
				void* TypePtr = Unit ? KeepersReflection::FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitType"), &TypeProperty) : nullptr;
				if (Unit && KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team) && Team != 0 && CastField<FByteProperty>(TypeProperty))
				{
					Types.Add(CastField<FByteProperty>(TypeProperty)->GetPropertyValue(TypePtr));
				}
			}
		}
		TArray<FString> Lines;
		if (Types.Contains(3)) { Lines.Add(TEXT("Слизни радостно булькают.")); }
		if (Types.Contains(4)) { Lines.Add(TEXT("Куры делают вид, что так и задумывали.")); }
		if (Types.Contains(5)) { Lines.Add(TEXT("Летучие мыши пищат от восторга.")); }
		if (Types.Contains(0) || Types.Contains(1) || Types.Contains(2)) { Lines.Add(TEXT("Отряд двойников ехидно машет вслед.")); }
		if (Lines.Num() == 0) { Lines.Add(TEXT("Враги празднуют. Хранители зализывают раны.")); }
		return FString::Join(Lines, TEXT(" "));
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersOptionButton::Setup(""")
print("patched")
