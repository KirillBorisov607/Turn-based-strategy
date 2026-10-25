#pragma once

#include "Blueprint/UserWidget.h"
#include "KeepersUnitPanels.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UVerticalBox;
class USizeBox;

namespace KeepersUnits
{
	/** Russian name of a battle unit ("Страж", "Слизень"). */
	KEEPERSGAME_API FText NameOf(UObject* Unit);
	/** Portrait of a battle unit: the live Toony portrait, or the unit's icon from the data table. */
	KEEPERSGAME_API UObject* PortraitOf(UObject* Unit);
	KEEPERSGAME_API int32 TeamOf(const UObject* Unit);
	KEEPERSGAME_API int32 Stat(UObject* Unit, const TCHAR* Name);
}

/**
 * The unit card, bottom left: the unit under the cursor, or the one whose turn it is. Portrait, name and
 * role, health with numbers, movement and action points as pips, effects of the tile it stands on.
 */
UCLASS()
class KEEPERSGAME_API UKeepersUnitCard : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowUnit(UObject* Unit);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TWeakObjectPtr<UObject> Shown;

	UPROPERTY()
	TObjectPtr<UBorder> Panel;

	UPROPERTY()
	TObjectPtr<UImage> Portrait;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RoleText;

	UPROPERTY()
	TObjectPtr<USizeBox> HpFill;

	UPROPERTY()
	TObjectPtr<UBorder> HpFillBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> HpText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> MpPips;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> ApPips;

	UPROPERTY()
	TObjectPtr<UTextBlock> EffectsText;
};

/** The turn order along the top: whose turn it is now (bigger) and who comes next. */
UCLASS()
class KEEPERSGAME_API UKeepersTurnRibbon : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowOrder(const TArray<UObject*>& Units, UObject* Current);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TArray<TWeakObjectPtr<UObject>> LastOrder;
	TArray<int32> LastHp;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Row;
};
