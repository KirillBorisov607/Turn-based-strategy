#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersMapGenerator.generated.h"

/** Biomes of generated battle fields. */
UENUM(BlueprintType)
enum class EKeepersBiome : uint8
{
	Meadow,
	Forest,
	Swamp,
	Mountains,
	Winter,
	Cursed
};

/** One cell of a generated field. */
struct FKeepersGenCell
{
	FIntPoint Index;
	FVector Location;
	/** E_TileType: 0 none (hole), 1 normal, 2 obstacle, 3 double cost, 4 triple cost. */
	uint8 Type = 1;
	/** Height level, 0..2 (100 cm each). */
	int32 Level = 0;
	bool bTree = false;
	bool bRock = false;
	bool bPuddle = false;
	/** A wooden bridge over water. */
	bool bBridge = false;
	/** Yaw of a bridge or a fence, degrees. */
	float Yaw = 0.f;
	/** A house of the Toony set (index into the generator's list), INDEX_NONE for none. */
	int32 House = INDEX_NONE;
	bool bFence = false;
	bool bBush = false;
	/** A raised coloured block (obstacle), like the stacks on the hand-made maps; its colour index. */
	int32 Stack = INDEX_NONE;
	/** Island the cell belongs to (its top pattern), INDEX_NONE for water. */
	int32 Island = INDEX_NONE;
	/** 0 none, 1 player start, 2 enemy start. */
	int32 Start = 0;
};

/**
 * Battle fields from a seed and a biome. The level L_Generated holds only the grid, the combat system,
 * a combat scenario and the units. Before anything begins to play, the generator lays out an island:
 *  1. shape: hexagons in an ellipse, Perlin noise and a radial mask, holes by biome;
 *  2. heights: noise, three levels; a step of more than one level is a cliff;
 *  3. terrain by biome: woods (double cost, trees block sight), rocks (obstacles), puddles;
 *  4. starts on opposite ends, cover near both;
 *  5. checks: every walkable cell is reachable, both starts are connected; otherwise the next sub-seed;
 * then spawns hexagon columns and grid modifiers. BP_Grid.SpawnGrid (through the scenario's
 * QuickStartCombat) traces them and builds the tiles, and the units start on the chosen cells.
 */
UCLASS()
class KEEPERSGAME_API UKeepersMapGenerator : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** The next generated field: Seed 0 picks one at random. */
	static void SetNextMap(int32 Seed, EKeepersBiome Biome);
	static FText BiomeName(EKeepersBiome Biome);
	/** A line about the biome for the loading screen. */
	static FText BiomeBlurb(EKeepersBiome Biome);
	/** The biome the next generated field will have; false when it will be picked at random. */
	static bool GetPendingBiome(EKeepersBiome& OutBiome);
	/** Level of generated battles. */
	static const FName Level;

	/** Lays out cells only (no actors): for checks and tests. Returns false if no valid field was found. */
	static bool Layout(int32 Seed, EKeepersBiome Biome, TFunctionRef<FVector(const FIntPoint&)> LocationOf, int32 CountX, int32 CountY, TArray<FKeepersGenCell>& OutCells);

	int32 GetSeed() const { return UsedSeed; }
	EKeepersBiome GetBiome() const { return UsedBiome; }

private:
	void Generate(UWorld& World);

	int32 UsedSeed = 0;
	EKeepersBiome UsedBiome = EKeepersBiome::Meadow;
};

/** Scripts and tests: choose the next map, check many seeds. */
UCLASS()
class KEEPERSGAME_API UKeepersMapGenLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Keepers|Generation")
	static void SetNextGeneratedMap(int32 Seed, EKeepersBiome Biome);

	/** Lays out Count seeds of a biome on a plain hex layout; returns how many passed the checks. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Generation")
	static int32 CheckSeeds(EKeepersBiome Biome, int32 FirstSeed, int32 Count);
};
