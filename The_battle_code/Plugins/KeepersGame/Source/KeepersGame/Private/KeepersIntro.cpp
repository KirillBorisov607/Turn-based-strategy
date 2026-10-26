#include "KeepersIntro.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "KeepersBarks.h"
#include "KeepersBattleObjective.h"
#include "KeepersCampaign.h"
#include "KeepersFigure.h"
#include "KeepersHeroes.h"
#include "KeepersJuice.h"
#include "KeepersLevelKit.h"
#include "KeepersLoadingScreen.h"
#include "KeepersReflection.h"
#include "KeepersRegions.h"
#include "KeepersStyle.h"
#include "KeepersTerrainSubsystem.h"
#include "KeepersTutorial.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersIntro, Log, All);

bool UKeepersIntroSubsystem::bSkipNext = false;

namespace KeepersIntro
{
	static int32 TeamOf(const AActor* Unit)
	{
		int32 Team = 0;
		KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		return Team;
	}

	static FVector Centre(const TArray<AActor*>& Units)
	{
		FVector Sum = FVector::ZeroVector;
		for (const AActor* Unit : Units)
		{
			Sum += Unit->GetActorLocation();
		}
		return Units.Num() > 0 ? Sum / Units.Num() : FVector::ZeroVector;
	}

	/** What a hero says when the squad arrives. */
	static const TCHAR* ArrivalLines[] =
	{
		TEXT("Ну и местечко. Кто первый?"),
		TEXT("Тихо… Слишком тихо. Даже для кур."),
		TEXT("Строимся! Раненых не бросаем."),
		TEXT("Я чую слизь. И курицу. И ещё что-то."),
		TEXT("План простой: побеждаем и идём ужинать."),
		TEXT("Местность — наш союзник. Не забывайте."),
		TEXT("Кто-нибудь взял карту? Нет? Ладно."),
		TEXT("Камни земель сами себя не вернут."),
	};
}

bool UKeepersIntroSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UKeepersIntroSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersIntroSubsystem, STATGROUP_Tickables);
}

void UKeepersIntroSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	if (!bChecked)
	{
		UObject* Combat = UKeepersTerrainSubsystem::CombatSystemOf(World);
		bool bInCombat = false;
		if (!Combat || !KeepersReflection::GetBool(Combat->GetClass(), Combat, TEXT("IsInCombat"), bInCombat) || !bInCombat)
		{
			return;
		}
		// Wait for the field to settle: mercenaries, the objective and its markers.
		const AKeepersBattleObjective* Objective = AKeepersBattleObjective::Find(World);
		Wait += DeltaTime;
		// A recording starts its frames after the warm-up: the intro waits for it.
		float Warmup = 0.f;
		if (FParse::Param(FCommandLine::Get(), TEXT("KeepersIntro")))
		{
			FParse::Value(FCommandLine::Get(), TEXT("-KeepersWarmup="), Warmup);
		}
		if (Wait < 0.4f + Warmup || (Objective && !Objective->IsReady() && Wait < 2.f + Warmup))
		{
			return;
		}
		bChecked = true;
		const UKeepersTutorialSubsystem* Tutorial = World->GetSubsystem<UKeepersTutorialSubsystem>();
		// Recordings skip it unless asked for (-KeepersIntro); so does a battle that was resumed.
		const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(World);
		const bool bRecording = FString(FCommandLine::Get()).Contains(TEXT("-KeepersRecord=")) && !FParse::Param(FCommandLine::Get(), TEXT("KeepersIntro"));
		const bool bSkip = bSkipNext || bRecording || (Campaign && Campaign->GetBattleToResume()) || FParse::Param(FCommandLine::Get(), TEXT("KeepersNoIntro")) || (Tutorial && Tutorial->IsTutorialActive())
			|| UKeepersTutorialSubsystem::bPendingTutorial;
		bSkipNext = false;
		if (!bSkip)
		{
			Begin();
		}
		return;
	}
	if (!bPlaying)
	{
		return;
	}
	Time += DeltaTime;
	// The battle's interface keeps showing itself (a new turn, a new unit): it stays hidden until the end.
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		const ESlateVisibility Visibility = It->GetVisibility();
		if (*It != Widget && It->GetWorld() == World && It->IsInViewport() && Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden)
		{
			if (!Hidden.ContainsByPredicate([&](const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Entry) { return Entry.Key.Get() == *It; }))
			{
				Hidden.Emplace(*It, Visibility);
			}
			It->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// The squad walks in from the edge, one after another.
	for (FWalker& Walker : Walkers)
	{
		AActor* Unit = Walker.Unit.Get();
		if (!Unit)
		{
			continue;
		}
		const float Alpha = FMath::Clamp((Time - Walker.Delay) / 2.2f, 0.f, 1.f);
		Unit->SetActorLocation(FMath::Lerp(Walker.Start, Walker.End, Alpha));
		if (Alpha >= 1.f && Walker.Delay >= 0.f)
		{
			Walker.Delay = -1000.f;
			Unit->SetActorRotation(Walker.Rotation);
			if (UKeepersFigureComponent* Figure = Unit->FindComponentByClass<UKeepersFigureComponent>())
			{
				Figure->Play(EKeepersPose::Idle);
			}
			UKeepersJuiceSubsystem::Burst(World, Walker.End, AKeepersBurst::EKind::Puff, FLinearColor(0.7f, 0.65f, 0.55f), 6, 0.6f);
		}
	}

	// The shots one after another; the camera glides along each.
	float Start = 0.f;
	int32 Current = INDEX_NONE;
	for (int32 I = 0; I < Shots.Num(); ++I)
	{
		if (Time < Start + Shots[I].Duration)
		{
			Current = I;
			break;
		}
		Start += Shots[I].Duration;
	}
	if (Current == INDEX_NONE)
	{
		Finish();
		return;
	}
	const FShot& Now = Shots[Current];
	const float Alpha = FMath::Clamp((Time - Start) / Now.Duration, 0.f, 1.f);
	const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
	const FVector Eye = FMath::Lerp(Now.From, Now.To, Smooth);
	const FVector Look = FMath::Lerp(Now.LookFrom, Now.LookTo, Smooth);
	if (Camera)
	{
		Camera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(Now.Fov);
	}
	if (Current != Shot)
	{
		Shot = Current;
		const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(World);
		const FText Region = Campaign && Campaign->IsActive() ? Campaign->GetRegionDef().Name : FText::FromString(TEXT("Быстрый бой"));
		TArray<AActor*> Heroes;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("BP_Unit_C") && KeepersIntro::TeamOf(*It) == 0)
			{
				Heroes.Add(*It);
			}
		}
		if (Shot == 0 && Widget)
		{
			Widget->SetCaption(Region, TaskTitle, FText::FromString(TEXT("Отряд Хранителей прибывает на место.")));
		}
		else if (Shot == 1 && Widget)
		{
			Widget->SetCaption(Region, TaskTitle, FText::GetEmpty());
			if (Heroes.Num() > 0)
			{
				UKeepersBarkLibrary::Bark(Heroes[FMath::RandRange(0, Heroes.Num() - 1)],
					FText::FromString(KeepersIntro::ArrivalLines[FMath::RandRange(0, UE_ARRAY_COUNT(KeepersIntro::ArrivalLines) - 1)]));
			}
		}
		else if (Shot == 2 && Widget)
		{
			Widget->SetCaption(FText::FromString(TEXT("Задание")), TaskLine, FText::GetEmpty());
			UKeepersJuiceSubsystem::Sound(this, TEXT("Card"), 0.8f);
		}
	}
	if (Widget)
	{
		const float Total = Shots.Num() > 0 ? Start + Now.Duration : 1.f;
		Widget->SetShown(FMath::Min(FMath::Clamp(Time / 0.4f, 0.f, 1.f), FMath::Clamp((Total - Time) / 0.4f, 0.f, 1.f)));
	}
}

FVector UKeepersIntroSubsystem::TaskFocus(FText& OutLine) const
{
	UWorld* World = GetWorld();
	const AKeepersBattleObjective* Objective = AKeepersBattleObjective::Find(World);
	OutLine = FText::FromString(TEXT("Победите всех врагов."));
	if (Objective)
	{
		const FString Goal = Objective->GoalText();
		OutLine = FText::FromString(FText::FromString(Goal.Left(1)).ToUpper().ToString() + Goal.RightChop(1) + TEXT("."));
		if (const AActor* Leader = Objective->GetLeader())
		{
			return Leader->GetActorLocation();
		}
		if (const AActor* Protected = Objective->GetProtected())
		{
			return Protected->GetActorLocation();
		}
		FIntPoint Beacon;
		UObject* Combat = UKeepersTerrainSubsystem::CombatSystemOf(World);
		UObject* Grid = Combat ? KeepersReflection::GetObject(Combat, TEXT("Grid")) : nullptr;
		FVector BeaconLocation;
		if (Objective->GetBeacon(Beacon) && Grid && UKeepersTerrainSubsystem::TileLocation(Grid, Beacon, BeaconLocation))
		{
			return BeaconLocation;
		}
	}
	TArray<AActor*> Enemies;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("BP_Unit_C") && KeepersIntro::TeamOf(*It) != 0)
		{
			Enemies.Add(*It);
		}
	}
	// No one thing to look at: the whole band, from further away.
	bWideTask = true;
	return KeepersIntro::Centre(Enemies);
}

void UKeepersIntroSubsystem::Begin()
{
	UWorld* World = GetWorld();
	APlayerController* Player = World->GetFirstPlayerController();
	TArray<AActor*> Heroes;
	TArray<AActor*> Enemies;
	TArray<AActor*> All;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("BP_Unit_C"))
		{
			(KeepersIntro::TeamOf(*It) == 0 ? Heroes : Enemies).Add(*It);
			All.Add(*It);
		}
	}
	if (!Player || Heroes.Num() == 0 || Enemies.Num() == 0)
	{
		return;
	}
	const FVector Field = KeepersIntro::Centre(All);
	const FVector HeroCentre = KeepersIntro::Centre(Heroes);
	const FVector EnemyCentre = KeepersIntro::Centre(Enemies);
	FVector Toward = (EnemyCentre - HeroCentre).GetSafeNormal2D();
	if (Toward.IsNearlyZero())
	{
		Toward = FVector::ForwardVector;
	}
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Toward);
	float Radius = 600.f;
	for (const AActor* Unit : All)
	{
		Radius = FMath::Max(Radius, FVector::Dist2D(Unit->GetActorLocation(), Field));
	}
	FText Line;
	bWideTask = false;
	const FVector Focus = TaskFocus(Line);
	const float Reach = bWideTask ? 2.f : 1.f;
	TaskLine = Line;
	// The battle's name: the level kit's title, else the level's own.
	const FString Level = FPackageName::GetShortName(World->GetOutermost()->GetName());
	TaskTitle = KeepersLoading::PlaceTitle(FName(*Level));
	if (const FKeepersLevelRow* Row = KeepersLevelKit::FindRow(FName(*World->GetOutermost()->GetName())))
	{
		if (!Row->Title.IsEmpty())
		{
			TaskTitle = Row->Title;
		}
	}
	if (const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(World))
	{
		if (Campaign->IsActive() && Campaign->IsInBattleNode() && Campaign->IsBossNode(Campaign->GetCurrentNode()))
		{
			TaskTitle = Campaign->GetRegionDef().BossName;
		}
	}

	// 1. The whole field from high above, sliding sideways.
	FShot Wide;
	const float Far = FMath::Max(Radius * 1.3f, 1600.f);
	Wide.From = Field - Toward * Far + Side * Far * 0.5f + FVector(0.f, 0.f, Far * 1.05f);
	Wide.To = Field - Toward * Far - Side * Far * 0.35f + FVector(0.f, 0.f, Far * 0.9f);
	Wide.LookFrom = Field;
	Wide.LookTo = Field;
	Wide.Duration = 2.4f;
	Wide.Fov = 60.f;
	// 2. Low behind the heroes as they arrive.
	FShot Squad;
	// Above and behind, high enough to clear the trees at the edge of the field.
	// From the side, so the squad crosses the frame as it walks in.
	Squad.From = HeroCentre - Toward * 420.f + Side * 820.f + FVector(0.f, 0.f, 520.f);
	Squad.To = HeroCentre - Toward * 160.f + Side * 700.f + FVector(0.f, 0.f, 440.f);
	Squad.LookFrom = HeroCentre - Toward * 380.f + FVector(0.f, 0.f, 60.f);
	Squad.LookTo = HeroCentre - Toward * 60.f + FVector(0.f, 0.f, 60.f);
	Squad.Duration = 2.6f;
	Squad.Fov = 50.f;
	// 3. The task: what the battle is about.
	FShot Task;
	Task.From = Focus - Toward * 560.f * Reach + Side * 300.f * Reach + FVector(0.f, 0.f, 520.f * Reach);
	Task.To = Focus - Toward * 440.f * Reach + Side * 120.f * Reach + FVector(0.f, 0.f, 420.f * Reach);
	Task.LookFrom = Focus + FVector(0.f, 0.f, 40.f);
	Task.LookTo = Focus + FVector(0.f, 0.f, 60.f);
	Task.Duration = 2.6f;
	Task.Fov = 48.f;
	Shots = { Wide, Squad, Task };

	// The squad steps in from behind, one by one.
	Walkers.Reset();
	for (int32 I = 0; I < Heroes.Num(); ++I)
	{
		AActor* Hero = Heroes[I];
		FWalker& Walker = Walkers.AddDefaulted_GetRef();
		Walker.Unit = Hero;
		Walker.End = Hero->GetActorLocation();
		Walker.Start = Walker.End - Toward * 520.f;
		Walker.Rotation = Hero->GetActorRotation();
		Walker.Delay = 2.0f + I * 0.35f;
		Hero->SetActorLocation(Walker.Start);
		Hero->SetActorRotation(Toward.Rotation());
		if (UKeepersFigureComponent* Figure = Hero->FindComponentByClass<UKeepersFigureComponent>())
		{
			Figure->Play(EKeepersPose::Walk);
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = World->SpawnActor<ACameraActor>(Wide.From, (Wide.LookFrom - Wide.From).Rotation(), Params);
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	Player->SetViewTargetWithBlend(Camera, 0.f);
	// The battle's own interface steps aside.
	Hidden.Reset();
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		if (It->GetWorld() == World && It->IsInViewport() && It->GetVisibility() != ESlateVisibility::Collapsed && It->GetVisibility() != ESlateVisibility::Hidden)
		{
			Hidden.Emplace(*It, It->GetVisibility());
			It->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	Widget = CreateWidget<UKeepersIntroWidget>(Player, UKeepersIntroWidget::StaticClass());
	TWeakObjectPtr<UKeepersIntroSubsystem> WeakThis(this);
	Widget->OnSkip = [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Finish(); } };
	Widget->AddToViewport(500);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Widget->TakeWidget());
	Player->SetInputMode(Mode);
	bPlaying = true;
	Time = 0.f;
	Shot = INDEX_NONE;
	UE_LOG(LogKeepersIntro, Log, TEXT("Intro: %d heroes, %d enemies, task \"%s\""), Heroes.Num(), Enemies.Num(), *TaskLine.ToString());
}

void UKeepersIntroSubsystem::Finish()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	// Everyone where the battle has them.
	for (const FWalker& Walker : Walkers)
	{
		if (AActor* Unit = Walker.Unit.Get())
		{
			Unit->SetActorLocation(Walker.End);
			Unit->SetActorRotation(Walker.Rotation);
			if (UKeepersFigureComponent* Figure = Unit->FindComponentByClass<UKeepersFigureComponent>())
			{
				Figure->Play(EKeepersPose::Idle);
			}
		}
	}
	Walkers.Reset();
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (Player)
	{
		Player->SetViewTargetWithBlend(Player->GetPawn(), 0.8f, VTBlend_Cubic);
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		Player->SetInputMode(Mode);
		Player->SetShowMouseCursor(true);
	}
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Other : Hidden)
	{
		if (Other.Key.IsValid())
		{
			Other.Key->SetVisibility(Other.Value);
		}
	}
	Hidden.Reset();
	if (Camera)
	{
		// After the blend back.
		TWeakObjectPtr<ACameraActor> WeakCamera(Camera);
		FTimerHandle Timer;
		GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakCamera]() { if (WeakCamera.IsValid()) { WeakCamera->Destroy(); } }), 1.f, false);
		Camera = nullptr;
	}
	UE_LOG(LogKeepersIntro, Log, TEXT("Intro over"));
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersIntroWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;
		// A clear catcher over the whole screen takes the clicks.
		UBorder* Catcher = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Catcher->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.01f));
		UCanvasPanelSlot* CatcherSlot = Canvas->AddChildToCanvas(Catcher);
		CatcherSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CatcherSlot->SetOffsets(FMargin(0.f));
		TopBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		TopBar->SetBrushColor(FLinearColor::Black);
		UCanvasPanelSlot* TopSlot = Canvas->AddChildToCanvas(TopBar);
		TopSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
		TopSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 110.f));
		BottomBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BottomBar->SetBrushColor(FLinearColor::Black);
		BottomBar->SetPadding(FMargin(80.f, 14.f));
		UVerticalBox* Captions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		KickerText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(20), KeepersStyle::Gold);
		Captions->AddChildToVerticalBox(KickerText);
		TitleText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(44), KeepersStyle::Text, true);
		Captions->AddChildToVerticalBox(TitleText);
		LineText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(18), FLinearColor(0.85f, 0.85f, 0.85f), true);
		Captions->AddChildToVerticalBox(LineText);
		BottomBar->SetContent(Captions);
		UCanvasPanelSlot* BottomSlot = Canvas->AddChildToCanvas(BottomBar);
		BottomSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		BottomSlot->SetOffsets(FMargin(0.f, -190.f, 0.f, 190.f));
		SkipText = KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("щёлкните или нажмите пробел, чтобы пропустить")), KeepersStyle::Body(13), KeepersStyle::TextDim);
		UCanvasPanelSlot* SkipSlot = Canvas->AddChildToCanvas(SkipText);
		SkipSlot->SetAnchors(FAnchors(1.f, 0.f));
		SkipSlot->SetAlignment(FVector2D(1.f, 0.5f));
		SkipSlot->SetPosition(FVector2D(-40.f, 55.f));
		SkipSlot->SetAutoSize(true);
		SetShown(0.f);
	}
	return Super::RebuildWidget();
}

void UKeepersIntroWidget::SetCaption(const FText& Kicker, const FText& Title, const FText& Line)
{
	if (KickerText)
	{
		KickerText->SetText(Kicker);
		TitleText->SetText(Title);
		LineText->SetText(Line);
		LineText->SetVisibility(Line.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UKeepersIntroWidget::SetShown(float Alpha)
{
	if (TopBar)
	{
		TopBar->SetRenderTranslation(FVector2D(0.f, -110.f * (1.f - Alpha)));
		BottomBar->SetRenderTranslation(FVector2D(0.f, 190.f * (1.f - Alpha)));
		SkipText->SetRenderOpacity(Alpha);
	}
}

FReply UKeepersIntroWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (OnSkip)
	{
		OnSkip();
	}
	return FReply::Handled();
}

FReply UKeepersIntroWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (OnSkip)
	{
		OnSkip();
	}
	return FReply::Handled();
}
