#pragma once

#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersPauseMenu.generated.h"

class UBorder;
class UVerticalBox;
class UTextBlock;
class UKeepersFnButton;

/** The pause menu: continue, settings, back to the main menu, quit. */
UCLASS()
class KEEPERSGAME_API UKeepersPauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TFunction<void()> OnClose;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void ShowMain();
	void ShowSettings();
	void ToMainMenu();
	void Quit();

	/** A question over the pause menu: Yes runs the action, No closes the question. */
	void Ask(const TCHAR* Question, const TCHAR* Details, const TCHAR* Yes, const TCHAR* No, TFunction<void()> Action);
	void CloseQuestion();
	/** Stops every actor's tick, so nothing in the level runs once its player is gone. */
	void FreezeLevel();

	bool bSettings = false;
	TFunction<void()> QuestionAction;

	UPROPERTY()
	TObjectPtr<UVerticalBox> List;

	UPROPERTY()
	TObjectPtr<UTextBlock> Title;

	UPROPERTY()
	TObjectPtr<UTextBlock> Note;

	UPROPERTY()
	TObjectPtr<UBorder> Question;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuestionTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuestionText;

	UPROPERTY()
	TObjectPtr<UKeepersFnButton> QuestionYes;

	UPROPERTY()
	TObjectPtr<UKeepersFnButton> QuestionNo;
};

/**
 * Esc in a battle or on the campaign map pauses the game and opens the pause menu; Esc again closes it.
 * Not in the main menu, which has its own Esc.
 */
UCLASS()
class KEEPERSGAME_API UKeepersPauseSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	UFUNCTION(BlueprintCallable, Category = "Keepers")
	void TogglePause();

	/** Opens or closes the pause menu of the world (for scripts and tests). */
	UFUNCTION(BlueprintCallable, Category = "Keepers", meta = (WorldContext = "WorldContextObject"))
	static void TogglePauseMenu(const UObject* WorldContextObject);

	bool IsOpen() const { return Widget != nullptr; }

private:
	void Close();

	UPROPERTY()
	TObjectPtr<UKeepersPauseWidget> Widget;

	/** The "Меню" button in the corner. */
	UPROPERTY()
	TObjectPtr<UKeepersFnButton> Button;
};
