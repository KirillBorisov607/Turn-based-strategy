"""Glue for the campaign: helpers, forest tracking and artifact effects in terrain/creature systems."""
import os

ROOT = r"C:\ProjectsUE\Turn-based-strategy\The_battle_code\Plugins\KeepersGame\Source\KeepersGame"


def patch(rel, old, new):
    path = os.path.join(ROOT, rel)
    text = open(path, encoding="utf-8-sig").read()
    if new in text:
        return
    assert old in text, f"{rel}: anchor not found: {old[:70]}"
    open(path, "w", encoding="utf-8-sig").write(text.replace(old, new, 1))


def bom(rel):
    path = os.path.join(ROOT, rel)
    text = open(path, encoding="utf-8-sig").read()
    open(path, "w", encoding="utf-8-sig").write(text)


# --- Campaign subsystem: EnsureLoaded, RetryCurrentNode -------------------------------------------------------
patch("Public/KeepersCampaign.h", """	/** Back to the main menu, keeping the save. */""", """	/** Loads the saved run, or starts a new one, without changing level (for opening the map level directly). */
	void EnsureLoaded();

	/** Plays the current battle again. */
	void RetryCurrentNode();

	/** Back to the main menu, keeping the save. */""")
patch("Private/KeepersCampaign.cpp", """void UKeepersCampaignSubsystem::ReturnToMainMenu(const UObject* WorldContext)""", """void UKeepersCampaignSubsystem::EnsureLoaded()
{
	if (bActive)
	{
		return;
	}
	UKeepersCampaignSave* Loaded = Cast<UKeepersCampaignSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (Loaded && Loaded->Nodes.Num() > 0)
	{
		Save = Loaded;
	}
	else
	{
		Save = NewObject<UKeepersCampaignSave>(this);
		GenerateMap(FMath::Rand());
		WriteSave();
	}
	bActive = true;
}

void UKeepersCampaignSubsystem::RetryCurrentNode()
{
	if (Save->Nodes.IsValidIndex(Save->CurrentNode) && !Save->Nodes[Save->CurrentNode].Level.IsNone())
	{
		OpenLevel(Save->Nodes[Save->CurrentNode].Level);
	}
}

void UKeepersCampaignSubsystem::ReturnToMainMenu(const UObject* WorldContext)""")

# --- Reflection: unit stats ------------------------------------------------------------------------------------
patch("Public/KeepersReflection.h", """	/** Sets a Blueprint object variable. */""", """	/** Reads / adds to an int in BP_Unit.UnitData.Stats (HP_Max, MP_Current, AP_Max, ...). */
	KEEPERSGAME_API bool GetUnitStat(UObject* Unit, const FString& Stat, int32& OutValue);
	KEEPERSGAME_API bool AddUnitStat(UObject* Unit, const FString& Stat, int32 Delta);

	/** Sets a Blueprint object variable. */""")
patch("Private/KeepersReflection.cpp", """	bool SetObject(UObject* Object, const FString& Name, UObject* Value)""", """	static int32* FindUnitStat(UObject* Unit, const FString& Stat)
	{
		if (!Unit)
		{
			return nullptr;
		}
		FProperty* DataProperty = nullptr;
		void* Data = FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitData"), &DataProperty);
		const FStructProperty* DataStruct = CastField<FStructProperty>(DataProperty);
		FProperty* StatsProperty = nullptr;
		void* Stats = DataStruct ? FindValuePtr(DataStruct->Struct, Data, TEXT("Stats"), &StatsProperty) : nullptr;
		const FStructProperty* StatsStruct = CastField<FStructProperty>(StatsProperty);
		FProperty* StatProperty = nullptr;
		void* Value = StatsStruct ? FindValuePtr(StatsStruct->Struct, Stats, Stat, &StatProperty) : nullptr;
		return CastField<FIntProperty>(StatProperty) ? static_cast<int32*>(Value) : nullptr;
	}

	bool GetUnitStat(UObject* Unit, const FString& Stat, int32& OutValue)
	{
		if (const int32* Value = FindUnitStat(Unit, Stat))
		{
			OutValue = *Value;
			return true;
		}
		return false;
	}

	bool AddUnitStat(UObject* Unit, const FString& Stat, int32 Delta)
	{
		if (int32* Value = FindUnitStat(Unit, Stat))
		{
			*Value += Delta;
			return true;
		}
		return false;
	}

	bool SetObject(UObject* Object, const FString& Name, UObject* Value)""")

# --- Terrain: forest tracking + artifacts ----------------------------------------------------------------------
patch("Public/KeepersTerrainSubsystem.h", """	/** Pushes the unit on Target one tile away from Origin. Returns false if there was no unit. */""", """	/** Was this tile turned into forest by Sprout? */
	bool IsForest(const FIntPoint& Index) const { return ForestTiles.Contains(Index); }

	/** Pushes the unit on Target one tile away from Origin. Returns false if there was no unit. */""")
patch("Public/KeepersTerrainSubsystem.h", """	TMap<TWeakObjectPtr<AActor>, EKeepersTerrainEffect> PendingEffects;""", """	TMap<TWeakObjectPtr<AActor>, EKeepersTerrainEffect> PendingEffects;

	TSet<FIntPoint> ForestTiles;""")
patch("Private/KeepersTerrainSubsystem.cpp", """#include "KeepersBarks.h\"""", """#include "KeepersBarks.h"
#include "KeepersCampaign.h\"""")
patch("Private/KeepersTerrainSubsystem.cpp", """	case EKeepersTerrainEffect::RaiseRock:
		if (RaiseRock(Grid, Target))""", """	case EKeepersTerrainEffect::RaiseRock:
		if (RaiseRock(Grid, Target, UKeepersCampaignSubsystem::HasArtifact(this, TEXT("AncestorStone")) ? 3 : 2))""")
patch("Private/KeepersTerrainSubsystem.cpp", """	const bool bOccupied = KeepersTerrain::GetUnitOnTile(Grid, Index) != nullptr;
	FTransform TileTransform;
	if (!SetTileType(Grid, Index, KeepersTerrain::ForestTileType, TileTransform))
	{
		return false;
	}""", """	UObject* Occupant = KeepersTerrain::GetUnitOnTile(Grid, Index);
	const bool bOccupied = Occupant != nullptr;
	FTransform TileTransform;
	if (!SetTileType(Grid, Index, KeepersTerrain::ForestTileType, TileTransform))
	{
		return false;
	}
	ForestTiles.Add(Index);
	if (Occupant && UKeepersCampaignSubsystem::HasArtifact(this, TEXT("OakHeart")))
	{
		int32 Team = -1;
		KeepersReflection::GetInt(Occupant->GetClass(), Occupant, TEXT("TeamIndex"), Team);
		if (Team == 0)
		{
			KeepersReflection::FCall Heal(Occupant, TEXT("ModifyCurrentHp"));
			Heal.Set<int32>(TEXT("Modifier"), 3);
			Heal.Set<bool>(TEXT("ShowNumber"), true);
			Heal.Call();
		}
	}""")
# Push damage: Storm Feather (+1 on every push), Ancestor Stone (+1 against rocks)
patch("Private/KeepersTerrainSubsystem.cpp", """	if (!Destination.IsSet())
	{""", """	const int32 FeatherBonus = UKeepersCampaignSubsystem::HasArtifact(this, TEXT("StormFeather")) ? 1 : 0;
	const int32 BumpDamage = KeepersTerrain::BumpDamage + FeatherBonus;
	if (!Destination.IsSet())
	{""")
patch("Private/KeepersTerrainSubsystem.cpp", """		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, KeepersTerrain::BumpDamage);
		return true;
	}

	uint8 DestinationType = 0;""", """		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, BumpDamage);
		return true;
	}

	uint8 DestinationType = 0;""")
patch("Private/KeepersTerrainSubsystem.cpp", """		DamageUnit(CombatSystem, Unit, KeepersTerrain::BumpDamage);
		DamageUnit(CombatSystem, Blocker, KeepersTerrain::BumpDamage);""", """		DamageUnit(CombatSystem, Unit, BumpDamage);
		DamageUnit(CombatSystem, Blocker, BumpDamage);""")
patch("Private/KeepersTerrainSubsystem.cpp", """		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, KeepersTerrain::BumpDamage);
		return true;
	}

	KeepersReflection::FCall Move""", """		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, BumpDamage + (UKeepersCampaignSubsystem::HasArtifact(this, TEXT("AncestorStone")) ? 1 : 0));
		return true;
	}

	KeepersReflection::FCall Move""")
patch("Private/KeepersTerrainSubsystem.cpp", """		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Pushed, 0.7f);""", """		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Pushed, 0.7f);
		if (FeatherBonus > 0)
		{
			DamageUnit(CombatSystem, Unit, FeatherBonus);
		}""")

# --- Creatures: Shepherd Bell, Salty Biscuit -------------------------------------------------------------------
patch("Private/KeepersCreatureSubsystem.cpp", """#include "KeepersBarks.h\"""", """#include "KeepersBarks.h"
#include "KeepersCampaign.h\"""")
patch("Private/KeepersCreatureSubsystem.cpp", """	if (ChickenCalls >= ChickenMaxCalls)
	{
		return;
	}""", """	if (ChickenCalls >= ChickenMaxCalls || UKeepersCampaignSubsystem::HasArtifact(this, TEXT("ShepherdBell")))
	{
		return;
	}""")
patch("Private/KeepersCreatureSubsystem.cpp", """			if (!Unit->ActorHasTag(KeepersCreatures::SmallSlimeTag))""", """			if (!Unit->ActorHasTag(KeepersCreatures::SmallSlimeTag) && !UKeepersCampaignSubsystem::HasArtifact(this, TEXT("SaltyBiscuit")))""")

bom("Private/KeepersCampaign.cpp")
bom("Private/KeepersCampaignUI.cpp")
print("patched")
