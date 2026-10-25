#include "KeepersBattleObjective.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KeepersReflection.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersObjective, Log, All);

namespace KeepersObjective
{
	static UObject* FindCombatSystem(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_CombatSystem_C"))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static bool IsAlive(UObject* Unit)
	{
		bool bAlive = false;
		KeepersReflection::FCall Call(Unit, TEXT("IsAlive"));
		if (Call.Call())
		{
			Call.Get<bool>(TEXT("IsAlive"), bAlive);
		}
		return bAlive;
	}

	static FText Plural(int32 N, const TCHAR* One, const TCHAR* Few, const TCHAR* Many)
	{
		const int32 Mod10 = N % 10;
		const int32 Mod100 = N % 100;
		const TCHAR* Word = (Mod10 == 1 && Mod100 != 11) ? One : (Mod10 >= 2 && Mod10 <= 4 && (Mod100 < 12 || Mod100 > 14)) ? Few : Many;
		return FText::FromString(FString::Printf(TEXT("%d %s"), N, Word));
	}
}

void UKeepersObjectiveWidget::SetLines(const FText& Title, const FText& Progress)
{
	PendingTitle = Title;
	PendingProgress = Progress;
	if (TitleText)
	{
		TitleText->SetText(Title);
		ProgressText->SetText(Progress);
		ProgressText->SetVisibility(Progress.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

TSharedRef<SWidget> UKeepersObjectiveWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
		Box->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.75f));
		Box->SetPadding(FMargin(18.f, 8.f));
		FSlateBrush Brush = Box->Background;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(12.f, 12.f, 12.f, 12.f));
		Box->SetBrush(Brush);

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		ProgressText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Progress"));
		FSlateFontInfo TitleFont = TitleText->GetFont();
		TitleFont.Size = 18;
		TitleText->SetFont(TitleFont);
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)));
		FSlateFontInfo ProgressFont = ProgressText->GetFont();
		ProgressFont.Size = 14;
		ProgressText->SetFont(ProgressFont);
		ProgressText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		if (UVerticalBoxSlot* TitleSlot = Lines->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* ProgressSlot = Lines->AddChildToVerticalBox(ProgressText))
		{
			ProgressSlot->SetHorizontalAlignment(HAlign_Center);
		}
		Box->SetContent(Lines);

		// Full-screen, click-through root that keeps the panel at the top centre.
		UBorder* Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
		Screen->SetBrushColor(FLinearColor::Transparent);
		Screen->SetHorizontalAlignment(HAlign_Center);
		Screen->SetVerticalAlignment(VAlign_Top);
		Screen->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		Screen->SetVisibility(ESlateVisibility::HitTestInvisible);
		Screen->SetContent(Box);
		WidgetTree->RootWidget = Screen;
		TitleText->SetText(PendingTitle);
		ProgressText->SetText(PendingProgress);
	}
	return Super::RebuildWidget();
}

AKeepersBattleObjective::AKeepersBattleObjective()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AKeepersBattleObjective::BeginPlay()
{
	Super::BeginPlay();
	CombatSystem = KeepersObjective::FindCombatSystem(GetWorld());

	if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
	{
		Widget = CreateWidget<UKeepersObjectiveWidget>(Player, UKeepersObjectiveWidget::StaticClass());
		if (Widget)
		{
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			Widget->AddToViewport(5);
		}
	}
	UpdateWidget();
}

void AKeepersBattleObjective::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
	}
	Super::EndPlay(EndPlayReason);
}

void AKeepersBattleObjective::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!CombatSystem || bFinished)
	{
		return;
	}
	bool bInCombat = false;
	bool bFinishing = false;
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);
	if (!bInCombat || bFinishing)
	{
		return;
	}

	// The protected unit fights on the heroes' side but is played by the AI.
	if (ProtectedUnit && !bProtectedSetUp)
	{
		bProtectedSetUp = true;
		KeepersReflection::FCall SetTeam(CombatSystem, TEXT("SetUnitTeamIndex"));
		SetTeam.Set<UObject*>(TEXT("Unit"), ProtectedUnit.Get());
		SetTeam.Set<int32>(TEXT("TeamIndex"), 0);
		SetTeam.Call();
		KeepersReflection::FCall UseAi(CombatSystem, TEXT("SetUnitUseAi"));
		UseAi.Set<UObject*>(TEXT("Unit"), ProtectedUnit.Get());
		UseAi.Set<bool>(TEXT("UseAi"), true);
		UseAi.Call();
	}

	// A new round starts each time play returns to team 0.
	const UObject* Current = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
	int32 Team = INDEX_NONE;
	if (Current)
	{
		KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), Team);
	}
	if (Current && Team == 0 && LastTeam != 0)
	{
		++Round;
		UE_LOG(LogKeepersObjective, Log, TEXT("Round %d"), Round);
		if (SurviveTurns > 0 && Round > SurviveTurns)
		{
			Finish(true);
			return;
		}
		UpdateWidget();
	}
	if (Current)
	{
		LastTeam = Team;
	}

	if (ProtectedUnit && !KeepersObjective::IsAlive(ProtectedUnit))
	{
		Finish(false);
	}
}

void AKeepersBattleObjective::UpdateWidget()
{
	if (!Widget)
	{
		return;
	}
	TArray<FString> Goals;
	if (SurviveTurns > 0)
	{
		Goals.Add(FString::Printf(TEXT("продержитесь %s"), *KeepersObjective::Plural(SurviveTurns, TEXT("ход"), TEXT("хода"), TEXT("ходов")).ToString()));
	}
	if (ProtectedUnit)
	{
		Goals.Add(FString::Printf(TEXT("защитите %s"), ProtectedName.IsEmpty() ? TEXT("союзника") : *ProtectedName.ToString()));
	}
	if (Goals.Num() == 0)
	{
		Goals.Add(TEXT("победите всех врагов"));
	}
	const FString Title = FString::Printf(TEXT("Цель: %s"), *FString::Join(Goals, TEXT(" и ")));
	const FString Progress = SurviveTurns > 0 ? FString::Printf(TEXT("Ход %d из %d"), FMath::Max(Round, 1), SurviveTurns) : FString();
	Widget->SetLines(FText::FromString(Title), FText::FromString(Progress));
}

void AKeepersBattleObjective::Finish(bool bHeroesWin)
{
	bFinished = true;
	UE_LOG(LogKeepersObjective, Log, TEXT("Objective %s"), bHeroesWin ? TEXT("complete") : TEXT("failed"));
	if (Widget)
	{
		Widget->SetLines(FText::FromString(bHeroesWin ? TEXT("Цель выполнена!") : TEXT("Цель провалена...")), FText::GetEmpty());
	}

	// Enemies of team 1 win when the heroes fail.
	KeepersReflection::FCall FinishCombat(CombatSystem, TEXT("FinishCombat"));
	FinishCombat.Set<int32>(TEXT("WiiningTeamIndex"), bHeroesWin ? 0 : 1);
	FinishCombat.Call();
}
