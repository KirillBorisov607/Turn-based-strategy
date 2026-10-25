#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "KeepersBattleObjective.generated.h"

class UTextBlock;

/** Top-of-screen panel: objective title and progress. Built in C++, no widget asset needed. */
UCLASS()
class KEEPERSGAME_API UKeepersObjectiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLines(const FText& Title, const FText& Progress);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ProgressText;

	FText PendingTitle;
	FText PendingProgress;
};

/**
 * Battle goals beyond "defeat every enemy". Place one in a combat level.
 *
 *  - SurviveTurns > 0: the heroes win when that many rounds are over (the enemies retreat).
 *  - ProtectedUnit set: the heroes lose if it dies. If it is not team 0 it is moved to team 0;
 *    it is played by the AI, so the enemies hunt it like any hero.
 *  Defeating every enemy still wins, and losing every hero still loses, as before.
 */
UCLASS()
class KEEPERSGAME_API AKeepersBattleObjective : public AActor
{
	GENERATED_BODY()

public:
	AKeepersBattleObjective();

	/** Rounds to hold out; 0 = no time limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	int32 SurviveTurns = 0;

	/** A BP_Unit that must stay alive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<AActor> ProtectedUnit;

	/** Name shown in the objective, e.g. "курицу Пеструшку". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText ProtectedName;

	/** Current round (1 = the first player phase). */
	UFUNCTION(BlueprintPure, Category = "Objective")
	int32 GetRound() const { return Round; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void UpdateWidget();
	void Finish(bool bHeroesWin);

	UPROPERTY()
	TObjectPtr<UObject> CombatSystem;

	UPROPERTY()
	TObjectPtr<UKeepersObjectiveWidget> Widget;

	int32 Round = 0;
	int32 LastTeam = INDEX_NONE;
	bool bProtectedSetUp = false;
	bool bFinished = false;
};
