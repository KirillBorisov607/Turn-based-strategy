#pragma once

#include "Blueprint/UserWidget.h"
#include "KeepersAbilityBar.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UVerticalBox;

/** One big ability button: icon, hotkey, cost, cooldown or casts left, name. */
UCLASS()
class KEEPERSGAME_API UKeepersAbilityButton : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InIndex, UObject* InIcon, const FText& InName, int32 InCost);
	/** State of the Blueprint button it mirrors. */
	void SetState(bool bSelected, bool bAvailable, const FText& Counter);

	TFunction<void()> OnClicked;
	TFunction<void(bool)> OnHoverChanged;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleClicked();

	int32 Index = 0;
	FText Name;
	int32 Cost = 0;
	bool bWasHovered = false;
	float Lift = 0.f;

	UPROPERTY()
	TObjectPtr<UObject> Icon;

	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<UBorder> Frame;

	UPROPERTY()
	TObjectPtr<UImage> IconImage;

	UPROPERTY()
	TObjectPtr<UBorder> Shade;

	UPROPERTY()
	TObjectPtr<UTextBlock> CounterText;
};

/**
 * The battle's ability bar: big buttons for the current unit's abilities with a tooltip that tells
 * damage or healing, range, line of sight, cost, cooldown and what the ability does to the terrain.
 * The Blueprint bar (ButtonList_Spells) stays and keeps the logic: this bar mirrors its buttons and
 * presses them; keys 1-6 work through the Blueprint as before.
 */
UCLASS()
class KEEPERSGAME_API UKeepersAbilityBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Mirrors the Blueprint spell list of the HUD. */
	void Sync(UUserWidget* Hud);

	/** Presses ability Index as a click would. */
	void Press(int32 Index);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void ShowTooltip(int32 Index);
	void HideTooltip(int32 Index);

	/** Spells of the shown buttons (E_Spell entry names), in order. */
	TArray<FString> Spells;
	int32 TooltipIndex = INDEX_NONE;

	TWeakObjectPtr<UUserWidget> SpellList;
	TArray<TWeakObjectPtr<UUserWidget>> SourceButtons;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Row;

	UPROPERTY()
	TArray<TObjectPtr<UKeepersAbilityButton>> Buttons;

	UPROPERTY()
	TObjectPtr<UBorder> TipPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> TooltipHeading;

	UPROPERTY()
	TObjectPtr<UTextBlock> TooltipBody;

	UPROPERTY()
	TObjectPtr<UVerticalBox> TooltipFacts;
};
