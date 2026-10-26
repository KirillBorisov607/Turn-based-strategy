#pragma once

#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KeepersCampaign.generated.h"

/** What happens at a node of the region map. */
UENUM(BlueprintType)
enum class EKeepersNodeType : uint8
{
	Battle,
	Elite,
	Ruins,
	Camp,
	Boss,
	/** An encounter with a choice: gold, relics, gear, glory — or a fight. */
	Event
};

/** One node of the region map. */
USTRUCT(BlueprintType)
struct FKeepersMapNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame)
	EKeepersNodeType Type = EKeepersNodeType::Battle;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	int32 Column = 0;

	/** Vertical position inside the column, 0..1. */
	UPROPERTY(BlueprintReadOnly, SaveGame)
	float Height = 0.5f;

	/** Indexes of the nodes in the next column this node leads to. */
	UPROPERTY(BlueprintReadOnly, SaveGame)
	TArray<int32> Next;

	/** Battle level for Battle/Elite/Boss nodes. */
	UPROPERTY(BlueprintReadOnly, SaveGame)
	FName Level;

	/** EKeepersObjectiveKind the battle gets if its level has no objective of its own. */
	UPROPERTY(BlueprintReadOnly, SaveGame)
	uint8 Objective = 0;
};

/** A unit of a saved battle. */
USTRUCT()
struct FKeepersUnitSnapshot
{
	GENERATED_BODY()

	/** Actor name: level units and units spawned in the same order keep their names. */
	UPROPERTY(SaveGame)
	FString Name;

	UPROPERTY(SaveGame)
	FIntPoint Index = FIntPoint(-1, -1);

	UPROPERTY(SaveGame)
	int32 Hp = 0;
};

/** A campaign battle at the start of a round: who stands where with what health, and what the field became. */
USTRUCT()
struct FKeepersBattleSnapshot
{
	GENERATED_BODY()

	/** Node of the battle; INDEX_NONE — nothing saved. */
	UPROPERTY(SaveGame)
	int32 Node = INDEX_NONE;

	UPROPERTY(SaveGame)
	int32 Round = 0;

	/** Units alive at the save; the others have fallen. */
	UPROPERTY(SaveGame)
	TArray<FKeepersUnitSnapshot> Units;

	UPROPERTY(SaveGame)
	TArray<FIntPoint> Rocks;

	UPROPERTY(SaveGame)
	TArray<FIntPoint> Forests;

	UPROPERTY(SaveGame)
	TArray<FIntPoint> Ice;

	/** The battle is won; only the reward was left. */
	UPROPERTY(SaveGame)
	bool bWon = false;
};

/** A relic found during the campaign; it changes a rule for the rest of the run. */
USTRUCT(BlueprintType)
struct FKeepersArtifact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName Id;

	UPROPERTY(BlueprintReadOnly)
	FText Name;

	UPROPERTY(BlueprintReadOnly)
	FText Description;
};

/** Saved campaign. */
UCLASS()
class KEEPERSGAME_API UKeepersCampaignSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 Seed = 0;

	UPROPERTY(SaveGame)
	TArray<FKeepersMapNode> Nodes;

	UPROPERTY(SaveGame)
	int32 CurrentNode = INDEX_NONE;

	UPROPERTY(SaveGame)
	TArray<int32> VisitedNodes;

	UPROPERTY(SaveGame)
	TArray<FName> Artifacts;

	/** Gold of the run: battles and ruins bring it, the camp's smith and merchant take it. */
	UPROPERTY(SaveGame)
	int32 Gold = 30;

	/** Gear ranks bought at the smith, per hero ("Warrior" -> rank). */
	UPROPERTY(SaveGame)
	TMap<FName, int32> ArmorRanks;

	UPROPERTY(SaveGame)
	TMap<FName, int32> BootsRanks;

	/** Hero growth in this run: what each level-up gave ("Warrior" -> count). */
	UPROPERTY(SaveGame)
	TMap<FName, int32> GrowthHp;

	UPROPERTY(SaveGame)
	TMap<FName, int32> GrowthMove;

	UPROPERTY(SaveGame)
	TMap<FName, int32> GrowthAction;

	/** Abilities learned on level-ups, as "Hero:Spell". */
	UPROPERTY(SaveGame)
	TArray<FName> LearnedSpells;

	/** Heroes who fell in the last won battle: they sit the next battle out. */
	UPROPERTY(SaveGame)
	TArray<FName> Wounded;

	/** Recruits hired at a camp: the wounded hero they stand in for, their unit type and name. */
	UPROPERTY(SaveGame)
	TArray<FName> RecruitFor;

	UPROPERTY(SaveGame)
	TArray<FName> RecruitBase;

	UPROPERTY(SaveGame)
	TArray<FString> RecruitNames;

	/** The battle in progress, saved at the start of its last round. */
	UPROPERTY(SaveGame)
	FKeepersBattleSnapshot Battle;

	/** Region of the run (KeepersRegions), from 0. */
	UPROPERTY(SaveGame)
	int32 Region = 0;

	/** Peaceful choices towards the chickens: three open the pact ending. */
	UPROPERTY(SaveGame)
	int32 Peace = 0;
};

/** What a hero learns on a level-up. */
enum class EKeepersGrowth : uint8
{
	/** +5 HP. */
	Hp,
	/** +1 movement point. */
	Move,
	/** +1 action point. */
	Action
};

/** Gear the camp's smith improves. */
enum class EKeepersGear : uint8
{
	/** +HP per rank. */
	Armor,
	/** +1 movement point per rank. */
	Boots
};

/**
 * The run: a region map of nodes (battles, ruins, camps, a boss), artifacts and saving.
 * Lives for the whole game session, across level loads.
 */
UCLASS()
class KEEPERSGAME_API UKeepersCampaignSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UKeepersCampaignSubsystem* Get(const UObject* WorldContext);

	/** The campaign of the running game (for Blueprints and editor scripts). */
	UFUNCTION(BlueprintPure, Category = "Keepers|Campaign", meta = (WorldContext = "WorldContextObject"))
	static UKeepersCampaignSubsystem* GetCampaign(const UObject* WorldContextObject) { return Get(WorldContextObject); }

	/** Level that shows the region map. */
	static const FName MapLevel;
	static const FName MainMenuLevel;
	static const FString SaveSlot;

	UFUNCTION(BlueprintCallable, Category = "Keepers|Campaign")
	void StartNewCampaign();

	/** Loads the saved run and opens the region map. Returns false if there is no save. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Campaign")
	bool ContinueCampaign();

	UFUNCTION(BlueprintPure, Category = "Keepers|Campaign")
	static bool HasSavedCampaign();

	UFUNCTION(BlueprintPure, Category = "Keepers|Campaign")
	bool IsActive() const { return bActive; }

	/** The player picked a node on the map. Battles open their level; ruins and camps show a choice. */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Campaign")
	void EnterNode(int32 NodeIndex);

	/** Nodes the player can pick now. */
	UFUNCTION(BlueprintPure, Category = "Keepers|Campaign")
	TArray<int32> GetAvailableNodes() const;

	const TArray<FKeepersMapNode>& GetNodes() const { return Save->Nodes; }
	const TArray<int32>& GetVisitedNodes() const { return Save->VisitedNodes; }
	int32 GetCurrentNode() const { return Save->CurrentNode; }
	int32 GetSeed() const { return Save ? Save->Seed : 0; }
	bool IsInBattleNode() const;
	bool IsBossNode(int32 NodeIndex) const;

	/** Artifacts. */
	UFUNCTION(BlueprintPure, Category = "Keepers|Campaign")
	bool HasArtifact(FName Id) const;
	static bool HasArtifact(const UObject* WorldContext, FName Id);
	const TArray<FName>& GetArtifacts() const { return Save->Artifacts; }
	void AddArtifact(FName Id);
	static const TArray<FKeepersArtifact>& GetAllArtifacts();
	static const FKeepersArtifact* FindArtifact(FName Id);
	/** Up to Count random artifacts the player does not have yet. */
	TArray<FName> RollArtifacts(int32 Count) const;

	/** Gold of the run. */
	int32 GetGold() const { return Save ? Save->Gold : 0; }
	void AddGold(int32 Amount);
	/** Spends gold; false when there is not enough. */
	bool SpendGold(int32 Amount);

	/** Gear of a hero in this run. */
	int32 GetGearRank(FName Hero, EKeepersGear Gear) const;
	static int32 MaxGearRank(EKeepersGear Gear) { return Gear == EKeepersGear::Armor ? 3 : 2; }
	/** Price of the next rank. */
	static int32 GearCost(EKeepersGear Gear, int32 NextRank) { return Gear == EKeepersGear::Armor ? 25 * NextRank : 40 * NextRank; }
	static int32 ArmorHpPerRank() { return 6; }
	/** Buys the next rank; false when maxed or too poor. */
	bool UpgradeGear(FName Hero, EKeepersGear Gear);

	/** Hero levels of the run: 1 + every level-up; a hero grows after won battles up to MaxHeroLevel. */
	static constexpr int32 MaxHeroLevel = 5;
	int32 GetHeroLevel(FName Hero) const;
	int32 GetGrowth(FName Hero, EKeepersGrowth Growth) const;
	/** The party hero who grows after this victory (the least grown one), None when all are at the top. */
	FName NextHeroToLevel(const TArray<FName>& Party) const;
	void LevelUp(FName Hero, EKeepersGrowth Growth);

	/** Levels on which a hero learns an ability instead of growing. */
	static bool IsSpellLevel(int32 Level) { return Level == 3 || Level == 5; }
	/** Learnable abilities the hero has not learned yet. */
	TArray<FName> GetSpellsToLearn(FName Hero) const;
	TArray<FName> GetLearnedSpells(FName Hero) const;
	/** A level-up that teaches an ability. */
	void LearnSpell(FName Hero, FName Spell);

	/** Wounded heroes sit the next battle out, unless a healer at a camp helps. */
	bool IsWounded(FName Hero) const { return Save && Save->Wounded.Contains(Hero); }
	const TArray<FName>& GetWounded() const { return Save->Wounded; }
	/** After a won battle: those who sat out are well again, the fallen are wounded. */
	void ResolveWounds(const TArray<FName>& SatOut, const TArray<FName>& Fallen);
	static int32 HealerCost() { return 30; }
	static int32 RecruitCost() { return 15; }
	/** The camp's healer: the hero is well at once. */
	bool HealWound(FName Hero);
	/** A recruit stands in for a wounded hero in the next battle; returns the recruit's name. */
	FString HireRecruit(FName WoundedHero);
	/** The recruit standing in for a hero: false when there is none. */
	bool GetRecruit(FName WoundedHero, FName& OutBase, FString& OutName) const;

	/** Gives the next rank for free (events); false when maxed. */
	bool GrantGear(FName Hero, EKeepersGear Gear);

	/** An event turned into a fight: the current node gets a battle level and opens it. */
	void StartEventBattle(bool bMercenaries);

	/** Price of an artifact at the camp's merchant. */
	static int32 ArtifactPrice() { return 45; }

	/** Set by the quick battle "Наёмники кур": the next battle gets a band of mercenaries. */
	bool bMercenaryQuickBattle = false;

	/** The region of the run and its rules. */
	int32 GetRegion() const { return Save ? Save->Region : 0; }
	const struct FKeepersRegion& GetRegionDef() const;
	bool IsFinalRegion() const;
	/** Test hook: the run jumps to a region with a new map. */
	void TestSetRegion(int32 Region);
	/** Test hook: enters a node whether it is open or not; -1 is the boss. */
	void TestEnterNode(int32 NodeIndex);
	int32 GetPeace() const { return Save ? Save->Peace : 0; }
	void AddPeace();

	/** The map shows the region's introduction once after a new region begins. */
	bool bRegionIntroPending = false;

	/** The battle in progress: saved every round, resumed by ContinueCampaign. */
	void SaveBattle(const FKeepersBattleSnapshot& Snapshot);
	/** The saved battle to put back after "Продолжить", or nullptr. */
	const FKeepersBattleSnapshot* GetBattleToResume() const;
	void ForgetBattle();
	/** The battle is won: "Продолжить" counts it even if the reward screen was left open. */
	void MarkBattleWon();
	/** Set by ContinueCampaign when it opens an interrupted battle. */
	bool bResumeBattle = false;

	/** Enemies (E_UnitType) who join the next battle, whatever it is (quick battles, tests). */
	TArray<uint8> NextBand;

	/** The next battle gets a band of mercenaries (for scripts and tests; the quick battle sets it too). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Campaign", meta = (WorldContext = "WorldContextObject"))
	static void SetMercenaryBattle(const UObject* WorldContextObject, bool bEnabled);

	/** Before a generated battle: the node's seed and biome for the generator. */
	void PrepareGeneratedField(int32 NodeIndex) const;

	/** Gold a battle node brings. */
	static int32 GoldFor(EKeepersNodeType Type);

	/** Node finished: marks it visited, saves and returns to the map (or ends the run after the boss). */
	void CompleteCurrentNode();

	/** The heroes lost: the run is over. */
	void FailCampaign();

	/** Loads the saved run, or starts a new one, without changing level (for opening the map level directly). */
	void EnsureLoaded();

	/** Plays the current battle again. */
	void RetryCurrentNode();

	/** Back to the main menu, keeping the save. */
	void ReturnToMainMenu(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	void GenerateMap(int32 Seed);
	void WriteSave() const;
	void OpenLevel(FName Level) const;

	UPROPERTY()
	TObjectPtr<UKeepersCampaignSave> Save;

	bool bActive = false;
};
