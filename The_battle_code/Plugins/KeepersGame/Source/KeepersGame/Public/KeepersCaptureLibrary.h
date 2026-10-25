#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "KeepersCaptureLibrary.generated.h"

/** Pictures of the running game from any point of view, for tests, documentation and loading screens. */
UCLASS()
class KEEPERSGAME_API UKeepersCaptureLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Renders the world from Location/Rotation into a PNG file (absolute path); ExposureBias brightens (stops). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Capture", meta = (WorldContext = "WorldContextObject"))
	static bool CaptureToPng(const UObject* WorldContextObject, FVector Location, FRotator Rotation, float Fov, int32 Width, int32 Height, const FString& FilePath, float ExposureBias = 0.f);

	/** Points the player's view at Location/Rotation through a helper camera (the real render, for shots). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Capture", meta = (WorldContext = "WorldContextObject"))
	static void ViewFrom(const UObject* WorldContextObject, FVector Location, FRotator Rotation, float Fov);

	/** Screenshot of the game viewport exactly as drawn, with all its widgets (only the viewport, not the desktop). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Capture", meta = (WorldContext = "WorldContextObject"))
	static bool CaptureViewport(const UObject* WorldContextObject, const FString& FilePath);

	/** The first actor whose name contains Part, in the world of WorldContextObject. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Capture", meta = (WorldContext = "WorldContextObject"))
	static AActor* FindActorByName(const UObject* WorldContextObject, const FString& Part);
};
