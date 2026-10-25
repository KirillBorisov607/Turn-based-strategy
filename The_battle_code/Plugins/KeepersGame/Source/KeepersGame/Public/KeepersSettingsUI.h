#pragma once

#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersSettingsUI.generated.h"

class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UKeepersFnButton;

/**
 * Settings in tabs — graphics, sound, controls, game. Every line applies and saves at once;
 * "Сбросить раздел" returns the tab to its defaults. Used by the main menu and the pause menu.
 */
UCLASS()
class KEEPERSGAME_API UKeepersSettingsPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowTab(int32 Tab);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void AddRow(const TCHAR* Label, TArray<FText> Options, int32 Current, TFunction<void(int32)> OnChanged);
	void ResetTab();

	int32 CurrentTab = 0;

	UPROPERTY()
	TArray<TObjectPtr<UKeepersFnButton>> Tabs;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Rows;
};

/** A small FPS counter in the top right corner. */
UCLASS()
class KEEPERSGAME_API UKeepersFpsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float Smoothed = 60.f;
	float Timer = 0.f;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text;
};

/**
 * Applies the player's settings in every level: camera, rotation and zoom speeds (BP_Pawn), inverted
 * wheel, scrolling at the screen edge, battle speed, the FPS counter and the volumes.
 */
UCLASS()
class KEEPERSGAME_API UKeepersSettingsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Settings changed: apply them again in the world of WorldContext. */
	static void ApplyNow(const UObject* WorldContext);

private:
	void ApplyToPawn(APawn* Pawn);
	void EdgeScroll(APlayerController* Player, float DeltaTime);

	bool bApplied = false;

	UPROPERTY()
	TObjectPtr<UKeepersFpsWidget> Fps;
};
