#pragma once

#include "Blueprint/UserWidget.h"
#include "KeepersSquad.generated.h"

class AKeepersMenuScene;
class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UTextureRenderTarget2D;
class UVerticalBox;

/** A styled button that runs a function when clicked. */
UCLASS()
class KEEPERSGAME_API UKeepersFnButton : public UUserWidget
{
	GENERATED_BODY()

public:
	enum class EKind : uint8
	{
		Primary,
		Secondary,
		Danger,
		/** A flat line that lights up gold: ability and animation lists. */
		Line
	};

	void Setup(const FText& Label, EKind Kind, int32 FontSize, TFunction<void()> InOnClicked, float Width = 0.f);
	void SetLabel(const FText& Label);
	void SetButtonEnabled(bool bEnabled);
	/** Keeps a Line button lit (the chosen ability). */
	void SetActive(bool bActive);

	TFunction<void()> OnHovered;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	FText LabelText;
	EKind Kind = EKind::Secondary;
	int32 FontSize = 18;
	float Width = 0.f;
	bool bButtonEnabled = true;
	bool bActive = false;
	TFunction<void()> OnClicked;

	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> Caption;
};

/**
 * Half of a hero card: a live portrait (the hero's figure filmed by a scene capture), name and class.
 * Used both for the roster at the bottom of the squad screen and for the three squad slots.
 */
UCLASS()
class KEEPERSGAME_API UKeepersHeroCard : public UUserWidget
{
	GENERATED_BODY()

public:
	/** An empty Hero makes an empty squad slot. */
	void Setup(FName InHero, UTextureRenderTarget2D* Portrait, FVector2D InSize, TFunction<void()> InOnClicked);
	void SetState(bool bInSelected, bool bInParty, bool bLocked);
	FName GetHero() const { return Hero; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleClicked();

	FName Hero;
	FVector2D Size = FVector2D(170.f, 230.f);
	bool bSelected = false;
	bool bInParty = false;
	bool bLocked = false;
	float Lift = 0.f;
	TFunction<void()> OnClicked;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> PortraitTarget;

	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<UBorder> Frame;

	UPROPERTY()
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY()
	TObjectPtr<UBorder> Badge;

	UPROPERTY()
	TObjectPtr<UBorder> LockShade;
};

/**
 * The squad screen before a run: three squad slots, the roster of heroes as half cards, and the chosen
 * hero up close in the camp — stats, abilities (clicking one plays it on the figure), animations, outfit
 * and colour. "Начать поход" starts the campaign with the squad.
 */
UCLASS()
class KEEPERSGAME_API UKeepersSquadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<AKeepersMenuScene> Scene;

	/** true: the screen opens a new campaign ("Начать поход"); false: only edits the squad ("Готово"). */
	bool bStartsCampaign = false;

	TFunction<void()> OnBack;
	TFunction<void()> OnStart;

	void SelectHero(FName Hero);

	/** The outfit switcher moved: wear it if open, otherwise try it on and offer to open it. */
	void ChooseLook(int32 Look);
	void UnlockPendingLook();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void RefreshCards();
	void RefreshDetails();
	void ShowAbility(int32 Index, bool bPlay);
	void ToggleParty();
	void RefreshGlory();

	FName Selected;
	bool bDragging = false;
	int32 ActiveAbility = INDEX_NONE;
	/** A closed outfit being tried on, or INDEX_NONE. */
	int32 PendingLook = INDEX_NONE;

	UPROPERTY()
	TArray<TObjectPtr<UKeepersHeroCard>> RosterCards;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Slots;

	UPROPERTY()
	TObjectPtr<UTextBlock> SquadCount;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeroName;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeroClass;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeroDescription;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Stats;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Abilities;

	UPROPERTY()
	TArray<TObjectPtr<UKeepersFnButton>> AbilityButtons;

	UPROPERTY()
	TObjectPtr<UTextBlock> AbilityText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Outfit;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Poses;

	UPROPERTY()
	TObjectPtr<UKeepersFnButton> PartyButton;

	UPROPERTY()
	TObjectPtr<UKeepersFnButton> UnlockButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> GloryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LockText;

	UPROPERTY()
	TObjectPtr<UKeepersFnButton> StartButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> Hint;
};
