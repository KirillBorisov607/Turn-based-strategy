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

		// The island behind, darker at the edges.
		UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shade->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.45f));
		Place(Shade, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f));

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

		// The board.
		UBorder* Board = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Board->SetBrush(KeepersStyle::Panel(FLinearColor(0.03f, 0.04f, 0.05f, 0.42f), 22.f, FLinearColor(1.f, 0.78f, 0.3f, 0.35f), 2.f));
		Place(Board, FAnchors(KeepersMap::BoardMin.X, KeepersMap::BoardMin.Y, KeepersMap::BoardMax.X, KeepersMap::BoardMax.Y), FMargin(0.f));
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
