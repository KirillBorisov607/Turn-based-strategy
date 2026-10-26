#include "KeepersCampaignUI.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Styling/CoreStyle.h"
#include "Components/OverlaySlot.h"
#include "Components/Overlay.h"
#include "Components/WrapBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "KeepersBarks.h"
#include "KeepersBattleHud.h"
#include "KeepersBattleObjective.h"
#include "KeepersJuice.h"
#include "KeepersLoadingScreen.h"
#include "KeepersTutorial.h"
#include "KeepersCamp.h"
#include "KeepersCampaign.h"
#include "KeepersHeroes.h"
#include "KeepersRegions.h"
#include "KeepersStory.h"
#include "KeepersPartyBattle.h"
#include "KeepersCreatureSubsystem.h"
#include "KeepersLevelKit.h"
#include "KeepersReflection.h"
#include "KeepersStyle.h"
#include "KeepersTerrainSubsystem.h"
#include "Rendering/DrawElements.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersCampaignUI, Log, All);

namespace KeepersUI
{
	static const FLinearColor Gold = KeepersStyle::Gold;
	static const FLinearColor PanelColor = KeepersStyle::Ink;

	/** Titles and captions use the display font, longer lines the reading font. */
	static UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color, bool bWrap = false)
	{
		const bool bHeading = Size >= 17;
		UTextBlock* Block = KeepersStyle::MakeText(Tree, Text, bHeading ? KeepersStyle::Heading(Size + 4) : KeepersStyle::Body(Size), Color, bWrap);
		Block->SetJustification(ETextJustify::Center);
		return Block;
	}

	static FSlateBrush Rounded(const FLinearColor& Color, float Radius = 10.f, const FLinearColor& Edge = KeepersStyle::GoldDim)
	{
		return KeepersStyle::Panel(Color, Radius, Edge);
	}

	static FLinearColor NodeColor(EKeepersNodeType Type)
	{
		switch (Type)
		{
		case EKeepersNodeType::Battle: return FLinearColor(0.55f, 0.18f, 0.14f);
		case EKeepersNodeType::Elite: return FLinearColor(0.42f, 0.16f, 0.5f);
		case EKeepersNodeType::Ruins: return FLinearColor(0.16f, 0.32f, 0.55f);
		case EKeepersNodeType::Camp: return FLinearColor(0.6f, 0.38f, 0.1f);
		case EKeepersNodeType::Boss: return FLinearColor(0.35f, 0.05f, 0.05f);
		case EKeepersNodeType::Event: return FLinearColor(0.18f, 0.45f, 0.35f);
		}
		return FLinearColor::Gray;
	}

	static FText NodeName(EKeepersNodeType Type)
	{
		switch (Type)
		{
		case EKeepersNodeType::Battle: return FText::FromString(TEXT("Бой"));
		case EKeepersNodeType::Elite: return FText::FromString(TEXT("Элита"));
		case EKeepersNodeType::Ruins: return FText::FromString(TEXT("Руины"));
		case EKeepersNodeType::Camp: return FText::FromString(TEXT("Привал"));
		case EKeepersNodeType::Boss: return FText::FromString(TEXT("Босс"));
		case EKeepersNodeType::Event: return FText::FromString(TEXT("Событие"));
		}
		return FText::GetEmpty();
	}

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

	static TArray<UObject*> Heroes(UObject* CombatSystem)
	{
		TArray<UObject*> Result;
		FArrayProperty* Property = CombatSystem ? CastField<FArrayProperty>(KeepersReflection::FindMember(CombatSystem->GetClass(), TEXT("UnitsInCombat"))) : nullptr;
		FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
		if (!Inner)
		{
			return Result;
		}
		FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(CombatSystem));
		for (int32 I = 0; I < Helper.Num(); ++I)
		{
			UObject* Unit = Inner->GetObjectPropertyValue(Helper.GetRawPtr(I));
			int32 Team = -1;
			if (Unit && KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team) && Team == 0)
			{
				Result.Add(Unit);
			}
		}
		return Result;
	}
}

namespace KeepersUI
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

UTexture2D* KeepersCards::Icon(const TCHAR* Name)
{
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Art/Textures/SpellIcons/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

UTexture2D* KeepersCards::Card(const TCHAR* Name)
{
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Keepers/Icons/T_Card_%s.T_Card_%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

UTexture2D* KeepersCards::RelicIcon(FName Id)
{
	static const TMap<FName, const TCHAR*> Icons =
	{
		{ TEXT("StormFeather"), TEXT("Icon_WindGust") },
		{ TEXT("AncestorStone"), TEXT("Icon_RaiseRock") },
		{ TEXT("WorldTreeRoot"), TEXT("Icon_Roots") },
		{ TEXT("RangerBoots"), TEXT("Move") },
		{ TEXT("ScaleMail"), TEXT("Armor") },
		{ TEXT("ShepherdBell"), TEXT("Bell") },
		{ TEXT("SaltyBiscuit"), TEXT("Icon_SlimeBall") },
		{ TEXT("OakHeart"), TEXT("Icon_Sprout") },
		{ TEXT("WarDrum"), TEXT("Drum") },
		{ TEXT("VeteranCloak"), TEXT("Cloak") },
		{ TEXT("HawkFeather"), TEXT("Feather") },
		{ TEXT("DruidAcorn"), TEXT("Acorn") },
		{ TEXT("CampEmber"), TEXT("Ember") },
		{ TEXT("LuckyCoin"), TEXT("Coin") },
		{ TEXT("GloryBanner"), TEXT("Banner") },
		{ TEXT("SwiftCharm"), TEXT("Charm") },
	};
	// Relics of the abilities wear the abilities' pictures; the rest have their own medallions.
	const TCHAR* const* Name = Icons.Find(Id);
	if (!Name)
	{
		return Card(TEXT("Relic"));
	}
	return FCString::Strncmp(*Name, TEXT("Icon_"), 5) == 0 ? Icon(*Name) : Card(*Name);
}

UTexture2D* KeepersCards::SpellIcon(FName Spell)
{
	const FString Name = Spell.ToString();
	if (UTexture2D* Medallion = Card(*Name))
	{
		return Medallion;
	}
	return Icon(Spell == TEXT("Gust") ? TEXT("Icon_WindGust") : *(TEXT("Icon_") + Name));
}

UTexture2D* KeepersCards::Portrait(FName Hero)
{
	const FString Name = Hero.ToString();
	return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Keepers/Portraits/T_Portrait_%s.T_Portrait_%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void UKeepersOptionButton::Setup(const FText& Title, const FText& Description, FLinearColor Color, TFunction<void()> InOnPicked, bool bEnabled, float Width)
{
	TitleText = Title;
	DescriptionText = Description;
	ButtonColor = Color;
	OnPicked = MoveTemp(InOnPicked);
	bIsEnabled = bEnabled;
	CardWidth = Width;
}

TSharedRef<SWidget> UKeepersOptionButton::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(CardWidth);

		Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		Style.Normal = KeepersUI::Rounded(ButtonColor);
		Style.Hovered = KeepersUI::Rounded(ButtonColor * 1.35f);
		Style.Pressed = KeepersUI::Rounded(ButtonColor * 0.8f);
		// Locked cards keep a muted version of their colour, so the map still reads at a glance.
		FLinearColor Muted = ButtonColor * 0.45f;
		Muted.A = 0.85f;
		Style.Disabled = KeepersUI::Rounded(Muted);
		Style.NormalPadding = FMargin(12.f, 10.f);
		Style.PressedPadding = FMargin(12.f, 11.f, 12.f, 9.f);
		Button->SetStyle(Style);
		Button->SetIsEnabled(bIsEnabled);
		Button->OnClicked.AddDynamic(this, &UKeepersOptionButton::HandleClicked);

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (Image)
		{
			// A card: the picture on a dark plate, then the title and the text, all centred.
			Size->SetMinDesiredHeight(290.f);
			UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Plate->SetBrush(KeepersUI::Rounded(FLinearColor(0.f, 0.f, 0.f, 0.35f), 14.f));
			Plate->SetPadding(FMargin(14.f));
			Plate->SetHorizontalAlignment(HAlign_Center);
			UImage* Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Picture->SetBrushFromTexture(Image);
			USizeBox* PictureSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			PictureSize->SetWidthOverride(112.f);
			PictureSize->SetHeightOverride(112.f);
			PictureSize->AddChild(Picture);
			Plate->SetContent(PictureSize);
			if (UVerticalBoxSlot* PlateSlot = Lines->AddChildToVerticalBox(Plate))
			{
				PlateSlot->SetHorizontalAlignment(HAlign_Center);
				PlateSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 12.f));
			}
		}
		UTextBlock* Title = KeepersUI::MakeText(WidgetTree, TitleText, Image ? 20 : 17, bIsEnabled ? FLinearColor::White : FLinearColor(0.75f, 0.75f, 0.75f));
		Title->SetJustification(Image ? ETextJustify::Center : ETextJustify::Left);
		if (UVerticalBoxSlot* TitleSlot = Lines->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetHorizontalAlignment(Image ? HAlign_Center : HAlign_Fill);
		}
		if (!DescriptionText.IsEmpty())
		{
			UTextBlock* Description = KeepersUI::MakeText(WidgetTree, DescriptionText, Image ? 14 : 12, FLinearColor(0.88f, 0.88f, 0.88f), true);
			Description->SetJustification(Image ? ETextJustify::Center : ETextJustify::Left);
			if (UVerticalBoxSlot* DescriptionSlot = Lines->AddChildToVerticalBox(Description))
			{
				DescriptionSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			}
		}
		Button->AddChild(Lines);
		Size->AddChild(Button);
		WidgetTree->RootWidget = Size;
	}
	return Super::RebuildWidget();
}

void UKeepersOptionButton::HandleClicked()
{
	if (OnPicked)
	{
		UKeepersJuiceSubsystem::Sound(this, TEXT("Card"));
		// The choice is made: its panel closes (the callback may open another one, or a level).
		if (UKeepersChoiceWidget* Panel = GetTypedOuter<UKeepersChoiceWidget>())
		{
			Panel->RemoveFromParent();
		}
		// The callback may destroy this widget (opening a level), so copy it first.
		TFunction<void()> Callback = OnPicked;
		Callback();
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersChoiceWidget::Setup(const FText& Title, const FText& Subtitle, TArray<FOption> Options)
{
	TitleText = Title;
	SubtitleText = Subtitle;
	PendingOptions = MoveTemp(Options);
}

void UKeepersChoiceWidget::TestPick(int32 Index)
{
	if (Cards.IsValidIndex(Index) && Cards[Index])
	{
		Cards[Index]->HandleClicked();
	}
}

TSharedRef<SWidget> UKeepersChoiceWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Dim, click-blocking backdrop.
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Backdrop->SetHorizontalAlignment(HAlign_Center);
		Backdrop->SetVerticalAlignment(VAlign_Center);

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrush(KeepersUI::Rounded(KeepersUI::PanelColor, 16.f));
		Panel->SetPadding(FMargin(32.f, 24.f));

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (HeaderImage)
		{
			UImage* Header = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Header->SetBrushFromTexture(HeaderImage);
			USizeBox* HeaderSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			HeaderSize->SetWidthOverride(120.f);
			HeaderSize->SetHeightOverride(120.f);
			HeaderSize->AddChild(Header);
			if (UVerticalBoxSlot* HeaderSlot = Column->AddChildToVerticalBox(HeaderSize))
			{
				HeaderSlot->SetHorizontalAlignment(HAlign_Center);
				HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			}
		}
		UTextBlock* Heading = KeepersUI::MakeText(WidgetTree, TitleText, 28, KeepersUI::Gold);
		Heading->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* HeadingSlot = Column->AddChildToVerticalBox(Heading))
		{
			HeadingSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (!SubtitleText.IsEmpty())
		{
			USizeBox* Wrap = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Wrap->SetMaxDesiredWidth(720.f);
			Wrap->AddChild(KeepersUI::MakeText(WidgetTree, SubtitleText, 15, FLinearColor::White, true));
			if (UVerticalBoxSlot* SubtitleSlot = Column->AddChildToVerticalBox(Wrap))
			{
				SubtitleSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
				SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
			}
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (FOption& Option : PendingOptions)
		{
			UKeepersOptionButton* Card = CreateWidget<UKeepersOptionButton>(this, UKeepersOptionButton::StaticClass());
			Card->Setup(Option.Title, Option.Description, Option.Color, MoveTemp(Option.OnPicked), true, Option.Image ? 230.f : 260.f);
			Card->SetImage(Option.Image);
			Cards.Add(Card);
			if (UHorizontalBoxSlot* CardSlot = Row->AddChildToHorizontalBox(Card))
			{
				CardSlot->SetPadding(FMargin(8.f, 0.f));
			}
		}
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 20.f, 0.f, 0.f));
			RowSlot->SetHorizontalAlignment(HAlign_Center);
		}

		Panel->SetContent(Column);
		Backdrop->SetContent(Panel);
		WidgetTree->RootWidget = Backdrop;
	}
	return Super::RebuildWidget();
}

// ---------------------------------------------------------------------------------------------------------------------
// The region map: a board of medallions joined by trails, the squad's token travelling between them, the squad,
// relics and stones on the left, a description of the node under the mouse at the bottom of the board.

namespace KeepersMap
{
	/** The board in the map's own space, as fractions of the screen. */
	static const FVector2D BoardMin(0.235f, 0.13f);
	static const FVector2D BoardMax(0.975f, 0.84f);

	static const TCHAR* NodeIcon(EKeepersNodeType Type)
	{
		switch (Type)
		{
		case EKeepersNodeType::Battle: return TEXT("Fight");
		case EKeepersNodeType::Elite: return TEXT("Glory");
		case EKeepersNodeType::Ruins: return TEXT("Relic");
		case EKeepersNodeType::Camp: return TEXT("Ember");
		case EKeepersNodeType::Boss: return TEXT("Rooster");
		case EKeepersNodeType::Event: return TEXT("Unknown");
		}
		return TEXT("Unknown");
	}

	static FString ObjectiveName(uint8 Objective)
	{
		switch (static_cast<EKeepersObjectiveKind>(Objective))
		{
		case EKeepersObjectiveKind::Reach: return TEXT("маяк");
		case EKeepersObjectiveKind::Leader: return TEXT("вожак");
		case EKeepersObjectiveKind::Survive: return TEXT("осада");
		case EKeepersObjectiveKind::Totem: return TEXT("тотем");
		default: return FString();
		}
	}

	static FString ObjectiveLine(uint8 Objective)
	{
		switch (static_cast<EKeepersObjectiveKind>(Objective))
		{
		case EKeepersObjectiveKind::Reach: return TEXT("Задание: довести героя до маяка на той стороне поля.");
		case EKeepersObjectiveKind::Leader: return TEXT("Задание: свалить вожака — самого крепкого врага.");
		case EKeepersObjectiveKind::Survive: return TEXT("Задание: продержаться несколько ходов.");
		case EKeepersObjectiveKind::Totem: return TEXT("Задание: разрушить тотем кур, пока он лечит врагов.");
		default: return TEXT("Задание: победить всех врагов.");
		}
	}

	static FString NodeLine(const UKeepersCampaignSubsystem* Campaign, EKeepersNodeType Type)
	{
		switch (Type)
		{
		case EKeepersNodeType::Battle: return FString::Printf(TEXT("Бой с врагами региона. Награда: %d золота и рост героя."), UKeepersCampaignSubsystem::GoldFor(Type));
		case EKeepersNodeType::Elite: return FString::Printf(TEXT("Опасный бой: элита региона. Награда: %d золота, рост героя, реликвия."), UKeepersCampaignSubsystem::GoldFor(Type));
		case EKeepersNodeType::Ruins: return TEXT("Древние руины: одна реликвия из двух на выбор.");
		case EKeepersNodeType::Camp: return TEXT("Привал у костра: кузница, торговец и лазарет для раненых.");
		case EKeepersNodeType::Event: return TEXT("Встреча в пути: выбор и его последствия. Кто знает, чем кончится.");
		case EKeepersNodeType::Boss: return FString::Printf(TEXT("%s стережёт Камень земель. Победа освобождает регион."), *Campaign->GetRegionDef().BossName.ToString());
		}
		return FString();
	}
}

void UKeepersMapNodeWidget::Setup(UKeepersCampaignMapWidget* InMap, int32 InIndex, UTexture2D* Icon, const FLinearColor& InColor, const FText& InLabel, EState InState, float InSize)
{
	Map = InMap;
	Index = InIndex;
	IconTexture = Icon;
	Color = InColor;
	Label = InLabel;
	State = InState;
	Size = InSize;
}

TSharedRef<SWidget> UKeepersMapNodeWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		// A glow behind open nodes; a ring in the node's colour; the icon; a tick on visited ones.
		Glow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Glow->SetBrush(KeepersStyle::Panel(FLinearColor(1.f, 0.78f, 0.3f, 0.35f), Size * 0.5f, FLinearColor::Transparent, 0.f));
		Glow->SetVisibility(State == EState::Open ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		UOverlaySlot* GlowSlot = Stack->AddChildToOverlay(Glow);
		GlowSlot->SetHorizontalAlignment(HAlign_Fill);
		GlowSlot->SetVerticalAlignment(VAlign_Fill);
		const bool bDim = State == EState::Locked;
		const FLinearColor Edge = State == EState::Open ? KeepersStyle::Gold : State == EState::Visited ? FLinearColor(0.55f, 0.5f, 0.4f) : FLinearColor(0.3f, 0.3f, 0.3f);
		UBorder* Ring = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Ring->SetBrush(KeepersStyle::Panel(bDim ? FLinearColor(0.12f, 0.12f, 0.13f, 0.9f) : FLinearColor(Color.R, Color.G, Color.B, 1.f), Size * 0.5f, Edge, State == EState::Open ? 4.f : 2.f));
		Ring->SetPadding(FMargin(Size * 0.05f));
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetBrushFromTexture(IconTexture);
		Icon->SetRenderOpacity(bDim ? 0.45f : State == EState::Visited ? 0.6f : 1.f);
		Ring->SetContent(Icon);
		UOverlaySlot* RingSlot = Stack->AddChildToOverlay(Ring);
		RingSlot->SetPadding(FMargin(Size * 0.04f));
		RingSlot->SetHorizontalAlignment(HAlign_Fill);
		RingSlot->SetVerticalAlignment(VAlign_Fill);
		if (State == EState::Visited)
		{
			UTextBlock* Tick = KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("✓")), KeepersStyle::Heading(FMath::RoundToInt(Size * 0.45f)), KeepersStyle::Good);
			UOverlaySlot* TickSlot = Stack->AddChildToOverlay(Tick);
			TickSlot->SetHorizontalAlignment(HAlign_Right);
			TickSlot->SetVerticalAlignment(VAlign_Bottom);
		}
		Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Clear;
		Clear.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		Clear.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
		Clear.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		Clear.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
		Button->SetStyle(Clear);
		Button->OnClicked.AddDynamic(this, &UKeepersMapNodeWidget::HandleClicked);
		Button->OnHovered.AddDynamic(this, &UKeepersMapNodeWidget::HandleHovered);
		Button->OnUnhovered.AddDynamic(this, &UKeepersMapNodeWidget::HandleUnhovered);
		UOverlaySlot* ButtonSlot = Stack->AddChildToOverlay(Button);
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		Box->AddChild(Stack);
		Column->AddChildToVerticalBox(Box)->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Caption = KeepersStyle::MakeText(WidgetTree, Label, KeepersStyle::Heading(18), bDim ? KeepersStyle::TextDim : KeepersStyle::Text);
		Caption->SetJustification(ETextJustify::Center);
		Caption->SetShadowOffset(FVector2D(1.f, 1.f));
		Caption->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Column->AddChildToVerticalBox(Caption)->SetHorizontalAlignment(HAlign_Center);
		WidgetTree->RootWidget = Column;
	}
	return Super::RebuildWidget();
}

void UKeepersMapNodeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	// Open nodes breathe; the one under the mouse grows.
	const float Pulse = State == EState::Open ? 1.f + 0.05f * FMath::Sin(Time * 3.f + Index) : 1.f;
	Hover = FMath::FInterpTo(Hover, bHovered && State != EState::Locked ? 1.f : 0.f, InDeltaTime, 12.f);
	SetRenderScale(FVector2D(Pulse + 0.12f * Hover));
	if (Glow)
	{
		Glow->SetRenderOpacity(0.55f + 0.45f * FMath::Sin(Time * 3.f + Index));
	}
}

void UKeepersMapNodeWidget::HandleClicked()
{
	if (Map.IsValid() && State == EState::Open)
	{
		UKeepersJuiceSubsystem::Sound(this, TEXT("Card"), 0.7f);
		Map->OnNodePicked(Index);
	}
}

void UKeepersMapNodeWidget::HandleHovered()
{
	bHovered = true;
	if (Map.IsValid())
	{
		Map->ShowTip(Index);
	}
}

void UKeepersMapNodeWidget::HandleUnhovered()
{
	bHovered = false;
}

// ---------------------------------------------------------------------------------------------------------------------

FVector2D UKeepersCampaignMapWidget::NodeAnchor(int32 NodeIndex) const
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Campaign->GetNodes().IsValidIndex(NodeIndex))
	{
		// Before the first step the squad waits at the left edge of the board.
		return FVector2D(0.03f, 0.5f);
	}
	int32 Columns = 1;
	for (const FKeepersMapNode& Node : Campaign->GetNodes())
	{
		Columns = FMath::Max(Columns, Node.Column + 1);
	}
	const FKeepersMapNode& Node = Campaign->GetNodes()[NodeIndex];
	return FVector2D(0.1f + 0.82f * Node.Column / FMath::Max(Columns - 1, 1), 0.04f + 0.9f * Node.Height);
}

FVector2D UKeepersCampaignMapWidget::BoardPoint(const FVector2D& Anchor, const FVector2D& Size) const
{
	const FVector2D Min = KeepersMap::BoardMin * Size;
	const FVector2D Max = KeepersMap::BoardMax * Size;
	return Min + Anchor * (Max - Min);
}

TSharedRef<SWidget> UKeepersCampaignMapWidget::RebuildWidget()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (WidgetTree && !WidgetTree->RootWidget && Campaign)
	{
		SetIsFocusable(true);
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;
		auto Place = [this](UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets)
		{
			UCanvasPanelSlot* WidgetSlot = Canvas->AddChildToCanvas(Widget);
			WidgetSlot->SetAnchors(Anchors);
			WidgetSlot->SetOffsets(Offsets);
			return WidgetSlot;
		};

		// The shade over the island and the board are painted under the trails (NativePaint).
		BoardBrush = KeepersStyle::Panel(FLinearColor(0.03f, 0.04f, 0.05f, 0.42f), 22.f, FLinearColor(1.f, 0.78f, 0.3f, 0.35f), 2.f);

		// Top bar: the region, its story line, the way back to the menu.
		const FKeepersRegion& Region = Campaign->GetRegionDef();
		UBorder* TopBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		TopBar->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, FLinearColor::Transparent));
		TopBar->SetPadding(FMargin(36.f, 8.f, 24.f, 8.f));
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Titles = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Titles->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, Region.Numeral, KeepersStyle::Heading(14), KeepersStyle::TextDim));
		Titles->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, Region.Name, KeepersStyle::Heading(40), KeepersStyle::Gold));
		TitleRow->AddChildToHorizontalBox(Titles)->SetVerticalAlignment(VAlign_Center);
		USizeBox* IntroSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IntroSize->SetMaxDesiredWidth(900.f);
		IntroSize->AddChild(KeepersStyle::MakeText(WidgetTree, Region.Intro, KeepersStyle::Body(15), KeepersStyle::Text, true));
		UHorizontalBoxSlot* IntroSlot = TitleRow->AddChildToHorizontalBox(IntroSize);
		IntroSlot->SetPadding(FMargin(28.f, 0.f, 0.f, 0.f));
		IntroSlot->SetVerticalAlignment(VAlign_Center);
		IntroSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UButton* Menu = KeepersStyle::MakeButton(WidgetTree, FText::FromString(TEXT("В меню")), KeepersStyle::EButton::Secondary, 18);
		Menu->OnClicked.AddDynamic(this, &UKeepersCampaignMapWidget::HandleMenuClicked);
		TitleRow->AddChildToHorizontalBox(Menu)->SetVerticalAlignment(VAlign_Center);
		TopBar->SetContent(TitleRow);
		Place(TopBar, FAnchors(0.f, 0.f, 1.f, 0.f), FMargin(0.f, 0.f, 0.f, 92.f));

		BoardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		Place(BoardCanvas, FAnchors(KeepersMap::BoardMin.X, KeepersMap::BoardMin.Y, KeepersMap::BoardMax.X, KeepersMap::BoardMax.Y), FMargin(0.f));

		const TArray<int32> Available = Campaign->GetAvailableNodes();
		NodeWidgets.Reset();
		for (int32 I = 0; I < Campaign->GetNodes().Num(); ++I)
		{
			const FKeepersMapNode& Node = Campaign->GetNodes()[I];
			const bool bVisited = Campaign->GetVisitedNodes().Contains(I);
			const bool bOpen = Available.Contains(I);
			const FString Goal = KeepersMap::ObjectiveName(Node.Objective);
			const FText Caption = Node.Type == EKeepersNodeType::Boss ? Region.BossName
				: FText::FromString(KeepersUI::NodeName(Node.Type).ToString() + (Goal.IsEmpty() ? FString() : TEXT(" · ") + Goal));
			UKeepersMapNodeWidget* Medallion = CreateWidget<UKeepersMapNodeWidget>(this, UKeepersMapNodeWidget::StaticClass());
			Medallion->Setup(this, I, KeepersCards::Card(KeepersMap::NodeIcon(Node.Type)), KeepersUI::NodeColor(Node.Type), Caption,
				bVisited ? UKeepersMapNodeWidget::EState::Visited : bOpen ? UKeepersMapNodeWidget::EState::Open : UKeepersMapNodeWidget::EState::Locked,
				Node.Type == EKeepersNodeType::Boss ? 132.f : 92.f);
			UCanvasPanelSlot* NodeSlot = BoardCanvas->AddChildToCanvas(Medallion);
			const FVector2D Anchor = NodeAnchor(I);
			NodeSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			// The medallion's centre sits on the anchor; the caption hangs below.
			NodeSlot->SetAlignment(FVector2D(0.5f, Node.Type == EKeepersNodeType::Boss ? 0.42f : 0.38f));
			NodeSlot->SetAutoSize(true);
			NodeWidgets.Add(Medallion);
		}

		// The squad's token: the first hero's portrait on a gold disc, where the squad stands now.
		UBorder* TokenRing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		TokenRing->SetBrush(KeepersStyle::Panel(KeepersStyle::Gold, 30.f, FLinearColor(0.2f, 0.12f, 0.04f), 3.f));
		TokenRing->SetPadding(FMargin(4.f));
		USizeBox* TokenSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TokenSize->SetWidthOverride(58.f);
		TokenSize->SetHeightOverride(58.f);
		UImage* TokenFace = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
		const TArray<FName> Squad = Party ? Party->GetParty() : TArray<FName>();
		if (Squad.Num() > 0)
		{
			TokenFace->SetBrushFromTexture(KeepersCards::Portrait(Squad[0]));
		}
		TokenRing->SetContent(TokenFace);
		TokenSize->AddChild(TokenRing);
		Token = TokenSize;
		UCanvasPanelSlot* TokenSlot = BoardCanvas->AddChildToCanvas(Token);
		TokenSlot->SetAlignment(FVector2D(0.5f, 1.15f));
		TokenSlot->SetAutoSize(true);
		TokenAnchor = NodeAnchor(Campaign->GetCurrentNode());
		TokenSlot->SetAnchors(FAnchors(TokenAnchor.X, TokenAnchor.Y));

		// The description of the node under the mouse, at the foot of the board.
		UBorder* Tip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Tip->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 12.f));
		Tip->SetPadding(FMargin(18.f, 10.f));
		UVerticalBox* TipColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TipTitle = KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Куда дальше?")), KeepersStyle::Heading(22), KeepersStyle::Gold);
		TipColumn->AddChildToVerticalBox(TipTitle);
		TipBody = KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Наведите на точку пути, чтобы узнать, что там. Открыты точки с золотым кольцом.")),
			KeepersStyle::Body(15), KeepersStyle::Text, true);
		TipColumn->AddChildToVerticalBox(TipBody);
		Tip->SetContent(TipColumn);
		UCanvasPanelSlot* TipSlot = Place(Tip, FAnchors(KeepersMap::BoardMin.X, 1.f, KeepersMap::BoardMax.X, 1.f), FMargin(0.f, -140.f, 0.f, 110.f));
		TipSlot->SetAutoSize(false);

		BuildSidebar();
	}
	return Super::RebuildWidget();
}

void UKeepersCampaignMapWidget::BuildSidebar()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (!Campaign || !Party)
	{
		return;
	}
	UBorder* Side = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Side->SetBrush(KeepersStyle::Panel(FLinearColor(0.02f, 0.02f, 0.03f, 0.78f), 18.f, FLinearColor(1.f, 0.78f, 0.3f, 0.3f), 1.5f));
	Side->SetPadding(FMargin(16.f, 14.f));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	Column->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Отряд")), KeepersStyle::Heading(24), KeepersStyle::Gold))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	for (const FName& Hero : Party->GetParty())
	{
		const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* FaceSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		FaceSize->SetWidthOverride(58.f);
		FaceSize->SetHeightOverride(58.f);
		UImage* Face = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Face->SetBrushFromTexture(KeepersCards::Portrait(Hero));
		const bool bWounded = Campaign->IsWounded(Hero);
		Face->SetColorAndOpacity(bWounded ? FLinearColor(1.f, 0.55f, 0.55f) : FLinearColor::White);
		FaceSize->AddChild(Face);
		Row->AddChildToHorizontalBox(FaceSize);
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Lines->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree,
			FText::FromString(FString::Printf(TEXT("%s · ур. %d"), Def ? *Def->Name.ToString() : *Hero.ToString(), Campaign->GetHeroLevel(Hero))), KeepersStyle::Heading(17)));
		FString Status;
		if (bWounded)
		{
			FName Base;
			FString Recruit;
			Status = Campaign->GetRecruit(Hero, Base, Recruit) ? FString::Printf(TEXT("ранен · вместо него %s"), *Recruit) : FString(TEXT("ранен · пропустит бой"));
		}
		else
		{
			const int32 Learned = Campaign->GetLearnedSpells(Hero).Num();
			Status = FString::Printf(TEXT("+%d HP · +%d движ. · +%d ОД%s"), Campaign->GetGrowth(Hero, EKeepersGrowth::Hp) * 5,
				Campaign->GetGrowth(Hero, EKeepersGrowth::Move), Campaign->GetGrowth(Hero, EKeepersGrowth::Action),
				Learned > 0 ? *FString::Printf(TEXT(" · умений: %d"), Learned) : TEXT(""));
		}
		Lines->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(Status), KeepersStyle::Body(12), bWounded ? FLinearColor(1.f, 0.5f, 0.45f) : KeepersStyle::TextDim, true));
		USizeBox* LinesSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LinesSize->SetWidthOverride(250.f);
		LinesSize->AddChild(Lines);
		Row->AddChildToHorizontalBox(LinesSize)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));
	}

	// Gold and the Stones of the Lands found so far.
	GoldText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(22), KeepersStyle::Gold);
	Column->AddChildToVerticalBox(GoldText)->SetPadding(FMargin(0.f, 14.f, 0.f, 4.f));
	const int32 Stones = Campaign->GetRegion();
	FString StoneLine = TEXT("Камни земель: ");
	for (int32 I = 0; I < KeepersRegions::All().Num(); ++I)
	{
		StoneLine += I < Stones ? TEXT("◆") : TEXT("◇");
	}
	Column->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(StoneLine), KeepersStyle::Body(16, true), FLinearColor(0.6f, 0.9f, 1.f)));
	Column->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree,
		FText::FromString(FString::Printf(TEXT("Мир с курами: %d из %d"), Campaign->GetPeace(), KeepersStory::PeaceNeeded)), KeepersStyle::Body(13), KeepersStyle::TextDim));

	// Relics: their pictures; the name on hover.
	Column->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Реликвии")), KeepersStyle::Heading(20), KeepersStyle::Gold))->SetPadding(FMargin(0.f, 14.f, 0.f, 6.f));
	UWrapBox* Relics = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	Relics->SetInnerSlotPadding(FVector2D(6.f, 6.f));
	Relics->SetWrapSize(300.f);
	Relics->SetExplicitWrapSize(true);
	for (const FName& Id : Campaign->GetArtifacts())
	{
		const FKeepersArtifact* Artifact = UKeepersCampaignSubsystem::FindArtifact(Id);
		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconSize->SetWidthOverride(46.f);
		IconSize->SetHeightOverride(46.f);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetBrushFromTexture(KeepersCards::RelicIcon(Id));
		if (Artifact)
		{
			Icon->SetToolTipText(FText::FromString(Artifact->Name.ToString() + TEXT(": ") + Artifact->Description.ToString()));
		}
		IconSize->AddChild(Icon);
		Relics->AddChildToWrapBox(IconSize);
	}
	if (Campaign->GetArtifacts().Num() == 0)
	{
		USizeBox* NoneSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NoneSize->SetWidthOverride(300.f);
		NoneSize->AddChild(KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Пока нет: их дают победы, руины и торговец.")), KeepersStyle::Body(13), KeepersStyle::TextDim, true));
		Relics->AddChildToWrapBox(NoneSize);
	}
	Column->AddChildToVerticalBox(Relics);
	Side->SetContent(Column);
	UCanvasPanelSlot* SideSlot = Canvas->AddChildToCanvas(Side);
	SideSlot->SetAnchors(FAnchors(0.f, KeepersMap::BoardMin.Y, 0.f, 1.f));
	SideSlot->SetOffsets(FMargin(18.f, 0.f, 360.f, 30.f));
}

void UKeepersCampaignMapWidget::ShowTip(int32 NodeIndex)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Campaign->GetNodes().IsValidIndex(NodeIndex) || !TipTitle)
	{
		return;
	}
	const FKeepersMapNode& Node = Campaign->GetNodes()[NodeIndex];
	const bool bBattle = Node.Type == EKeepersNodeType::Battle || Node.Type == EKeepersNodeType::Elite;
	const bool bVisited = Campaign->GetVisitedNodes().Contains(NodeIndex);
	const bool bOpen = Campaign->GetAvailableNodes().Contains(NodeIndex);
	TipTitle->SetText(Node.Type == EKeepersNodeType::Boss ? Campaign->GetRegionDef().BossName : KeepersUI::NodeName(Node.Type));
	FString Body = KeepersMap::NodeLine(Campaign, Node.Type);
	if (bBattle)
	{
		Body += TEXT(" ") + KeepersMap::ObjectiveLine(Node.Objective);
	}
	Body += bVisited ? TEXT("  Пройдено.") : bOpen ? TEXT("  Щёлкните, чтобы отправиться.") : TEXT("  Пока недоступно: сначала соседние точки.");
	TipBody->SetText(FText::FromString(Body));
}

int32 UKeepersCampaignMapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// The trails go under the medallions: drawn first on the board's layer, the children above.
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const int32 TrailLayer = LayerId + 3;
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None, FLinearColor(0.01f, 0.02f, 0.04f, 0.45f));
	const FVector2D BoardTopLeft = KeepersMap::BoardMin * Size;
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry((KeepersMap::BoardMax - KeepersMap::BoardMin) * Size, FSlateLayoutTransform(BoardTopLeft)),
		&BoardBrush, ESlateDrawEffect::None, BoardBrush.TintColor.GetSpecifiedColor());
	if (Campaign)
	{
		const TArray<FKeepersMapNode>& Nodes = Campaign->GetNodes();
		const TArray<int32> Available = Campaign->GetAvailableNodes();
		auto Trail = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Width, bool bDashed, int32 Seed)
		{
			// A gentle curve: a quadratic bend off the straight line, cut into dashes.
			const FVector2D Normal(-(B - A).Y, (B - A).X);
			const FVector2D Bend = (A + B) * 0.5f + Normal.GetSafeNormal() * ((Seed % 2 == 0) ? 18.f : -18.f);
			const float Length = (B - A).Size();
			const int32 Steps = FMath::Max(8, FMath::RoundToInt(Length / 14.f));
			for (int32 I = 0; I < Steps; ++I)
			{
				if (bDashed && I % 2 == 1)
				{
					continue;
				}
				auto At = [&](float T) { return FMath::Lerp(FMath::Lerp(A, Bend, T), FMath::Lerp(Bend, B, T), T); };
				const TArray<FVector2D> Points = { At(float(I) / Steps), At(float(I + 1) / Steps) };
				FSlateDrawElement::MakeLines(OutDrawElements, TrailLayer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Width);
			}
		};
		for (int32 From = 0; From < Nodes.Num(); ++From)
		{
			for (int32 To : Nodes[From].Next)
			{
				const FVector2D A = BoardPoint(NodeAnchor(From), Size);
				const FVector2D B = BoardPoint(NodeAnchor(To), Size);
				const bool bTaken = Campaign->GetVisitedNodes().Contains(From) && Campaign->GetVisitedNodes().Contains(To);
				const bool bNext = From == Campaign->GetCurrentNode() && Available.Contains(To);
				Trail(A, B, bTaken ? FLinearColor(1.f, 0.8f, 0.3f, 0.95f) : bNext ? FLinearColor(1.f, 0.92f, 0.7f, 0.85f) : FLinearColor(0.9f, 0.85f, 0.75f, 0.28f),
					bTaken ? 5.f : bNext ? 4.f : 3.f, !bTaken, From + To);
			}
		}
		// From the start to the first column.
		if (Campaign->GetCurrentNode() == INDEX_NONE)
		{
			for (int32 To : Available)
			{
				Trail(BoardPoint(NodeAnchor(INDEX_NONE), Size), BoardPoint(NodeAnchor(To), Size), FLinearColor(1.f, 0.92f, 0.7f, 0.85f), 4.f, true, To);
			}
		}
	}
	// The board and the trails under the medallions: paint the children above the trail layer.
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, TrailLayer + 1, InWidgetStyle, bParentEnabled);
}

FString UKeepersCampaignMapWidget::StatusLine() const
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	return Campaign ? FString::Printf(TEXT("Золото: %d"), Campaign->GetGold()) : FString();
}

void UKeepersCampaignMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Gold changes in events and at the camp without a new map.
	if (GoldText)
	{
		const FString Line = StatusLine();
		if (GoldText->GetText().ToString() != Line)
		{
			GoldText->SetText(FText::FromString(Line));
		}
	}
	// The token walks the trail to the picked node, then the node opens.
	if (Travelling != INDEX_NONE && Token)
	{
		TravelTime += InDeltaTime;
		const float Alpha = FMath::Clamp(TravelTime / 1.1f, 0.f, 1.f);
		const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
		const FVector2D Target = NodeAnchor(Travelling);
		const FVector2D Position = FMath::Lerp(TravelFrom, Target, Smooth) + FVector2D(0.f, -0.03f * FMath::Abs(FMath::Sin(Smooth * PI * 4.f)));
		if (UCanvasPanelSlot* TokenSlot = Cast<UCanvasPanelSlot>(Token->Slot))
		{
			TokenSlot->SetAnchors(FAnchors(Position.X, Position.Y));
		}
		if (Alpha >= 1.f)
		{
			const int32 Arrived = Travelling;
			Travelling = INDEX_NONE;
			Arrive(Arrived);
		}
	}
}

void UKeepersCampaignMapWidget::HandleMenuClicked()
{
	if (UKeepersCampaignSubsystem* Subsystem = UKeepersCampaignSubsystem::Get(this))
	{
		Subsystem->ReturnToMainMenu(this);
	}
}

void UKeepersCampaignMapWidget::OnNodePicked(int32 NodeIndex)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Campaign->GetAvailableNodes().Contains(NodeIndex) || Travelling != INDEX_NONE)
	{
		return;
	}
	// The squad walks there first.
	Travelling = NodeIndex;
	TravelTime = 0.f;
	TravelFrom = TokenAnchor;
	TokenAnchor = NodeAnchor(NodeIndex);
	ShowTip(NodeIndex);
	UKeepersJuiceSubsystem::Sound(this, TEXT("Leaves"), 0.6f);
}

void UKeepersCampaignMapWidget::Arrive(int32 NodeIndex)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Campaign->GetNodes().IsValidIndex(NodeIndex))
	{
		return;
	}
	const EKeepersNodeType Type = Campaign->GetNodes()[NodeIndex].Type;
	Campaign->EnterNode(NodeIndex);
	if (Type == EKeepersNodeType::Ruins)
	{
		ShowArtifactChoice(FText::FromString(TEXT("Древние руины")),
			FText::FromString(TEXT("Под мхом блестят две реликвии. Унести можно только одну — вторую стережёт очень принципиальная улитка.")), 2);
	}
	else if (Type == EKeepersNodeType::Event)
	{
		ShowEvent(NodeIndex);
	}
	else if (Type == EKeepersNodeType::Camp)
	{
		if (UKeepersCampaignWorldSubsystem* World = GetWorld() ? GetWorld()->GetSubsystem<UKeepersCampaignWorldSubsystem>() : nullptr)
		{
			World->ShowCamp();
		}
	}
}

void UKeepersCampaignMapWidget::ShowArtifactChoice(const FText& Title, const FText& Subtitle, int32 Count)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	UKeepersCampaignWorldSubsystem* World = GetWorld() ? GetWorld()->GetSubsystem<UKeepersCampaignWorldSubsystem>() : nullptr;
	if (!Campaign || !World)
	{
		return;
	}
	TArray<UKeepersChoiceWidget::FOption> Options;
	TWeakObjectPtr<UKeepersCampaignSubsystem> WeakCampaign(Campaign);
	for (const FName& Id : Campaign->RollArtifacts(Count))
	{
		const FKeepersArtifact* Artifact = UKeepersCampaignSubsystem::FindArtifact(Id);
		UKeepersChoiceWidget::FOption Option;
		Option.Title = Artifact->Name;
		Option.Description = Artifact->Description;
		Option.Color = FLinearColor(0.3f, 0.25f, 0.1f);
		Option.Image = KeepersCards::RelicIcon(Id);
		Option.OnPicked = [WeakCampaign, Id]()
		{
			if (WeakCampaign.IsValid())
			{
				WeakCampaign->AddArtifact(Id);
				WeakCampaign->CompleteCurrentNode();
			}
		};
		Options.Add(MoveTemp(Option));
	}
	UKeepersChoiceWidget::FOption Leave;
	Leave.Title = FText::FromString(Options.Num() > 0 ? TEXT("Идти дальше") : TEXT("Здесь пусто. Дальше"));
	Leave.Color = FLinearColor(0.2f, 0.2f, 0.25f);
	Leave.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->CompleteCurrentNode(); } };
	Options.Add(MoveTemp(Leave));
	World->ShowChoice(Title, Subtitle, MoveTemp(Options));
}

// ---------------------------------------------------------------------------------------------------------------------

TStatId UKeepersCampaignWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersCampaignWorldSubsystem, STATGROUP_Tickables);
}

bool UKeepersCampaignWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersCampaignWorldSubsystem::SetUiInput(UUserWidget* Focus)
{
	if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
	{
		Player->SetShowMouseCursor(true);
		// Game and UI, never UI-only: the input mode lives in the game viewport and survives level loads,
		// so UI-only would leave the next battle without camera or clicks.
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		if (Focus)
		{
			Mode.SetWidgetToFocus(Focus->TakeWidget());
		}
		Player->SetInputMode(Mode);
	}
}

void UKeepersCampaignWorldSubsystem::ShowCamp()
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || Camp)
	{
		return;
	}
	Camp = CreateWidget<UKeepersCampWidget>(Player, UKeepersCampWidget::StaticClass());
	TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
	Camp->OnLeave = [WeakThis]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(WeakThis->GetWorld()))
		{
			Campaign->CompleteCurrentNode();
		}
	};
	Camp->AddToViewport(50);
	SetUiInput(Camp);
}

void UKeepersCampaignWorldSubsystem::TestCamp(const UObject* WorldContextObject, const FString& Action, int32 Index)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UKeepersCampaignWorldSubsystem* Self = World ? World->GetSubsystem<UKeepersCampaignWorldSubsystem>() : nullptr;
	if (!Self)
	{
		return;
	}
	if (Action == TEXT("open"))
	{
		Self->ShowCamp();
	}
	else if (Self->Camp && (Action == TEXT("armor") || Action == TEXT("boots")))
	{
		Self->Camp->TestUpgrade(Index, Action == TEXT("boots"));
	}
	else if (Self->Camp && (Action == TEXT("heal") || Action == TEXT("recruit")))
	{
		Self->Camp->TestInfirmary(Index, Action == TEXT("recruit"));
	}
	else if (Self->Camp && Action == TEXT("buy"))
	{
		Self->Camp->TestBuy(Index);
	}
	else if (Self->Camp && Action == TEXT("leave") && Self->Camp->OnLeave)
	{
		Self->Camp->OnLeave();
	}
	else if (Action == TEXT("pick") || Action == TEXT("event"))
	{
		for (TObjectIterator<UKeepersCampaignMapWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport())
			{
				if (Action == TEXT("pick"))
				{
					It->OnNodePicked(Index);
				}
				else
				{
					It->ShowEvent(Index);
				}
				break;
			}
		}
	}
	else if (Action == TEXT("levelup"))
	{
		Self->ShowLevelUp();
	}
	else if (Action == TEXT("dialog"))
	{
		Self->ShowDialog(KeepersStory::RegionIntro(Index));
	}
	else if (Action == TEXT("finale"))
	{
		TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakSelf(Self);
		Self->ShowDialog(KeepersStory::Finale(), [WeakSelf]() { if (WeakSelf.IsValid()) { WeakSelf->ShowEndings(); } });
	}
	else if (Action == TEXT("next"))
	{
		for (TObjectIterator<UKeepersDialogWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport())
			{
				It->Advance();
				break;
			}
		}
	}
	else if (Action == TEXT("close"))
	{
		for (TObjectIterator<UKeepersChoiceWidget> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				It->RemoveFromParent();
			}
		}
	}
	else if (Action == TEXT("choose"))
	{
		for (TObjectIterator<UKeepersChoiceWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport())
			{
				It->TestPick(Index);
				break;
			}
		}
	}
}

UKeepersChoiceWidget* UKeepersCampaignWorldSubsystem::ShowChoice(const FText& Title, const FText& Subtitle, TArray<UKeepersChoiceWidget::FOption> Options, UTexture2D* Header)
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	UKeepersChoiceWidget* Choice = Player ? CreateWidget<UKeepersChoiceWidget>(Player, UKeepersChoiceWidget::StaticClass()) : nullptr;
	if (Choice)
	{
		Choice->Setup(Title, Subtitle, MoveTemp(Options));
		Choice->SetHeaderImage(Header);
		Choice->AddToViewport(50);
		SetUiInput(Choice);
	}
	return Choice;
}

void UKeepersCampaignWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	if (!bMapShown && World->GetOutermost()->GetName().Contains(TEXT("L_CampaignMap")))
	{
		ShowMap();
	}
	if (!bObjectiveChecked)
	{
		EnsureObjective();
	}
	TickBattle();
	TickMercenaries();
}

void UKeepersCampaignWorldSubsystem::TickMercenaries()
{
	if (bMercenariesDone)
	{
		return;
	}
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	UObject* CombatSystem = Campaign ? KeepersUI::FindCombatSystem(GetWorld()) : nullptr;
	bool bInCombat = false;
	if (!CombatSystem || !KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat) || !bInCombat)
	{
		return;
	}

	// Who comes: by the node of the campaign, or the quick battle with mercenaries.
	TArray<uint8> Band;
	constexpr uint8 Bandit = 6;
	constexpr uint8 Archer = 7;
	constexpr uint8 Cavalry = 12;
	constexpr uint8 Shaman = 15;
	if (Campaign->NextBand.Num() > 0)
	{
		Band = MoveTemp(Campaign->NextBand);
		Campaign->NextBand.Reset();
		bQuickMercenaries = !Campaign->IsInBattleNode();
	}
	else if (Campaign->bMercenaryQuickBattle)
	{
		Campaign->bMercenaryQuickBattle = false;
		bQuickMercenaries = true;
		Band = { Bandit, Cavalry, Archer, Shaman };
	}
	else if (Campaign->IsInBattleNode())
	{
		// Every region has its own band; the first region eases in with the level's own enemies.
		const FKeepersMapNode& Node = Campaign->GetNodes()[Campaign->GetCurrentNode()];
		const FKeepersRegion& Region = Campaign->GetRegionDef();
		if (Node.Type == EKeepersNodeType::Elite)
		{
			Band = Region.EliteBand;
		}
		else if (Node.Type == EKeepersNodeType::Boss)
		{
			Band = Region.BossBand;
		}
		else if (Campaign->GetRegion() > 0 || Node.Column >= 2)
		{
			Band = Region.Band;
		}
	}
	if (Band.Num() == 0)
	{
		// Campaign battles decide once; other battles keep listening for the mercenaries' flag.
		bMercenariesDone = Campaign->IsInBattleNode();
		return;
	}
	bMercenariesDone = true;

	// They stand next to the enemies already on the field.
	TArray<UObject*> Enemies;
	UObject* AnyUnit = nullptr;
	FArrayProperty* Property = CastField<FArrayProperty>(KeepersReflection::FindMember(CombatSystem->GetClass(), TEXT("UnitsInCombat")));
	FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
	if (Inner)
	{
		FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(CombatSystem));
		for (int32 I = 0; I < Helper.Num(); ++I)
		{
			UObject* Unit = Inner->GetObjectPropertyValue(Helper.GetRawPtr(I));
			int32 Team = 0;
			if (Unit && KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team))
			{
				AnyUnit = AnyUnit ? AnyUnit : Unit;
				if (Team != 0)
				{
					Enemies.Add(Unit);
				}
			}
		}
	}
	UKeepersCreatureSubsystem* Creatures = GetWorld()->GetSubsystem<UKeepersCreatureSubsystem>();
	if (Enemies.Num() == 0 || !AnyUnit || !Creatures)
	{
		return;
	}
	// Same size as the heroes on this map.
	float Scale = 1.f;
	for (UObject* Hero : KeepersUI::Heroes(CombatSystem))
	{
		if (const AActor* HeroActor = Cast<AActor>(Hero))
		{
			Scale = HeroActor->GetActorScale3D().X;
			break;
		}
	}
	for (int32 I = 0; I < Band.Num(); ++I)
	{
		UObject* Near = Enemies[I % Enemies.Num()];
		FIntPoint Index;
		int32 Team = 1;
		KeepersReflection::GetIntPoint(Near->GetClass(), Near, TEXT("Index"), Index);
		KeepersReflection::GetInt(Near->GetClass(), Near, TEXT("TeamIndex"), Team);
		Creatures->SpawnUnitNear(CombatSystem, AnyUnit->GetClass(), Band[I], Index, Team, 0, Scale, TEXT("Mercenary"));
	}
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Mercenaries joined: %d"), Band.Num());
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
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const bool bBoss = Campaign && Campaign->IsActive() && Campaign->IsInBattleNode() && Campaign->IsBossNode(Campaign->GetCurrentNode());
	for (TActorIterator<AKeepersBattleObjective> It(GetWorld()); It; ++It)
	{
		if (!bBoss)
		{
			return; // the level has its own goal
		}
		It->Destroy(); // the region's boss is the goal
	}
	if (bBoss)
	{
		SpawnBoss(CombatSystem);
		return;
	}

	// Campaign battles get a goal picked by their node; others defeat every enemy.
	int32 Variant = 0;
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

void UKeepersCampaignWorldSubsystem::SpawnBoss(UObject* CombatSystem)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const FKeepersRegion& Region = Campaign->GetRegionDef();
	AKeepersBattleObjective* Objective = GetWorld()->SpawnActorDeferred<AKeepersBattleObjective>(AKeepersBattleObjective::StaticClass(), FTransform::Identity);
	if (!Objective)
	{
		return;
	}
	Objective->bDefeatLeader = true;
	Objective->BossName = Region.BossName;
	Objective->bTotem = Region.bBossTotem;
	if (!Region.bBossTotem)
	{
		// The boss steps out next to the enemy furthest from the heroes: bigger and tougher than its kind.
		TArray<UObject*> Heroes = KeepersUI::Heroes(CombatSystem);
		UKeepersCreatureSubsystem* Creatures = GetWorld()->GetSubsystem<UKeepersCreatureSubsystem>();
		UObject* Far = nullptr;
		float FarDistance = -1.f;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			int32 Team = 0;
			if (It->GetClass()->GetName() != TEXT("BP_Unit_C") || !KeepersReflection::GetInt(It->GetClass(), *It, TEXT("TeamIndex"), Team) || Team == 0)
			{
				continue;
			}
			float Nearest = MAX_flt;
			for (UObject* Hero : Heroes)
			{
				if (const AActor* HeroActor = Cast<AActor>(Hero))
				{
					Nearest = FMath::Min(Nearest, FVector::Dist2D(HeroActor->GetActorLocation(), It->GetActorLocation()));
				}
			}
			if (Nearest > FarDistance)
			{
				FarDistance = Nearest;
				Far = *It;
			}
		}
		const AActor* HeroActor = Heroes.Num() > 0 ? Cast<AActor>(Heroes[0]) : nullptr;
		if (Far && HeroActor && Creatures)
		{
			FIntPoint Index;
			int32 Team = 1;
			KeepersReflection::GetIntPoint(Far->GetClass(), Far, TEXT("Index"), Index);
			KeepersReflection::GetInt(Far->GetClass(), Far, TEXT("TeamIndex"), Team);
			if (AActor* Boss = Creatures->SpawnUnitNear(CombatSystem, HeroActor->GetClass(), Region.BossType, Index, Team, 0, HeroActor->GetActorScale3D().X * Region.BossScale, TEXT("Boss")))
			{
				KeepersReflection::AddUnitStat(Boss, TEXT("HP_Max"), Region.BossHp);
				KeepersReflection::AddUnitStat(Boss, TEXT("HP_Current"), Region.BossHp);
				Objective->Leader = Boss;
				UKeepersBarkLibrary::Bark(Boss, FText::FromString(FString::Printf(TEXT("%s не отдаст камень!"), *Region.BossName.ToString())));
			}
		}
	}
	Objective->FinishSpawning(FTransform::Identity);
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Boss of region %d: %s"), Campaign->GetRegion(), *Region.BossName.ToString());
}

TArray<FIntPoint> UKeepersCampaignWorldSubsystem::FreeCells(UObject* Grid, float MinDistance) const
{
	TArray<FIntPoint> Cells;
	FKeepersLevelGrid Level;
	if (!Grid || !KeepersLevelKit::PrepareGrid(GetWorld(), Level))
	{
		return Cells;
	}
	TArray<FVector> Units;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("BP_Unit_C"))
		{
			Units.Add(It->GetActorLocation());
		}
	}
	for (const FIntPoint& Index : KeepersLevelKit::AllIndexes(Level))
	{
		uint8 Type = 0;
		FVector Location;
		if (UKeepersTerrainSubsystem::TileType(Grid, Index, Type) && Type == 1 && !UKeepersTerrainSubsystem::UnitOnTile(Grid, Index)
			&& UKeepersTerrainSubsystem::TileLocation(Grid, Index, Location)
			&& !Units.ContainsByPredicate([&](const FVector& Unit) { return FVector::Dist2D(Unit, Location) < MinDistance; }))
		{
			Cells.Add(Index);
		}
	}
	return Cells;
}

void UKeepersCampaignWorldSubsystem::ApplyRegionField(UObject* CombatSystem)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>();
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	if (!Campaign || !IsNodeLevel() || !Terrain || !Grid)
	{
		return;
	}
	const FKeepersRegion& Region = Campaign->GetRegionDef();
	FRandomStream Random(Campaign->GetSeed() + Campaign->GetCurrentNode() * 131);
	TArray<FIntPoint> Cells = FreeCells(Grid, 150.f);
	auto Take = [&]() { return Cells.Num() > 0 ? Cells[Random.RandRange(0, Cells.Num() - 1)] : FIntPoint(-1, -1); };
	for (int32 I = 0; I < Region.Forests && Cells.Num() > 0; ++I)
	{
		const FIntPoint Cell = Take();
		Cells.Remove(Cell);
		Terrain->GrowForest(Grid, Cell);
	}
	// Barricades keep a cell away from everyone, so nobody is walled in from the start.
	TArray<FIntPoint> Open = FreeCells(Grid, 330.f);
	for (int32 I = 0; I < Region.Barricades && Open.Num() > 0; ++I)
	{
		const FIntPoint Cell = Open[Random.RandRange(0, Open.Num() - 1)];
		Open.Remove(Cell);
		Terrain->RaiseRock(Grid, Cell, 1);
	}
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Region field: %d forest, %d barricades"), Region.Forests, Region.Barricades);
}

void UKeepersCampaignWorldSubsystem::ApplyRegionRound(UObject* CombatSystem)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>();
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	if (!Campaign || !IsNodeLevel() || !Terrain || !Grid || Round < 2)
	{
		return;
	}
	const FKeepersRegion& Region = Campaign->GetRegionDef();
	// The Heart: without the stones the land will not keep still.
	if (Region.bShifting && Round % 2 == 0)
	{
		TArray<FIntPoint> Cells = FreeCells(Grid, 150.f);
		if (Cells.Num() >= 2)
		{
			const FIntPoint Rock = Cells[FMath::RandRange(0, Cells.Num() - 1)];
			Cells.Remove(Rock);
			Terrain->RaiseRock(Grid, Rock, 1);
			Terrain->GrowForest(Grid, Cells[FMath::RandRange(0, Cells.Num() - 1)]);
		}
		const TArray<FIntPoint> Rocks = Terrain->GetRocks();
		if (Rocks.Num() > 2)
		{
			Terrain->CrumbleRock(Grid, Rocks[FMath::RandRange(0, Rocks.Num() - 1)]);
		}
		UE_LOG(LogKeepersCampaignUI, Log, TEXT("The land shifts"));
	}
	// The forest boss: trees close in on a hero.
	if (Region.bBossGrowsForest && Campaign->IsBossNode(Campaign->GetCurrentNode()))
	{
		TArray<UObject*> Heroes = KeepersUI::Heroes(CombatSystem);
		if (Heroes.Num() > 0)
		{
			UObject* Hero = Heroes[FMath::RandRange(0, Heroes.Num() - 1)];
			FIntPoint Index;
			KeepersReflection::GetIntPoint(Hero->GetClass(), Hero, TEXT("Index"), Index);
			FVector Center;
			if (UKeepersTerrainSubsystem::TileLocation(Grid, Index, Center))
			{
				for (const FIntPoint& Cell : FreeCells(Grid, 1.f))
				{
					FVector Location;
					if (UKeepersTerrainSubsystem::TileLocation(Grid, Cell, Location) && FVector::Dist2D(Location, Center) < 320.f)
					{
						Terrain->GrowForest(Grid, Cell);
						break;
					}
				}
			}
		}
	}
}

UKeepersDialogWidget* UKeepersCampaignWorldSubsystem::ShowDialog(TArray<FKeepersLine> Lines, TFunction<void()> OnFinished)
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	UKeepersDialogWidget* Dialog = Player ? CreateWidget<UKeepersDialogWidget>(Player, UKeepersDialogWidget::StaticClass()) : nullptr;
	if (Dialog)
	{
		Dialog->Setup(MoveTemp(Lines), MoveTemp(OnFinished));
		Dialog->AddToViewport(60);
		SetUiInput(Dialog);
	}
	return Dialog;
}

void UKeepersCampaignWorldSubsystem::ShowEndings()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (!Campaign)
	{
		return;
	}
	using KeepersStory::EEnding;
	TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
	TWeakObjectPtr<UKeepersCampaignSubsystem> WeakCampaign(Campaign);
	struct FEndingCard
	{
		EEnding Ending;
		const TCHAR* Description;
		const TCHAR* Icon;
		FLinearColor Color;
	};
	static const FEndingCard Cards[] =
	{
		{ EEnding::Return, TEXT("Камни вернутся в святилище, земли успокоятся. Так велит долг Хранителей."), TEXT("Leaf"), FLinearColor(0.2f, 0.4f, 0.3f) },
		{ EEnding::Keep, TEXT("Власть над землями — в ваших руках. Что может пойти не так?"), TEXT("Relic"), FLinearColor(0.35f, 0.12f, 0.3f) },
		{ EEnding::Pact, TEXT("Поделить камни с курами и жить в мире."), TEXT("Charm"), FLinearColor(0.5f, 0.38f, 0.12f) },
	};
	TArray<UKeepersChoiceWidget::FOption> Options;
	for (const FEndingCard& Card : Cards)
	{
		const bool bOpen = Card.Ending != EEnding::Pact || Campaign->GetPeace() >= KeepersStory::PeaceNeeded;
		UKeepersChoiceWidget::FOption Option;
		Option.Title = KeepersStory::EndingName(Card.Ending);
		Option.Description = FText::FromString(bOpen ? FString(Card.Description)
			: FString::Printf(TEXT("Куры вам не верят: нужно %d мирных решения, а было %d."), KeepersStory::PeaceNeeded, Campaign->GetPeace()));
		Option.Color = bOpen ? Card.Color : FLinearColor(0.18f, 0.18f, 0.2f);
		Option.Image = KeepersCards::Card(Card.Icon);
		const EEnding Ending = Card.Ending;
		Option.OnPicked = [WeakThis, WeakCampaign, Ending, bOpen]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			if (!bOpen)
			{
				WeakThis->ShowEndings();
				return;
			}
			UE_LOG(LogKeepersCampaignUI, Log, TEXT("Ending: %s"), *KeepersStory::EndingName(Ending).ToString());
			WeakThis->ShowDialog(KeepersStory::Ending(Ending), [WeakCampaign]()
			{
				if (WeakCampaign.IsValid())
				{
					WeakCampaign->CompleteCurrentNode();
				}
			});
		};
		Options.Add(MoveTemp(Option));
	}
	ShowChoice(FText::FromString(TEXT("Камни земель")), FText::FromString(TEXT("Шесть камней в руках Хранителей. Как с ними поступить?")), MoveTemp(Options));
}

void UKeepersCampaignWorldSubsystem::ShowMap()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!Campaign || !Player)
	{
		return;
	}
	bMapShown = true;
	Campaign->EnsureLoaded();

	// The green hexagon island from the main menu as the region backdrop.
	FLatentActionInfo Latent;
	Latent.UUID = 1;
	UGameplayStatics::LoadStreamLevel(GetWorld(), TEXT("L_Hexagon"), true, false, Latent);
	Screen = CreateWidget<UKeepersCampaignMapWidget>(Player, UKeepersCampaignMapWidget::StaticClass());
	Screen->AddToViewport(10);
	SetUiInput(Screen);
	// A new region begins with its scene.
	if (Campaign->bRegionIntroPending)
	{
		Campaign->bRegionIntroPending = false;
		TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
		ShowDialog(KeepersStory::RegionIntro(Campaign->GetRegion()), [WeakThis]()
		{
			if (WeakThis.IsValid() && WeakThis->Screen)
			{
				WeakThis->SetUiInput(WeakThis->Screen);
			}
		});
	}
}

void UKeepersCampaignWorldSubsystem::TickBattle()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (Campaign && !Campaign->IsInBattleNode())
	{
		TickQuickBattle();
		return;
	}
	if (!Campaign || bBattleResolved)
	{
		return;
	}
	UObject* CombatSystem = KeepersUI::FindCombatSystem(GetWorld());
	if (!CombatSystem)
	{
		return;
	}
	bool bInCombat = false;
	bool bFinishing = false;
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);

	if (bInCombat && !bArtifactsApplied)
	{
		bArtifactsApplied = true;
		// Give the battle full game input, whatever screen came before it.
		if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
		{
			FInputModeGameAndUI Mode;
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Mode.SetHideCursorDuringCapture(false);
			Player->SetInputMode(Mode);
			Player->SetShowMouseCursor(true);
		}
		ApplyArtifactsAtBattleStart(CombatSystem);
	}
	if (bInCombat && !bFinishing)
	{
		if (const UObject* Current = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit")))
		{
			int32 Team = INDEX_NONE;
			KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), Team);
			if (Team == 0 && LastTeam != 0)
			{
				++Round;
				OnRoundStarted(CombatSystem);
				bSaveDue = true;
			}
			LastTeam = Team;
		}
		// The field is ready once the mercenaries have come and the objective is set.
		if (bMercenariesDone && bObjectiveChecked)
		{
			if (!bResumeChecked)
			{
				bResumeChecked = true;
				if (!ResumeBattle(CombatSystem))
				{
					ApplyRegionField(CombatSystem);
				}
			}
			if (bSaveDue)
			{
				bSaveDue = false;
				ApplyRegionRound(CombatSystem);
				SaveBattle(CombatSystem);
			}
		}
	}
	if (bFinishing)
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
		if (ResultDelay > 0.8f)
		{
			bBattleResolved = true;
			int32 Winner = INDEX_NONE;
			KeepersReflection::GetInt(CombatSystem->GetClass(), CombatSystem, TEXT("TeamIndexThatWonLastCombat"), Winner);
			OnBattleFinished(CombatSystem, Winner);
		}
	}
}

void UKeepersCampaignWorldSubsystem::TickQuickBattle()
{
	UObject* CombatSystem = bQuickResolved ? nullptr : KeepersUI::FindCombatSystem(GetWorld());
	const UKeepersTutorialSubsystem* Tutorial = GetWorld()->GetSubsystem<UKeepersTutorialSubsystem>();
	if (!CombatSystem || (Tutorial && Tutorial->IsTutorialActive()))
	{
		return;
	}
	bool bFinishing = false;
	KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);
	if (!bFinishing)
	{
		return;
	}
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->GetClass()->GetName() == TEXT("W_CombatEndScreen_C"))
		{
			It->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	QuickDelay += GetWorld()->GetDeltaSeconds();
	if (QuickDelay < 0.8f)
	{
		return;
	}
	bQuickResolved = true;
	int32 Winner = INDEX_NONE;
	KeepersReflection::GetInt(CombatSystem->GetClass(), CombatSystem, TEXT("TeamIndexThatWonLastCombat"), Winner);
	if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
	{
		Player->SetShowMouseCursor(true);
	}

	// Play the same battle again (with its objective and mercenaries), or go back to the menu.
	const FName Level(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));
	const EKeepersObjectiveKind Objective = AKeepersBattleObjective::LastGiven;
	const bool bMercenaries = bQuickMercenaries;
	TWeakObjectPtr<UWorld> WeakWorld(GetWorld());
	TArray<UKeepersChoiceWidget::FOption> Options;
	UKeepersChoiceWidget::FOption Again;
	Again.Title = FText::FromString(TEXT("Ещё раз"));
	Again.Description = FText::FromString(TEXT("Тот же бой с начала."));
	Again.Color = FLinearColor(0.2f, 0.35f, 0.25f);
	Again.Image = KeepersCards::Card(TEXT("Fight"));
	Again.OnPicked = [WeakWorld, Level, Objective, bMercenaries]()
	{
		if (UWorld* World = WeakWorld.Get())
		{
			AKeepersBattleObjective::RequestForNextBattle(Objective);
			UKeepersCampaignSubsystem::SetMercenaryBattle(World, bMercenaries);
			KeepersLoading::OpenLevel(World, Level);
		}
	};
	Options.Add(MoveTemp(Again));
	UKeepersChoiceWidget::FOption Menu;
	Menu.Title = FText::FromString(TEXT("В меню"));
	Menu.Description = FText::FromString(TEXT("К костру."));
	Menu.Color = FLinearColor(0.3f, 0.25f, 0.1f);
	Menu.Image = KeepersCards::Card(TEXT("Leave"));
	Menu.OnPicked = [WeakWorld]()
	{
		if (UWorld* World = WeakWorld.Get())
		{
			KeepersLoading::OpenLevel(World, UKeepersCampaignSubsystem::MainMenuLevel);
		}
	};
	Options.Add(MoveTemp(Menu));
	UKeepersJuiceSubsystem::Sound(GetWorld(), Winner == 0 ? TEXT("Victory") : TEXT("Defeat"));
	if (Winner == 0)
	{
		ShowChoice(FText::FromString(TEXT("Победа!")), FText::FromString(TEXT("Поле за Хранителями. Куры будут рассказывать об этом бое ещё долго.")), MoveTemp(Options));
	}
	else
	{
		ShowChoice(FText::FromString(TEXT("Поражение")), FText::FromString(KeepersUI::DefeatLine(CombatSystem)), MoveTemp(Options));
	}
}

void UKeepersCampaignWorldSubsystem::ApplyArtifactsAtBattleStart(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	for (UObject* Hero : KeepersUI::Heroes(CombatSystem))
	{
		// Gear from the camp's smith.
		if (const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Hero)))
		{
			const int32 ArmorHp = Campaign->GetGearRank(Def->Id, EKeepersGear::Armor) * UKeepersCampaignSubsystem::ArmorHpPerRank();
			if (ArmorHp > 0)
			{
				KeepersReflection::AddUnitStat(Hero, TEXT("HP_Max"), ArmorHp);
				KeepersReflection::FCall Heal(Hero, TEXT("ModifyCurrentHp"));
				Heal.Set<int32>(TEXT("Modifier"), ArmorHp);
				Heal.Set<bool>(TEXT("ShowNumber"), false);
				Heal.Call();
			}
			const int32 Boots = Campaign->GetGearRank(Def->Id, EKeepersGear::Boots);
			if (Boots > 0)
			{
				KeepersReflection::AddUnitStat(Hero, TEXT("MP_Max"), Boots);
				KeepersReflection::AddUnitStat(Hero, TEXT("MP_Current"), Boots);
			}
		}
		// Relics for one hero or for all; a recruit has only the relics for everyone.
		const bool bRecruit = UKeepersPartyBattleSubsystem::IsRecruit(Hero);
		const FKeepersHeroDef* Def = bRecruit ? nullptr : KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Hero));
		const FName Id = Def ? Def->Id : NAME_None;
		if (Def)
		{
			BattleHeroes.Add(Hero, Id);
		}
		auto AddHp = [Hero](int32 Amount)
		{
			if (Amount <= 0)
			{
				return;
			}
			KeepersReflection::AddUnitStat(Hero, TEXT("HP_Max"), Amount);
			KeepersReflection::FCall Heal(Hero, TEXT("ModifyCurrentHp"));
			Heal.Set<int32>(TEXT("Modifier"), Amount);
			Heal.Set<bool>(TEXT("ShowNumber"), false);
			Heal.Call();
		};
		auto AddStat = [Hero](const TCHAR* Max, const TCHAR* Current, int32 Amount)
		{
			if (Amount <= 0)
			{
				return;
			}
			KeepersReflection::AddUnitStat(Hero, Max, Amount);
			KeepersReflection::AddUnitStat(Hero, Current, Amount);
		};
		// Growth of the hero in this run.
		AddHp(Campaign->GetGrowth(Id, EKeepersGrowth::Hp) * 5);
		AddStat(TEXT("MP_Max"), TEXT("MP_Current"), Campaign->GetGrowth(Id, EKeepersGrowth::Move));
		AddStat(TEXT("AP_Max"), TEXT("AP_Current"), Campaign->GetGrowth(Id, EKeepersGrowth::Action));
		if (Campaign->HasArtifact(TEXT("WarDrum")))
		{
			AddStat(TEXT("AP_Max"), TEXT("AP_Current"), 1);
		}
		if (Campaign->HasArtifact(TEXT("VeteranCloak")) && Id == TEXT("Warrior"))
		{
			AddHp(10);
		}
		if (Campaign->HasArtifact(TEXT("HawkFeather")) && Id == TEXT("Ranger"))
		{
			AddStat(TEXT("AP_Max"), TEXT("AP_Current"), 1);
			AddStat(TEXT("MP_Max"), TEXT("MP_Current"), 1);
		}
		if (Campaign->HasArtifact(TEXT("DruidAcorn")) && Id == TEXT("Priest"))
		{
			AddHp(8);
		}
		if (Campaign->HasArtifact(TEXT("SwiftCharm")) && (Id == TEXT("Priest") || Id == TEXT("Warrior")))
		{
			AddStat(TEXT("MP_Max"), TEXT("MP_Current"), 1);
		}
		if (Campaign->HasArtifact(TEXT("RangerBoots")))
		{
			KeepersReflection::AddUnitStat(Hero, TEXT("MP_Max"), 1);
			KeepersReflection::AddUnitStat(Hero, TEXT("MP_Current"), 1);
		}
		if (Campaign->HasArtifact(TEXT("ScaleMail")))
		{
			KeepersReflection::AddUnitStat(Hero, TEXT("HP_Max"), 5);
			KeepersReflection::FCall Heal(Hero, TEXT("ModifyCurrentHp"));
			Heal.Set<int32>(TEXT("Modifier"), 5);
			Heal.Set<bool>(TEXT("ShowNumber"), false);
			Heal.Call();
		}
	}
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Artifacts applied: %d"), Campaign->GetArtifacts().Num());
}

void UKeepersCampaignWorldSubsystem::OnRoundStarted(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>();
	const bool bRoot = Campaign->HasArtifact(TEXT("WorldTreeRoot")) && Terrain;
	const bool bEmber = Campaign->HasArtifact(TEXT("CampEmber"));
	if (!bRoot && !bEmber)
	{
		return;
	}
	for (UObject* Hero : KeepersUI::Heroes(CombatSystem))
	{
		FIntPoint Index;
		const bool bInForest = bRoot && KeepersReflection::GetIntPoint(Hero->GetClass(), Hero, TEXT("Index"), Index) && Terrain->IsForest(Index);
		const int32 Amount = (bInForest ? 2 : 0) + (bEmber ? 1 : 0);
		int32 Hp = 0;
		int32 HpMax = 0;
		KeepersReflection::GetUnitStat(Hero, TEXT("HP_Current"), Hp);
		KeepersReflection::GetUnitStat(Hero, TEXT("HP_Max"), HpMax);
		if (Amount > 0 && Hp > 0 && Hp < HpMax)
		{
			KeepersReflection::FCall Heal(Hero, TEXT("ModifyCurrentHp"));
			Heal.Set<int32>(TEXT("Modifier"), FMath::Min(Amount, HpMax - Hp));
			Heal.Set<bool>(TEXT("ShowNumber"), true);
			Heal.Call();
		}
	}
}

bool UKeepersCampaignWorldSubsystem::IsNodeLevel() const
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (!Campaign || !Campaign->IsInBattleNode() || !Campaign->GetNodes().IsValidIndex(Campaign->GetCurrentNode()))
	{
		return false;
	}
	const FString Level = FPackageName::GetShortName(Campaign->GetNodes()[Campaign->GetCurrentNode()].Level.ToString());
	return FPackageName::GetShortName(GetWorld()->GetOutermost()->GetName()) == Level;
}

void UKeepersCampaignWorldSubsystem::SaveBattle(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (!Campaign || !IsNodeLevel())
	{
		return;
	}
	FKeepersBattleSnapshot Snapshot;
	Snapshot.Node = Campaign->GetCurrentNode();
	Snapshot.Round = Round;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() != TEXT("BP_Unit_C"))
		{
			continue;
		}
		bool bAlive = false;
		KeepersReflection::FCall IsAlive(*It, TEXT("IsAlive"));
		if (!IsAlive.Call() || !IsAlive.Get<bool>(TEXT("IsAlive"), bAlive) || !bAlive)
		{
			continue;
		}
		FKeepersUnitSnapshot& Unit = Snapshot.Units.AddDefaulted_GetRef();
		Unit.Name = It->GetName();
		KeepersReflection::GetIntPoint(It->GetClass(), *It, TEXT("Index"), Unit.Index);
		KeepersReflection::GetUnitStat(*It, TEXT("HP_Current"), Unit.Hp);
	}
	if (const UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>())
	{
		Snapshot.Rocks = Terrain->GetRocks();
		Snapshot.Forests = Terrain->GetForests();
		Snapshot.Ice = Terrain->GetIce();
	}
	Campaign->SaveBattle(Snapshot);
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Battle saved: round %d, %d units, %d rocks, %d forest"), Round, Snapshot.Units.Num(), Snapshot.Rocks.Num(), Snapshot.Forests.Num());
}

bool UKeepersCampaignWorldSubsystem::ResumeBattle(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const FKeepersBattleSnapshot* Saved = Campaign ? Campaign->GetBattleToResume() : nullptr;
	if (!Saved || !IsNodeLevel())
	{
		return false;
	}
	const FKeepersBattleSnapshot Snapshot = *Saved;
	Campaign->bResumeBattle = false;
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>();
	if (!Grid || !Terrain)
	{
		return false;
	}
	// The field first: rocks, forest and ice where they were.
	for (const FIntPoint& Index : Snapshot.Ice)
	{
		Terrain->FreezeWater(Grid, Index);
	}
	for (const FIntPoint& Index : Snapshot.Forests)
	{
		Terrain->GrowForest(Grid, Index);
	}
	TMap<FString, const FKeepersUnitSnapshot*> ByName;
	for (const FKeepersUnitSnapshot& Unit : Snapshot.Units)
	{
		ByName.Add(Unit.Name, &Unit);
	}
	// The fallen fall again; the living get their health.
	TArray<TPair<AActor*, FIntPoint>> Moves;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() != TEXT("BP_Unit_C"))
		{
			continue;
		}
		const FKeepersUnitSnapshot* const* Found = ByName.Find(It->GetName());
		if (!Found)
		{
			UKeepersTerrainSubsystem::DamageUnit(CombatSystem, *It, 9999);
			continue;
		}
		int32 Hp = 0;
		KeepersReflection::GetUnitStat(*It, TEXT("HP_Current"), Hp);
		if (Hp != (*Found)->Hp)
		{
			KeepersReflection::FCall Modify(*It, TEXT("ModifyCurrentHp"));
			Modify.Set<int32>(TEXT("Modifier"), (*Found)->Hp - Hp);
			Modify.Set<bool>(TEXT("ShowNumber"), false);
			Modify.Call();
		}
		FIntPoint Index;
		KeepersReflection::GetIntPoint(It->GetClass(), *It, TEXT("Index"), Index);
		if (Index != (*Found)->Index)
		{
			Moves.Emplace(*It, (*Found)->Index);
		}
	}
	// Moves onto free cells first, so that no unit lands where another still stands.
	for (int32 Pass = 0; Pass < 4 && Moves.Num() > 0; ++Pass)
	{
		for (int32 I = Moves.Num() - 1; I >= 0; --I)
		{
			if (UKeepersTerrainSubsystem::UnitOnTile(Grid, Moves[I].Value) && Pass < 3)
			{
				continue;
			}
			KeepersReflection::FCall Move(CombatSystem, TEXT("SetUnitIndexOnGrid"));
			Move.Set<UObject*>(TEXT("Unit"), Moves[I].Key);
			Move.Set<FIntPoint>(TEXT("Index"), Moves[I].Value);
			Move.Set<bool>(TEXT("Force"), true);
			Move.Set<bool>(TEXT("SnapLocation"), true);
			Move.Call();
			Moves.RemoveAt(I);
		}
	}
	for (const FIntPoint& Index : Snapshot.Rocks)
	{
		Terrain->RaiseRock(Grid, Index, 2);
	}
	Round = Snapshot.Round;
	if (AKeepersBattleObjective* Objective = AKeepersBattleObjective::Find(GetWorld()))
	{
		Objective->RestoreRound(Snapshot.Round);
	}
	if (UKeepersBattleHudSubsystem* Hud = GetWorld()->GetSubsystem<UKeepersBattleHudSubsystem>())
	{
		Hud->RestoreRound(Snapshot.Round);
	}
	bSaveDue = false;
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Battle resumed at round %d: %d units"), Snapshot.Round, Snapshot.Units.Num());
	return true;
}

void UKeepersCampaignWorldSubsystem::OnBattleFinished(UObject* CombatSystem, int32 Winner)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	TWeakObjectPtr<UKeepersCampaignSubsystem> WeakCampaign(Campaign);
	TArray<UKeepersChoiceWidget::FOption> Options;
	UKeepersJuiceSubsystem::Sound(GetWorld(), Winner == 0 ? TEXT("Victory") : TEXT("Defeat"));

	if (Winner == 0)
	{
		Campaign->MarkBattleWon();
	}
	if (Winner != 0)
	{
		UKeepersChoiceWidget::FOption Retry;
		Retry.Title = FText::FromString(TEXT("Попробовать ещё раз"));
		Retry.Color = FLinearColor(0.2f, 0.35f, 0.25f);
		Retry.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->RetryCurrentNode(); } };
		Options.Add(MoveTemp(Retry));
		UKeepersChoiceWidget::FOption GiveUp;
		GiveUp.Title = FText::FromString(TEXT("Сдаться"));
		GiveUp.Description = FText::FromString(TEXT("Кампания закончится."));
		GiveUp.Color = FLinearColor(0.35f, 0.1f, 0.1f);
		GiveUp.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->FailCampaign(); } };
		Options.Add(MoveTemp(GiveUp));
		ShowChoice(FText::FromString(TEXT("Хранители отступили...")), FText::FromString(KeepersUI::DefeatLine(CombatSystem)), MoveTemp(Options));
		return;
	}

	// The fallen are wounded and sit the next battle out; those who sat this one out are well again.
	TArray<FName> Fallen;
	for (const TPair<TWeakObjectPtr<UObject>, FName>& Entry : BattleHeroes)
	{
		int32 Hp = 0;
		if (!Entry.Key.IsValid() || (KeepersReflection::GetUnitStat(Entry.Key.Get(), TEXT("HP_Current"), Hp) && Hp <= 0))
		{
			Fallen.Add(Entry.Value);
		}
	}
	const UKeepersPartyBattleSubsystem* PartyBattle = GetWorld()->GetSubsystem<UKeepersPartyBattleSubsystem>();
	Campaign->ResolveWounds(PartyBattle ? PartyBattle->SatOut : TArray<FName>(), Fallen);

	if (Campaign->IsBossNode(Campaign->GetCurrentNode()))
	{
		const FKeepersRegion& Region = Campaign->GetRegionDef();
		const bool bFinal = Campaign->IsFinalRegion();
		// The last boss: the finale and the choice of the ending.
		if (bFinal)
		{
			TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
			ShowDialog(KeepersStory::Finale(), [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowEndings(); } });
			return;
		}
		UKeepersChoiceWidget::FOption Done;
		Done.Title = FText::FromString(bFinal ? FString(TEXT("В главное меню"))
			: FString::Printf(TEXT("В путь: %s"), *KeepersRegions::Get(Campaign->GetRegion() + 1).Name.ToString()));
		Done.Description = bFinal ? FText::GetEmpty() : KeepersRegions::Get(Campaign->GetRegion() + 1).Intro;
		Done.Color = FLinearColor(0.3f, 0.25f, 0.1f);
		Done.Image = KeepersCards::Card(TEXT("Leave"));
		Done.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->CompleteCurrentNode(); } };
		Options.Add(MoveTemp(Done));
		ShowChoice(FText::FromString(FString::Printf(TEXT("%s освобождён!"), *Region.Name.ToString())), Region.Liberated, MoveTemp(Options));
		return;
	}

	if (Fallen.Num() > 0)
	{
		FString Names;
		for (const FName& Hero : Fallen)
		{
			const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + (Def ? Def->Name.ToString() : Hero.ToString());
		}
		TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
		UKeepersChoiceWidget::FOption Next;
		Next.Title = FText::FromString(TEXT("Дальше"));
		Next.Description = FText::FromString(TEXT("На привале лекарь поставит на ноги, а за раненого может выйти новобранец."));
		Next.Color = FLinearColor(0.35f, 0.15f, 0.12f);
		Next.Image = KeepersCards::Card(TEXT("Hp"));
		Next.OnPicked = [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ShowLevelUp(); } };
		Options.Add(MoveTemp(Next));
		ShowChoice(FText::FromString(TEXT("Победа, но не без потерь")),
			FText::FromString(FString::Printf(TEXT("Ранены: %s. Следующий бой они пропустят."), *Names)), MoveTemp(Options));
		return;
	}
	ShowLevelUp();
}

void UKeepersCampaignWorldSubsystem::ShowLevelUp()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(GetWorld());
	const FName Hero = Campaign && Party ? Campaign->NextHeroToLevel(Party->GetParty()) : NAME_None;
	if (Hero.IsNone())
	{
		ShowVictoryRelics();
		return;
	}
	const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
	const FString Name = Def ? Def->Name.ToString() : Hero.ToString();
	UKeepersJuiceSubsystem::Sound(GetWorld(), TEXT("LevelUp"));
	TWeakObjectPtr<UKeepersCampaignWorldSubsystem> WeakThis(this);
	TWeakObjectPtr<UKeepersCampaignSubsystem> WeakCampaign(Campaign);
	struct FGrowthCard
	{
		EKeepersGrowth Growth;
		const TCHAR* Title;
		const TCHAR* Description;
		FLinearColor Color;
		const TCHAR* Icon;
	};
	static const FGrowthCard Cards[] =
	{
		{ EKeepersGrowth::Hp, TEXT("Закалка"), TEXT("+5 к здоровью"), FLinearColor(0.45f, 0.15f, 0.12f), TEXT("Hp") },
		{ EKeepersGrowth::Move, TEXT("Лёгкий шаг"), TEXT("+1 к очкам движения"), FLinearColor(0.15f, 0.35f, 0.5f), TEXT("Move") },
		{ EKeepersGrowth::Action, TEXT("Боевой опыт"), TEXT("+1 к очкам действия"), FLinearColor(0.4f, 0.32f, 0.1f), TEXT("Action") },
	};
	TArray<UKeepersChoiceWidget::FOption> Options;
	// Levels 3 and 5 teach an ability: one of the hero's learnable ones, or toughness instead.
	const int32 NextLevel = Campaign->GetHeroLevel(Hero) + 1;
	const TArray<FName> ToLearn = Campaign->GetSpellsToLearn(Hero);
	const bool bLearns = UKeepersCampaignSubsystem::IsSpellLevel(NextLevel) && ToLearn.Num() > 0;
	if (bLearns)
	{
		for (int32 I = 0; I < FMath::Min(ToLearn.Num(), 2); ++I)
		{
			const FName Spell = ToLearn[I];
			const FKeepersSpellInfo* Info = KeepersHeroes::FindSpell(Spell);
			UKeepersChoiceWidget::FOption Option;
			Option.Title = Info ? Info->Name : FText::FromName(Spell);
			Option.Description = Info ? FText::FromString(Info->Description.ToString() + TEXT("\n") + Info->Numbers.ToString()) : FText::GetEmpty();
			Option.Color = FLinearColor(0.32f, 0.2f, 0.45f);
			Option.Image = KeepersCards::SpellIcon(Spell);
			Option.OnPicked = [WeakThis, WeakCampaign, Hero, Spell]()
			{
				if (WeakCampaign.IsValid())
				{
					WeakCampaign->LearnSpell(Hero, Spell);
				}
				if (WeakThis.IsValid())
				{
					WeakThis->ShowVictoryRelics();
				}
			};
			Options.Add(MoveTemp(Option));
		}
	}
	for (const FGrowthCard& Card : Cards)
	{
		if (bLearns && Card.Growth != EKeepersGrowth::Hp)
		{
			continue;
		}
		UKeepersChoiceWidget::FOption Option;
		Option.Title = FText::FromString(Card.Title);
		Option.Description = FText::FromString(Card.Description);
		Option.Color = Card.Color;
		Option.Image = KeepersCards::Card(Card.Icon);
		const EKeepersGrowth Growth = Card.Growth;
		Option.OnPicked = [WeakThis, WeakCampaign, Hero, Growth]()
		{
			if (WeakCampaign.IsValid())
			{
				WeakCampaign->LevelUp(Hero, Growth);
			}
			if (WeakThis.IsValid())
			{
				WeakThis->ShowVictoryRelics();
			}
		};
		Options.Add(MoveTemp(Option));
	}
	const FString Grown = FString::Printf(TEXT("Сейчас: +%d здоровья, +%d движения, +%d действия."),
		Campaign->GetGrowth(Hero, EKeepersGrowth::Hp) * 5, Campaign->GetGrowth(Hero, EKeepersGrowth::Move), Campaign->GetGrowth(Hero, EKeepersGrowth::Action));
	ShowChoice(FText::FromString(FString::Printf(TEXT("%s — уровень %d%s"), *Name, NextLevel, bLearns ? TEXT(": новое умение") : TEXT(""))),
		FText::FromString(bLearns ? FString::Printf(TEXT("Опыт открывает новое умение. Какое возьмёт %s? %s"), *Name, *Grown)
			: FString::Printf(TEXT("Бой не прошёл даром. Чему %s научился? %s"), *Name, *Grown)),
		MoveTemp(Options), KeepersCards::Portrait(Hero));
}

void UKeepersCampaignWorldSubsystem::ShowVictoryRelics()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (!Campaign)
	{
		return;
	}
	TWeakObjectPtr<UKeepersCampaignSubsystem> WeakCampaign(Campaign);
	TArray<UKeepersChoiceWidget::FOption> Options;
	for (const FName& Id : Campaign->RollArtifacts(3))
	{
		const FKeepersArtifact* Artifact = UKeepersCampaignSubsystem::FindArtifact(Id);
		UKeepersChoiceWidget::FOption Option;
		Option.Title = Artifact->Name;
		Option.Description = Artifact->Description;
		Option.Color = FLinearColor(0.3f, 0.25f, 0.1f);
		Option.Image = KeepersCards::RelicIcon(Id);
		Option.OnPicked = [WeakCampaign, Id]()
		{
			if (WeakCampaign.IsValid())
			{
				WeakCampaign->AddArtifact(Id);
				WeakCampaign->CompleteCurrentNode();
			}
		};
		Options.Add(MoveTemp(Option));
	}
	if (Options.Num() == 0)
	{
		UKeepersChoiceWidget::FOption Next;
		Next.Title = FText::FromString(TEXT("Дальше"));
		Next.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->CompleteCurrentNode(); } };
		Options.Add(MoveTemp(Next));
	}
	const EKeepersNodeType Type = Campaign->GetNodes().IsValidIndex(Campaign->GetCurrentNode()) ? Campaign->GetNodes()[Campaign->GetCurrentNode()].Type : EKeepersNodeType::Battle;
	const int32 Gold = UKeepersCampaignSubsystem::GoldFor(Type == EKeepersNodeType::Event ? EKeepersNodeType::Battle : Type)
		+ (Campaign->HasArtifact(TEXT("LuckyCoin")) ? 10 : 0);
	ShowChoice(FText::FromString(TEXT("Победа!")), FText::FromString(FString::Printf(TEXT("Добыча: %d золота. Выберите награду."), Gold)), MoveTemp(Options));
}
