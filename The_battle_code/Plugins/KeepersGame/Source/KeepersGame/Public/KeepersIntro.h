#pragma once

#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersIntro.generated.h"

class ACameraActor;
class UBorder;
class UTextBlock;

/** Letterbox bars and the captions of the battle intro; a click or a key skips it. */
UCLASS()
class KEEPERSGAME_API UKeepersIntroWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCaption(const FText& Kicker, const FText& Title, const FText& Line);
	/** 0 — hidden, 1 — bars and captions in place. */
	void SetShown(float Alpha);

	TFunction<void()> OnSkip;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY()
	TObjectPtr<UBorder> TopBar;

	UPROPERTY()
	TObjectPtr<UBorder> BottomBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> KickerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SkipText;
};

/**
 * The intro of a battle, before the first move: a wide shot of the field with its name, the squad walking in
 * from the edge with a word from a hero, then the camera goes to what the battle is about (the beacon, the
 * leader, the totem, the boss, the one to protect, or the enemies) with the task on a card, and back to the
 * player's camera. The heroes' first turn waits; a click or a key skips. Not in the tutorial, not after a saved
 * battle is resumed, not with -KeepersNoIntro.
 */
UCLASS()
class KEEPERSGAME_API UKeepersIntroSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	bool IsPlaying() const { return bPlaying; }
	/** A resumed battle has no intro. */
	static void SkipNextIntro() { bSkipNext = true; }
	/** Stops the intro at once (a saved battle came back). */
	void Cancel() { Finish(); }

private:
	void Begin();
	void Finish();
	/** Where the camera looks in the task shot, and what is said about it. */
	FVector TaskFocus(FText& OutLine) const;
	/** The task is the whole enemy band: the camera stays further away. */
	mutable bool bWideTask = false;

	struct FShot
	{
		FVector From;
		FVector To;
		FVector LookFrom;
		FVector LookTo;
		float Duration = 2.f;
		float Fov = 55.f;
	};
	struct FWalker
	{
		TWeakObjectPtr<AActor> Unit;
		FVector Start;
		FVector End;
		FRotator Rotation;
		float Delay = 0.f;
	};

	static bool bSkipNext;

	bool bChecked = false;
	bool bPlaying = false;
	float Wait = 0.f;
	float Time = 0.f;
	int32 Shot = INDEX_NONE;
	TArray<FShot> Shots;
	TArray<FWalker> Walkers;
	FText TaskLine;
	/** The battle's interface, hidden while the intro plays. */
	TArray<TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>> Hidden;
	FText TaskTitle;

	UPROPERTY()
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY()
	TObjectPtr<UKeepersIntroWidget> Widget;
};
