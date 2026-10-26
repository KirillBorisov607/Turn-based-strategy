#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersBattleObjective.generated.h"

class UTextBlock;
class UStaticMeshComponent;
class UPointLightComponent;

/** Goals the campaign and the quick battles give to a level that has none of its own. */
UENUM(BlueprintType)
enum class EKeepersObjectiveKind : uint8
{
	None,
	/** A hero must reach the beacon on the far side. */
	Reach,
	/** Defeat the enemies' leader. */
	Leader,
	/** Hold out for a few rounds. */
	Survive,
	/** Destroy the chickens' totem: it does not move, it heals the enemies around it. */
	Totem
};

/** Top-of-screen panel: objective title and progress. Built in C++, no widget asset needed. */
UCLASS()
class KEEPERSGAME_API UKeepersObjectiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLines(const FText& Title, const FText& Progress);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ProgressText;

	FText PendingTitle;
	FText PendingProgress;
};

/**
 * Battle goals beyond "defeat every enemy". Place one in a combat level.
 *
 *  - SurviveTurns > 0: the heroes win when that many rounds are over (the enemies retreat).
 *  - ProtectedUnit set: the heroes lose if it dies. If it is not team 0 it is moved to team 0;
 *    it is played by the AI, so the enemies hunt it like any hero.
 *  - bReachBeacon: the heroes win when one of them steps on the beacon (far from where they start);
 *    the enemies guard it.
 *  - bDefeatLeader: the heroes win when the leader falls (the strongest enemy, tougher than usual);
 *    the leader keeps back and the others guard it.
 *  Defeating every enemy still wins, and losing every hero still loses, as before.
 *  The enemies' AI reads the objective (KeepersAiLibrary): it hunts the protected unit, rushes when time runs out,
 *  guards the beacon and the leader.
 */
UCLASS()
class KEEPERSGAME_API AKeepersBattleObjective : public AActor
{
	GENERATED_BODY()

public:
	AKeepersBattleObjective();

	/** Rounds to hold out; 0 = no time limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	int32 SurviveTurns = 0;

	/** A BP_Unit that must stay alive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<AActor> ProtectedUnit;

	/** Name shown in the objective, e.g. "курицу Пеструшку". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText ProtectedName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bReachBeacon = false;

	/** Beacon cell; left at (-1, -1) the farthest cell the heroes can walk to is chosen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FIntPoint BeaconIndex = FIntPoint(-1, -1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bDefeatLeader = false;

	/** The leader; empty — the toughest enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<AActor> Leader;

	/** With bDefeatLeader: the leader is a totem (empty Leader — the enemy furthest from the heroes). It stands still,
	 *  does not act and heals the enemies around it at the start of each of their turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bTotem = false;

	/** The objective of the level, if any (cached per frame). */
	static AKeepersBattleObjective* Find(const UWorld* World);
	/** The next battle level without an objective of its own gets this one. */
	static void RequestForNextBattle(EKeepersObjectiveKind Kind);
	static EKeepersObjectiveKind TakeRequest();
	/** The objective given to the current battle (to play it again). */
	static EKeepersObjectiveKind LastGiven;

	bool IsTimed() const { return SurviveTurns > 0; }
	AActor* GetProtected() const { return ProtectedUnit; }
	AActor* GetLeader() const { return bDefeatLeader ? Leader.Get() : nullptr; }
	bool GetBeacon(FIntPoint& OutIndex) const { OutIndex = BeaconIndex; return bReachBeacon && bSetUp && BeaconIndex.X >= 0; }

	/** A saved battle goes on from this round. */
	void RestoreRound(int32 InRound);

	/** Current round (1 = the first player phase). */
	UFUNCTION(BlueprintPure, Category = "Objective")
	int32 GetRound() const { return Round; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void UpdateWidget();
	void Finish(bool bHeroesWin);
	/** When combat starts: the beacon cell and its marker, the leader and its crown. */
	void SetUp();
	/** The leader becomes the totem: still, tough, hidden under a carved chicken pole. */
	void MakeTotem();
	/** The enemies' turn begins: the totem heals those around it. */
	void TotemHeals();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Beacon;
	UPROPERTY()
	TObjectPtr<UPointLightComponent> BeaconLight;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Crown;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BeaconGem;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TotemParts;
	UPROPERTY()
	TObjectPtr<UPointLightComponent> TotemLight;
	bool bTotemFallen = false;
	bool bSetUp = false;
	FString LeaderName;

	UPROPERTY()
	TObjectPtr<UObject> CombatSystem;

	UPROPERTY()
	TObjectPtr<UKeepersObjectiveWidget> Widget;

	int32 Round = 0;
	int32 LastTeam = INDEX_NONE;
	bool bProtectedSetUp = false;
	bool bFinished = false;
};

/** Spawns the requested objective (AKeepersBattleObjective::RequestForNextBattle) in a battle level without one. */
UCLASS()
class KEEPERSGAME_API UKeepersObjectiveSpawner : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
