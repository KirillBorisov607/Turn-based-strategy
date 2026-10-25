#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "KeepersEditorLibrary.generated.h"

class UUserDefinedEnum;

/** Editor helpers that Python scripts use to change content the Python API cannot reach. */
UCLASS()
class UKeepersEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Appends an entry to a Blueprint enum (E_Spell, E_TileType, ...) and returns its index,
	 * or the existing index when an entry with that display name is already there.
	 */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Editor")
	static int32 AddUserEnumEntry(UUserDefinedEnum* Enum, const FString& DisplayName);
};
