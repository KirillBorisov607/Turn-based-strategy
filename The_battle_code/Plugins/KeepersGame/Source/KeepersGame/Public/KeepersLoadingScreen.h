#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KeepersLoadingScreen.generated.h"

class SWidget;
class UTexture2D;
class UWorld;

namespace KeepersLoading
{
	/** Opens a level behind a loading screen (picture of the place, its name, a tip). */
	KEEPERSGAME_API void OpenLevel(const UObject* WorldContext, FName Level);
}

/**
 * Loading screens between levels: a picture of the place with a slow zoom, the place's name and what
 * awaits there, a tip, and a gold spinner. The screen goes up before the level change, stays while the
 * level loads (the last frame stays on screen; packaged builds also use the movie player) and fades out
 * once the new level is in.
 */
UCLASS()
class KEEPERSGAME_API UKeepersLoadingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UKeepersLoadingSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void OpenLevel(FName Level);

	/** Shows the screen for a level without travelling (for screenshots). */
	UFUNCTION(BlueprintCallable, Category = "Keepers")
	void Preview(FName Level, float Seconds);

	/** Preview for scripts that cannot reach game instance subsystems. */
	UFUNCTION(BlueprintCallable, Category = "Keepers", meta = (WorldContext = "WorldContextObject"))
	static void PreviewLoadingScreen(const UObject* WorldContextObject, FName Level, float Seconds);

private:
	struct FView;

	TSharedRef<SWidget> BuildWidget(FName Level);
	void Show(FName Level);
	void Hide();
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* World);
	bool TickFade(float DeltaTime);

	TSharedPtr<FView> View;
	TSharedPtr<SWidget> Widget;
	FName PendingLevel;
	bool bTravelling = false;
	float HoldTime = 0.f;
	int32 TipIndex = 0;
	FTSTicker::FDelegateHandle TickHandle;

	UPROPERTY()
	TObjectPtr<UTexture2D> Background;
};
