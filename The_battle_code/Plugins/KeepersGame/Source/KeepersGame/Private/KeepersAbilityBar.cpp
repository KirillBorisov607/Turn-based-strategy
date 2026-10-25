#include "KeepersAbilityBar.h"

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
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "KeepersHeroes.h"
#include "KeepersReflection.h"
#include "KeepersStyle.h"

namespace KeepersAbilities
{
	static const TCHAR* SpellTable = TEXT("/Game/Blueprints/Core/Combat/Spells/Utilities/DT_SpellData.DT_SpellData");

	static FText T(const FString& Text)
	{
		return FText::FromString(Text);
	}

	/** Numbers of an ability from DT_SpellData. */
	struct FFacts
	{
		bool bValid = false;
		FIntPoint Range = FIntPoint::ZeroValue;
		bool bLineOfSight = false;
		FIntPoint Hp = FIntPoint::ZeroValue;
		FIntPoint Mp = FIntPoint::ZeroValue;
		FIntPoint Ap = FIntPoint::ZeroValue;
		int32 CostAp = 0;
		int32 CostMp = 0;
		int32 CostHp = 0;
		int32 MaxCast = 999;
		int32 Cooldown = 0;
	};

	static bool Member(const UStruct*& Struct, void*& Container, const TCHAR* Name)
	{
		FProperty* Property = nullptr;
		void* Ptr = KeepersReflection::FindValuePtr(Struct, Container, Name, &Property);
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty || !Ptr)
		{
			return false;
		}
		Struct = StructProperty->Struct;
		Container = Ptr;
		return true;
	}

	static FFacts Facts(const FString& Spell)
	{
		FFacts Result;
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, SpellTable);
		uint8* Row = Table ? Table->FindRowUnchecked(FName(*Spell)) : nullptr;
		if (!Row)
		{
			return Result;
		}
		Result.bValid = true;
		const UStruct* RowStruct = Table->GetRowStruct();

		const UStruct* Cast = RowStruct;
		void* CastPtr = Row;
		if (Member(Cast, CastPtr, TEXT("Cast")))
		{
			KeepersReflection::GetIntPoint(Cast, CastPtr, TEXT("RangeMinMax"), Result.Range);
			const UStruct* Sight = Cast;
			void* SightPtr = CastPtr;
			if (Member(Sight, SightPtr, TEXT("LineOfSight")))
			{
				KeepersReflection::GetBool(Sight, SightPtr, TEXT("RequireLineOfSight"), Result.bLineOfSight);
			}
		}
		const UStruct* Impact = RowStruct;
		void* ImpactPtr = Row;
		if (Member(Impact, ImpactPtr, TEXT("Impact")))
		{
			KeepersReflection::GetIntPoint(Impact, ImpactPtr, TEXT("MinMaxModifier_Hp"), Result.Hp);
			KeepersReflection::GetIntPoint(Impact, ImpactPtr, TEXT("MinMaxModifier_Mp"), Result.Mp);
			KeepersReflection::GetIntPoint(Impact, ImpactPtr, TEXT("MinMaxModifier_AP"), Result.Ap);
		}
		const UStruct* Requirements = RowStruct;
		void* RequirementsPtr = Row;
		if (Member(Requirements, RequirementsPtr, TEXT("Requirements")))
		{
			KeepersReflection::GetInt(Requirements, RequirementsPtr, TEXT("Cost_AP"), Result.CostAp);
			KeepersReflection::GetInt(Requirements, RequirementsPtr, TEXT("Cost_MP"), Result.CostMp);
			KeepersReflection::GetInt(Requirements, RequirementsPtr, TEXT("Cost_HP"), Result.CostHp);
			KeepersReflection::GetInt(Requirements, RequirementsPtr, TEXT("MaxCastPerTurn"), Result.MaxCast);
			KeepersReflection::GetInt(Requirements, RequirementsPtr, TEXT("CooldownTurnCount"), Result.Cooldown);
		}
		return Result;
	}

	/** "2–3" or "3". */
	static FString Span(int32 A, int32 B)
	{
		const int32 Low = FMath::Min(FMath::Abs(A), FMath::Abs(B));
		const int32 High = FMath::Max(FMath::Abs(A), FMath::Abs(B));
		return Low == High ? FString::FromInt(High) : FString::Printf(TEXT("%d–%d"), Low, High);
	}

	static FString Cells(int32 N)
	{
		const int32 Last = N % 10;
		const int32 Teens = N % 100;
		const TCHAR* Word = (Teens >= 11 && Teens <= 14) ? TEXT("клеток") : Last == 1 ? TEXT("клетка") : (Last >= 2 && Last <= 4) ? TEXT("клетки") : TEXT("клеток");
		return Word;
	}

	/** Spell entry name of a Blueprint Button_Spell ("SwordSlash"). */
	static FString SpellOf(UObject* Button)
	{
		FProperty* Property = nullptr;
		void* Ptr = Button ? KeepersReflection::FindValuePtr(Button->GetClass(), Button, TEXT("Spell"), &Property) : nullptr;
		const FByteProperty* Byte = CastField<FByteProperty>(Property);
		if (!Ptr || !Byte || !Byte->Enum)
		{
			return FString();
		}
		return Byte->Enum->GetDisplayNameTextByValue(Byte->GetPropertyValue(Ptr)).ToString();
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersAbilityButton::Setup(int32 InIndex, UObject* InIcon, const FText& InName, int32 InCost)
{
	Index = InIndex;
	Icon = InIcon;
	Name = InName;
	Cost = InCost;
}

TSharedRef<SWidget> UKeepersAbilityButton::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

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
		Button->OnClicked.AddDynamic(this, &UKeepersAbilityButton::HandleClicked);

		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		auto Fill = [](UOverlaySlot* OverlaySlot)
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		};
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetBrush(KeepersStyle::Panel(FLinearColor(0.06f, 0.07f, 0.1f, 1.f), 14.f, FLinearColor::Transparent));
		Fill(Layers->AddChildToOverlay(Back));

		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (Icon)
		{
			IconImage->SetBrushResourceObject(Icon);
		}
		UOverlaySlot* IconSlot = Layers->AddChildToOverlay(IconImage);
		Fill(IconSlot);
		IconSlot->SetPadding(FMargin(8.f));

		// Dark veil while the ability cannot be used, with its cooldown or casts left.
		Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shade->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.62f), 14.f, FLinearColor::Transparent));
		Shade->SetVisibility(ESlateVisibility::Collapsed);
		Fill(Layers->AddChildToOverlay(Shade));
		CounterText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(22), KeepersStyle::Text);
		CounterText->SetShadowOffset(FVector2D(0.f, 2.f));
		CounterText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		UOverlaySlot* CounterSlot = Layers->AddChildToOverlay(CounterText);
		CounterSlot->SetHorizontalAlignment(HAlign_Right);
		CounterSlot->SetVerticalAlignment(VAlign_Bottom);
		CounterSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 4.f));

		// Hotkey in the corner.
		UBorder* Key = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Key->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.7f), 6.f, KeepersStyle::GoldDim, 1.f));
		Key->SetPadding(FMargin(6.f, 0.f));
		Key->SetContent(KeepersStyle::MakeText(WidgetTree, FText::AsNumber(Index + 1), KeepersStyle::Heading(15), KeepersStyle::Gold));
		UOverlaySlot* KeySlot = Layers->AddChildToOverlay(Key);
		KeySlot->SetHorizontalAlignment(HAlign_Left);
		KeySlot->SetVerticalAlignment(VAlign_Top);
		KeySlot->SetPadding(FMargin(5.f));

		// Cost in action points.
		if (Cost > 0)
		{
			UBorder* CostPill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			CostPill->SetBrush(KeepersStyle::Panel(FLinearColor(0.12f, 0.25f, 0.55f, 0.95f), 6.f, FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f));
			CostPill->SetPadding(FMargin(5.f, 0.f));
			CostPill->SetContent(KeepersStyle::MakeText(WidgetTree, KeepersAbilities::T(FString::Printf(TEXT("%d ОД"), Cost)), KeepersStyle::Body(11, true)));
			UOverlaySlot* CostSlot = Layers->AddChildToOverlay(CostPill);
			CostSlot->SetHorizontalAlignment(HAlign_Right);
			CostSlot->SetVerticalAlignment(VAlign_Top);
			CostSlot->SetPadding(FMargin(5.f));
		}

		Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fill(Layers->AddChildToOverlay(Frame));

		if (UButtonSlot* LayersSlot = Cast<UButtonSlot>(Button->AddChild(Layers)))
		{
			LayersSlot->SetHorizontalAlignment(HAlign_Fill);
			LayersSlot->SetVerticalAlignment(VAlign_Fill);
		}
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(84.f);
		Size->SetHeightOverride(84.f);
		Size->AddChild(Button);
		Column->AddChildToVerticalBox(Size)->SetHorizontalAlignment(HAlign_Center);

		USizeBox* NameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NameSize->SetWidthOverride(104.f);
		UTextBlock* NameText = KeepersStyle::MakeText(WidgetTree, Name, KeepersStyle::Body(11, true), KeepersStyle::Text, true);
		NameText->SetJustification(ETextJustify::Center);
		NameSize->AddChild(NameText);
		Column->AddChildToVerticalBox(NameSize)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		WidgetTree->RootWidget = Column;
		SetState(false, true, FText::GetEmpty());
	}
	return Super::RebuildWidget();
}

void UKeepersAbilityButton::SetState(bool bSelected, bool bAvailable, const FText& Counter)
{
	if (!Frame)
	{
		return;
	}
	Frame->SetBrush(KeepersStyle::Panel(FLinearColor::Transparent, 14.f, bSelected ? KeepersStyle::Gold : FLinearColor(1.f, 1.f, 1.f, 0.18f), bSelected ? 3.f : 1.5f));
	Shade->SetVisibility(bAvailable ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	CounterText->SetText(Counter);
	IconImage->SetColorAndOpacity(bAvailable ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.6f));
}

void UKeepersAbilityButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const bool bHovered = Button && Button->IsHovered();
	if (bHovered != bWasHovered)
	{
		bWasHovered = bHovered;
		if (OnHoverChanged)
		{
			OnHoverChanged(bHovered);
		}
	}
	Lift = FMath::FInterpTo(Lift, bHovered ? 1.f : 0.f, InDeltaTime, 14.f);
	if (Button)
	{
		Button->SetRenderTranslation(FVector2D(0.f, -6.f * Lift));
	}
}

void UKeepersAbilityButton::HandleClicked()
{
	if (OnClicked)
	{
		TFunction<void()> Callback = OnClicked;
		Callback();
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersAbilityBar::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 16.f));
		Panel->SetPadding(FMargin(12.f, 10.f, 12.f, 8.f));
		Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Panel->SetContent(Row);
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.5f, 1.f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
		PanelSlot->SetOffsets(FMargin(0.f, -14.f, 0.f, 0.f));
		PanelSlot->SetAutoSize(true);

		// TipPanel above the bar.
		TipPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		TipPanel->SetBrush(KeepersStyle::Panel(KeepersStyle::InkLight, 12.f, KeepersStyle::Gold, 1.5f));
		TipPanel->SetPadding(FMargin(18.f, 12.f));
		TipPanel->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBox* TipColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TooltipHeading = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(26), KeepersStyle::Gold);
		TipColumn->AddChildToVerticalBox(TooltipHeading);
		USizeBox* TextSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TextSize->SetWidthOverride(380.f);
		TooltipBody = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(14), KeepersStyle::Text, true);
		TextSize->AddChild(TooltipBody);
		TipColumn->AddChildToVerticalBox(TextSize)->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));
		TooltipFacts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TipColumn->AddChildToVerticalBox(TooltipFacts);
		TipPanel->SetContent(TipColumn);
		UCanvasPanelSlot* TipSlot = Canvas->AddChildToCanvas(TipPanel);
		TipSlot->SetAnchors(FAnchors(0.5f, 1.f));
		TipSlot->SetAlignment(FVector2D(0.5f, 1.f));
		TipSlot->SetOffsets(FMargin(0.f, -160.f, 0.f, 0.f));
		TipSlot->SetAutoSize(true);
	}
	return Super::RebuildWidget();
}

void UKeepersAbilityBar::Sync(UUserWidget* Hud)
{
	if (!Row || !Hud)
	{
		return;
	}
	UUserWidget* List = Cast<UUserWidget>(KeepersReflection::GetObject(Hud, TEXT("ButtonList_Spells")));
	UPanelWidget* Grid = List && List->WidgetTree ? Cast<UPanelWidget>(List->WidgetTree->FindWidget(TEXT("GridPanel_Spells"))) : nullptr;
	TArray<UUserWidget*> Source;
	TArray<FString> Names;
	if (Grid)
	{
		for (int32 I = 0; I < Grid->GetChildrenCount(); ++I)
		{
			if (UUserWidget* Child = Cast<UUserWidget>(Grid->GetChildAt(I)))
			{
				Source.Add(Child);
				Names.Add(KeepersAbilities::SpellOf(Child));
			}
		}
	}
	SetVisibility(Source.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	// Another unit's abilities: build the buttons again.
	if (Names != Spells || List != SpellList.Get())
	{
		Spells = Names;
		SpellList = List;
		SourceButtons.Reset();
		Buttons.Reset();
		Row->ClearChildren();
		HideTooltip(TooltipIndex);
		TWeakObjectPtr<UKeepersAbilityBar> WeakThis(this);
		for (int32 I = 0; I < Source.Num(); ++I)
		{
			const FKeepersSpellInfo* Info = KeepersHeroes::FindSpell(*Names[I]);
			const KeepersAbilities::FFacts Facts = KeepersAbilities::Facts(Names[I]);
			UImage* SourceIcon = Source[I]->WidgetTree ? Cast<UImage>(Source[I]->WidgetTree->FindWidget(TEXT("Image_SpellIcon"))) : nullptr;
			UKeepersAbilityButton* Button = CreateWidget<UKeepersAbilityButton>(this, UKeepersAbilityButton::StaticClass());
			Button->Setup(I, SourceIcon ? SourceIcon->GetBrush().GetResourceObject() : nullptr, Info ? Info->Name : KeepersAbilities::T(Names[I]), Facts.CostAp);
			Button->OnClicked = [WeakThis, I]() { if (WeakThis.IsValid()) { WeakThis->Press(I); } };
			Button->OnHoverChanged = [WeakThis, I](bool bHovered) { if (WeakThis.IsValid()) { bHovered ? WeakThis->ShowTooltip(I) : WeakThis->HideTooltip(I); } };
			Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(6.f, 0.f));
			Buttons.Add(Button);
			SourceButtons.Add(Source[I]);
		}
	}

	// States follow the Blueprint buttons: selected, usable, cooldown or casts left.
	for (int32 I = 0; I < Buttons.Num() && I < Source.Num(); ++I)
	{
		bool bSelected = false;
		bool bAvailable = true;
		KeepersReflection::GetBool(Source[I]->GetClass(), Source[I], TEXT("IsSelected"), bSelected);
		KeepersReflection::GetBool(Source[I]->GetClass(), Source[I], TEXT("CanBeSelected"), bAvailable);
		const UTextBlock* Counter = Source[I]->WidgetTree ? Cast<UTextBlock>(Source[I]->WidgetTree->FindWidget(TEXT("Text_CooldownAndMaxCast"))) : nullptr;
		Buttons[I]->SetState(bSelected, bAvailable, Counter && Counter->IsVisible() ? Counter->GetText() : FText::GetEmpty());
	}
}

void UKeepersAbilityBar::Press(int32 Index)
{
	UUserWidget* List = SpellList.Get();
	UUserWidget* Source = SourceButtons.IsValidIndex(Index) ? SourceButtons[Index].Get() : nullptr;
	if (!List || !Source)
	{
		return;
	}
	KeepersReflection::FCall Click(List, TEXT("OnSpellButtonClicked"));
	Click.Set<UObject*>(TEXT("Button"), Source);
	Click.Call();
}

void UKeepersAbilityBar::ShowTooltip(int32 Index)
{
	if (!Spells.IsValidIndex(Index) || !TipPanel)
	{
		return;
	}
	TooltipIndex = Index;
	const FString& Spell = Spells[Index];
	const FKeepersSpellInfo* Info = KeepersHeroes::FindSpell(*Spell);
	const KeepersAbilities::FFacts Facts = KeepersAbilities::Facts(Spell);
	TooltipHeading->SetText(Info ? Info->Name : KeepersAbilities::T(Spell));
	TooltipBody->SetText(Info ? Info->Description : FText::GetEmpty());

	TooltipFacts->ClearChildren();
	auto Fact = [this](const TCHAR* Label, const FString& Value, const FLinearColor& Color)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelSize->SetWidthOverride(150.f);
		LabelSize->AddChild(KeepersStyle::MakeText(WidgetTree, KeepersAbilities::T(Label), KeepersStyle::Body(13), KeepersStyle::TextDim));
		Line->AddChildToHorizontalBox(LabelSize);
		Line->AddChildToHorizontalBox(KeepersStyle::MakeText(WidgetTree, KeepersAbilities::T(Value), KeepersStyle::Body(13, true), Color));
		TooltipFacts->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 1.f));
	};
	if (Facts.bValid)
	{
		using namespace KeepersAbilities;
		const bool bHeal = FMath::Max(Facts.Hp.X, Facts.Hp.Y) > 0;
		if (Facts.Hp != FIntPoint::ZeroValue)
		{
			Fact(bHeal ? TEXT("Лечение") : TEXT("Урон"), (bHeal ? TEXT("+") : TEXT("")) + Span(Facts.Hp.X, Facts.Hp.Y) + TEXT(" HP"), bHeal ? KeepersStyle::Good : FLinearColor(1.f, 0.45f, 0.35f));
		}
		if (Facts.Mp != FIntPoint::ZeroValue)
		{
			const bool bGain = FMath::Max(Facts.Mp.X, Facts.Mp.Y) > 0;
			Fact(TEXT("Очки передвижения"), (bGain ? TEXT("+") : TEXT("−")) + Span(Facts.Mp.X, Facts.Mp.Y), KeepersStyle::Text);
		}
		if (Facts.Ap != FIntPoint::ZeroValue)
		{
			const bool bGain = FMath::Max(Facts.Ap.X, Facts.Ap.Y) > 0;
			Fact(TEXT("Очки действия"), (bGain ? TEXT("+") : TEXT("−")) + Span(Facts.Ap.X, Facts.Ap.Y), KeepersStyle::Text);
		}
		const int32 MaxRange = FMath::Max(Facts.Range.X, Facts.Range.Y);
		Fact(TEXT("Дальность"), MaxRange <= 0 ? FString(TEXT("на себя")) : MaxRange == 1 && Facts.Range.X <= 1 ? FString(TEXT("соседняя клетка"))
			: Span(Facts.Range.X, Facts.Range.Y) + TEXT(" ") + Cells(MaxRange), KeepersStyle::Text);
		Fact(TEXT("Видимость"), Facts.bLineOfSight ? TEXT("нужна прямая видимость") : TEXT("бьёт через препятствия"), KeepersStyle::Text);
		FString Cost = FString::Printf(TEXT("%d ОД"), Facts.CostAp);
		if (Facts.CostMp > 0)
		{
			Cost += FString::Printf(TEXT(", %d ОП"), Facts.CostMp);
		}
		if (Facts.CostHp > 0)
		{
			Cost += FString::Printf(TEXT(", %d HP"), Facts.CostHp);
		}
		Fact(TEXT("Стоимость"), Cost, KeepersStyle::Text);
		if (Facts.Cooldown > 0)
		{
			Fact(TEXT("Перезарядка"), FString::Printf(TEXT("%d ход(а)"), Facts.Cooldown), KeepersStyle::Text);
		}
		if (Facts.MaxCast > 0 && Facts.MaxCast < 99)
		{
			Fact(TEXT("За ход"), FString::Printf(TEXT("не больше %d раз(а)"), Facts.MaxCast), KeepersStyle::Text);
		}
	}
	TipPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UKeepersAbilityBar::HideTooltip(int32 Index)
{
	if (TipPanel && (Index == TooltipIndex || Index == INDEX_NONE))
	{
		TipPanel->SetVisibility(ESlateVisibility::Collapsed);
		TooltipIndex = INDEX_NONE;
	}
}
