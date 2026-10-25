#include "KeepersPauseMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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

		ShowMain();
	}
	return Super::RebuildWidget();
}

void UKeepersPauseWidget::ShowMain()
{
	bSettings = false;
	bQuitAsked = false;
	bMenuAsked = false;
	List->ClearChildren();
	Title->SetText(KeepersPause::T(TEXT("Пауза")));
	const bool bCampaign = UKeepersCampaignSubsystem::Get(this) && UKeepersCampaignSubsystem::Get(this)->IsActive();
	Note->SetText(KeepersPause::T(bCampaign ? TEXT("Кампания сохраняется после каждого пройденного узла.") : TEXT("Esc — вернуться в игру.")));

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
	if (!bMenuAsked)
	{
		// Ask once: the battle in progress is lost.
		bMenuAsked = true;
		Note->SetText(KeepersPause::T(TEXT("Текущий бой не сохранится. Нажмите «В главное меню» ещё раз.")));
		Note->SetColorAndOpacity(FSlateColor(KeepersStyle::Gold));
		return;
	}
	UGameplayStatics::SetGamePaused(this, false);
	if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this); Campaign && Campaign->IsActive())
	{
		Campaign->ReturnToMainMenu(this);
	}
	else
	{
		KeepersLoading::OpenLevel(this, UKeepersCampaignSubsystem::MainMenuLevel);
	}
}

void UKeepersPauseWidget::Quit()
{
	if (!bQuitAsked)
	{
		bQuitAsked = true;
		Note->SetText(KeepersPause::T(TEXT("Точно выйти? Нажмите «Выйти из игры» ещё раз.")));
		Note->SetColorAndOpacity(FSlateColor(KeepersStyle::Gold));
		return;
	}
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

FReply UKeepersPauseWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (bSettings)
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
	if (!Player || Widget)
	{
		return;
	}
	if (Player->WasInputKeyJustPressed(EKeys::Escape))
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
