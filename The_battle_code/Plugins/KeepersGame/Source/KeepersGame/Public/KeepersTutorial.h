#pragma once

#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersTutorial.generated.h"

class UTextBlock;
class UButton;

/** The hint panel: step counter, text, "Next" (for steps that wait for a click) and "Skip". */
UCLASS()
class KEEPERSGAME_API UKeepersTutorialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowStep(int32 Index, int32 Count, const FText& Title, const FText& Text, bool bShowNext);

	TFunction<void()> OnNext;
	TFunction<void()> OnSkip;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleNext();

	UFUNCTION()
	void HandleSkip();

	UPROPERTY()
	TObjectPtr<UTextBlock> CounterText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY()
	TObjectPtr<UButton> NextButton;
};

/**
 * The tutorial battle: hints that advance when the player does what they ask
 * (move, use an ability, switch hero, end the turn, see an enemy intent).
 * Started from the main menu ("Обучение"), which opens L_Slice_Hex with the tutorial on.
 */
UCLASS()
class KEEPERSGAME_API UKeepersTutorialSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Opens the tutorial battle. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Tutorial", meta = (WorldContext = "WorldContext"))
	static void StartTutorial(UObject* WorldContext);

	/** Set by StartTutorial for the next level; the tutorial reads and clears it. */
	static bool bPendingTutorial;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	bool IsTutorialActive() const { return bActive; }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	enum class EStep : uint8
	{
		Welcome,
		Camera,
		Move,
		Ability,
		SwitchHero,
		EndTurn,
		Intents,
		Finale,
		Done
	};

	void Begin(UObject* CombatSystem);
	void Show(EStep Step);
	void Advance();
	bool IsStepDone(UObject* CombatSystem);
	void Finish();

	UPROPERTY()
	TObjectPtr<UKeepersTutorialWidget> Widget;

	TWeakObjectPtr<UObject> Combat;
	bool bActive = false;
	EStep Current = EStep::Welcome;

	/** Snapshot taken when a step starts, to detect the player's action. */
	TWeakObjectPtr<UObject> StepUnit;
	FIntPoint StepUnitIndex = FIntPoint(-999, -999);
	int32 StepApSpent = 0;
	float StepTime = 0.f;
	bool bSawEnemyPhase = false;
};
