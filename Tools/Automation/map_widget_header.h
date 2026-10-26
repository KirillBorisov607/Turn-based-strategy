class KEEPERSGAME_API UKeepersCampaignMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The node under the mouse: its description at the foot of the board. */
	void ShowTip(int32 NodeIndex);
	/** The player picked a node: the squad's token walks there, then the node opens. */
	void OnNodePicked(int32 NodeIndex);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	friend class UKeepersCampaignWorldSubsystem;

	/** "Золото: …" for the squad panel. */
	FString StatusLine() const;
	/** The squad, gold, stones and relics on the left. */
	void BuildSidebar();
	/** The squad reached a node: battles open, ruins, events and camps show their screens. */
	void Arrive(int32 NodeIndex);
	void ShowArtifactChoice(const FText& Title, const FText& Subtitle, int32 Count);

	/** An event node: an encounter with a choice and its consequence (see KeepersEvents). */
	void ShowEvent(int32 NodeIndex);
	/** A node's place on the board, 0..1; INDEX_NONE is the start at the left edge. */
	FVector2D NodeAnchor(int32 NodeIndex) const;
	/** A board place in the widget's space. */
	FVector2D BoardPoint(const FVector2D& Anchor, const FVector2D& Size) const;

	UFUNCTION()
	void HandleMenuClicked();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> BoardCanvas;

	UPROPERTY()
	TArray<TObjectPtr<class UKeepersMapNodeWidget>> NodeWidgets;

	UPROPERTY()
	TObjectPtr<UWidget> Token;

	UPROPERTY()
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TipTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> TipBody;

	FSlateBrush BoardBrush;
	FVector2D TokenAnchor = FVector2D::ZeroVector;
	FVector2D TravelFrom = FVector2D::ZeroVector;
	int32 Travelling = INDEX_NONE;
	float TravelTime = 0.f;
};

/** A node of the region map: a medallion with the node's picture, its caption, a glow when it is open. */
UCLASS()
class KEEPERSGAME_API UKeepersMapNodeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	enum class EState : uint8
	{
		Locked,
		Open,
		Visited
	};

	void Setup(UKeepersCampaignMapWidget* InMap, int32 InIndex, UTexture2D* Icon, const FLinearColor& InColor, const FText& InLabel, EState InState, float InSize);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleClicked();
	UFUNCTION()
	void HandleHovered();
	UFUNCTION()
	void HandleUnhovered();

	TWeakObjectPtr<UKeepersCampaignMapWidget> Map;
	int32 Index = 0;
	FLinearColor Color = FLinearColor::Gray;
	FText Label;
	EState State = EState::Locked;
	float Size = 76.f;
	float Time = 0.f;
	float Hover = 0.f;
	bool bHovered = false;

	UPROPERTY()
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY()
	TObjectPtr<class UButton> Button;

	UPROPERTY()
	TObjectPtr<class UBorder> Glow;
};
