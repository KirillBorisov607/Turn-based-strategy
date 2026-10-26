#pragma once

#include "Blueprint/UserWidget.h"
#include "KeepersStory.generated.h"

class UImage;
class USizeBox;
class UTextBlock;

/**
 * A line of a scene. Speaker: "Hero1".."Hero3" (the squad's heroes in order), "Narrator", or a character
 * of the story ("Witch", "Beak", "Rooster").
 */
struct FKeepersLine
{
	FName Speaker;
	FString Text;
};

/** The story of the Stones of the Lands: region scenes, the finale and the endings. */
namespace KeepersStory
{
	/** Endings of the run. */
	enum class EEnding : uint8
	{
		/** The stones go back to the spirits: the canonical ending. */
		Return,
		/** The Keepers keep the stones: the dark ending. */
		Keep,
		/** A pact with the chickens: needs PeaceNeeded peaceful choices in the run. */
		Pact
	};

	constexpr int32 PeaceNeeded = 3;

	KEEPERSGAME_API TArray<FKeepersLine> RegionIntro(int32 Region);
	KEEPERSGAME_API TArray<FKeepersLine> Finale();
	KEEPERSGAME_API TArray<FKeepersLine> Ending(EEnding Ending);
	KEEPERSGAME_API FText EndingName(EEnding Ending);
}

/**
 * A dialogue box at the bottom of the screen: the speaker's portrait and name, the line typed out letter by
 * letter. A click (or Space, Enter) shows the whole line, the next click goes on; after the last line the
 * box closes and OnFinished runs.
 */
UCLASS()
class KEEPERSGAME_API UKeepersDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(TArray<FKeepersLine> InLines, TFunction<void()> InOnFinished);
	/** Test hook: as a click. */
	void Advance();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void ShowLine();

	TArray<FKeepersLine> Lines;
	TFunction<void()> OnFinished;
	int32 Current = 0;
	float Shown = 0.f;

	UPROPERTY()
	TObjectPtr<UImage> Portrait;

	UPROPERTY()
	TObjectPtr<USizeBox> PortraitBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;
};
