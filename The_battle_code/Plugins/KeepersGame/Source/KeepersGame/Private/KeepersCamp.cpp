#include "KeepersCamp.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "KeepersCampaign.h"
#include "KeepersHeroes.h"
#include "KeepersSquad.h"
#include "KeepersStyle.h"

namespace KeepersCamp
{
	static FText T(const FString& Text)
	{
		return FText::FromString(Text);
	}

	/** "●●○" for a rank out of a maximum. */
	static FString Pips(int32 Rank, int32 Max)
	{
		FString Result;
		for (int32 I = 0; I < Max; ++I)
		{
			Result += I < Rank ? TEXT("●") : TEXT("○");
		}
		return Result;
	}

	static UBorder* SectionTitle(UWidgetTree* Tree, const TCHAR* Text)
	{
		UBorder* Title = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Title->SetBrush(KeepersStyle::Fade(FLinearColor(1.f, 0.74f, 0.3f, 0.16f)));
		Title->SetPadding(FMargin(10.f, 3.f));
		Title->SetContent(KeepersStyle::MakeText(Tree, T(Text), KeepersStyle::Heading(22), KeepersStyle::Gold));
		return Title;
	}
}

TSharedRef<SWidget> UKeepersCampWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Warm dark veil over the region map.
		UBorder* Veil = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Veil->SetBrushColor(FLinearColor(0.03f, 0.015f, 0.005f, 0.82f));
		UCanvasPanelSlot* VeilSlot = Canvas->AddChildToCanvas(Veil);
		VeilSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		VeilSlot->SetOffsets(FMargin(0.f));

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// Header: title, a line of camp life, gold.
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Titles = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Titles->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, KeepersCamp::T(TEXT("привал")), KeepersStyle::Heading(18), KeepersStyle::Gold));
		Titles->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, KeepersCamp::T(TEXT("У костра")), KeepersStyle::Heading(56)));
		USizeBox* FlavorSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		FlavorSize->SetWidthOverride(760.f);
		FlavorSize->AddChild(KeepersStyle::MakeText(WidgetTree,
			KeepersCamp::T(TEXT("Друид заваривает чай из подозрительных грибов, Страж чинит щит. Неподалёку стучит молотом кузнец, а торговец раскладывает на пне свои сокровища.")),
			KeepersStyle::Body(15), FLinearColor(0.85f, 0.83f, 0.8f), true));
		Titles->AddChildToVerticalBox(FlavorSize);
		UHorizontalBoxSlot* TitlesSlot = Header->AddChildToHorizontalBox(Titles);
		TitlesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UBorder* GoldPill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		GoldPill->SetBrush(KeepersStyle::Panel(FLinearColor(1.f, 0.74f, 0.3f, 0.16f), 12.f, KeepersStyle::Gold, 1.5f));
		GoldPill->SetPadding(FMargin(18.f, 6.f));
		GoldText = KeepersStyle::MakeText(WidgetTree, FText::GetEmpty(), KeepersStyle::Heading(28), KeepersStyle::Gold);
		GoldPill->SetContent(GoldText);
		Header->AddChildToHorizontalBox(GoldPill)->SetVerticalAlignment(VAlign_Bottom);
		Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));

		Column->AddChildToVerticalBox(KeepersCamp::SectionTitle(WidgetTree, TEXT("Кузница")))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		Smith = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Smith)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));

		Column->AddChildToVerticalBox(KeepersCamp::SectionTitle(WidgetTree, TEXT("Торговец")))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		Merchant = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Merchant)->SetPadding(FMargin(0.f, 0.f, 0.f, 26.f));

		TWeakObjectPtr<UKeepersCampWidget> WeakThis(this);
		UKeepersFnButton* Leave = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		Leave->Setup(KeepersCamp::T(TEXT("Отдохнуть и идти дальше")), UKeepersFnButton::EKind::Primary, 24,
			[WeakThis]() { if (WeakThis.IsValid() && WeakThis->OnLeave) { WeakThis->OnLeave(); } }, 380.f);
		Column->AddChildToVerticalBox(Leave)->SetHorizontalAlignment(HAlign_Right);

		UCanvasPanelSlot* ColumnSlot = Canvas->AddChildToCanvas(Column);
		ColumnSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ColumnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ColumnSlot->SetAutoSize(true);

		if (const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this))
		{
			Offers = Campaign->RollArtifacts(3);
			Sold.Init(false, Offers.Num());
		}
		Refresh();
	}
	return Super::RebuildWidget();
}

void UKeepersCampWidget::Refresh()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	UKeepersHeroLookSubsystem* Looks = GetWorld() ? GetWorld()->GetSubsystem<UKeepersHeroLookSubsystem>() : nullptr;
	if (!Campaign || !Party || !Smith)
	{
		return;
	}
	const int32 Gold = Campaign->GetGold();
	GoldText->SetText(KeepersCamp::T(FString::Printf(TEXT("Золото: %d"), Gold)));
	TWeakObjectPtr<UKeepersCampWidget> WeakThis(this);

	// The smith: one card per squad member, armour and boots.
	Smith->ClearChildren();
	for (const FName& Hero : Party->GetParty())
	{
		const FKeepersHeroDef* Def = KeepersHeroes::Find(Hero);
		if (!Def)
		{
			continue;
		}
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 12.f));
		Card->SetPadding(FMargin(14.f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UTextureRenderTarget2D* Portrait = Looks ? Looks->GetPortrait(Hero, false) : nullptr)
		{
			UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Image->SetBrushResourceObject(Portrait);
			USizeBox* PortraitSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			PortraitSize->SetWidthOverride(96.f);
			PortraitSize->SetHeightOverride(96.f);
			PortraitSize->AddChild(Image);
			Row->AddChildToHorizontalBox(PortraitSize)->SetVerticalAlignment(VAlign_Top);
		}
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Lines->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, Def->Name, KeepersStyle::Heading(24)));
		auto AddGear = [&](const TCHAR* Label, EKeepersGear Gear, const FString& Effect)
		{
			const int32 Rank = Campaign->GetGearRank(Hero, Gear);
			const int32 Max = UKeepersCampaignSubsystem::MaxGearRank(Gear);
			UHorizontalBox* GearRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			LabelSize->SetWidthOverride(150.f);
			LabelSize->AddChild(KeepersStyle::MakeText(WidgetTree, KeepersCamp::T(FString::Printf(TEXT("%s  %s"), Label, *KeepersCamp::Pips(Rank, Max))), KeepersStyle::Body(14, true)));
			GearRow->AddChildToHorizontalBox(LabelSize)->SetVerticalAlignment(VAlign_Center);
			USizeBox* EffectSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			EffectSize->SetWidthOverride(70.f);
			EffectSize->AddChild(KeepersStyle::MakeText(WidgetTree, KeepersCamp::T(Effect), KeepersStyle::Body(13), KeepersStyle::Good));
			GearRow->AddChildToHorizontalBox(EffectSize)->SetVerticalAlignment(VAlign_Center);
			UKeepersFnButton* Button = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
			const bool bBoots = Gear == EKeepersGear::Boots;
			const int32 Cost = UKeepersCampaignSubsystem::GearCost(Gear, Rank + 1);
			Button->Setup(KeepersCamp::T(Rank >= Max ? TEXT("Лучше не бывает") : FString::Printf(TEXT("Улучшить · %d"), Cost)), UKeepersFnButton::EKind::Secondary, 14,
				[WeakThis, Hero, bBoots]() { if (WeakThis.IsValid()) { WeakThis->Upgrade(Hero, bBoots); } }, 150.f);
			Button->SetButtonEnabled(Rank < Max && Gold >= Cost);
			GearRow->AddChildToHorizontalBox(Button)->SetVerticalAlignment(VAlign_Center);
			Lines->AddChildToVerticalBox(GearRow)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		};
		const int32 ArmorRank = Campaign->GetGearRank(Hero, EKeepersGear::Armor);
		const int32 BootsRank = Campaign->GetGearRank(Hero, EKeepersGear::Boots);
		AddGear(TEXT("Доспех"), EKeepersGear::Armor, ArmorRank > 0 ? FString::Printf(TEXT("+%d HP"), ArmorRank * UKeepersCampaignSubsystem::ArmorHpPerRank()) : TEXT(""));
		AddGear(TEXT("Сапоги"), EKeepersGear::Boots, BootsRank > 0 ? FString::Printf(TEXT("+%d ОП"), BootsRank) : TEXT(""));
		Row->AddChildToHorizontalBox(Lines)->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
		Card->SetContent(Row);
		Smith->AddChildToHorizontalBox(Card)->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
	}

	// The merchant: artifacts for gold.
	Merchant->ClearChildren();
	for (int32 I = 0; I < Offers.Num(); ++I)
	{
		const FKeepersArtifact* Artifact = UKeepersCampaignSubsystem::FindArtifact(Offers[I]);
		if (!Artifact)
		{
			continue;
		}
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card->SetBrush(KeepersStyle::Panel(KeepersStyle::Ink, 12.f, Sold[I] ? FLinearColor(1.f, 1.f, 1.f, 0.1f) : KeepersStyle::GoldDim));
		Card->SetPadding(FMargin(16.f, 12.f));
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetWidthOverride(300.f);
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Lines->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, Artifact->Name, KeepersStyle::Heading(22), Sold[I] ? KeepersStyle::TextDim : KeepersStyle::Text));
		USizeBox* DescriptionSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DescriptionSize->SetMinDesiredHeight(54.f);
		DescriptionSize->AddChild(KeepersStyle::MakeText(WidgetTree, Artifact->Description, KeepersStyle::Body(13), KeepersStyle::TextDim, true));
		Lines->AddChildToVerticalBox(DescriptionSize)->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));
		UKeepersFnButton* Button = CreateWidget<UKeepersFnButton>(this, UKeepersFnButton::StaticClass());
		const int32 Price = UKeepersCampaignSubsystem::ArtifactPrice();
		Button->Setup(KeepersCamp::T(Sold[I] ? TEXT("Куплено") : FString::Printf(TEXT("Купить · %d"), Price)), UKeepersFnButton::EKind::Secondary, 16,
			[WeakThis, I]() { if (WeakThis.IsValid()) { WeakThis->Buy(I); } }, 160.f);
		Button->SetButtonEnabled(!Sold[I] && Gold >= Price);
		Lines->AddChildToVerticalBox(Button)->SetHorizontalAlignment(HAlign_Left);
		CardSize->AddChild(Lines);
		Card->SetContent(CardSize);
		Merchant->AddChildToHorizontalBox(Card)->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
	}
	if (Offers.Num() == 0)
	{
		Merchant->AddChildToHorizontalBox(KeepersStyle::MakeText(WidgetTree, KeepersCamp::T(TEXT("Торговцу больше нечего вам предложить: все его реликвии уже у вас.")), KeepersStyle::Body(15), KeepersStyle::TextDim));
	}
}

void UKeepersCampWidget::Upgrade(FName Hero, bool bBoots)
{
	if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this))
	{
		Campaign->UpgradeGear(Hero, bBoots ? EKeepersGear::Boots : EKeepersGear::Armor);
		Refresh();
	}
}

void UKeepersCampWidget::Buy(int32 Offer)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Offers.IsValidIndex(Offer) || Sold[Offer] || !Campaign->SpendGold(UKeepersCampaignSubsystem::ArtifactPrice()))
	{
		return;
	}
	Campaign->AddArtifact(Offers[Offer]);
	Sold[Offer] = true;
	Refresh();
}

void UKeepersCampWidget::TestUpgrade(int32 SquadIndex, bool bBoots)
{
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(this);
	if (Party && Party->GetParty().IsValidIndex(SquadIndex))
	{
		Upgrade(Party->GetParty()[SquadIndex], bBoots);
	}
}

void UKeepersCampWidget::TestBuy(int32 Offer)
{
	Buy(Offer);
}
