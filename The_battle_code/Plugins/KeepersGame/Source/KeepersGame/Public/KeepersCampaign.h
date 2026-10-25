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
	Boss
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
