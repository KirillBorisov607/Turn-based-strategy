#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KeepersBarks.generated.h"

class UTextBlock;
class UWidgetComponent;

/** Situations a unit can comment on. */
UENUM(BlueprintType)
enum class EKeepersBarkSituation : uint8
{
	PlansAttack,
	Pushed,
	Bumped,
	Falls,
	RaisesRock,
	GrowsForest
};

/** A speech bubble: rounded dark box with white text. Built in C++, no widget asset needed. */
UCLASS()
class KEEPERSGAME_API UKeepersBarkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetText(const FText& Text);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	FText PendingText;
};

/** Floats above a unit for a couple of seconds, then removes itself. */
UCLASS(NotPlaceable, Transient)
class KEEPERSGAME_API AKeepersBarkActor : public AActor
{
	GENERATED_BODY()

public:
	AKeepersBarkActor();

	void Show(AActor* Speaker, const FText& Text);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Bark")
	float Lifetime = 2.6f;

	UPROPERTY(EditAnywhere, Category = "Bark")
	float HeightAboveUnit = 230.f;

private:
	UPROPERTY()
	TObjectPtr<UWidgetComponent> Bubble;

	TWeakObjectPtr<AActor> SpeakerActor;
	float Age = 0.f;
};

/** Unit one-liners: the humour of Keepers of the Lands comes from units reacting to what happens. */
UCLASS()
class KEEPERSGAME_API UKeepersBarkLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Shows Text above Unit. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Barks")
	static void Bark(AActor* Unit, const FText& Text);

	/**
	 * Picks a line for the unit's type (BP_Unit.UnitType) and situation and shows it.
	 * Chance (0..1) keeps frequent situations from turning into noise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Barks")
	static void BarkAbout(AActor* Unit, EKeepersBarkSituation Situation, float Chance = 1.f);
};
