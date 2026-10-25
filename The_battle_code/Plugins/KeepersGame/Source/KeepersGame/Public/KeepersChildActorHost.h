#pragma once

#include "GameFramework/Actor.h"
#include "KeepersChildActorHost.generated.h"

/**
 * Base class for Blueprints that keep references to their Child Actor Components' actors.
 *
 * In UE4 the project's Blueprints (BP_Grid, BP_GridVisual, BP_DebugMenu) cached those references in
 * their construction script, e.g. GridVisual = ChildActor_GridVisual.ChildActor. In UE5 child actors are
 * recreated when a level loads, so the cached references end up None at runtime.
 *
 * This class re-links them before any BeginPlay runs:
 *  - a component named "ChildActor_X" fills the Blueprint variable "X" when the types match;
 *  - an empty object variable on the child actor that can hold this actor (for example Grid) is
 *    pointed back at this actor.
 */
UCLASS(Abstract, Blueprintable)
class KEEPERSGAME_API AKeepersChildActorHost : public AActor
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;

	/** Re-links child actor references; safe to call again after child actors are recreated. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Child Actors")
	void RefreshChildActorReferences();
};
