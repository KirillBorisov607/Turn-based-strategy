#pragma once

#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersCampaignUI.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;

/** A clickable card: title and an optional description line. Calls OnPicked when clicked. */
UCLASS()
class KEEPERSGAME_API UKeepersOptionButton : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& Title, const FText& Description, FLinearColor Color, TFunction<void()> InOnPicked, bool bEnabled = true, float Width = 260.f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY()
	TObjectPtr<UButton> Button;

	FText TitleText;
	FText DescriptionText;
	FLinearColor ButtonColor = FLinearColor::White;
	bool bIsEnabled = true;
	float CardWidth = 260.f;
	TFunction<void()> OnPicked;
};

/** A modal panel with a title, a line of text and a row of option cards. */
UCLASS()
class KEEPERSGAME_API UKeepersChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	struct FOption
	{
		FText Title;
		FText Description;
		FLinearColor Color = FLinearColor(0.25f, 0.4f, 0.6f);
		TFunction<void()> OnPicked;
	};

	void Setup(const FText& Title, const FText& Subtitle, TArray<FOption> Options);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FText TitleText;
	FText SubtitleText;
	TArray<FOption> PendingOptions;
};

/** The region map: nodes in columns, roads between them, artifacts owned. */
UCLASS()
class KEEPERSGAME_API UKeepersCampaignMapWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void OnNodePicked(int32 NodeIndex);
	void ShowArtifactChoice(const FText& Title, const FText& Subtitle, int32 Count);
	FVector2D NodeAnchor(int32 NodeIndex) const;

	UFUNCTION()
	void HandleMenuClicked();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> Canvas;
};

/**
 * Campaign behaviour of the current level:
 *  - region map level: shows the map;
 *  - a campaign battle: applies artifacts when the battle starts, heals heroes in forests with the
 *    World Tree Root, and when the battle ends offers a reward (or ends the run).
 */
UCLASS()
class KEEPERSGAME_API UKeepersCampaignWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Shows a modal choice and gives the mouse to the UI. */
	UKeepersChoiceWidget* ShowChoice(const FText& Title, const FText& Subtitle, TArray<UKeepersChoiceWidget::FOption> Options);

	/** The camp node: smith, merchant, rest. */
	void ShowCamp();

	/** Test hook: "open" the camp, "armor" / "boots" for squad member Index, "buy" offer Index, "leave". */
	UFUNCTION(BlueprintCallable, Category = "Keepers", meta = (WorldContext = "WorldContextObject"))
	static void TestCamp(const UObject* WorldContextObject, const FString& Action, int32 Index);

private:
	void ShowMap();
	void TickBattle();
	/** Chickens hire people: bandits, archers and brutes join elite, late and boss battles of the campaign. */
	void TickMercenaries();
	bool bMercenariesDone = false;
	void ApplyArtifactsAtBattleStart(UObject* CombatSystem);
	void OnRoundStarted(UObject* CombatSystem);
	void OnBattleFinished(UObject* CombatSystem, int32 Winner);
	void SetUiInput(UUserWidget* Focus);

	/** Every battle shows a goal: a campaign battle gets one picked by its node, others "defeat all". */
	void EnsureObjective();

	bool bObjectiveChecked = false;

	UPROPERTY()
	TObjectPtr<UUserWidget> Screen;

	UPROPERTY()
	TObjectPtr<class UKeepersCampWidget> Camp;

	bool bMapShown = false;
	bool bArtifactsApplied = false;
	bool bBattleResolved = false;
	int32 Round = 0;
	int32 LastTeam = INDEX_NONE;
	float ResultDelay = 0.f;
};
