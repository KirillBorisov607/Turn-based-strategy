#pragma once

#include "Blueprint/UserWidget.h"
#include "KeepersCamp.generated.h"

class UHorizontalBox;
class UTextBlock;

/**
 * The camp node of a run: the smith improves the squad's armour and boots for gold, the merchant sells
 * artifacts, and "Отдохнуть и идти дальше" finishes the node. Gear works from the first turn of every
 * following battle (UKeepersCampaignWorldSubsystem applies it with the artifacts).
 */
UCLASS()
class KEEPERSGAME_API UKeepersCampWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called when the squad leaves the camp. */
	TFunction<void()> OnLeave;

	/** Test hooks: buy the next armour rank of a squad member, buy a merchant offer. */
	void TestUpgrade(int32 SquadIndex, bool bBoots);
	void TestBuy(int32 Offer);
	/** Test hook: heal (or hire a recruit for) the wounded hero Index. */
	void TestInfirmary(int32 Index, bool bRecruit);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void Refresh();
	void Upgrade(FName Hero, bool bBoots);
	void Buy(int32 Offer);
	void Heal(FName Hero);
	void Hire(FName Hero);

	/** Artifacts on sale, rolled when the camp opens. */
	TArray<FName> Offers;
	TArray<bool> Sold;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Smith;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Merchant;

	/** Wounded heroes: a healer or a recruit. */
	UPROPERTY()
	TObjectPtr<UHorizontalBox> Infirmary;

	UPROPERTY()
	TObjectPtr<class UBorder> InfirmaryTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> GoldText;
};
