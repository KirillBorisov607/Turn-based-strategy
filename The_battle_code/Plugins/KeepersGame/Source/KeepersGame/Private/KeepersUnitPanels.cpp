#include "KeepersUnitPanels.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "KeepersBattleHud.h"
#include "KeepersBattleObjective.h"
#include "KeepersCampaign.h"
#include "KeepersHeroes.h"
#include "KeepersReflection.h"
#include "KeepersStyle.h"
#include "KeepersTerrainSubsystem.h"

namespace KeepersUnits
{
	static FText T(const FString& Text)
	{
		return FText::FromString(Text);
	}

	int32 TeamOf(const UObject* Unit)
	{
		int32 Team = 0;
		if (Unit)
		{
			KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		}
		return Team;
	}

	int32 Stat(UObject* Unit, const TCHAR* Name)
	{
		int32 Value = 0;
		KeepersReflection::GetUnitStat(Unit, Name, Value);
		return Value;
	}

	FText NameOf(UObject* Unit)
	{
		FProperty* Property = nullptr;
		void* Ptr = Unit ? KeepersReflection::FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitType"), &Property) : nullptr;
		const FByteProperty* Byte = CastField<FByteProperty>(Property);
		if (!Ptr || !Byte || !Byte->Enum)
		{
			return FText::GetEmpty();
		}
		const FString English = Byte->Enum->GetDisplayNameTextByValue(Byte->GetPropertyValue(Ptr)).ToString();
		const FText Local = UKeepersBattleHudSubsystem::LocalUnitName(English);
		return Local.IsEmpty() ? T(English) : Local;
	}

	UObject* PortraitOf(UObject* Unit)
	{
		if (!Unit)
		{
			return nullptr;
		}
		if (const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit)))
		{
			if (UKeepersHeroLookSubsystem* Looks = Unit->GetWorld() ? Unit->GetWorld()->GetSubsystem<UKeepersHeroLookSubsystem>() : nullptr)
			{
				return Looks->GetPortrait(Def->Id, TeamOf(Unit) != 0);
			}
		}
		// Others: UnitData.Assets.Icon.
		FProperty* DataProperty = nullptr;
		void* DataPtr = KeepersReflection::FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitData"), &DataProperty);
		const FStructProperty* DataStruct = CastField<FStructProperty>(DataProperty);
		FProperty* AssetsProperty = nullptr;
		void* AssetsPtr = DataStruct ? KeepersReflection::FindValuePtr(DataStruct->Struct, DataPtr, TEXT("Assets"), &AssetsProperty) : nullptr;
		const FStructProperty* AssetsStruct = CastField<FStructProperty>(AssetsProperty);
		FProperty* IconProperty = nullptr;
		void* IconPtr = AssetsStruct ? KeepersReflection::FindValuePtr(AssetsStruct->Struct, AssetsPtr, TEXT("Icon"), &IconProperty) : nullptr;
		if (const FObjectPropertyBase* Icon = CastField<FObjectPropertyBase>(IconProperty))
		{
			return Icon->GetObjectPropertyValue(IconPtr);
		}
		return nullptr;
	}

	static FLinearColor TeamColor(int32 Team)
	{
		return Team == 0 ? KeepersStyle::Ally : Team == 1 ? KeepersStyle::Enemy : Team == 2 ? FLinearColor(0.55f, 0.3f, 0.8f) : FLinearColor(0.85f, 0.7f, 0.2f);
	}

	static void FillPips(UWidgetTree* Tree, UHorizontalBox* Box, int32 Value, int32 Max, const FLinearColor& Color)
	{
		Box->ClearChildren();
		for (int32 I = 0; I < FMath::Max(Max, Value); ++I)
		{
			UBorder* Pip = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
			const bool bOn = I < Value;
			Pip->SetBrush(KeepersStyle::Panel(bOn ? Color : FLinearColor(1.f, 1.f, 1.f, 0.06f), 7.f, bOn ? FLinearColor(1.f, 1.f, 1.f, 0.5f) : FLinearColor(1.f, 1.f, 1.f, 0.2f), 1.f));
			USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Size->SetWidthOverride(14.f);
			Size->SetHeightOverride(14.f);
			Size->AddChild(Pip);
			Box->AddChildToHorizontalBox(Size)->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
		}
		Box->AddChildToHorizontalBox(KeepersStyle::MakeText(Tree, FText::AsNumber(Value), KeepersStyle::Heading(16)))->SetPadding(FMargin(4.f, -3.f, 0.f, 0.f));
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersUnitCard::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetPadding(FMargin(12.f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		USizeBox* PortraitSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PortraitSize->SetWidthOverride(112.f);
		PortraitSize->SetHeightOverride(112.f);
		PortraitSize->AddChild(Portrait);
		Row->AddChildToHorizontalBox(PortraitSize)->SetVerticalAlignment(VAlign_Center);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		NameText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(30));
		Column->AddChildToVerticalBox(NameText);
		RoleText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(12), KeepersStyle::TextDim);
		Column->AddChildToVerticalBox(RoleText)->SetPadding(FMargin(0.f, -2.f, 0.f, 6.f));

		// Health bar with numbers on it.
		UOverlay* Bar = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* BarBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BarBack->SetBrush(KeepersStyle::Panel(FLinearColor(0.f, 0.f, 0.f, 0.6f), 8.f, FLinearColor(1.f, 1.f, 1.f, 0.2f), 1.f));
		UOverlaySlot* BackSlot = Bar->AddChildToOverlay(BarBack);
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
		BackSlot->SetVerticalAlignment(VAlign_Fill);
		HpFillBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		HpFill = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		HpFill->AddChild(HpFillBar);
		UOverlaySlot* FillSlot = Bar->AddChildToOverlay(HpFill);
		FillSlot->SetHorizontalAlignment(HAlign_Left);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
		HpText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(18));
		HpText->SetShadowOffset(FVector2D(0.f, 1.f));
		UOverlaySlot* TextSlot = Bar->AddChildToOverlay(HpText);
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
		USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetWidthOverride(230.f);
		BarSize->SetHeightOverride(24.f);
		BarSize->AddChild(Bar);
		Column->AddChildToVerticalBox(BarSize);

		auto PipRow = [this, Column](const TCHAR* Label) -> UHorizontalBox*
		{
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LabelSize->SetWidthOverride(36.f);
			LabelSize->AddChild(KeepersStyle::MakeText(WidgetTree, KeepersUnits::T(Label), KeepersStyle::Body(12, true), KeepersStyle::TextDim));
			Line->AddChildToHorizontalBox(LabelSize)->SetVerticalAlignment(VAlign_Center);
			UHorizontalBox* Pips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Line->AddChildToHorizontalBox(Pips)->SetVerticalAlignment(VAlign_Center);
			Column->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			return Pips;
		};
		MpPips = PipRow(TEXT("ОП"));
		ApPips = PipRow(TEXT("ОД"));
		EffectsText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Body(12), KeepersStyle::Gold);
		Column->AddChildToVerticalBox(EffectsText)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		Row->AddChildToHorizontalBox(Column)->SetPadding(FMargin(14.f, 0.f, 4.f, 0.f));
		Panel->SetContent(Row);

		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.f, 1.f));
		PanelSlot->SetAlignment(FVector2D(0.f, 1.f));
		PanelSlot->SetOffsets(FMargin(16.f, -72.f, 0.f, 0.f));
		PanelSlot->SetAutoSize(true);
		SetVisibility(ESlateVisibility::Collapsed);
	}
	return Super::RebuildWidget();
}

void UKeepersUnitCard::ShowUnit(UObject* Unit)
{
	if (!Panel)
	{
		return;
	}
	if (!Unit)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		Shown.Reset();
		return;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	const int32 Team = KeepersUnits::TeamOf(Unit);
	Panel->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 14.f, KeepersUnits::TeamColor(Team), 2.f));
	if (UObject* Picture = KeepersUnits::PortraitOf(Unit))
	{
		Portrait->SetBrushResourceObject(Picture);
	}
	NameText->SetText(KeepersUnits::NameOf(Unit));
	const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit));
	FString Role = Team == 0 ? TEXT("ваш отряд") : TEXT("враг");
	if (Def)
	{
		Role = Def->Class.ToString() + TEXT(" · ") + Role;
	}
	// A hero who has grown in the campaign shows the level; the leader of the enemies says so.
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (Def && Team == 0 && Campaign && Campaign->IsActive() && Campaign->GetHeroLevel(Def->Id) > 1)
	{
		Role += FString::Printf(TEXT(" · ур. %d"), Campaign->GetHeroLevel(Def->Id));
	}
	const AKeepersBattleObjective* Objective = AKeepersBattleObjective::Find(GetWorld());
	if (Objective && Objective->GetLeader() == Unit)
	{
		Role = TEXT("вожак · ") + Role;
	}
	RoleText->SetText(KeepersUnits::T(Role));

	const int32 Hp = KeepersUnits::Stat(Unit, TEXT("HP_Current"));
	const int32 HpMax = FMath::Max(KeepersUnits::Stat(Unit, TEXT("HP_Max")), 1);
	HpFill->SetWidthOverride(230.f * FMath::Clamp(static_cast<float>(Hp) / HpMax, 0.f, 1.f));
	const float Ratio = static_cast<float>(Hp) / HpMax;
	HpFillBar->SetBrush(KeepersStyle::Panel(Ratio > 0.5f ? KeepersStyle::Good : Ratio > 0.25f ? FLinearColor(0.85f, 0.65f, 0.2f) : KeepersStyle::Enemy, 8.f, FLinearColor::Transparent));
	HpText->SetText(KeepersUnits::T(FString::Printf(TEXT("%d / %d"), Hp, HpMax)));
	KeepersUnits::FillPips(WidgetTree, MpPips, KeepersUnits::Stat(Unit, TEXT("MP_Current")), KeepersUnits::Stat(Unit, TEXT("MP_Max")), FLinearColor(0.25f, 0.75f, 0.45f));
	KeepersUnits::FillPips(WidgetTree, ApPips, KeepersUnits::Stat(Unit, TEXT("AP_Current")), KeepersUnits::Stat(Unit, TEXT("AP_Max")), FLinearColor(0.3f, 0.55f, 1.f));

	FString Effects;
	FIntPoint Index;
	const UKeepersTerrainSubsystem* Terrain = Unit->GetWorld() ? Unit->GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>() : nullptr;
	if (Terrain && KeepersReflection::GetIntPoint(Unit->GetClass(), Unit, TEXT("Index"), Index) && Terrain->IsForest(Index))
	{
		Effects = TEXT("В лесу: идти вдвое дольше, стрелы не достают");
	}
	if (KeepersUnits::Stat(Unit, TEXT("MP_Max")) > 0 && KeepersUnits::Stat(Unit, TEXT("MP_Current")) == 0 && Team != 0)
	{
		Effects += Effects.IsEmpty() ? TEXT("Не может двигаться") : TEXT(" · не может двигаться");
	}
	EffectsText->SetText(KeepersUnits::T(Effects));
	EffectsText->SetVisibility(Effects.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Shown = Unit;
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UKeepersTurnRibbon::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrush(KeepersStyle::Panel(FLinearColor(0.01f, 0.013f, 0.02f, 0.75f), 14.f, KeepersStyle::GoldDim, 1.f));
		Panel->SetPadding(FMargin(10.f, 8.f));
		Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Panel->SetContent(Row);
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.f));
		PanelSlot->SetOffsets(FMargin(0.f, 96.f, 0.f, 0.f));
		PanelSlot->SetAutoSize(true);
	}
	return Super::RebuildWidget();
}

void UKeepersTurnRibbon::ShowOrder(const TArray<UObject*>& Units, UObject* Current)
{
	if (!Row)
	{
		return;
	}
	// Rebuild only when the order or someone's health changed.
	TArray<int32> Hp;
	TArray<TWeakObjectPtr<UObject>> Order;
	for (UObject* Unit : Units)
	{
		Order.Add(Unit);
		Hp.Add(KeepersUnits::Stat(Unit, TEXT("HP_Current")));
	}
	if (Order == LastOrder && Hp == LastHp)
	{
		return;
	}
	LastOrder = Order;
	LastHp = Hp;
	Row->ClearChildren();
	SetVisibility(Units.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	for (UObject* Unit : Units)
	{
		const bool bCurrent = Unit == Current;
		const float Size = bCurrent ? 74.f : 54.f;
		const int32 Team = KeepersUnits::TeamOf(Unit);
		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrush(KeepersStyle::Panel(FLinearColor(0.06f, 0.07f, 0.1f, 1.f), 10.f, bCurrent ? KeepersStyle::Gold : FLinearColor(1.f, 1.f, 1.f, 0.15f), bCurrent ? 2.5f : 1.f));
		Frame->SetPadding(FMargin(3.f));
		UImage* Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UObject* Source = KeepersUnits::PortraitOf(Unit))
		{
			Picture->SetBrushResourceObject(Source);
		}
		Frame->SetContent(Picture);
		USizeBox* PictureSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PictureSize->SetWidthOverride(Size);
		PictureSize->SetHeightOverride(Size);
		PictureSize->AddChild(Frame);
		Tile->AddChildToVerticalBox(PictureSize)->SetHorizontalAlignment(HAlign_Center);

		// Team strip with the health on it.
		const int32 HpNow = KeepersUnits::Stat(Unit, TEXT("HP_Current"));
		const int32 HpMax = FMath::Max(KeepersUnits::Stat(Unit, TEXT("HP_Max")), 1);
		UOverlay* Strip = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* StripBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FLinearColor Dark = KeepersUnits::TeamColor(Team) * 0.35f;
		Dark.A = 1.f;
		StripBack->SetBrush(KeepersStyle::Panel(Dark, 3.f, FLinearColor::Transparent));
		UOverlaySlot* StripBackSlot = Strip->AddChildToOverlay(StripBack);
		StripBackSlot->SetHorizontalAlignment(HAlign_Fill);
		StripBackSlot->SetVerticalAlignment(VAlign_Fill);
		UBorder* StripFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		StripFill->SetBrush(KeepersStyle::Panel(KeepersUnits::TeamColor(Team), 3.f, FLinearColor::Transparent));
		USizeBox* StripFillSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		StripFillSize->SetWidthOverride(Size * FMath::Clamp(static_cast<float>(HpNow) / HpMax, 0.f, 1.f));
		StripFillSize->AddChild(StripFill);
		UOverlaySlot* StripFillSlot = Strip->AddChildToOverlay(StripFillSize);
		StripFillSlot->SetHorizontalAlignment(HAlign_Left);
		StripFillSlot->SetVerticalAlignment(VAlign_Fill);
		USizeBox* StripSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		StripSize->SetWidthOverride(Size);
		StripSize->SetHeightOverride(6.f);
		StripSize->AddChild(Strip);
		Tile->AddChildToVerticalBox(StripSize)->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));

		Tile->SetToolTipText(KeepersUnits::T(FString::Printf(TEXT("%s · %d / %d HP"), *KeepersUnits::NameOf(Unit).ToString(), HpNow, HpMax)));
		UHorizontalBoxSlot* TileSlot = Row->AddChildToHorizontalBox(Tile);
		TileSlot->SetVerticalAlignment(VAlign_Bottom);
		TileSlot->SetPadding(FMargin(4.f, 0.f));
	}
}
