#include "KeepersPauseMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KeepersCampaign.h"
#include "KeepersLoadingScreen.h"
#include "KeepersMainMenu.h"
#include "KeepersSquad.h"
#include "KeepersStyle.h"

namespace KeepersPause
{
	static FText T(const TCHAR* Text)
	{
		return FText::FromString(Text);
	}
}

TSharedRef<SWidget> UKeepersPauseWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Darken the paused battle.
		UBorder* Veil = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Veil->SetBrushColor(FLinearColor(0.f, 0.f, 0.02f, 0.62f));
		UCanvasPanelSlot* VeilSlot = Canvas->AddChildToCanvas(Veil);
		VeilSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		VeilSlot->SetOffsets(FMargin(0.f));

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 16.f));
		Panel->SetPadding(FMargin(40.f, 30.f, 40.f, 32.f));
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Title = KeepersStyle::MakeText(WidgetTree, KeepersPause::T(TEXT("Пауза")), KeepersStyle::Heading(46));
		Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);
		UBorder* Underline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Underline->SetBrush(KeepersStyle::Panel(KeepersStyle::Gold, 0.f, FLinearColor::Transparent));
		USizeBox* UnderlineSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		UnderlineSize->SetWidthOverride(160.f);
		UnderlineSize->SetHeightOverride(2.f);
		UnderlineSize->AddChild(Underline);
		Column->AddChildToVerticalBox(UnderlineSize)->SetHorizontalAlignment(HAlign_Center);
		List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Column->AddChildToVerticalBox(List)->SetPadding(FMargin(0.f, 22.f, 0.f, 0.f));
		Note = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(13), KeepersStyle::TextDim, true);
		Note->SetJustification(ETextJustify::Center);
		USizeBox* NoteSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NoteSize->SetWidthOverride(420.f);
		NoteSize->AddChild(Note);
		UVerticalBoxSlot* NoteSlot = Column->AddChildToVerticalBox(NoteSize);
		NoteSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		NoteSlot->SetHorizontalAlignment(HAlign_Center);
		Panel->SetContent(Column);
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);

		// The question layer: darkens the pause menu and asks over it.
		Question = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Question->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Question->SetHorizontalAlignment(HAlign_Center);
		Question->SetVerticalAlignment(VAlign_Center);
		Question->SetVisibility(ESlateVisibility::Collapsed);
		UBorder* QuestionPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		QuestionPanel->SetBrush(KeepersStyle::Panel(KeepersStyle::InkLight, 16.f, KeepersStyle::Gold, 2.f));
		QuestionPanel->SetPadding(FMargin(44.f, 30.f, 44.f, 32.f));
		UVerticalBox* QuestionColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		QuestionTitle = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(38), KeepersStyle::Gold);
		QuestionTitle->SetJustification(ETextJustify::Center);
		QuestionColumn->AddChildToVerticalBox(QuestionTitle)->SetHorizontalAlignment(HAlign_Center);
		QuestionText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(16), KeepersStyle::Text, true);
		QuestionText->SetJustification(ETextJustify::Center);
		USizeBox* QuestionTextSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		QuestionTextSize->SetWidthOverride(440.f);
		QuestionTextSize->AddChild(QuestionText);
		UVerticalBoxSlot* QuestionTextSlot = QuestionColumn->AddChildToVerticalBox(QuestionTextSize);
		QuestionTextSlot->SetHorizontalAlignment(HAlign_Center);
		QuestionTextSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 24.f));
		UHorizontalBox* QuestionButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		TWeakObjectPtr<UKeepersPauseWidget> WeakThis(this);
		QuestionYes = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		QuestionYes->Setup(FText::GetEmpty(), UKeepersFnButton::EKind::Danger, 22, [WeakThis]()
		{
			if (WeakThis.IsValid() && WeakThis->QuestionAction)
			{
				TFunction<void()> Action = WeakThis->QuestionAction;
				Action();
			}
		}, 220.f);
		QuestionButtons->AddChildToHorizontalBox(QuestionYes)->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		QuestionNo = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		QuestionNo->Setup(FText::GetEmpty(), UKeepersFnButton::EKind::Primary, 22, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->CloseQuestion(); } }, 220.f);
		QuestionButtons->AddChildToHorizontalBox(QuestionNo);
		QuestionColumn->AddChildToVerticalBox(QuestionButtons)->SetHorizontalAlignment(HAlign_Center);
		QuestionPanel->SetContent(QuestionColumn);
		Question->SetContent(QuestionPanel);
		UCanvasPanelSlot* QuestionSlot = Canvas->AddChildToCanvas(Question);
		QuestionSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		QuestionSlot->SetOffsets(FMargin(0.f));

		ShowMain();
	}
	return Super::RebuildWidget();
}

void UKeepersPauseWidget::ShowMain()
{
	bSettings = false;
	List->ClearChildren();
	Title->SetText(KeepersPause::T(TEXT("Пауза")));
	const bool bCampaign = UKeepersCampaignSubsystem::Get(this) && UKeepersCampaignSubsystem::Get(this)->IsActive();
	Note->SetText(KeepersPause::T(bCampaign ? TEXT("Кампания сохраняется после каждого пройденного узла.") : TEXT("P или Esc — вернуться в игру.")));

	TWeakObjectPtr<UKeepersPauseWidget> WeakThis(this);
	auto Add = [this](const TCHAR* Label, UKeepersFnButton::EKind Kind, TFunction<void()> OnClicked)
	{
		UKeepersFnButton* Button = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		Button->Setup(KeepersPause::T(Label), Kind, 24, MoveTemp(OnClicked), 420.f);
		List->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 5.f));
		return Button;
	};
	Add(TEXT("Продолжить"), UKeepersFnButton::EKind::Primary, [WeakThis]() { if (WeakThis.IsValid() && WeakThis->OnClose) { WeakThis->OnClose(); } });
	Add(TEXT("Настройки"), UKeepersFnButton::EKind::Secondary, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowSettings(); } });
	Add(TEXT("В главное меню"), UKeepersFnButton::EKind::Secondary, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ToMainMenu(); } });
	Add(TEXT("Выйти из игры"), UKeepersFnButton::EKind::Danger, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Quit(); } });
}

void UKeepersPauseWidget::ShowSettings()
{
	bSettings = true;
	List->ClearChildren();
	Title->SetText(KeepersPause::T(TEXT("Настройки")));
	Note->SetText(KeepersPause::T(TEXT("Изменения применяются и сохраняются сразу.")));
	UKeepersMainMenuWidget::AddSettingRows(this, List, 0.f);
	UKeepersFnButton* Back = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
	TWeakObjectPtr<UKeepersPauseWidget> WeakThis(this);
	Back->Setup(KeepersPause::T(TEXT("‹ Назад")), UKeepersFnButton::EKind::Secondary, 20, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowMain(); } }, 200.f);
	List->AddChildToVerticalBox(Back)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
}

void UKeepersPauseWidget::ToMainMenu()
{
	const bool bCampaign = UKeepersCampaignSubsystem::Get(this) && UKeepersCampaignSubsystem::Get(this)->IsActive();
	TWeakObjectPtr<UKeepersPauseWidget> WeakThis(this);
	Ask(TEXT("Вернуться к костру?"),
		bCampaign ? TEXT("Этот бой придётся начать заново, но поход не пропадёт: продолжите его из главного меню.")
			: TEXT("Этот бой не сохранится."),
		TEXT("Да, к костру"), TEXT("Нет, остаюсь"), [WeakThis]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->FreezeLevel();
		if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(WeakThis.Get()); Campaign && Campaign->IsActive())
		{
			Campaign->ReturnToMainMenu(WeakThis.Get());
		}
		else
		{
			KeepersLoading::OpenLevel(WeakThis.Get(), UKeepersCampaignSubsystem::MainMenuLevel);
		}
	});
}

void UKeepersPauseWidget::Quit()
{
	TWeakObjectPtr<UKeepersPauseWidget> WeakThis(this);
	Ask(TEXT("Покинуть игру?"), TEXT("Хранители подождут у костра. Текущий бой не сохранится."),
		TEXT("Да, бежать"), TEXT("Нет, остаюсь"), [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->FreezeLevel();
			UKismetSystemLibrary::QuitGame(WeakThis.Get(), WeakThis->GetOwningPlayer(), EQuitPreference::Quit, false);
		}
	});
}

void UKeepersPauseWidget::Ask(const TCHAR* InQuestion, const TCHAR* Details, const TCHAR* Yes, const TCHAR* No, TFunction<void()> Action)
{
	QuestionAction = MoveTemp(Action);
	QuestionTitle->SetText(KeepersPause::T(InQuestion));
	QuestionText->SetText(KeepersPause::T(Details));
	QuestionYes->SetLabel(KeepersPause::T(Yes));
	QuestionNo->SetLabel(KeepersPause::T(No));
	Question->SetVisibility(ESlateVisibility::Visible);
}

void UKeepersPauseWidget::CloseQuestion()
{
	QuestionAction = nullptr;
	Question->SetVisibility(ESlateVisibility::Collapsed);
}

void UKeepersPauseWidget::FreezeLevel()
{
	// The Blueprints of the grid and the player actions ask for the player controller every tick;
	// on the way out it is already gone.
	if (UWorld* World = GetWorld())
	{
		// Unpaused, so the loading screen's timer runs.
		UGameplayStatics::SetGamePaused(World, false);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
		}
	}
}

FReply UKeepersPauseWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::F10)
	{
		if (Question && Question->GetVisibility() == ESlateVisibility::Visible)
		{
			CloseQuestion();
		}
		else if (bSettings)
		{
			ShowMain();
		}
		else if (OnClose)
		{
			OnClose();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---------------------------------------------------------------------------------------------------------------------

TStatId UKeepersPauseSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersPauseSubsystem, STATGROUP_Tickables);
}

bool UKeepersPauseSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersPauseSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	if (!Player || !World->HasBegunPlay())
	{
		return;
	}
	// A visible "Меню" button: in the editor Esc stops Play, so the keys alone are not enough.
	// The campaign map has its own "В меню" button and its title in this corner.
	if (!Button && !TActorIterator<AKeepersMenuScene>(World) && !World->GetMapName().Contains(TEXT("CampaignMap")))
	{
		Button = CreateWidget<UKeepersFnButton>(Player, UKeepersFnButton::StaticClass());
		TWeakObjectPtr<UKeepersPauseSubsystem> WeakThis(this);
		Button->Setup(FText::FromString(TEXT("Меню · P")), UKeepersFnButton::EKind::Secondary, 18, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->TogglePause(); } }, 150.f);
		Button->AddToViewport(40);
		Button->SetAlignmentInViewport(FVector2D(0.f, 0.f));
		Button->SetPositionInViewport(FVector2D(20.f, 20.f), false);
	}
	if (Widget)
	{
		return;
	}
	if (Player->WasInputKeyJustPressed(EKeys::Escape) || Player->WasInputKeyJustPressed(EKeys::P) || Player->WasInputKeyJustPressed(EKeys::F10))
	{
		TogglePause();
	}
}

void UKeepersPauseSubsystem::TogglePause()
{
	if (Widget)
	{
		Close();
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	if (!Player)
	{
		return;
	}
	// The main menu has its own Esc.
	if (TActorIterator<AKeepersMenuScene>(World))
	{
		return;
	}
	Widget = CreateWidget<UKeepersPauseWidget>(Player, UKeepersPauseWidget::StaticClass());
	TWeakObjectPtr<UKeepersPauseSubsystem> WeakThis(this);
	Widget->OnClose = [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->Close(); } };
	Widget->AddToViewport(50);
	UGameplayStatics::SetGamePaused(World, true);
	Player->SetShowMouseCursor(true);
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetWidgetToFocus(Widget->TakeWidget());
	Player->SetInputMode(Mode);
}

void UKeepersPauseSubsystem::TestQuestion(const UObject* WorldContextObject, bool bQuit)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UKeepersPauseSubsystem* Pause = World ? World->GetSubsystem<UKeepersPauseSubsystem>() : nullptr;
	if (!Pause)
	{
		return;
	}
	if (!Pause->Widget)
	{
		Pause->TogglePause();
	}
	if (Pause->Widget)
	{
		bQuit ? Pause->Widget->AskQuit() : Pause->Widget->AskMainMenu();
	}
}

void UKeepersPauseSubsystem::TogglePauseMenu(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (UKeepersPauseSubsystem* Pause = World ? World->GetSubsystem<UKeepersPauseSubsystem>() : nullptr)
	{
		Pause->TogglePause();
	}
}

void UKeepersPauseSubsystem::Close()
{
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	UWorld* World = GetWorld();
	UGameplayStatics::SetGamePaused(World, false);
	if (APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		Player->SetInputMode(Mode);
	}
}
