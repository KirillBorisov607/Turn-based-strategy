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

	/** Level kit: a new battle level from the template, added to the game. Returns its package. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Editor")
	static FString CreateBattleLevel(const FString& Title);

	/** Level kit: the check of the open level, as text. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Editor")
	static FString CheckBattleLevel();
};
