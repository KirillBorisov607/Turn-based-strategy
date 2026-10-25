#include "KeepersCampaignUI.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "KeepersCampaign.h"
#include "KeepersReflection.h"
#include "KeepersTerrainSubsystem.h"
#include "Rendering/DrawElements.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersCampaignUI, Log, All);

namespace KeepersUI
{
	static const FLinearColor Gold(1.f, 0.85f, 0.4f);
	static const FLinearColor PanelColor(0.02f, 0.03f, 0.05f, 0.85f);

	static UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color, bool bWrap = false)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(Text);
		Block->SetJustification(ETextJustify::Center);
		Block->SetAutoWrapText(bWrap);
		return Block;
	}

	static FSlateBrush Rounded(const FLinearColor& Color, float Radius = 10.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius));
		return Brush;
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

// ---------------------------------------------------------------------------------------------------------------------

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
		Style.Disabled = KeepersUI::Rounded(FLinearColor(0.12f, 0.12f, 0.12f, 0.8f));
		Style.NormalPadding = FMargin(12.f, 10.f);
		Style.PressedPadding = FMargin(12.f, 11.f, 12.f, 9.f);
		Button->SetStyle(Style);
		Button->SetIsEnabled(bIsEnabled);
		Button->OnClicked.AddDynamic(this, &UKeepersOptionButton::HandleClicked);

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Lines->AddChildToVerticalBox(KeepersUI::MakeText(WidgetTree, TitleText, 17, bIsEnabled ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.55f)));
		if (!DescriptionText.IsEmpty())
		{
			if (UVerticalBoxSlot* DescriptionSlot = Lines->AddChildToVerticalBox(KeepersUI::MakeText(WidgetTree, DescriptionText, 12, FLinearColor(0.85f, 0.85f, 0.85f), true)))
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
		Column->AddChildToVerticalBox(KeepersUI::MakeText(WidgetTree, TitleText, 28, KeepersUI::Gold));
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
			Card->Setup(Option.Title, Option.Description, Option.Color, MoveTemp(Option.OnPicked));
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

FVector2D UKeepersCampaignMapWidget::NodeAnchor(int32 NodeIndex) const
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Campaign->GetNodes().IsValidIndex(NodeIndex))
	{
		return FVector2D::ZeroVector;
	}
	int32 Columns = 1;
	for (const FKeepersMapNode& Node : Campaign->GetNodes())
	{
		Columns = FMath::Max(Columns, Node.Column + 1);
	}
	const FKeepersMapNode& Node = Campaign->GetNodes()[NodeIndex];
	return FVector2D(0.1f + 0.8f * Node.Column / FMath::Max(Columns - 1, 1), 0.2f + 0.58f * Node.Height);
}

TSharedRef<SWidget> UKeepersCampaignMapWidget::RebuildWidget()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (WidgetTree && !WidgetTree->RootWidget && Campaign)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shade->SetBrushColor(FLinearColor(0.f, 0.02f, 0.05f, 0.45f));
		if (UCanvasPanelSlot* ShadeSlot = Canvas->AddChildToCanvas(Shade))
		{
			ShadeSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			ShadeSlot->SetOffsets(FMargin(0.f));
		}

		UTextBlock* Title = KeepersUI::MakeText(WidgetTree, FText::FromString(TEXT("Зелёные долы — выберите путь")), 30, KeepersUI::Gold);
		if (UCanvasPanelSlot* TitleSlot = Canvas->AddChildToCanvas(Title))
		{
			TitleSlot->SetAnchors(FAnchors(0.5f, 0.06f));
			TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			TitleSlot->SetAutoSize(true);
		}

		const TArray<int32> Available = Campaign->GetAvailableNodes();
		for (int32 I = 0; I < Campaign->GetNodes().Num(); ++I)
		{
			const FKeepersMapNode& Node = Campaign->GetNodes()[I];
			const bool bVisited = Campaign->GetVisitedNodes().Contains(I);
			const bool bOpen = Available.Contains(I);
			FText Label = KeepersUI::NodeName(Node.Type);
			if (bVisited)
			{
				Label = FText::FromString(Label.ToString() + TEXT(" (пройдено)"));
			}
			UKeepersOptionButton* Button = CreateWidget<UKeepersOptionButton>(this, UKeepersOptionButton::StaticClass());
			const FLinearColor Color = bVisited ? FLinearColor(0.2f, 0.25f, 0.2f) : KeepersUI::NodeColor(Node.Type);
			TWeakObjectPtr<UKeepersCampaignMapWidget> WeakThis(this);
			Button->Setup(Label, FText::GetEmpty(), Color, [WeakThis, I]() { if (WeakThis.IsValid()) { WeakThis->OnNodePicked(I); } },
				bOpen, Node.Type == EKeepersNodeType::Boss ? 150.f : 120.f);
			if (UCanvasPanelSlot* NodeSlot = Canvas->AddChildToCanvas(Button))
			{
				const FVector2D Anchor = NodeAnchor(I);
				NodeSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
				NodeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				NodeSlot->SetAutoSize(true);
			}
		}

		// Artifacts owned.
		TArray<FString> Owned;
		for (const FName& Id : Campaign->GetArtifacts())
		{
			if (const FKeepersArtifact* Artifact = UKeepersCampaignSubsystem::FindArtifact(Id))
			{
				Owned.Add(Artifact->Name.ToString());
			}
		}
		const FString ArtifactsLine = Owned.Num() > 0 ? TEXT("Артефакты: ") + FString::Join(Owned, TEXT(", ")) : TEXT("Артефактов пока нет — их дают победы и руины.");
		UTextBlock* ArtifactsText = KeepersUI::MakeText(WidgetTree, FText::FromString(ArtifactsLine), 15, FLinearColor::White);
		if (UCanvasPanelSlot* ArtifactsSlot = Canvas->AddChildToCanvas(ArtifactsText))
		{
			ArtifactsSlot->SetAnchors(FAnchors(0.5f, 0.9f));
			ArtifactsSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ArtifactsSlot->SetAutoSize(true);
		}

		UKeepersOptionButton* Menu = CreateWidget<UKeepersOptionButton>(this, UKeepersOptionButton::StaticClass());
		TWeakObjectPtr<UKeepersCampaignMapWidget> WeakThis(this);
		Menu->Setup(FText::FromString(TEXT("В меню")), FText::GetEmpty(), FLinearColor(0.2f, 0.2f, 0.25f), [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				if (UKeepersCampaignSubsystem* Subsystem = UKeepersCampaignSubsystem::Get(WeakThis.Get()))
				{
					Subsystem->ReturnToMainMenu(WeakThis.Get());
				}
			}
		}, true, 140.f);
		if (UCanvasPanelSlot* MenuSlot = Canvas->AddChildToCanvas(Menu))
		{
			MenuSlot->SetAnchors(FAnchors(0.97f, 0.96f));
			MenuSlot->SetAlignment(FVector2D(1.f, 1.f));
			MenuSlot->SetAutoSize(true);
		}
	}
	return Super::RebuildWidget();
}

int32 UKeepersCampaignMapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign)
	{
		return Layer;
	}
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const TArray<FKeepersMapNode>& Nodes = Campaign->GetNodes();
	for (int32 From = 0; From < Nodes.Num(); ++From)
	{
		for (int32 To : Nodes[From].Next)
		{
			const FVector2D A = NodeAnchor(From) * Size;
			const FVector2D B = NodeAnchor(To) * Size;
			const FVector2D Direction = (B - A).GetSafeNormal();
			// Stop short of the node cards so the roads run between them.
			const TArray<FVector2D> Points = { A + Direction * 70.f, B - Direction * 70.f };
			const bool bTaken = Campaign->GetVisitedNodes().Contains(From) && Campaign->GetVisitedNodes().Contains(To);
			FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
				bTaken ? FLinearColor(1.f, 0.85f, 0.4f, 0.9f) : FLinearColor(1.f, 1.f, 1.f, 0.35f), true, bTaken ? 4.f : 2.f);
		}
	}
	return Layer + 1;
}

void UKeepersCampaignMapWidget::OnNodePicked(int32 NodeIndex)
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
	else if (Type == EKeepersNodeType::Camp)
	{
		ShowArtifactChoice(FText::FromString(TEXT("Привал у костра")),
			FText::FromString(TEXT("Друид заваривает чай из подозрительных грибов. Страж делится историей про кур. Утром кто-то находит в рюкзаке подарок.")), 2);
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
		FInputModeUIOnly Mode;
		if (Focus)
		{
			Mode.SetWidgetToFocus(Focus->TakeWidget());
		}
		Player->SetInputMode(Mode);
	}
}

UKeepersChoiceWidget* UKeepersCampaignWorldSubsystem::ShowChoice(const FText& Title, const FText& Subtitle, TArray<UKeepersChoiceWidget::FOption> Options)
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	UKeepersChoiceWidget* Choice = Player ? CreateWidget<UKeepersChoiceWidget>(Player, UKeepersChoiceWidget::StaticClass()) : nullptr;
	if (Choice)
	{
		Choice->Setup(Title, Subtitle, MoveTemp(Options));
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
	if (!bMenuExtended)
	{
		TryExtendMainMenu();
	}
	if (!bMapShown && World->GetOutermost()->GetName().Contains(TEXT("L_CampaignMap")))
	{
		ShowMap();
	}
	TickBattle();
}

void UKeepersCampaignWorldSubsystem::TryExtendMainMenu()
{
	UUserWidget* Menu = nullptr;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->GetClass()->GetName() == TEXT("W_MainMenu_C") && It->IsInViewport())
		{
			Menu = *It;
			break;
		}
	}
	if (!Menu || !Menu->WidgetTree)
	{
		return;
	}
	bMenuExtended = true;

	UGridPanel* Grid = Cast<UGridPanel>(Menu->WidgetTree->FindWidget(TEXT("GridPanel_104")));
	const UButton* Reference = Cast<UButton>(Menu->WidgetTree->FindWidget(TEXT("Button_DebugMap")));
	const UTextBlock* ReferenceText = Cast<UTextBlock>(Menu->WidgetTree->FindWidget(TEXT("Text_7")));
	if (!Grid || !Reference)
	{
		UE_LOG(LogKeepersCampaignUI, Warning, TEXT("Main menu layout changed: campaign buttons not added"));
		return;
	}
	const UGridSlot* ReferenceSlot = Cast<UGridSlot>(Reference->Slot);

	auto AddButton = [&](const TCHAR* Label, int32 Row, int32 Column, bool bContinue)
	{
		UButton* Button = Menu->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(Reference->GetStyle());
		UTextBlock* Text = Menu->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (ReferenceText)
		{
			Text->SetFont(ReferenceText->GetFont());
			Text->SetColorAndOpacity(ReferenceText->GetColorAndOpacity());
		}
		Text->SetText(FText::FromString(Label));
		Button->AddChild(Text);
		if (UGridSlot* NewSlot = Grid->AddChildToGrid(Button, Row, Column))
		{
			if (ReferenceSlot)
			{
				NewSlot->SetPadding(ReferenceSlot->GetPadding());
				NewSlot->SetHorizontalAlignment(ReferenceSlot->GetHorizontalAlignment());
				NewSlot->SetVerticalAlignment(ReferenceSlot->GetVerticalAlignment());
			}
		}
		if (bContinue)
		{
			Button->OnClicked.AddDynamic(this, &UKeepersCampaignWorldSubsystem::OnContinueClicked);
		}
		else
		{
			Button->OnClicked.AddDynamic(this, &UKeepersCampaignWorldSubsystem::OnCampaignClicked);
		}
	};
	AddButton(TEXT("Кампания"), 0, 1, false);
	if (UKeepersCampaignSubsystem::HasSavedCampaign())
	{
		AddButton(TEXT("Продолжить"), 3, 0, true);
	}
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Campaign buttons added to the main menu"));
}

void UKeepersCampaignWorldSubsystem::OnCampaignClicked()
{
	if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld()))
	{
		Campaign->StartNewCampaign();
	}
}

void UKeepersCampaignWorldSubsystem::OnContinueClicked()
{
	if (UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld()))
	{
		Campaign->ContinueCampaign();
	}
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
	Screen = CreateWidget<UKeepersCampaignMapWidget>(Player, UKeepersCampaignMapWidget::StaticClass());
	Screen->AddToViewport(10);
	SetUiInput(Screen);
}

void UKeepersCampaignWorldSubsystem::TickBattle()
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (!Campaign || !Campaign->IsInBattleNode() || bBattleResolved)
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
			}
			LastTeam = Team;
		}
	}
	if (bFinishing)
	{
		// Let the last blow land before the result screen.
		ResultDelay += GetWorld()->GetDeltaSeconds();
		if (ResultDelay > 1.5f)
		{
			bBattleResolved = true;
			int32 Winner = INDEX_NONE;
			KeepersReflection::GetInt(CombatSystem->GetClass(), CombatSystem, TEXT("TeamIndexThatWonLastCombat"), Winner);
			OnBattleFinished(CombatSystem, Winner);
		}
	}
}

void UKeepersCampaignWorldSubsystem::ApplyArtifactsAtBattleStart(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	for (UObject* Hero : KeepersUI::Heroes(CombatSystem))
	{
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
	if (!Campaign->HasArtifact(TEXT("WorldTreeRoot")) || !Terrain)
	{
		return;
	}
	for (UObject* Hero : KeepersUI::Heroes(CombatSystem))
	{
		FIntPoint Index;
		if (KeepersReflection::GetIntPoint(Hero->GetClass(), Hero, TEXT("Index"), Index) && Terrain->IsForest(Index))
		{
			KeepersReflection::FCall Heal(Hero, TEXT("ModifyCurrentHp"));
			Heal.Set<int32>(TEXT("Modifier"), 2);
			Heal.Set<bool>(TEXT("ShowNumber"), true);
			Heal.Call();
		}
	}
}

void UKeepersCampaignWorldSubsystem::OnBattleFinished(UObject* CombatSystem, int32 Winner)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	TWeakObjectPtr<UKeepersCampaignSubsystem> WeakCampaign(Campaign);
	TArray<UKeepersChoiceWidget::FOption> Options;

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
		ShowChoice(FText::FromString(TEXT("Хранители отступили...")),
			FText::FromString(TEXT("Слизни празднуют. Куры делают вид, что так и задумывали.")), MoveTemp(Options));
		return;
	}

	if (Campaign->IsBossNode(Campaign->GetCurrentNode()))
	{
		UKeepersChoiceWidget::FOption Done;
		Done.Title = FText::FromString(TEXT("В главное меню"));
		Done.Color = FLinearColor(0.3f, 0.25f, 0.1f);
		Done.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->CompleteCurrentNode(); } };
		Options.Add(MoveTemp(Done));
		ShowChoice(FText::FromString(TEXT("Земли освобождены!")),
			FText::FromString(TEXT("Зелёные долы снова зеленеют. Даже куры притихли — ненадолго.")), MoveTemp(Options));
		return;
	}

	for (const FName& Id : Campaign->RollArtifacts(3))
	{
		const FKeepersArtifact* Artifact = UKeepersCampaignSubsystem::FindArtifact(Id);
		UKeepersChoiceWidget::FOption Option;
		Option.Title = Artifact->Name;
		Option.Description = Artifact->Description;
		Option.Color = FLinearColor(0.3f, 0.25f, 0.1f);
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
	ShowChoice(FText::FromString(TEXT("Победа!")), FText::FromString(TEXT("Выберите награду.")), MoveTemp(Options));
}
