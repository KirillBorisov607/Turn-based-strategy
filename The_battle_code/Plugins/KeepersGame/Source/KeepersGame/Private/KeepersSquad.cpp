#include "KeepersSquad.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "KeepersHeroes.h"
#include "KeepersMainMenu.h"
#include "KeepersStyle.h"

namespace KeepersSquad
{
	static FText T(const TCHAR* Text)
	{
		return FText::FromString(Text);
	}

	/** A row of round pips ("●●●○") with a caption, for movement and action points. */
	static UWidget* MakePipRow(UWidgetTree* Tree, const TCHAR* Label, int32 Value, int32 Max, const FLinearColor& Color)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelSize->SetWidthOverride(215.f);
		LabelSize->AddChild(KeepersStyle::MakeText(Tree, T(Label), KeepersStyle::Body(15), KeepersStyle::TextDim));
		Row->AddChildToHorizontalBox(LabelSize)->SetVerticalAlignment(VAlign_Center);
		for (int32 I = 0; I < Max; ++I)
		{
			UBorder* Pip = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
			const bool bOn = I < Value;
			Pip->SetBrush(KeepersStyle::Panel(bOn ? Color : FLinearColor(1.f, 1.f, 1.f, 0.06f), 8.f, bOn ? FLinearColor(1.f, 1.f, 1.f, 0.5f) : FLinearColor(1.f, 1.f, 1.f, 0.2f), 1.f));
			USizeBox* PipSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			PipSize->SetWidthOverride(16.f);
			PipSize->SetHeightOverride(16.f);
			PipSize->AddChild(Pip);
			Row->AddChildToHorizontalBox(PipSize)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		Row->AddChildToHorizontalBox(KeepersStyle::MakeText(Tree, FText::AsNumber(Value), KeepersStyle::Heading(18), KeepersStyle::Text))
			->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		return Row;
	}

	/** Health as a bar with the number on it; MaxScale is the largest HP among heroes. */
	static UWidget* MakeHpRow(UWidgetTree* Tree, int32 Hp, int32 MaxScale)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelSize->SetWidthOverride(215.f);
		LabelSize->AddChild(KeepersStyle::MakeText(Tree, T(TEXT("Здоровье")), KeepersStyle::Body(15), KeepersStyle::TextDim));
		Row->AddChildToHorizontalBox(LabelSize)->SetVerticalAlignment(VAlign_Center);

		UOverlay* Bar = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* Back = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetBrush(KeepersStyle::Panel(FLinearColor(1.f, 1.f, 1.f, 0.06f), 6.f, FLinearColor(1.f, 1.f, 1.f, 0.2f), 1.f));
		UOverlaySlot* BackSlot = Bar->AddChildToOverlay(Back);
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
		BackSlot->SetVerticalAlignment(VAlign_Fill);
		UBorder* Fill = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Fill->SetBrush(KeepersStyle::Panel(KeepersStyle::Good, 6.f, FLinearColor::Transparent));
		USizeBox* FillSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		constexpr float BarWidth = 230.f;
		FillSize->SetWidthOverride(BarWidth * FMath::Clamp(static_cast<float>(Hp) / FMath::Max(MaxScale, 1), 0.05f, 1.f));
		FillSize->AddChild(Fill);
		UOverlaySlot* FillSlot = Bar->AddChildToOverlay(FillSize);
		FillSlot->SetHorizontalAlignment(HAlign_Left);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
		UTextBlock* Number = KeepersStyle::MakeText(Tree, FText::AsNumber(Hp), KeepersStyle::Heading(16));
		UOverlaySlot* NumberSlot = Bar->AddChildToOverlay(Number);
		NumberSlot->SetHorizontalAlignment(HAlign_Center);
		NumberSlot->SetVerticalAlignment(VAlign_Center);
		USizeBox* BarSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetWidthOverride(BarWidth);
		BarSize->SetHeightOverride(22.f);
		BarSize->AddChild(Bar);
		Row->AddChildToHorizontalBox(BarSize)->SetVerticalAlignment(VAlign_Center);
		return Row;
	}

	static UBorder* MakeSectionTitle(UWidgetTree* Tree, const TCHAR* Text)
	{
		UBorder* Title = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Title->SetBrush(KeepersStyle::Fade(FLinearColor(1.f, 0.74f, 0.3f, 0.16f)));
		Title->SetPadding(FMargin(10.f, 3.f));
		Title->SetContent(KeepersStyle::MakeText(Tree, T(Text), KeepersStyle::Heading(18), KeepersStyle::Gold));
		return Title;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersFnButton::Setup(const FText& Label, EKind InKind, int32 InFontSize, TFunction<void()> InOnClicked, float InWidth)
{
	LabelText = Label;
	Kind = InKind;
	FontSize = InFontSize;
	OnClicked = MoveTemp(InOnClicked);
	Width = InWidth;
}

TSharedRef<SWidget> UKeepersFnButton::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		const FLinearColor TextColor = Kind == EKind::Primary ? FLinearColor(1.f, 0.97f, 0.9f) : KeepersStyle::Text;
		Caption = KeepersStyle::MakeText(WidgetTree, LabelText, Kind == EKind::Line ? KeepersStyle::Body(FontSize, true) : KeepersStyle::Heading(FontSize), TextColor);
		Caption->SetJustification(Kind == EKind::Line ? ETextJustify::Left : ETextJustify::Center);
		if (UButtonSlot* CaptionSlot = Cast<UButtonSlot>(Button->AddChild(Caption)))
		{
			CaptionSlot->SetHorizontalAlignment(Kind == EKind::Line ? HAlign_Left : HAlign_Center);
		}
		Button->OnClicked.AddDynamic(this, &UKeepersFnButton::HandleClicked);
		Button->OnHovered.AddDynamic(this, &UKeepersFnButton::HandleHovered);
		SetActive(bActive);
		Button->SetIsEnabled(bButtonEnabled);
		if (Width > 0.f)
		{
			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Size->SetWidthOverride(Width);
			Size->AddChild(Button);
			WidgetTree->RootWidget = Size;
		}
		else
		{
			WidgetTree->RootWidget = Button;
		}
	}
	return Super::RebuildWidget();
}

void UKeepersFnButton::SetLabel(const FText& Label)
{
	LabelText = Label;
	if (Caption)
	{
		Caption->SetText(Label);
	}
}

void UKeepersFnButton::SetButtonEnabled(bool bEnabled)
{
	bButtonEnabled = bEnabled;
	if (Button)
	{
		Button->SetIsEnabled(bEnabled);
	}
}

void UKeepersFnButton::SetActive(bool bInActive)
{
	bActive = bInActive;
	if (!Button)
	{
		return;
	}
	switch (Kind)
	{
	case EKind::Primary: Button->SetStyle(KeepersStyle::ButtonStyle(KeepersStyle::EButton::Primary)); break;
	case EKind::Danger: Button->SetStyle(KeepersStyle::ButtonStyle(KeepersStyle::EButton::Danger)); break;
	case EKind::Secondary: Button->SetStyle(KeepersStyle::ButtonStyle(bActive ? KeepersStyle::EButton::Primary : KeepersStyle::EButton::Secondary)); break;
	case EKind::Line:
	{
		FButtonStyle Style;
		Style.Normal = bActive ? KeepersStyle::Fade(FLinearColor(1.f, 0.74f, 0.3f, 0.3f)) : KeepersStyle::Fade(FLinearColor(1.f, 1.f, 1.f, 0.05f));
		Style.Hovered = KeepersStyle::Fade(FLinearColor(1.f, 0.74f, 0.3f, 0.22f));
		Style.Pressed = KeepersStyle::Fade(FLinearColor(1.f, 0.74f, 0.3f, 0.4f));
		Style.Disabled = Style.Normal;
		Style.NormalPadding = FMargin(12.f, 7.f);
		Style.PressedPadding = FMargin(12.f, 8.f, 12.f, 6.f);
		Style.HoveredSlateSound = KeepersStyle::HoverSound();
		Style.PressedSlateSound = KeepersStyle::ClickSound();
		Button->SetStyle(Style);
		if (Caption)
		{
			Caption->SetColorAndOpacity(FSlateColor(bActive ? KeepersStyle::Gold : KeepersStyle::Text));
		}
		break;
	}
	}
}

void UKeepersFnButton::HandleClicked()
{
	if (OnClicked)
	{
		TFunction<void()> Callback = OnClicked;
		Callback();
	}
}

void UKeepersFnButton::HandleHovered()
{
	if (OnHovered)
	{
		OnHovered();
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersHeroCard::Setup(FName InHero, UTextureRenderTarget2D* Portrait, FVector2D InSize, TFunction<void()> InOnClicked)
{
	Hero = InHero;
	PortraitTarget = Portrait;
	Size = InSize;
	OnClicked = MoveTemp(InOnClicked);
}

TSharedRef<SWidget> UKeepersHeroCard::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);

		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SizeBox->SetWidthOverride(Size.X);
		SizeBox->SetHeightOverride(Size.Y);

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
		Button->OnClicked.AddDynamic(this, &UKeepersHeroCard::HandleClicked);

		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		auto Fill = [](UOverlaySlot* OverlaySlot)
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		};

		// Card background: a warm vertical glow behind the portrait.
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetBrush(KeepersStyle::Panel(FLinearColor(0.045f, 0.05f, 0.07f, 0.95f), 12.f, FLinearColor::Transparent));
		Fill(Layers->AddChildToOverlay(Back));

		if (Def)
		{
			PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			if (PortraitTarget)
			{
				PortraitImage->SetBrushResourceObject(PortraitTarget);
			}
			UOverlaySlot* PortraitSlot = Layers->AddChildToOverlay(PortraitImage);
			Fill(PortraitSlot);
			PortraitSlot->SetPadding(FMargin(4.f, 4.f, 4.f, 0.f));

			// Name strip over the lower part of the portrait.
			UBorder* Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Strip->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, FLinearColor::Transparent));
			Strip->SetPadding(FMargin(8.f, 4.f, 8.f, 6.f));
			UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UTextBlock* Name = KeepersStyle::MakeText(WidgetTree, Def->Name, KeepersStyle::Heading(Size.X > 150.f ? 22 : 18));
			Name->SetJustification(ETextJustify::Center);
			Names->AddChildToVerticalBox(Name)->SetHorizontalAlignment(HAlign_Center);
			UTextBlock* Class = KeepersStyle::MakeText(WidgetTree, Def->Class, KeepersStyle::Body(11), KeepersStyle::TextDim);
			Class->SetJustification(ETextJustify::Center);
			Names->AddChildToVerticalBox(Class)->SetHorizontalAlignment(HAlign_Center);
			Strip->SetContent(Names);
			UOverlaySlot* StripSlot = Layers->AddChildToOverlay(Strip);
			StripSlot->SetHorizontalAlignment(HAlign_Fill);
			StripSlot->SetVerticalAlignment(VAlign_Bottom);
			StripSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));

			// Locked heroes: dark veil and how to open them.
			LockShade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			LockShade->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.62f), 12.f, FLinearColor::Transparent));
			LockShade->SetHorizontalAlignment(HAlign_Center);
			LockShade->SetVerticalAlignment(VAlign_Center);
			LockShade->SetPadding(FMargin(10.f, 10.f, 10.f, 60.f));
			UVerticalBox* LockColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UTextBlock* LockTitle = KeepersStyle::MakeText(WidgetTree, KeepersSquad::T(TEXT("Закрыт")), KeepersStyle::Heading(18), KeepersStyle::Gold);
			LockTitle->SetJustification(ETextJustify::Center);
			LockColumn->AddChildToVerticalBox(LockTitle)->SetHorizontalAlignment(HAlign_Center);
			UTextBlock* LockHow = KeepersStyle::MakeText(WidgetTree, Def->Unlock, KeepersStyle::Body(11), KeepersStyle::Text, true);
			LockHow->SetJustification(ETextJustify::Center);
			LockColumn->AddChildToVerticalBox(LockHow)->SetHorizontalAlignment(HAlign_Center);
			LockShade->SetContent(LockColumn);
			LockShade->SetVisibility(ESlateVisibility::Collapsed);
			Fill(Layers->AddChildToOverlay(LockShade));

			// "In the squad" badge in the corner.
			Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Badge->SetBrush(KeepersStyle::Panel(FLinearColor(0.62f, 0.40f, 0.10f, 0.95f), 6.f, KeepersStyle::Gold, 1.f));
			Badge->SetPadding(FMargin(6.f, 1.f));
			Badge->SetContent(KeepersStyle::MakeText(WidgetTree, KeepersSquad::T(TEXT("в отряде")), KeepersStyle::Body(10, true)));
			Badge->SetVisibility(ESlateVisibility::Collapsed);
			UOverlaySlot* BadgeSlot = Layers->AddChildToOverlay(Badge);
			BadgeSlot->SetHorizontalAlignment(HAlign_Right);
			BadgeSlot->SetVerticalAlignment(VAlign_Top);
			BadgeSlot->SetPadding(FMargin(0.f, 8.f, 8.f, 0.f));
		}
		else
		{
			// Empty slot.
			UTextBlock* Plus = KeepersStyle::MakeText(WidgetTree, KeepersSquad::T(TEXT("+")), KeepersStyle::Heading(48), FLinearColor(1.f, 1.f, 1.f, 0.25f));
			UOverlaySlot* PlusSlot = Layers->AddChildToOverlay(Plus);
			PlusSlot->SetHorizontalAlignment(HAlign_Center);
			PlusSlot->SetVerticalAlignment(VAlign_Center);
			UTextBlock* Empty = KeepersStyle::MakeText(WidgetTree, KeepersSquad::T(TEXT("свободно")), KeepersStyle::Body(11), KeepersStyle::TextDim);
			UOverlaySlot* EmptySlot = Layers->AddChildToOverlay(Empty);
			EmptySlot->SetHorizontalAlignment(HAlign_Center);
			EmptySlot->SetVerticalAlignment(VAlign_Bottom);
			EmptySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}

		// Frame on top of everything: gold when chosen.
		Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fill(Layers->AddChildToOverlay(Frame));

		if (UButtonSlot* LayersSlot = Cast<UButtonSlot>(Button->AddChild(Layers)))
		{
			LayersSlot->SetHorizontalAlignment(HAlign_Fill);
			LayersSlot->SetVerticalAlignment(VAlign_Fill);
		}
		SizeBox->AddChild(Button);
		WidgetTree->RootWidget = SizeBox;
		SetState(bSelected, bInParty, bLocked);
	}
	return Super::RebuildWidget();
}

void UKeepersHeroCard::SetState(bool bInSelected, bool bInPartyNow, bool bInLocked)
{
	bSelected = bInSelected;
	bInParty = bInPartyNow;
	bLocked = bInLocked;
	if (Frame)
	{
		const FLinearColor Edge = bSelected ? KeepersStyle::Gold : bInParty ? FLinearColor(1.f, 0.74f, 0.3f, 0.55f) : FLinearColor(1.f, 1.f, 1.f, 0.16f);
		Frame->SetBrush(KeepersStyle::Panel(FLinearColor::Transparent, 12.f, Edge, bSelected ? 3.f : 1.5f));
	}
	if (Badge)
	{
		Badge->SetVisibility(bInParty && Size.X > 150.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LockShade)
	{
		LockShade->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (PortraitImage)
	{
		PortraitImage->SetColorAndOpacity(bLocked ? FLinearColor(0.35f, 0.35f, 0.4f) : FLinearColor::White);
	}
}

void UKeepersHeroCard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Hovered or chosen cards rise a little.
	const float Target = (Button && Button->IsHovered()) || bSelected ? 1.f : 0.f;
	Lift = FMath::FInterpTo(Lift, Target, InDeltaTime, 12.f);
	if (Button)
	{
		Button->SetRenderTranslation(FVector2D(0.f, -10.f * Lift));
	}
}

void UKeepersHeroCard::HandleClicked()
{
	if (OnClicked)
	{
		TFunction<void()> Callback = OnClicked;
		Callback();
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersSquadWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		// Hit-testable, so dragging over the empty middle turns the hero.
		Canvas->SetVisibility(ESlateVisibility::Visible);
		WidgetTree->RootWidget = Canvas;
		auto Place = [Canvas](UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment, bool bAutoSize)
		{
			UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
			CanvasSlot->SetAnchors(Anchors);
			CanvasSlot->SetOffsets(Offsets);
			CanvasSlot->SetAlignment(Alignment);
			CanvasSlot->SetAutoSize(bAutoSize);
			return CanvasSlot;
		};
		TWeakObjectPtr<UKeepersSquadWidget> WeakThis(this);

		// Shades: left and right sides and the bottom, the camp stays bright in the middle.
		UBorder* LeftShade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LeftShade->SetBrush(KeepersStyle::Fade(FLinearColor(0.005f, 0.008f, 0.015f, 0.9f)));
		LeftShade->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(LeftShade, FAnchors(0.f, 0.f, 0.36f, 1.f), FMargin(0.f), FVector2D::ZeroVector, false);
		UBorder* RightShade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		RightShade->SetBrush(KeepersStyle::Fade(FLinearColor(0.005f, 0.008f, 0.015f, 0.93f), false));
		RightShade->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(RightShade, FAnchors(0.56f, 0.f, 1.f, 1.f), FMargin(0.f), FVector2D::ZeroVector, false);

		// Title and the squad slots, top left.
		UVerticalBox* TitleBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TitleBox->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, KeepersSquad::T(TEXT("перед походом")), KeepersStyle::Heading(18), KeepersStyle::Gold));
		TitleBox->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, KeepersSquad::T(TEXT("Соберите отряд")), KeepersStyle::Heading(54)));
		UHorizontalBox* CountRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		SquadCount = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(15), KeepersStyle::TextDim);
		CountRow->AddChildToHorizontalBox(SquadCount)->SetVerticalAlignment(VAlign_Center);
		UBorder* GloryPill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		GloryPill->SetBrush(KeepersStyle::Panel(FLinearColor(1.f, 0.74f, 0.3f, 0.16f), 10.f, KeepersStyle::GoldDim, 1.f));
		GloryPill->SetPadding(FMargin(10.f, 2.f));
		GloryText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(16), KeepersStyle::Gold);
		GloryPill->SetContent(GloryText);
		CountRow->AddChildToHorizontalBox(GloryPill)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
		TitleBox->AddChildToVerticalBox(CountRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		Slots = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		TitleBox->AddChildToVerticalBox(Slots);
		Place(TitleBox, FAnchors(0.f, 0.f), FMargin(60.f, 46.f, 0.f, 0.f), FVector2D::ZeroVector, true);

		// Roster: half cards along the bottom.
		UVerticalBox* RosterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RosterBox->AddChildToVerticalBox(KeepersSquad::MakeSectionTitle(WidgetTree, TEXT("Хранители")))->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		UHorizontalBox* Roster = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const AKeepersMenuScene* MenuScene = Scene.Get();
		for (const FKeepersHeroDef& Def : KeepersHeroes::All())
		{
			UKeepersHeroCard* Card = CreateWidget<UKeepersHeroCard>(this, UKeepersHeroCard::StaticClass());
			const FName Id = Def.Id;
			Card->Setup(Id, MenuScene ? MenuScene->GetPortrait(Id) : nullptr, FVector2D(150.f, 196.f), [WeakThis, Id]() { if (WeakThis.IsValid()) { WeakThis->SelectHero(Id); } });
			Roster->AddChildToHorizontalBox(Card)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
			RosterCards.Add(Card);
		}
		RosterBox->AddChildToVerticalBox(Roster);
		Place(RosterBox, FAnchors(0.f, 1.f), FMargin(60.f, -36.f, 0.f, 0.f), FVector2D(0.f, 1.f), true);

		// Details of the chosen hero, right side.
		UVerticalBox* Details = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		HeroName = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(50));
		Details->AddChildToVerticalBox(HeroName);
		HeroClass = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(18), KeepersStyle::Gold);
		Details->AddChildToVerticalBox(HeroClass);
		USizeBox* DescriptionSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DescriptionSize->SetWidthOverride(520.f);
		HeroDescription = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(14), FLinearColor(0.82f, 0.84f, 0.86f), true);
		DescriptionSize->AddChild(HeroDescription);
		Details->AddChildToVerticalBox(DescriptionSize)->SetPadding(FMargin(0.f, 8.f, 0.f, 14.f));

		Details->AddChildToVerticalBox(KeepersSquad::MakeSectionTitle(WidgetTree, TEXT("Характеристики")))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		Stats = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Details->AddChildToVerticalBox(Stats)->SetPadding(FMargin(10.f, 0.f, 0.f, 14.f));

		Details->AddChildToVerticalBox(KeepersSquad::MakeSectionTitle(WidgetTree, TEXT("Способности")))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		Abilities = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Details->AddChildToVerticalBox(Abilities);
		USizeBox* AbilityTextSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		AbilityTextSize->SetWidthOverride(520.f);
		AbilityTextSize->SetMinDesiredHeight(44.f);
		AbilityText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(13), FLinearColor(0.82f, 0.84f, 0.86f), true);
		AbilityTextSize->AddChild(AbilityText);
		Details->AddChildToVerticalBox(AbilityTextSize)->SetPadding(FMargin(12.f, 6.f, 0.f, 12.f));

		Details->AddChildToVerticalBox(KeepersSquad::MakeSectionTitle(WidgetTree, TEXT("Облик")))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		Outfit = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Details->AddChildToVerticalBox(Outfit);
		Poses = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Details->AddChildToVerticalBox(Poses)->SetPadding(FMargin(0.f, 8.f, 0.f, 16.f));

		UHorizontalBox* PartyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		PartyButton = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		PartyButton->Setup(FText::GetEmpty(), UKeepersFnButton::EKind::Secondary, 20, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ToggleParty(); } }, 250.f);
		PartyRow->AddChildToHorizontalBox(PartyButton);
		UnlockButton = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		UnlockButton->Setup(FText::GetEmpty(), UKeepersFnButton::EKind::Primary, 18, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->UnlockPendingLook(); } }, 250.f);
		UnlockButton->SetVisibility(ESlateVisibility::Collapsed);
		PartyRow->AddChildToHorizontalBox(UnlockButton)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		Details->AddChildToVerticalBox(PartyRow);
		// Lock reasons go under the buttons, so they never push the panel wider.
		LockText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(14), KeepersStyle::Gold, true);
		USizeBox* LockSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LockSize->SetWidthOverride(520.f);
		LockSize->AddChild(LockText);
		Details->AddChildToVerticalBox(LockSize)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		// A fixed width: the panel keeps its place at any resolution and whatever the buttons say.
		USizeBox* DetailsSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DetailsSize->SetWidthOverride(530.f);
		DetailsSize->AddChild(Details);
		Place(DetailsSize, FAnchors(1.f, 0.f), FMargin(-570.f, 40.f, 0.f, 0.f), FVector2D::ZeroVector, true);

		// Hint and the main buttons, bottom right.
		UVerticalBox* Actions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Hint = KeepersStyle::MakeText(WidgetTree, KeepersSquad::T(TEXT("Тяните мышью по сцене, чтобы повернуть героя")), KeepersStyle::Body(12), KeepersStyle::TextDim);
		Actions->AddChildToVerticalBox(Hint)->SetHorizontalAlignment(HAlign_Right);
		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UKeepersFnButton* Back = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		Back->Setup(KeepersSquad::T(TEXT("‹ Назад")), UKeepersFnButton::EKind::Secondary, 22, [WeakThis]() { if (WeakThis.IsValid() && WeakThis->OnBack) { WeakThis->OnBack(); } }, 170.f);
		Buttons->AddChildToHorizontalBox(Back)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		StartButton = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		StartButton->Setup(KeepersSquad::T(bStartsCampaign ? TEXT("Начать поход") : TEXT("Готово")), UKeepersFnButton::EKind::Primary, 26,
			[WeakThis]() { if (WeakThis.IsValid() && WeakThis->OnStart) { WeakThis->OnStart(); } }, 300.f);
		Buttons->AddChildToHorizontalBox(StartButton);
		Actions->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		Place(Actions, FAnchors(1.f, 1.f), FMargin(-60.f, -40.f, 0.f, 0.f), FVector2D(1.f, 1.f), true);

		const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
		SelectHero(Party && Party->GetParty().Num() > 0 ? Party->GetParty()[0] : KeepersHeroes::All()[0].Id);
	}
	return Super::RebuildWidget();
}

void UKeepersSquadWidget::SelectHero(FName Hero)
{
	Selected = Hero;
	ActiveAbility = INDEX_NONE;
	if (AKeepersMenuScene* MenuScene = Scene.Get())
	{
		MenuScene->ShowPreview(Hero);
	}
	RefreshCards();
	RefreshDetails();
}

void UKeepersSquadWidget::RefreshCards()
{
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (!Party || !Slots)
	{
		return;
	}
	for (UKeepersHeroCard* Card : RosterCards)
	{
		Card->SetState(Card->GetHero() == Selected, Party->IsInParty(Card->GetHero()), !Party->IsUnlocked(Card->GetHero()));
	}

	Slots->ClearChildren();
	TWeakObjectPtr<UKeepersSquadWidget> WeakThis(this);
	const AKeepersMenuScene* MenuScene = Scene.Get();
	for (int32 I = 0; I < UKeepersPartySubsystem::SquadSize; ++I)
	{
		const FName Hero = Party->GetParty().IsValidIndex(I) ? Party->GetParty()[I] : NAME_None;
		UKeepersHeroCard* Card = CreateWidget<UKeepersHeroCard>(this, UKeepersHeroCard::StaticClass());
		Card->Setup(Hero, MenuScene && !Hero.IsNone() ? MenuScene->GetPortrait(Hero) : nullptr, FVector2D(124.f, 150.f),
			[WeakThis, Hero]() { if (WeakThis.IsValid() && !Hero.IsNone()) { WeakThis->SelectHero(Hero); } });
		Card->SetState(Hero == Selected, false, false);
		Slots->AddChildToHorizontalBox(Card)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}
	const int32 Count = Party->GetParty().Num();
	if (SquadCount)
	{
		SquadCount->SetText(FText::FromString(Count < UKeepersPartySubsystem::SquadSize
			? FString::Printf(TEXT("В отряде %d из %d. Выберите ещё."), Count, UKeepersPartySubsystem::SquadSize)
			: FString::Printf(TEXT("Отряд собран: %d из %d."), Count, UKeepersPartySubsystem::SquadSize)));
	}
	if (StartButton)
	{
		StartButton->SetButtonEnabled(Count == UKeepersPartySubsystem::SquadSize);
	}
}

void UKeepersSquadWidget::RefreshDetails()
{
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Selected);
	UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (!Def || !Party || !HeroName)
	{
		return;
	}
	HeroName->SetText(Def->Name);
	HeroClass->SetText(FText::FromString(Def->Class.ToString() + TEXT(" · ") + Def->Role.ToString()));
	HeroDescription->SetText(Def->Description);

	int32 MaxHp = 1;
	for (const FKeepersHeroDef& Other : KeepersHeroes::All())
	{
		MaxHp = FMath::Max(MaxHp, KeepersHeroes::Stats(Other).Hp);
	}
	const FKeepersHeroStats HeroStats = KeepersHeroes::Stats(*Def);
	Stats->ClearChildren();
	Stats->AddChildToVerticalBox(KeepersSquad::MakeHpRow(WidgetTree, HeroStats.Hp, MaxHp))->SetPadding(FMargin(0.f, 2.f));
	Stats->AddChildToVerticalBox(KeepersSquad::MakePipRow(WidgetTree, TEXT("Очки передвижения"), HeroStats.Mp, 6, FLinearColor(0.25f, 0.75f, 0.45f)))->SetPadding(FMargin(0.f, 2.f));
	Stats->AddChildToVerticalBox(KeepersSquad::MakePipRow(WidgetTree, TEXT("Очки действия"), HeroStats.Ap, 6, FLinearColor(0.3f, 0.55f, 1.f)))->SetPadding(FMargin(0.f, 2.f));

	TWeakObjectPtr<UKeepersSquadWidget> WeakThis(this);
	Abilities->ClearChildren();
	AbilityButtons.Reset();
	for (int32 I = 0; I < Def->Spells.Num(); ++I)
	{
		const FKeepersSpellInfo* Spell = KeepersHeroes::FindSpell(Def->Spells[I]);
		if (!Spell)
		{
			continue;
		}
		UKeepersFnButton* Line = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		Line->Setup(FText::FromString(FString::Printf(TEXT("%d   %s"), I + 1, *Spell->Name.ToString())), UKeepersFnButton::EKind::Line, 16,
			[WeakThis, I]() { if (WeakThis.IsValid()) { WeakThis->ShowAbility(I, true); } }, 520.f);
		Line->OnHovered = [WeakThis, I]() { if (WeakThis.IsValid()) { WeakThis->ShowAbility(I, false); } };
		Abilities->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 1.f));
		AbilityButtons.Add(Line);
	}
	AbilityText->SetText(KeepersSquad::T(TEXT("Нажмите на способность, чтобы увидеть её в деле.")));

	// Outfit and colour switchers.
	Outfit->ClearChildren();
	const FName Hero = Selected;
	TArray<FText> LookNames;
	for (int32 I = 0; I < Def->Looks.Num(); ++I)
	{
		// Closed outfits show their price.
		LookNames.Add(Party->IsLookUnlocked(Hero, I) ? Def->Looks[I].Name
			: FText::FromString(FString::Printf(TEXT("%s · %d"), *Def->Looks[I].Name.ToString(), UKeepersPartySubsystem::LookCost(I))));
	}
	PendingLook = INDEX_NONE;
	UKeepersSettingRow* LookRow = CreateWidget<UKeepersSettingRow>(this, UKeepersSettingRow::StaticClass());
	LookRow->SetRowWidth(520.f);
	const int32 Worn = Party->IsLookUnlocked(Hero, Party->GetLookIndex(Hero)) ? Party->GetLookIndex(Hero) : 0;
	LookRow->Setup(KeepersSquad::T(TEXT("Наряд")), LookNames, Worn, [WeakThis](int32 Index)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ChooseLook(Index);
		}
	});
	Outfit->AddChildToVerticalBox(LookRow)->SetPadding(FMargin(0.f, 2.f));
	TArray<FText> ColorNames;
	int32 CurrentColor = 0;
	const TArray<FName>& Colors = UKeepersFigureComponent::Colors();
	for (int32 I = 0; I < Colors.Num(); ++I)
	{
		ColorNames.Add(UKeepersFigureComponent::ColorName(Colors[I]));
		if (Colors[I] == Party->GetColor(Hero))
		{
			CurrentColor = I;
		}
	}
	UKeepersSettingRow* ColorRow = CreateWidget<UKeepersSettingRow>(this, UKeepersSettingRow::StaticClass());
	ColorRow->SetRowWidth(520.f);
	ColorRow->Setup(KeepersSquad::T(TEXT("Цвет")), ColorNames, CurrentColor, [WeakThis, Hero](int32 Index)
	{
		UKeepersPartySubsystem* PartyNow = UKeepersPartySubsystem::Get(WeakThis.Get());
		if (PartyNow && WeakThis.IsValid())
		{
			PartyNow->SetColor(Hero, UKeepersFigureComponent::Colors()[Index]);
			if (AKeepersMenuScene* MenuScene = WeakThis->Scene.Get())
			{
				MenuScene->RefreshHero(Hero);
			}
		}
	});
	Outfit->AddChildToVerticalBox(ColorRow)->SetPadding(FMargin(0.f, 2.f));

	// Animation buttons.
	Poses->ClearChildren();
	struct FPoseButton { const TCHAR* Label; EKeepersPose Pose; };
	const bool bCaster = Def->Looks.Num() > 0 && Def->Looks[0].AnimSet == EKeepersAnimSet::Staff;
	const FPoseButton PoseButtons[] =
	{
		{ TEXT("Стойка"), EKeepersPose::Idle },
		{ TEXT("Шаг"), EKeepersPose::Walk },
		{ TEXT("Бег"), EKeepersPose::Run },
		{ bCaster ? TEXT("Чары") : TEXT("Удар"), bCaster ? EKeepersPose::Cast : EKeepersPose::Attack },
		{ TEXT("Ранение"), EKeepersPose::Hit },
		{ TEXT("Падение"), EKeepersPose::Death },
	};
	for (const FPoseButton& PoseButton : PoseButtons)
	{
		UKeepersFnButton* Button = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		const EKeepersPose Pose = PoseButton.Pose;
		Button->Setup(KeepersSquad::T(PoseButton.Label), UKeepersFnButton::EKind::Secondary, 14, [WeakThis, Pose]()
		{
			if (WeakThis.IsValid())
			{
				if (AKeepersMenuScene* MenuScene = WeakThis->Scene.Get())
				{
					MenuScene->PlayPreview(Pose);
				}
			}
		});
		Poses->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}

	const bool bUnlocked = Party->IsUnlocked(Hero);
	UnlockButton->SetVisibility(ESlateVisibility::Collapsed);
	RefreshGlory();
	PartyButton->SetVisibility(bUnlocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PartyButton->SetLabel(KeepersSquad::T(Party->IsInParty(Hero) ? TEXT("Убрать из отряда") : TEXT("Взять в отряд")));
	LockText->SetText(bUnlocked ? FText::GetEmpty() : FText::FromString(TEXT("Откроется: ") + Def->Unlock.ToString()));
}

void UKeepersSquadWidget::ChooseLook(int32 Look)
{
	UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	AKeepersMenuScene* MenuScene = Scene.Get();
	if (!Party)
	{
		return;
	}
	if (Party->IsLookUnlocked(Selected, Look))
	{
		PendingLook = INDEX_NONE;
		Party->SetLookIndex(Selected, Look);
		if (MenuScene)
		{
			MenuScene->RefreshHero(Selected);
		}
		UnlockButton->SetVisibility(ESlateVisibility::Collapsed);
		LockText->SetText(FText::GetEmpty());
		return;
	}
	// Closed: try it on, and offer to open it.
	PendingLook = Look;
	if (MenuScene)
	{
		MenuScene->TryOnLook(Selected, Look);
	}
	const int32 Cost = UKeepersPartySubsystem::LookCost(Look);
	UnlockButton->SetLabel(FText::FromString(FString::Printf(TEXT("Открыть за %d славы"), Cost)));
	UnlockButton->SetButtonEnabled(Party->GetGlory() >= Cost);
	UnlockButton->SetVisibility(ESlateVisibility::Visible);
	LockText->SetText(FText::FromString(Party->GetGlory() >= Cost ? TEXT("") : TEXT("Не хватает славы: она даётся за победы в походе.")));
}

void UKeepersSquadWidget::UnlockPendingLook()
{
	UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (!Party || PendingLook == INDEX_NONE || !Party->UnlockLook(Selected, PendingLook))
	{
		return;
	}
	Party->SetLookIndex(Selected, PendingLook);
	if (AKeepersMenuScene* MenuScene = Scene.Get())
	{
		MenuScene->RefreshHero(Selected);
		MenuScene->PlayPreview(EKeepersPose::Cast);
	}
	RefreshDetails();
}

void UKeepersSquadWidget::RefreshGlory()
{
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (Party && GloryText)
	{
		GloryText->SetText(FText::FromString(FString::Printf(TEXT("Слава: %d"), Party->GetGlory())));
	}
}

void UKeepersSquadWidget::ShowAbility(int32 Index, bool bPlay)
{
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Selected);
	const FKeepersSpellInfo* Spell = Def && Def->Spells.IsValidIndex(Index) ? KeepersHeroes::FindSpell(Def->Spells[Index]) : nullptr;
	if (!Spell)
	{
		return;
	}
	AbilityText->SetText(FText::FromString(Spell->Description.ToString() + TEXT("\n") + Spell->Numbers.ToString()));
	if (!bPlay)
	{
		return;
	}
	ActiveAbility = Index;
	for (int32 I = 0; I < AbilityButtons.Num(); ++I)
	{
		AbilityButtons[I]->SetActive(I == Index);
	}
	if (AKeepersMenuScene* MenuScene = Scene.Get())
	{
		MenuScene->PlayAbility(Selected, Spell->Id);
	}
}

void UKeepersSquadWidget::ToggleParty()
{
	UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (!Party)
	{
		return;
	}
	if (Party->IsInParty(Selected))
	{
		Party->RemoveFromParty(Selected);
	}
	else if (!Party->AddToParty(Selected))
	{
		if (Hint)
		{
			Hint->SetText(KeepersSquad::T(TEXT("Отряд полон: сначала уберите кого-нибудь")));
			Hint->SetColorAndOpacity(FSlateColor(KeepersStyle::Gold));
		}
		return;
	}
	if (AKeepersMenuScene* MenuScene = Scene.Get())
	{
		MenuScene->RefreshCamp();
	}
	RefreshCards();
	RefreshDetails();
}

FReply UKeepersSquadWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		if (OnBack)
		{
			OnBack();
		}
		return FReply::Handled();
	}
	// 1-3 play the abilities, like in battle.
	const FKey Numbers[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
	for (int32 I = 0; I < UE_ARRAY_COUNT(Numbers); ++I)
	{
		if (Key == Numbers[I])
		{
			ShowAbility(I, true);
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UKeepersSquadWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UKeepersSquadWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UKeepersSquadWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		if (AKeepersMenuScene* MenuScene = Scene.Get())
		{
			MenuScene->RotatePreview(-InMouseEvent.GetCursorDelta().X * 0.6f);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}
