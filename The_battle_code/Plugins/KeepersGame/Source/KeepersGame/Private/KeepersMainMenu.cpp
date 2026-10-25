#include "KeepersMainMenu.h"

#include "Animation/AnimSequence.h"
#include "AudioDevice.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PointLightComponent.h"
#include "Components/SizeBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/Spacer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KeepersCampaign.h"
#include "KeepersBattleScenery.h"
#include "KeepersHeroes.h"
#include "KeepersLoadingScreen.h"
#include "KeepersRockActor.h"
#include "KeepersPlantActor.h"
#include "KeepersSettings.h"
#include "KeepersSettingsUI.h"
#include "KeepersSquad.h"
#include "KeepersStyle.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "KeepersTutorial.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace KeepersMenu
{
	static const TCHAR* Version = TEXT("Хранители земель · сборка 0.5");

	struct FQuickBattle
	{
		const TCHAR* Level;
		const TCHAR* Name;
		const TCHAR* Description;
	};

	static const FQuickBattle QuickBattles[] =
	{
		{ TEXT("L_Slice_Hex"), TEXT("Наёмники кур"), TEXT("Куры наняли людей: разбойники, стрелки и громила вместе со слизнями. Держите строй.") },
		{ TEXT("L_Combat_FairDuel"), TEXT("Честная дуэль"), TEXT("Три Хранителя против трёх таких же. Кто лучше использует местность?") },
		{ TEXT("L_Combat_SoSlimy"), TEXT("Слишком скользко"), TEXT("Слизни оставляют лужи и делятся надвое. Не дайте им заполнить поле.") },
		{ TEXT("L_Combat_ChickenHunt"), TEXT("Охота на кур"), TEXT("Куры зовут подмогу каждые два хода. Действуйте быстро.") },
		{ TEXT("L_Combat_FlyingIsCheating"), TEXT("Летать — нечестно"), TEXT("Летучие мыши перелетают пропасти. Прижмите их скалами.") },
		{ TEXT("L_Combat_TooManyEnemies"), TEXT("Слишком много врагов"), TEXT("Врагов втрое больше. Узкие проходы — ваши друзья.") },
		{ TEXT("L_Combat_TablesHaveTurned"), TEXT("Всё наоборот"), TEXT("Сегодня в меньшинстве не вы. Не расслабляйтесь.") },
		{ TEXT("L_Combat_FourPlayersChess"), TEXT("Шахматы на четверых"), TEXT("Четыре отряда на одной доске. Пусть враги дерутся друг с другом.") },
		{ TEXT("L_Debug"), TEXT("Полигон"), TEXT("Отладочная карта: всё поле и все способности под рукой.") },
	};
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersMenuItem::Setup(const FText& Label, const FText& Description, TFunction<void()> InOnClicked, bool bEnabled)
{
	LabelText = Label;
	DescriptionText = Description;
	OnClicked = MoveTemp(InOnClicked);
	bItemEnabled = bEnabled;
}

TSharedRef<SWidget> UKeepersMenuItem::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(460.f);
		Size->SetHeightOverride(50.f);

		Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style;
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Normal = None;
		Style.Hovered = None;
		Style.Pressed = None;
		Style.Disabled = None;
		Style.NormalPadding = FMargin(0.f);
		Style.PressedPadding = FMargin(0.f);
		Style.HoveredSlateSound = KeepersStyle::HoverSound();
		Style.PressedSlateSound = KeepersStyle::ClickSound();
		Button->SetStyle(Style);
		Button->SetIsEnabled(bItemEnabled);
		Button->OnClicked.AddDynamic(this, &UKeepersMenuItem::HandleClicked);
		Button->OnHovered.AddDynamic(this, &UKeepersMenuItem::HandleHovered);

		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

		// A gold fade behind the hovered line.
		Glow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Glow->SetBrush(KeepersStyle::Fade(FLinearColor(1.f, 0.74f, 0.3f, 0.28f)));
		Glow->SetRenderOpacity(0.f);
		if (UOverlaySlot* GlowSlot = Layers->AddChildToOverlay(Glow))
		{
			GlowSlot->SetHorizontalAlignment(HAlign_Fill);
			GlowSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Bar->SetBrush(KeepersStyle::Panel(KeepersStyle::Gold, 2.f, FLinearColor::Transparent));
		Bar->SetRenderOpacity(0.f);
		USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetWidthOverride(5.f);
		BarSize->AddChild(Bar);
		if (UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarSize))
		{
			BarSlot->SetVerticalAlignment(VAlign_Fill);
			BarSlot->SetPadding(FMargin(0.f, 6.f, 16.f, 6.f));
		}
		Caption = KeepersStyle::MakeText(WidgetTree, LabelText, KeepersStyle::Heading(30), bItemEnabled ? KeepersStyle::Text : FLinearColor(0.4f, 0.42f, 0.45f));
		if (UHorizontalBoxSlot* CaptionSlot = Row->AddChildToHorizontalBox(Caption))
		{
			CaptionSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* RowSlot = Layers->AddChildToOverlay(Row))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Fill);
		}

		if (UButtonSlot* LayersSlot = Cast<UButtonSlot>(Button->AddChild(Layers)))
		{
			LayersSlot->SetHorizontalAlignment(HAlign_Fill);
			LayersSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Size->AddChild(Button);
		WidgetTree->RootWidget = Size;
	}
	return Super::RebuildWidget();
}

void UKeepersMenuItem::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Target = Button && bItemEnabled && Button->IsHovered() ? 1.f : 0.f;
	Highlight = FMath::FInterpTo(Highlight, Target, InDeltaTime, 14.f);
	if (Caption && bItemEnabled)
	{
		Caption->SetRenderTranslation(FVector2D(14.f * Highlight, 0.f));
		Caption->SetColorAndOpacity(FSlateColor(FMath::Lerp(KeepersStyle::Text, KeepersStyle::Gold, Highlight)));
	}
	if (Bar)
	{
		Bar->SetRenderOpacity(Highlight);
	}
	if (Glow)
	{
		Glow->SetRenderOpacity(Highlight);
	}
}

void UKeepersMenuItem::HandleClicked()
{
	if (OnClicked)
	{
		// The action may destroy this widget.
		TFunction<void()> Callback = OnClicked;
		Callback();
	}
}

void UKeepersMenuItem::HandleHovered()
{
	if (OnHoveredDescription)
	{
		OnHoveredDescription(DescriptionText);
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersSettingRow::Setup(const FText& Label, TArray<FText> InOptions, int32 InCurrent, TFunction<void(int32)> InOnChanged)
{
	LabelText = Label;
	Options = MoveTemp(InOptions);
	Current = FMath::Clamp(InCurrent, 0, FMath::Max(Options.Num() - 1, 0));
	OnChanged = MoveTemp(InOnChanged);
}

TSharedRef<SWidget> UKeepersSettingRow::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(RowWidth);
		Size->SetHeightOverride(50.f);

		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetBrush(KeepersStyle::Fade(FLinearColor(0.f, 0.f, 0.f, 0.45f)));
		Back->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
		Back->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Label = KeepersStyle::MakeText(WidgetTree, LabelText, KeepersStyle::Heading(22));
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		UButton* Previous = KeepersStyle::MakeButton(WidgetTree, FText::FromString(TEXT("‹")), KeepersStyle::EButton::Secondary, 20);
		Previous->OnClicked.AddDynamic(this, &UKeepersSettingRow::HandlePrevious);
		Row->AddChildToHorizontalBox(Previous)->SetVerticalAlignment(VAlign_Center);

		USizeBox* ValueSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ValueSize->SetWidthOverride(190.f);
		ValueText = KeepersStyle::MakeText(WidgetTree, Options.IsValidIndex(Current) ? Options[Current] : FText::GetEmpty(), KeepersStyle::Heading(22), KeepersStyle::Gold);
		ValueText->SetJustification(ETextJustify::Center);
		ValueSize->AddChild(ValueText);
		Row->AddChildToHorizontalBox(ValueSize)->SetVerticalAlignment(VAlign_Center);

		UButton* Next = KeepersStyle::MakeButton(WidgetTree, FText::FromString(TEXT("›")), KeepersStyle::EButton::Secondary, 20);
		Next->OnClicked.AddDynamic(this, &UKeepersSettingRow::HandleNext);
		Row->AddChildToHorizontalBox(Next)->SetVerticalAlignment(VAlign_Center);

		Back->SetContent(Row);
		Size->AddChild(Back);
		WidgetTree->RootWidget = Size;
	}
	return Super::RebuildWidget();
}

void UKeepersSettingRow::HandlePrevious()
{
	Step(-1);
}

void UKeepersSettingRow::HandleNext()
{
	Step(1);
}

void UKeepersSettingRow::Step(int32 Delta)
{
	if (Options.Num() == 0)
	{
		return;
	}
	Current = (Current + Delta + Options.Num()) % Options.Num();
	if (ValueText)
	{
		ValueText->SetText(Options[Current]);
	}
	if (OnChanged)
	{
		OnChanged(Current);
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		auto Place = [Canvas](UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment, bool bAutoSize)
		{
			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(Offsets);
			Slot->SetAlignment(Alignment);
			Slot->SetAutoSize(bAutoSize);
			return Slot;
		};

		// Dark fade over the left part of the scene, under the menu.
		UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shade->SetBrush(KeepersStyle::Fade(FLinearColor(0.005f, 0.008f, 0.015f, 0.92f)));
		Place(Shade, FAnchors(0.f, 0.f, 0.62f, 1.f), FMargin(0.f), FVector2D::ZeroVector, false);

		// Title.
		UVerticalBox* TitleBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TitleBox->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("пошаговая тактика с местностью")), KeepersStyle::Heading(20), KeepersStyle::Gold));
		TitleBox->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Хранители земель")), KeepersStyle::Heading(72)));
		UBorder* Underline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Underline->SetBrush(KeepersStyle::Fade(KeepersStyle::Gold));
		USizeBox* UnderlineSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		UnderlineSize->SetWidthOverride(420.f);
		UnderlineSize->SetHeightOverride(3.f);
		UnderlineSize->AddChild(Underline);
		TitleBox->AddChildToVerticalBox(UnderlineSize)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		Place(TitleBox, FAnchors(0.f, 0.f), FMargin(80.f, 70.f, 0.f, 0.f), FVector2D::ZeroVector, true);

		// Page title and the list of items.
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PageTitle = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(20), KeepersStyle::TextDim);
		Column->AddChildToVerticalBox(PageTitle)->SetPadding(FMargin(21.f, 0.f, 0.f, 10.f));
		List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Column->AddChildToVerticalBox(List);
		Place(Column, FAnchors(0.f, 0.f), FMargin(80.f, 270.f, 0.f, 0.f), FVector2D::ZeroVector, true);

		// Description of the hovered item.
		USizeBox* DescriptionSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DescriptionSize->SetWidthOverride(520.f);
		Description = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(15), FLinearColor(0.8f, 0.82f, 0.85f), true);
		DescriptionSize->AddChild(Description);
		Place(DescriptionSize, FAnchors(0.f, 1.f), FMargin(101.f, -80.f, 0.f, 0.f), FVector2D(0.f, 1.f), true);

		// Bottom hint bar.
		UBorder* BottomBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BottomBar->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, FLinearColor::Transparent));
		BottomBar->SetPadding(FMargin(80.f, 0.f, 40.f, 0.f));
		BottomBar->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox* Hints = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddHint = [this, Hints](const TCHAR* Key, const TCHAR* Action)
		{
			UBorder* KeyBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			KeyBox->SetBrush(KeepersStyle::Panel(FLinearColor(1.f, 1.f, 1.f, 0.08f), 4.f, FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f));
			KeyBox->SetPadding(FMargin(8.f, 2.f));
			KeyBox->SetContent(KeepersStyle::MakeText(WidgetTree, FText::FromString(Key), KeepersStyle::Body(12, true)));
			Hints->AddChildToHorizontalBox(KeyBox)->SetVerticalAlignment(VAlign_Center);
			Hints->AddChildToHorizontalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(Action), KeepersStyle::Body(13), KeepersStyle::TextDim))
				->SetPadding(FMargin(8.f, 0.f, 28.f, 0.f));
		};
		AddHint(TEXT("ЛКМ"), TEXT("Выбрать"));
		AddHint(TEXT("Esc"), TEXT("Назад"));
		UTextBlock* VersionText = KeepersStyle::MakeText(WidgetTree, FText::FromString(KeepersMenu::Version), KeepersStyle::Body(12), KeepersStyle::TextDim);
		UHorizontalBoxSlot* VersionSlot = Hints->AddChildToHorizontalBox(VersionText);
		VersionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		VersionSlot->SetHorizontalAlignment(HAlign_Right);
		VersionSlot->SetVerticalAlignment(VAlign_Center);
		BottomBar->SetContent(Hints);
		Place(BottomBar, FAnchors(0.f, 1.f, 1.f, 1.f), FMargin(0.f, -44.f, 0.f, 44.f), FVector2D::ZeroVector, false);

		// "Start over?" confirmation.
		Confirm = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Confirm->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Confirm->SetHorizontalAlignment(HAlign_Center);
		Confirm->SetVerticalAlignment(VAlign_Center);
		Confirm->SetVisibility(ESlateVisibility::Collapsed);
		UBorder* ConfirmPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ConfirmPanel->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 14.f));
		ConfirmPanel->SetPadding(FMargin(36.f, 26.f));
		UVerticalBox* ConfirmColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* ConfirmTitle = KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Начать заново?")), KeepersStyle::Heading(34), KeepersStyle::Gold);
		ConfirmTitle->SetJustification(ETextJustify::Center);
		ConfirmColumn->AddChildToVerticalBox(ConfirmTitle)->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* ConfirmText = KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Текущая кампания и найденные артефакты пропадут.")), KeepersStyle::Body(15));
		ConfirmColumn->AddChildToVerticalBox(ConfirmText)->SetPadding(FMargin(0.f, 8.f, 0.f, 20.f));
		UHorizontalBox* ConfirmButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UButton* Yes = KeepersStyle::MakeButton(WidgetTree, FText::FromString(TEXT("Да, заново")), KeepersStyle::EButton::Danger, 20);
		Yes->OnClicked.AddDynamic(this, &UKeepersMainMenuWidget::HandleConfirmYes);
		ConfirmButtons->AddChildToHorizontalBox(Yes)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		UButton* No = KeepersStyle::MakeButton(WidgetTree, FText::FromString(TEXT("Отмена")), KeepersStyle::EButton::Secondary, 20);
		No->OnClicked.AddDynamic(this, &UKeepersMainMenuWidget::HandleConfirmNo);
		ConfirmButtons->AddChildToHorizontalBox(No);
		ConfirmColumn->AddChildToVerticalBox(ConfirmButtons)->SetHorizontalAlignment(HAlign_Center);
		ConfirmPanel->SetContent(ConfirmColumn);
		Confirm->SetContent(ConfirmPanel);
		Place(Confirm, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f), FVector2D::ZeroVector, false);

		ShowPage(EPage::Main);
	}
	return Super::RebuildWidget();
}

UKeepersMenuItem* UKeepersMainMenuWidget::AddItem(const TCHAR* Label, const TCHAR* DescriptionLine, TFunction<void()> OnClicked, bool bEnabled)
{
	UKeepersMenuItem* Item = CreateWidget<UKeepersMenuItem>(this, UKeepersMenuItem::StaticClass());
	Item->Setup(FText::FromString(Label), FText::FromString(DescriptionLine), MoveTemp(OnClicked), bEnabled);
	TWeakObjectPtr<UKeepersMainMenuWidget> WeakThis(this);
	Item->OnHoveredDescription = [WeakThis](const FText& Text) { if (WeakThis.IsValid()) { WeakThis->SetDescription(Text); } };
	List->AddChildToVerticalBox(Item)->SetPadding(FMargin(0.f, 2.f));
	return Item;
}

void UKeepersMainMenuWidget::SetDescription(const FText& Text)
{
	if (Description)
	{
		Description->SetText(Text);
	}
}

void UKeepersMainMenuWidget::ShowPage(EPage Page)
{
	CurrentPage = Page;
	List->ClearChildren();
	SetDescription(FText::GetEmpty());
	TWeakObjectPtr<UKeepersMainMenuWidget> WeakThis(this);
	auto Go = [WeakThis](EPage Target) { return [WeakThis, Target]() { if (WeakThis.IsValid()) { WeakThis->ShowPage(Target); } }; };

	switch (Page)
	{
	case EPage::Main:
	{
		PageTitle->SetText(FText::GetEmpty());
		PageTitle->SetVisibility(ESlateVisibility::Collapsed);
		const bool bHasSave = UKeepersCampaignSubsystem::HasSavedCampaign();
		if (bHasSave)
		{
			AddItem(TEXT("Продолжить"), TEXT("Вернуться на карту Зелёных долов, туда, где вы остановились."), [WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->Leave([WeakThis]() { if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(WeakThis.Get())) { Campaign->ContinueCampaign(); } });
				}
			});
		}
		AddItem(TEXT("Новая кампания"), TEXT("Пройдите Зелёные долы: бои, руины, привалы и босс в конце пути. Артефакты меняют правила."), [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->AskNewCampaign();
			}
		});
		AddItem(TEXT("Отряд"), TEXT("Хранители, их способности, наряды и цвета. Кто пойдёт в следующий поход?"), [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OpenSquad(false);
			}
		});
		AddItem(TEXT("Обучение"), TEXT("Короткий бой со слизнями и подсказками: камера, ход, способности, намерения врагов."), [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->Leave([WeakThis]() { UKeepersTutorialSubsystem::StartTutorial(WeakThis.Get()); });
			}
		});
		AddItem(TEXT("Быстрый бой"), TEXT("Отдельные сражения без кампании."), Go(EPage::QuickBattle));
		AddItem(TEXT("Настройки"), TEXT("Графика, звук, управление и игра."), Go(EPage::Settings));
		AddItem(TEXT("Выход"), TEXT("Хранители подождут у костра."), [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->Leave([WeakThis]()
				{
					if (WeakThis.IsValid())
					{
						UKismetSystemLibrary::QuitGame(WeakThis.Get(), WeakThis->GetOwningPlayer(), EQuitPreference::Quit, false);
					}
				});
			}
		});
		SetDescription(FText::FromString(bHasSave ? TEXT("Кампания ждёт продолжения.") : TEXT("Слизни наступают, куры что-то замышляют. Пора в путь.")));
		break;
	}
	case EPage::QuickBattle:
		PageTitle->SetText(FText::FromString(TEXT("Быстрый бой")));
		PageTitle->SetVisibility(ESlateVisibility::HitTestInvisible);
		for (const KeepersMenu::FQuickBattle& Battle : KeepersMenu::QuickBattles)
		{
			const FName Level(Battle.Level);
			const bool bMercenaries = FCString::Strcmp(Battle.Name, TEXT("Наёмники кур")) == 0;
			AddItem(Battle.Name, Battle.Description, [WeakThis, Level, bMercenaries]()
			{
				if (WeakThis.IsValid())
				{
					if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(WeakThis.Get()))
					{
						Campaign->bMercenaryQuickBattle = bMercenaries;
					}
					WeakThis->Leave([WeakThis, Level]() { if (WeakThis.IsValid()) { KeepersLoading::OpenLevel(WeakThis.Get(), Level); } });
				}
			});
		}
		AddItem(TEXT("‹ Назад"), TEXT(""), Go(EPage::Main));
		break;
	case EPage::Settings:
		PageTitle->SetText(FText::FromString(TEXT("Настройки")));
		PageTitle->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddSettings();
		AddItem(TEXT("‹ Назад"), TEXT(""), Go(EPage::Main));
		SetDescription(FText::FromString(TEXT("Изменения применяются и сохраняются сразу.")));
		break;
	}

	if (AKeepersMenuScene* MenuScene = Scene.Get())
	{
		MenuScene->SetShot(static_cast<int32>(Page));
	}
}

void UKeepersMainMenuWidget::AddSettings()
{
	AddSettingRows(this, List, 21.f);
}

void UKeepersMainMenuWidget::AddSettingRows(UUserWidget* Owner, UVerticalBox* Target, float LeftPadding)
{
	if (!Owner || !Target)
	{
		return;
	}
	// Settings in tabs: graphics, sound, controls, game.
	UKeepersSettingsPanel* Panel = CreateWidget<UKeepersSettingsPanel>(Owner, UKeepersSettingsPanel::StaticClass());
	Target->AddChildToVerticalBox(Panel)->SetPadding(FMargin(LeftPadding, 3.f, 0.f, 3.f));
}

void UKeepersMainMenuWidget::AskNewCampaign()
{
	if (!UKeepersCampaignSubsystem::HasSavedCampaign())
	{
		HandleConfirmYes();
		return;
	}
	Confirm->SetVisibility(ESlateVisibility::Visible);
}

void UKeepersMainMenuWidget::HandleConfirmYes()
{
	Confirm->SetVisibility(ESlateVisibility::Collapsed);
	// A new run starts with gathering the squad.
	OpenSquad(true);
}

void UKeepersMainMenuWidget::OpenSquad(bool bStartCampaign)
{
	APlayerController* Player = GetOwningPlayer();
	if (Squad || !Player)
	{
		return;
	}
	Squad = CreateWidget<UKeepersSquadWidget>(Player, UKeepersSquadWidget::StaticClass());
	Squad->Scene = Scene;
	Squad->bStartsCampaign = bStartCampaign;
	TWeakObjectPtr<UKeepersMainMenuWidget> WeakThis(this);
	Squad->OnBack = [WeakThis]()
	{
		if (!WeakThis.IsValid() || !WeakThis->Squad)
		{
			return;
		}
		WeakThis->Squad->RemoveFromParent();
		WeakThis->Squad = nullptr;
		WeakThis->SetVisibility(ESlateVisibility::Visible);
		WeakThis->ShowPage(EPage::Main);
		WeakThis->SetFocus();
	};
	Squad->OnStart = [WeakThis, bStartCampaign]()
	{
		if (!WeakThis.IsValid() || !WeakThis->Squad)
		{
			return;
		}
		if (!bStartCampaign)
		{
			WeakThis->Squad->OnBack();
			return;
		}
		WeakThis->Squad->SetVisibility(ESlateVisibility::HitTestInvisible);
		WeakThis->Leave([WeakThis]() { if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(WeakThis.Get())) { Campaign->StartNewCampaign(); } });
	};
	SetVisibility(ESlateVisibility::Collapsed);
	Squad->AddToViewport(6);
	if (AKeepersMenuScene* MenuScene = Scene.Get())
	{
		MenuScene->SetShot(3);
	}
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetWidgetToFocus(Squad->TakeWidget());
	Player->SetInputMode(Mode);
}

void UKeepersMainMenuWidget::HandleConfirmNo()
{
	Confirm->SetVisibility(ESlateVisibility::Collapsed);
}

void UKeepersMainMenuWidget::Leave(TFunction<void()> Action)
{
	if (bLeaving)
	{
		return;
	}
	bLeaving = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	APlayerController* Player = GetOwningPlayer();
	if (Player && Player->PlayerCameraManager)
	{
		Player->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.35f, FLinearColor::Black, false, true);
	}
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Action]() { Action(); }), 0.4f, false);
}

FReply UKeepersMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (Confirm && Confirm->GetVisibility() == ESlateVisibility::Visible)
		{
			HandleConfirmNo();
		}
		else if (CurrentPage != EPage::Main)
		{
			ShowPage(EPage::Main);
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---------------------------------------------------------------------------------------------------------------------

AKeepersMenuScene::AKeepersMenuScene()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetFieldOfView(55.f);
}

void AKeepersMenuScene::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);

	FLatentActionInfo Latent;
	Latent.UUID = 7;
	UGameplayStatics::LoadStreamLevel(this, TEXT("L_Hexagon"), true, false, Latent);

	CameraLocation = CampCenter + ShotLocations[0] + FVector(-250.f, 0.f, 120.f);
	CameraTarget = CampCenter + ShotTargets[0];

	BuildCamp();
	SetEveningLight();
	// The island streams in a moment later; the podium and the seats need its ground.
	FTimerHandle StageTimer;
	GetWorldTimerManager().SetTimer(StageTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		BuildSquadStage();
		RefreshCamp();
		UKeepersBattleScenerySubsystem::SurroundField(*GetWorld());
	}), 0.5f, false);
	KeepersSettings::ApplyVolume(this);

	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!Player)
	{
		return;
	}
	Player->SetViewTarget(this);
	if (Player->PlayerCameraManager)
	{
		Player->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.2f, FLinearColor::Black);
	}
	Menu = CreateWidget<UKeepersMainMenuWidget>(Player, UKeepersMainMenuWidget::StaticClass());
	Menu->Scene = this;
	Menu->AddToViewport(5);
	Player->SetShowMouseCursor(true);
	// Game and UI (never UI-only): the input mode outlives this level.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetWidgetToFocus(Menu->TakeWidget());
	Player->SetInputMode(Mode);
}

UStaticMeshComponent* AKeepersMenuScene::AddMesh(const TCHAR* MeshPath, const FTransform& Transform, UMaterialInterface* Material)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetRelativeTransform(Transform);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	Component->RegisterComponent();
	return Component;
}

USkeletalMeshComponent* AKeepersMenuScene::AddUnit(const TCHAR* Name, const FVector& Offset, float Turn, float Scale)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Art/Units/%s/SK_%s.SK_%s"), Name, Name, Name));
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Art/Units/%s/AN_%s_Idle.AN_%s_Idle"), Name, Name, Name));
	if (!Mesh)
	{
		return nullptr;
	}
	USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(this);
	Component->SetSkeletalMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetRelativeLocation(CampCenter + Offset);
	// Face the fire (the unit meshes look along +Y), then turn a little for variety.
	Component->SetRelativeRotation(FRotator(0.f, (-Offset).Rotation().Yaw + MeshYawOffset + Turn, 0.f));
	Component->SetRelativeScale3D(FVector(Scale));
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->RegisterComponent();
	if (Idle)
	{
		Component->PlayAnimation(Idle, true);
		// Out of step, so the three do not breathe in unison.
		Component->SetPosition(FMath::FRandRange(0.f, Idle->GetPlayLength()));
	}
	return Component;
}

void AKeepersMenuScene::BuildCamp()
{
	const FVector C = CampCenter;
	UMaterialInterface* Wood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/Colors/MI_Brown_03.MI_Brown_03"));
	UMaterialInterface* Stone = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/Colors/MI_Brown_07.MI_Brown_07"));
	static const TCHAR* Cube = TEXT("/Game/Art/Meshes/SM_Cube_Chamfer.SM_Cube_Chamfer");

	// Logs crossed over the fire and a ring of stones. SM_Cube_Chamfer is a 100 cm cube.
	for (int32 I = 0; I < 3; ++I)
	{
		AddMesh(Cube, FTransform(FRotator(0.f, I * 60.f, 8.f), C + FVector(0.f, 0.f, 6.f), FVector(0.5f, 0.09f, 0.09f)), Wood);
	}
	for (int32 I = 0; I < 8; ++I)
	{
		const float Angle = I * 45.f;
		const FVector Offset = FRotator(0.f, Angle, 0.f).Vector() * 36.f;
		AddMesh(Cube, FTransform(FRotator(0.f, Angle + 20.f, 0.f), C + Offset + FVector(0.f, 0.f, 4.f), FVector(0.11f, 0.14f, 0.09f)), Stone);
	}
	// A log to sit on behind the Keepers.
	AddMesh(Cube, FTransform(FRotator(0.f, 90.f, 0.f), C + FVector(-215.f, 0.f, 12.f), FVector(1.3f, 0.22f, 0.22f)), Wood);

	// Low-poly flames: glowing unlit cones that flicker in Tick.
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_TranslucentColor.M_TranslucentColor"));
	struct FFlame { FVector Offset; float Scale; FLinearColor Color; };
	const FFlame FlameSpecs[] =
	{
		{ FVector(0.f, 0.f, 10.f), 0.85f, FLinearColor(6.f, 1.5f, 0.2f) },
		{ FVector(12.f, -14.f, 8.f), 0.6f, FLinearColor(8.f, 2.6f, 0.4f) },
		{ FVector(-14.f, 10.f, 8.f), 0.65f, FLinearColor(7.f, 2.0f, 0.3f) },
		{ FVector(10.f, 14.f, 8.f), 0.5f, FLinearColor(8.f, 2.4f, 0.35f) },
		{ FVector(2.f, 3.f, 10.f), 0.5f, FLinearColor(12.f, 7.f, 1.8f) },
	};
	for (const FFlame& Spec : FlameSpecs)
	{
		UMaterialInstanceDynamic* Material = Glow ? UMaterialInstanceDynamic::Create(Glow, this) : nullptr;
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Spec.Color);
			Material->SetScalarParameterValue(TEXT("Opacity"), 0.85f);
		}
		if (UStaticMeshComponent* Flame = AddMesh(TEXT("/Engine/BasicShapes/Cone.Cone"), FTransform(FRotator::ZeroRotator, C + Spec.Offset + FVector(0.f, 0.f, 50.f * Spec.Scale), FVector(0.3f, 0.3f, 1.f) * Spec.Scale), Material))
		{
			Flame->SetCastShadow(false);
			Flames.Add(Flame);
		}
	}

	FireLight = NewObject<UPointLightComponent>(this);
	FireLight->SetupAttachment(RootComponent);
	FireLight->SetRelativeLocation(C + FVector(0.f, 0.f, 70.f));
	FireLight->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	FireLight->SetAttenuationRadius(900.f);
	FireLight->SetIntensity(6000.f);
	FireLight->SetCastShadows(true);
	FireLight->RegisterComponent();

	if (UParticleSystem* Smoke = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/UltimateMenu/Environment/Particles/P_Smoke.P_Smoke")))
	{
		UParticleSystemComponent* SmokeComponent = NewObject<UParticleSystemComponent>(this);
		SmokeComponent->SetTemplate(Smoke);
		SmokeComponent->SetupAttachment(RootComponent);
		SmokeComponent->SetRelativeLocation(C + FVector(0.f, 0.f, 60.f));
		SmokeComponent->SetRelativeScale3D(FVector(0.35f));
		SmokeComponent->RegisterComponent();
	}
	if (UParticleSystem* Dust = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/UltimateMenu/Environment/Particles/P_Ambient_Dust.P_Ambient_Dust")))
	{
		UParticleSystemComponent* DustComponent = NewObject<UParticleSystemComponent>(this);
		DustComponent->SetTemplate(Dust);
		DustComponent->SetupAttachment(RootComponent);
		DustComponent->SetRelativeLocation(C + FVector(-200.f, -250.f, 120.f));
		DustComponent->RegisterComponent();
	}

	auto AddSound = [this, &C](const TCHAR* Path, float Volume)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path))
		{
			UAudioComponent* Audio = NewObject<UAudioComponent>(this);
			Audio->SetSound(Sound);
			Audio->SetupAttachment(RootComponent);
			Audio->SetRelativeLocation(C);
			Audio->bAllowSpatialization = false;
			Audio->SetVolumeMultiplier(Volume);
			Audio->RegisterComponent();
			Audio->Play();
		}
	};
	AddSound(TEXT("/Game/UltimateMenu/Sounds/Fire_Sparks01_Cue.Fire_Sparks01_Cue"), 0.5f);
	AddSound(TEXT("/Game/UltimateMenu/Sounds/Starter_Background_Cue.Starter_Background_Cue"), 0.35f);

	// The squad around the fire (RefreshCamp) and a chicken that came for the warmth.
	AddUnit(TEXT("Chicken"), FVector(110.f, 150.f, 0.f), 70.f, 0.4f);
	RefreshCamp();
}

UKeepersFigureComponent* AKeepersMenuScene::AddFigure(const FVector& Location, float Yaw, float Scale)
{
	UKeepersFigureComponent* Figure = NewObject<UKeepersFigureComponent>(this);
	Figure->SetupAttachment(RootComponent);
	Figure->SetWorldLocation(Location);
	// Figures look along +Y; Yaw is the direction they face.
	Figure->SetWorldRotation(FRotator(0.f, Yaw + MeshYawOffset, 0.f));
	Figure->SetWorldScale3D(FVector(Scale));
	Figure->RegisterComponent();
	return Figure;
}

float AKeepersMenuScene::GroundZ(const FVector& Location) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("KeepersMenuGround"), false, this);
	// Start low: tree crowns over the camp must not count as ground.
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 120.f), Location - FVector(0.f, 0.f, 1200.f), ECC_Visibility, Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return Location.Z;
}

void AKeepersMenuScene::RefreshCamp()
{
	for (UKeepersFigureComponent* Figure : CampFigures)
	{
		if (Figure)
		{
			Figure->DestroyComponent();
		}
	}
	CampFigures.Reset();
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (!Party)
	{
		return;
	}
	// Seats around the fire; each Keeper faces the flames, turned a little for variety.
	const FVector Seats[] = { FVector(30.f, -120.f, 0.f), FVector(-160.f, 90.f, 0.f), FVector(-150.f, -85.f, 0.f) };
	const float Turns[] = { 10.f, -15.f, 15.f };
	for (int32 I = 0; I < Party->GetParty().Num() && I < UE_ARRAY_COUNT(Seats); ++I)
	{
		const FName Hero = Party->GetParty()[I];
		FVector Location = CampCenter + Seats[I];
		Location.Z = GroundZ(Location);
		UKeepersFigureComponent* Figure = AddFigure(Location, (-Seats[I]).Rotation().Yaw + Turns[I], FigureScale);
		Figure->SetLook(Party->GetLook(Hero));
		Figure->SetColor(Party->GetColor(Hero));
		CampFigures.Add(Figure);
	}
}

void AKeepersMenuScene::BuildSquadStage()
{
	// The podium: a spot in front of the fire, the camera looks at it from the side of the main shot.
	PodiumLocation = CampCenter + PodiumOffset;
	PodiumLocation.Z = GroundZ(PodiumLocation);
	const FVector ToCamera = SquadCameraDirection.GetSafeNormal2D();
	// Screen right as the squad camera sees it.
	const FVector Right = FRotationMatrix(FRotator(0.f, (-ToCamera).Rotation().Yaw, 0.f)).GetScaledAxis(EAxis::Y);
	// The camera looks a little right of the hero, so the target slime is in the free middle of the screen too.
	ShotTargets[3] = PodiumLocation - CampCenter + Right * 24.f + FVector(0.f, 0.f, SquadLookHeight);
	ShotLocations[3] = PodiumLocation - CampCenter + Right * 24.f + ToCamera * SquadCameraDistance + FVector(0.f, 0.f, SquadCameraHeight);
	PreviewBaseYaw = ToCamera.Rotation().Yaw;

	Preview = AddFigure(PodiumLocation, PreviewBaseYaw, FigureScale);
	Preview->SetVisibility(false, true);

	// The fire is behind the hero: a soft key light from the camera side, a little to the left.
	const FVector Left = FRotationMatrix(FRotator(0.f, (-ToCamera).Rotation().Yaw, 0.f)).GetScaledAxis(EAxis::Y) * -1.f;
	PodiumLight = NewObject<UPointLightComponent>(this);
	PodiumLight->SetupAttachment(RootComponent);
	PodiumLight->SetWorldLocation(PodiumLocation + ToCamera * 170.f + Left * 90.f + FVector(0.f, 0.f, 150.f));
	PodiumLight->SetLightColor(FLinearColor(1.f, 0.86f, 0.72f));
	PodiumLight->SetIntensity(5000.f);
	PodiumLight->SetAttenuationRadius(520.f);
	PodiumLight->SetCastShadows(false);
	PodiumLight->SetVisibility(false);
	PodiumLight->RegisterComponent();

	// A slime to show abilities on, to the right of the hero and further from the camera.
	TargetLocation = PodiumLocation + Right * 80.f - ToCamera * 110.f;
	TargetLocation.Z = GroundZ(TargetLocation);
	TargetSlime = AddUnit(TEXT("Slime"), TargetLocation - CampCenter, 0.f, 0.32f);
	if (TargetSlime)
	{
		TargetSlime->SetWorldRotation(FRotator(0.f, (PodiumLocation - TargetLocation).Rotation().Yaw + MeshYawOffset, 0.f));
		TargetSlime->SetVisibility(false);
	}

	// A small studio high above the island: every hero stands there in front of its own camera, the
	// cards show these live pictures.
	const FVector Studio = CampCenter + FVector(0.f, 0.f, 6000.f);
	UPointLightComponent* StudioLight = NewObject<UPointLightComponent>(this);
	StudioLight->SetupAttachment(RootComponent);
	StudioLight->SetWorldLocation(Studio + FVector(-200.f, -400.f, 300.f));
	StudioLight->SetIntensity(60000.f);
	StudioLight->SetAttenuationRadius(5000.f);
	StudioLight->SetLightColor(FLinearColor(1.f, 0.85f, 0.7f));
	StudioLight->SetCastShadows(false);
	StudioLight->RegisterComponent();

	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	int32 Index = 0;
	for (const FKeepersHeroDef& Hero : KeepersHeroes::All())
	{
		const FVector Spot = Studio + FVector(Index++ * 400.f, 0.f, 0.f);
		// Facing -Y, towards the camera.
		UKeepersFigureComponent* Figure = AddFigure(Spot, -90.f, 1.f);
		Figure->SetLook(Party ? Party->GetLook(Hero.Id) : Hero.Looks[0]);
		Figure->SetColor(Party ? Party->GetColor(Hero.Id) : FName(TEXT("Blue")));
		StudioFigures.Add(Hero.Id, Figure);

		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
		Target->InitCustomFormat(240, 314, PF_B8G8R8A8, true);
		Target->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		Portraits.Add(Hero.Id, Target);

		USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(this);
		Capture->SetupAttachment(RootComponent);
		Capture->SetWorldLocation(Spot + FVector(0.f, -235.f, 128.f));
		Capture->SetWorldRotation(FRotator(-4.f, 90.f, 0.f));
		Capture->FOVAngle = 36.f;
		Capture->TextureTarget = Target;
		Capture->CaptureSource = SCS_FinalColorLDR;
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->ShowOnlyComponents.Append(Figure->GetParts());
		Capture->ShowFlags.SetAtmosphere(false);
		Capture->ShowFlags.SetFog(false);
		Capture->ShowFlags.SetEyeAdaptation(false);
		Capture->bCaptureEveryFrame = true;
		Capture->RegisterComponent();
		Capture->ComponentTags.Add(Hero.Id);
	}
}

UTextureRenderTarget2D* AKeepersMenuScene::GetPortrait(FName Hero) const
{
	const TObjectPtr<UTextureRenderTarget2D>* Found = Portraits.Find(Hero);
	return Found ? Found->Get() : nullptr;
}

void AKeepersMenuScene::ShowPreview(FName Hero)
{
	PreviewHero = Hero;
	PreviewYaw = 0.f;
	FaceTargetTime = 0.f;
	RefreshHero(Hero);
	if (Preview)
	{
		Preview->SetVisibility(true, true);
	}
	if (TargetSlime)
	{
		TargetSlime->SetVisibility(true);
	}
}

void AKeepersMenuScene::RefreshHero(FName Hero)
{
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (!Party)
	{
		return;
	}
	const FKeepersLook Look = Party->GetLook(Hero);
	const FName Color = Party->GetColor(Hero);
	if (Preview && Hero == PreviewHero)
	{
		Preview->SetLook(Look);
		Preview->SetColor(Color);
	}
	if (const TObjectPtr<UKeepersFigureComponent>* Studio = StudioFigures.Find(Hero))
	{
		(*Studio)->SetLook(Look);
		(*Studio)->SetColor(Color);
		// New attachments: tell the hero's portrait camera what to film.
		TArray<USceneCaptureComponent2D*> Captures;
		GetComponents(Captures);
		for (USceneCaptureComponent2D* Capture : Captures)
		{
			if (Capture->ComponentHasTag(Hero))
			{
				Capture->ShowOnlyComponents.Reset();
				Capture->ShowOnlyComponents.Append((*Studio)->GetParts());
			}
		}
	}
	if (Party->IsInParty(Hero))
	{
		RefreshCamp();
	}
}

void AKeepersMenuScene::TryOnLook(FName Hero, int32 Look)
{
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (Preview && Def && Def->Looks.IsValidIndex(Look) && Hero == PreviewHero)
	{
		Preview->SetLook(Def->Looks[Look]);
		Preview->SetColor(Party ? Party->GetColor(Hero) : FName(TEXT("Blue")));
	}
}

void AKeepersMenuScene::PlayPreview(EKeepersPose Pose)
{
	if (Preview)
	{
		Preview->Play(Pose);
	}
}

void AKeepersMenuScene::RotatePreview(float DeltaYaw)
{
	PreviewYaw += DeltaYaw;
}

void AKeepersMenuScene::PlayAbility(FName Hero, FName Spell)
{
	const FKeepersSpellInfo* Info = KeepersHeroes::FindSpell(Spell);
	if (!Preview || !Info)
	{
		return;
	}
	EKeepersPose Pose = Info->Pose;
	if (Pose == EKeepersPose::Attack && Preview->GetLook().AnimSet == EKeepersAnimSet::Staff)
	{
		Pose = EKeepersPose::Cast;
	}
	Preview->Play(Pose);
	FaceTargetTime = 2.2f;

	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_TranslucentColor.M_TranslucentColor"));
	auto MakeGlowMesh = [this, Glow](const TCHAR* MeshPath, const FVector& Scale, const FLinearColor& Color) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Mesh = AddMesh(MeshPath, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, Scale), nullptr);
		if (Mesh && Glow)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Glow, this);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Material->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
			Mesh->SetMaterial(0, Material);
			Mesh->SetCastShadow(false);
			Mesh->SetVisibility(false);
		}
		return Mesh;
	};
	auto AddLight = [this](const FVector& Location, const FLinearColor& Color, float Intensity, float Delay, float Duration)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(RootComponent);
		Light->SetWorldLocation(Location);
		Light->SetLightColor(Color);
		Light->SetIntensity(0.f);
		Light->SetAttenuationRadius(400.f);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		FEffect Effect;
		Effect.Light = Light;
		Effect.LightIntensity = Intensity;
		Effect.Delay = Delay;
		Effect.Duration = Duration;
		Effects.Add(Effect);
	};
	auto AddProjectile = [this, &MakeGlowMesh](const TCHAR* MeshPath, const FVector& Scale, const FLinearColor& Color, float Delay, float Duration, bool bArc)
	{
		FEffect Effect;
		Effect.Component = MakeGlowMesh(MeshPath, Scale, Color);
		Effect.From = PodiumLocation + FVector(0.f, 0.f, 70.f * FigureScale);
		Effect.To = TargetLocation + FVector(0.f, 0.f, 25.f);
		Effect.Delay = Delay;
		Effect.Duration = Duration;
		Effect.bArc = bArc;
		Effect.bHitsTarget = true;
		Effects.Add(Effect);
	};
	auto AddHit = [this](float Delay)
	{
		FEffect Effect;
		Effect.Delay = Delay;
		Effect.Duration = 0.01f;
		Effect.bHitsTarget = true;
		Effects.Add(Effect);
	};
	auto AddActorEffect = [this](AActor* Actor, float Life)
	{
		FEffect Effect;
		Effect.SpawnedActor = Actor;
		Effect.Duration = Life;
		Effects.Add(Effect);
		SpawnedEffects.Add(Actor);
	};
	const FVector Middle = FMath::Lerp(PodiumLocation, TargetLocation, 0.55f);
	const FString Id = Spell.ToString();

	if (Id == TEXT("LongRangeArrow") || Id == TEXT("ShortRangeArrow") || Id == TEXT("SwordThrow"))
	{
		AddProjectile(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0.03f, 0.03f, 0.35f), FLinearColor(3.f, 2.4f, 1.2f), 0.45f, Id == TEXT("LongRangeArrow") ? 0.55f : 0.3f, Id == TEXT("LongRangeArrow"));
	}
	else if (Id == TEXT("SlimeBall") || Id == TEXT("Fireball") || Id == TEXT("ChainLightning") || Id == TEXT("WindGust"))
	{
		const FLinearColor Color = Id == TEXT("Fireball") ? FLinearColor(8.f, 2.f, 0.3f) : Id == TEXT("SlimeBall") ? FLinearColor(1.f, 4.f, 0.5f) : FLinearColor(1.5f, 3.f, 8.f);
		AddProjectile(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(0.14f), Color, 0.5f, 0.4f, Id == TEXT("Fireball"));
		AddLight(TargetLocation + FVector(0.f, 0.f, 40.f), Color.GetClamped(0.f, 1.f), 8000.f, 0.9f, 0.5f);
	}
	else if (Id == TEXT("Heal"))
	{
		AddLight(PodiumLocation + FVector(0.f, 0.f, 60.f), FLinearColor(0.4f, 1.f, 0.4f), 9000.f, 0.4f, 1.2f);
	}
	else if (Id == TEXT("RaiseRock"))
	{
		if (AKeepersRockActor* Rock = GetWorld()->SpawnActor<AKeepersRockActor>(AKeepersRockActor::StaticClass(), FTransform(Middle)))
		{
			Rock->SetActorScale3D(FVector(0.22f));
			Rock->Rise(FVector(Middle.X, Middle.Y, GroundZ(Middle)), GroundZ(Middle) + 45.f);
			AddActorEffect(Rock, 2.6f);
		}
	}
	else if (Id == TEXT("Sprout") || Id == TEXT("Roots"))
	{
		const bool bTree = Id == TEXT("Sprout");
		const FVector Spot = bTree ? Middle : TargetLocation;
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, bTree ? TEXT("/Game/Art/Meshes/SM_Tree_02.SM_Tree_02") : TEXT("/Game/Art/Meshes/SM_Bush_01.SM_Bush_01"));
		if (AKeepersPlantActor* Plant = Mesh ? GetWorld()->SpawnActor<AKeepersPlantActor>(AKeepersPlantActor::StaticClass(), FTransform(FVector(Spot.X, Spot.Y, GroundZ(Spot)))) : nullptr)
		{
			Plant->Grow(Mesh, bTree ? 0.3f : 0.32f, false);
			AddActorEffect(Plant, 2.6f);
		}
		if (!bTree)
		{
			AddHit(0.7f);
		}
	}
	else if (Id == TEXT("Gust"))
	{
		AddLight(Middle + FVector(0.f, 0.f, 50.f), FLinearColor(0.6f, 0.85f, 1.f), 5000.f, 0.4f, 0.5f);
		AddHit(0.55f);
		TargetPush = -1.f; // starts with the hit
	}
	else if (Info->Pose == EKeepersPose::Cast)
	{
		AddLight(TargetLocation + FVector(0.f, 0.f, 40.f), FLinearColor(0.8f, 0.5f, 1.f), 7000.f, 0.6f, 0.6f);
		AddHit(0.7f);
	}
	else
	{
		// Melee: the blow lands in the middle of the swing.
		AddHit(0.4f);
	}
}

void AKeepersMenuScene::TickEffects(float DeltaTime)
{
	for (int32 I = Effects.Num() - 1; I >= 0; --I)
	{
		FEffect& Effect = Effects[I];
		Effect.Age += DeltaTime;
		const float T = (Effect.Age - Effect.Delay) / FMath::Max(Effect.Duration, 0.01f);
		if (T < 0.f)
		{
			continue;
		}
		if (UPrimitiveComponent* Component = Effect.Component.Get())
		{
			Component->SetVisibility(true);
			FVector Location = FMath::Lerp(Effect.From, Effect.To, FMath::Min(T, 1.f));
			if (Effect.bArc)
			{
				Location.Z += FMath::Sin(FMath::Min(T, 1.f) * PI) * 90.f;
			}
			const FVector Direction = (Effect.To - Effect.From).GetSafeNormal();
			Component->SetWorldLocationAndRotation(Location, FRotationMatrix::MakeFromZ(Direction).Rotator());
		}
		if (UPointLightComponent* Light = Effect.Light.Get())
		{
			Light->SetIntensity(Effect.LightIntensity * FMath::Sin(FMath::Clamp(T, 0.f, 1.f) * PI));
		}
		if (T >= 1.f)
		{
			if (Effect.bHitsTarget)
			{
				TargetShake = 0.45f;
				if (TargetPush < 0.f)
				{
					TargetPush = 1.f;
				}
			}
			if (UPrimitiveComponent* Component = Effect.Component.Get())
			{
				Component->DestroyComponent();
			}
			if (UPointLightComponent* Light = Effect.Light.Get())
			{
				Light->DestroyComponent();
			}
			if (Effect.SpawnedActor)
			{
				SpawnedEffects.Remove(Effect.SpawnedActor);
				Effect.SpawnedActor->Destroy();
			}
			Effects.RemoveAtSwap(I);
		}
	}

	// The preview hero turns to the slime while showing an ability, then back to the camera.
	FaceTargetTime = FMath::Max(FaceTargetTime - DeltaTime, 0.f);
	FaceTargetBlend = FMath::FInterpTo(FaceTargetBlend, FaceTargetTime > 0.f ? 1.f : 0.f, DeltaTime, 7.f);
	if (Preview)
	{
		const float TargetYaw = (TargetLocation - PodiumLocation).Rotation().Yaw;
		const float FrontYaw = PreviewBaseYaw + PreviewYaw;
		const float Yaw = FrontYaw + FMath::FindDeltaAngleDegrees(FrontYaw, TargetYaw) * FaceTargetBlend;
		Preview->SetWorldRotation(FRotator(0.f, Yaw + MeshYawOffset, 0.f));
	}

	// The slime squashes when hit and slides back from a gust.
	TargetShake = FMath::Max(TargetShake - DeltaTime, 0.f);
	TargetPush = TargetPush > 0.f ? FMath::Max(TargetPush - DeltaTime * 0.6f, 0.f) : TargetPush;
	if (TargetSlime)
	{
		const float Wobble = FMath::Sin(TargetShake * 40.f) * TargetShake;
		const FVector Away = (TargetLocation - PodiumLocation).GetSafeNormal2D();
		const float Push = TargetPush > 0.f ? FMath::Sin(FMath::Min(TargetPush, 1.f) * PI) * 60.f : 0.f;
		TargetSlime->SetWorldLocation(TargetLocation + Away * Push + FVector(0.f, 0.f, 0.f));
		TargetSlime->SetWorldScale3D(FVector(0.32f * (1.f + Wobble * 0.5f), 0.32f * (1.f + Wobble * 0.5f), 0.32f * (1.f - Wobble * 0.6f)));
	}
}

void AKeepersMenuScene::OpenSquadScreen(bool bStartCampaign)
{
	if (Menu)
	{
		Menu->OpenSquad(bStartCampaign);
	}
}

void AKeepersMenuScene::TestLook(int32 Look, bool bUnlock)
{
	for (TObjectIterator<UKeepersSquadWidget> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->IsInViewport())
		{
			It->ChooseLook(Look);
			if (bUnlock)
			{
				It->UnlockPendingLook();
			}
		}
	}
}

void AKeepersMenuScene::TestSquad(FName Hero, int32 Ability)
{
	for (TObjectIterator<UKeepersSquadWidget> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->IsInViewport())
		{
			if (!Hero.IsNone())
			{
				It->SelectHero(Hero);
			}
			if (const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero.IsNone() ? PreviewHero : Hero))
			{
				if (Def->Spells.IsValidIndex(Ability))
				{
					PlayAbility(Def->Id, Def->Spells[Ability]);
				}
			}
		}
	}
}

void AKeepersMenuScene::SetEveningLight()
{
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			Sun->SetIntensity(Sun->Intensity * 0.35f);
			Sun->SetLightColor(FLinearColor(1.f, 0.62f, 0.42f));
		}
	}
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		if (USkyLightComponent* Sky = It->GetLightComponent())
		{
			Sky->SetIntensity(Sky->Intensity * 0.45f);
			Sky->SetLightColor(FLinearColor(0.55f, 0.6f, 0.85f));
		}
	}
}

void AKeepersMenuScene::ShowMenuPage(int32 Page)
{
	if (Menu)
	{
		Menu->ShowPage(static_cast<UKeepersMainMenuWidget::EPage>(FMath::Clamp(Page, 0, 2)));
	}
}

void AKeepersMenuScene::SetShot(int32 Shot)
{
	CurrentShot = ShotLocations.IsValidIndex(Shot) && ShotTargets.IsValidIndex(Shot) ? Shot : 0;
	const bool bSquad = CurrentShot == 3;
	if (PodiumLight)
	{
		PodiumLight->SetVisibility(bSquad);
	}
	if (!bSquad)
	{
		if (Preview)
		{
			Preview->SetVisibility(false, true);
		}
		if (TargetSlime)
		{
			TargetSlime->SetVisibility(false);
		}
	}
}

void AKeepersMenuScene::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Time += DeltaTime;
	TickEffects(DeltaTime);
	if (!ShotLocations.IsValidIndex(CurrentShot) || !ShotTargets.IsValidIndex(CurrentShot))
	{
		return;
	}

	// Fly towards the page's shot, with a slow breathing sway.
	CameraLocation = FMath::VInterpTo(CameraLocation, CampCenter + ShotLocations[CurrentShot], DeltaTime, 1.6f);
	CameraTarget = FMath::VInterpTo(CameraTarget, CampCenter + ShotTargets[CurrentShot], DeltaTime, 1.9f);
	const FVector Sway(0.f, FMath::Sin(Time * 0.21f) * 30.f, FMath::Sin(Time * 0.33f) * 12.f);
	const FVector Location = CameraLocation + Sway;
	Camera->SetWorldLocationAndRotation(Location, (CameraTarget - Location).Rotation());

	// Flicker.
	const float Noise = FMath::PerlinNoise1D(Time * 3.1f) * 0.6f + FMath::PerlinNoise1D(Time * 7.7f + 4.f) * 0.4f;
	if (FireLight)
	{
		FireLight->SetIntensity(6000.f * (1.f + 0.35f * Noise));
	}
	for (int32 I = 0; I < Flames.Num(); ++I)
	{
		if (UStaticMeshComponent* Flame = Flames[I])
		{
			const float Phase = FMath::PerlinNoise1D(Time * (4.f + I) + I * 13.f);
			FVector Scale = Flame->GetRelativeScale3D();
			const float Base = Scale.X / 0.3f;
			Scale.Z = Base * (1.f + 0.3f * Phase);
			Flame->SetRelativeScale3D(Scale);
			Flame->SetRelativeRotation(FRotator(Phase * 6.f, Time * 40.f * (I % 2 ? 1.f : -1.f), 0.f));
		}
	}
}
