#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "KeepersInputSimulator.generated.h"

/**
 * Sends real Slate mouse and keyboard events to a running game viewport (PIE), so editor scripts can
 * play the game the way a person does: through the viewport, input modes, focus and input bindings.
 */
UCLASS()
class UKeepersInputSimulator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Moves the mouse over a world location and clicks it (left or right button). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Testing", meta = (WorldContext = "WorldContext"))
	static bool ClickWorldLocation(UObject* WorldContext, FVector Location, bool bRightButton = false);

	/** Clicks a point of the viewport given as a fraction of its size (0..1). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Testing", meta = (WorldContext = "WorldContext"))
	static bool ClickViewport(UObject* WorldContext, FVector2D Fraction, bool bRightButton = false);

	/** Moves the mouse to a viewport point (fraction 0..1). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Testing", meta = (WorldContext = "WorldContext"))
	static bool MoveMouse(UObject* WorldContext, FVector2D Fraction);

	/** Mouse wheel over the middle of the viewport (positive = up). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Testing", meta = (WorldContext = "WorldContext"))
	static bool ScrollWheel(UObject* WorldContext, float Delta);

	/** Presses or releases a key by name ("W", "E", "Escape"). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Testing")
	static bool SetKey(FName Key, bool bDown);
};
