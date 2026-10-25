#pragma once

#include "Engine/DataTable.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersLevelKit.generated.h"

class UTexture2D;

/** Where a hand-made battle level is used. */
UENUM(BlueprintType)
enum class EKeepersLevelKind : uint8
{
	/** An ordinary battle node of the campaign. */
	Battle,
	/** An elite battle node. */
	Elite,
	/** Only in the quick battle menu. */
	QuickOnly
};

/**
 * A row of DT_KeepersLevels: one hand-made battle level. The level kit adds the row when it creates a
 * level; the campaign, the quick battle menu and the loading screen read it, so a new level needs no code.
 */
USTRUCT(BlueprintType)
struct KEEPERSGAME_API FKeepersLevelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	TSoftObjectPtr<UWorld> Level;

	/** Name on the loading screen and in the quick battle menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FText Title;

	/** A line under the name: what the battle is about. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (MultiLine = true))
	FText Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	EKeepersLevelKind Kind = EKeepersLevelKind::Battle;

	/** Listed in the main menu's quick battles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	bool bQuickBattle = true;

	/**
	 * Units start on the cells they stand on in the level (no need to type cell indexes into the scenario);
	 * units placed in the level but missing from the scenario's teams join them: tag "Enemy" — the enemies.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	bool bStartsFromPlacement = true;

	/** Picture for the loading screen; empty — /Game/Keepers/Loading/T_Load_<level>, or the common one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	TSoftObjectPtr<UTexture2D> LoadingImage;
};

/** The grid of a battle level, read from its BP_CombatScenario and BP_Grid; works in the editor too. */
struct KEEPERSGAME_API FKeepersLevelGrid
{
	AActor* Grid = nullptr;
	AActor* Scenario = nullptr;
	FVector TileSize = FVector(200.f, 200.f, 100.f);
	FIntPoint Count = FIntPoint(10, 10);
	/** E_GridShape: 1 square, 2 hexagon, 3 triangle. */
	uint8 Shape = 2;

	bool IsHex() const { return Shape == 2; }
};

namespace KeepersLevelKit
{
	KEEPERSGAME_API extern const TCHAR* TablePath;

	KEEPERSGAME_API UDataTable* GetTable();
	/** The row of a level by its package ("/Game/Maps/Battles/L_Bridge") or its short name. */
	KEEPERSGAME_API const FKeepersLevelRow* FindRow(FName Level);
	KEEPERSGAME_API TArray<const FKeepersLevelRow*> GetRows();

	/** Finds the grid and the scenario and gives the grid the scenario's size, so the grid's own functions work. */
	KEEPERSGAME_API bool PrepareGrid(UWorld* World, FKeepersLevelGrid& Out);
	KEEPERSGAME_API FVector TileLocation(const FKeepersLevelGrid& Grid, const FIntPoint& Index);
	/** Cell under a point; false outside the grid. */
	KEEPERSGAME_API bool TileIndex(const FKeepersLevelGrid& Grid, const FVector& Location, FIntPoint& OutIndex);
	KEEPERSGAME_API TArray<FIntPoint> AllIndexes(const FKeepersLevelGrid& Grid);
	KEEPERSGAME_API TArray<FIntPoint> Neighbors(const FKeepersLevelGrid& Grid, const FIntPoint& Index);
	/** What the grid finds on a cell when combat starts: E_TileType (0 — no tile) and the ground point. */
	KEEPERSGAME_API uint8 TraceTile(const FKeepersLevelGrid& Grid, const FIntPoint& Index, FVector& OutGround);

	/** Units of the scenario's teams. */
	KEEPERSGAME_API TArray<TArray<AActor*>> GetTeams(const FKeepersLevelGrid& Grid);
	/** All BP_Unit actors of the world. */
	KEEPERSGAME_API TArray<AActor*> GetLevelUnits(UWorld* World);
	/** Adds placed units missing from the teams (tag "Enemy" — team 1, otherwise 0); returns how many. */
	KEEPERSGAME_API int32 CollectUnits(const FKeepersLevelGrid& Grid);
	/** Every unit of the teams starts on the cell it stands on (the scenario's UnitDesiredIndexes). */
	KEEPERSGAME_API int32 StartUnitsWhereTheyStand(const FKeepersLevelGrid& Grid);
}

/** Levels of the kit: units start where they were placed. Runs before the scenario begins combat. */
UCLASS()
class KEEPERSGAME_API UKeepersLevelSetupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
