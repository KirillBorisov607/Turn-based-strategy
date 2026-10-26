#pragma once

#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "KeepersFigure.h"
#include "KeepersHeroes.generated.h"

class ACharacter;
class UTextureRenderTarget2D;

/** An ability as the squad screen and tooltips describe it. */
struct FKeepersSpellInfo
{
	/** E_Spell entry name ("SwordSlash"). */
	FName Id;
	FText Name;
	FText Description;
	/** Short numbers line: damage, range, cooldown. */
	FText Numbers;
	/** How a figure shows the ability. */
	EKeepersPose Pose = EKeepersPose::Attack;
};

/** A hero of the roster. */
struct FKeepersHeroDef
{
	/** Row of DT_DefaultUnitDataPerType for playable heroes ("Warrior"); a plain id for heroes still to come. */
	FName Id;
	FText Name;
	FText Class;
	FText Role;
	FText Description;
	/** E_UnitType of the hero in battle, INDEX_NONE while the hero has no battle unit yet. */
	int32 UnitType = INDEX_NONE;
	/** How the hero is opened, for locked cards. */
	FText Unlock;
	TArray<FName> Spells;
	/** Abilities the hero can learn on the run's level-ups (UKeepersCampaignSubsystem::IsSpellLevel). */
	TArray<FName> LearnableSpells;
	TArray<FKeepersLook> Looks;
	/** Enemies of the Toony Tiny RTS Set (bandits, brutes): not in the roster, every unit picks one of the looks. */
	bool bEnemy = false;
	/** Size of the figure relative to a hero. */
	float Scale = 1.f;
	/** Stats for heroes without a data table row. */
	int32 Hp = 0;
	int32 Mp = 0;
	int32 Ap = 0;
};

/** Hero stats as the battle uses them. */
struct FKeepersHeroStats
{
	int32 Hp = 0;
	int32 Mp = 0;
	int32 Ap = 0;
};

namespace KeepersHeroes
{
	KEEPERSGAME_API const TArray<FKeepersHeroDef>& All();
	KEEPERSGAME_API const FKeepersHeroDef* Find(FName Id);
	/** Toony enemies (bandits, archers, brutes). */
	KEEPERSGAME_API const TArray<FKeepersHeroDef>& Enemies();
	/** A hero or a Toony enemy by id. */
	KEEPERSGAME_API const FKeepersHeroDef* FindAny(FName Id);
	KEEPERSGAME_API const FKeepersHeroDef* FindByUnitType(int32 UnitType);
	KEEPERSGAME_API const FKeepersSpellInfo* FindSpell(FName Id);

	/** HP, movement and action points: from DT_DefaultUnitDataPerType when the hero has a row. */
	KEEPERSGAME_API FKeepersHeroStats Stats(const FKeepersHeroDef& Hero);

	/** E_UnitType of a battle unit, 255 if it has none. */
	KEEPERSGAME_API uint8 UnitTypeOf(const UObject* Unit);
}

/** One hero's choices on the squad screen. */
USTRUCT()
struct FKeepersHeroChoice
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName Hero;

	UPROPERTY(SaveGame)
	int32 Look = 0;

	UPROPERTY(SaveGame)
	FName Color = TEXT("Blue");
};

UCLASS()
class KEEPERSGAME_API UKeepersPartySave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	TArray<FKeepersHeroChoice> Choices;

	/** Heroes in the squad, in slot order. */
	UPROPERTY(SaveGame)
	TArray<FName> Party;

	/** Glory: earned in every run, won or lost, and spent on outfits. */
	UPROPERTY(SaveGame)
	int32 Glory = 60;

	/** Opened outfits as "Hero:Index"; the first outfit of every hero is always open. */
	UPROPERTY(SaveGame)
	TArray<FName> UnlockedLooks;

	/** Heroes opened beyond the first three (the Mage, the Rogue, the Barbarian). */
	UPROPERTY(SaveGame)
	TArray<FName> UnlockedHeroes;
};

/**
 * The squad: which heroes go on the run and how each of them looks. Saved between sessions.
 * Battles read it to dress the hero units (UKeepersHeroLookSubsystem).
 */
UCLASS()
class KEEPERSGAME_API UKeepersPartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UKeepersPartySubsystem* Get(const UObject* WorldContext);

	static constexpr int32 SquadSize = 3;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Whether the hero can be taken into the squad now. */
	bool IsUnlocked(FName Hero) const;

	/** Opens a hero for good (a region cleared, an event); returns true the first time. */
	bool UnlockHero(FName Hero);

	/** Test hook: opens the heroes and makes them the squad ("Mage,Rogue,Barbarian"). */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Party", meta = (WorldContext = "WorldContextObject"))
	static void TestSetParty(const UObject* WorldContextObject, const FString& Heroes);

	const TArray<FName>& GetParty() const { return Save->Party; }
	bool IsInParty(FName Hero) const { return Save->Party.Contains(Hero); }
	/** Puts the hero into the first free slot; false when the squad is full or the hero is locked. */
	bool AddToParty(FName Hero);
	void RemoveFromParty(FName Hero);

	int32 GetLookIndex(FName Hero) const;
	void SetLookIndex(FName Hero, int32 Look);
	FName GetColor(FName Hero) const;
	void SetColor(FName Hero, FName Color);

	/** The look a hero wears now. */
	FKeepersLook GetLook(FName Hero) const;

	int32 GetGlory() const { return Save->Glory; }
	void AddGlory(int32 Amount);
	bool IsLookUnlocked(FName Hero, int32 Look) const;
	/** Price of an outfit in glory: the further down the list, the dearer. */
	static int32 LookCost(int32 Look) { return Look * 25; }
	/** Spends glory on an outfit; false when there is not enough. */
	bool UnlockLook(FName Hero, int32 Look);

	void WriteSave() const;

	/** Recordings and tests change the squad without touching the player's save. */
	bool bTestSquad = false;

	/** Test hook: forgets opened heroes and brings the first three back. */
	void ResetHeroes();

private:
	FKeepersHeroChoice& Choice(FName Hero);
	const FKeepersHeroChoice* FindChoice(FName Hero) const;

	UPROPERTY()
	TObjectPtr<UKeepersPartySave> Save;
};

/**
 * Dresses battle units in the Toony Tiny RTS Set: every hero unit (Страж, Следопыт, Друид) gets a figure
 * assembled from parts, the old mesh is hidden but keeps animating, and the figure mirrors its animation
 * state (idle, walk, attack, hit, death). Player heroes wear the squad's looks and colours, enemy heroes red.
 */
UCLASS()
class KEEPERSGAME_API UKeepersHeroLookSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/**
	 * Live portrait of a hero as it looks in this battle (the player's outfit, or the enemy one), filmed in a
	 * small studio high above the field; the battle HUD shows it instead of the old icons.
	 */
	UTextureRenderTarget2D* GetPortrait(FName Hero, bool bEnemy);

private:
	void Dress(ACharacter* Unit);
	/** Outfit and colour of a hero unit of a team. */
	void LookFor(const struct FKeepersHeroDef& Hero, int32 Team, int32 Variant, FKeepersLook& OutLook, FName& OutColor) const;

	struct FDressed
	{
		TWeakObjectPtr<ACharacter> Unit;
		TWeakObjectPtr<UKeepersFigureComponent> Figure;
		uint8 LastState = 255;
	};

	TArray<FDressed> Dressed;

	UPROPERTY()
	TObjectPtr<AActor> Studio;

	UPROPERTY()
	TMap<FName, TObjectPtr<UTextureRenderTarget2D>> Portraits;
	TSet<TWeakObjectPtr<ACharacter>> Seen;
	float ScanTimer = 0.f;
};
