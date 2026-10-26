#include "KeepersAiLibrary.h"

#include "Engine/World.h"
#include "KeepersBattleObjective.h"
#include "KeepersIntentSubsystem.h"
#include "KeepersReflection.h"

namespace KeepersAi
{
	struct FTile
	{
		uint8 Type = 0;
		float Z = 0.f;
		UObject* Unit = nullptr;
	};

	/** BP_Grid.GridTiles, read once per frame (the AI scores hundreds of plans at once). */
	struct FGridCache
	{
		TWeakObjectPtr<UObject> Grid;
		uint64 Frame = MAX_uint64;
		TMap<FIntPoint, FTile> Tiles;
		float Step = 100.f;
		bool bHex = true;
	};

	static const FGridCache& ReadGrid(UObject* Grid)
	{
		static FGridCache Cache;
		if (Cache.Grid.Get() == Grid && Cache.Frame == GFrameCounter)
		{
			return Cache;
		}
		Cache.Grid = Grid;
		Cache.Frame = GFrameCounter;
		Cache.Tiles.Reset();
		if (const FVector* Size = static_cast<const FVector*>(KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTileSize"))))
		{
			Cache.Step = Size->Z;
		}
		if (const uint8* Shape = static_cast<const uint8*>(KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridShape"))))
		{
			Cache.bHex = *Shape == 2;
		}
		FProperty* TilesProperty = nullptr;
		void* TilesValue = KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTiles"), &TilesProperty);
		FMapProperty* Map = CastField<FMapProperty>(TilesProperty);
		const FStructProperty* Value = Map ? CastField<FStructProperty>(Map->ValueProp) : nullptr;
		if (!Value)
		{
			return Cache;
		}
		FScriptMapHelper Helper(Map, TilesValue);
		for (FScriptMapHelper::FIterator It(Helper); It; ++It)
		{
			const FIntPoint Index = *reinterpret_cast<const FIntPoint*>(Helper.GetKeyPtr(It));
			void* Data = Helper.GetValuePtr(It);
			FTile Tile;
			KeepersReflection::GetEnumByte(Value->Struct, Data, TEXT("Type"), Tile.Type);
			if (const FTransform* Transform = static_cast<const FTransform*>(KeepersReflection::FindValuePtr(Value->Struct, Data, TEXT("Transform"))))
			{
				Tile.Z = Transform->GetLocation().Z;
			}
			FProperty* UnitProperty = nullptr;
			if (void* UnitPtr = KeepersReflection::FindValuePtr(Value->Struct, Data, TEXT("UnitOnTile"), &UnitProperty))
			{
				if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(UnitProperty))
				{
					Tile.Unit = Object->GetObjectPropertyValue(UnitPtr);
				}
			}
			Cache.Tiles.Add(Index, Tile);
		}
		return Cache;
	}

	/** Steps between two cells (hexagons with doubled Y, or squares). */
	static int32 StepsBetween(bool bHex, const FIntPoint& A, const FIntPoint& B)
	{
		const int32 DX = FMath::Abs(A.X - B.X);
		const int32 DY = FMath::Abs(A.Y - B.Y);
		return bHex ? DX + FMath::Max(0, (DY - DX) / 2) : DX + DY;
	}

	static int32 TeamOf(const UObject* Unit)
	{
		int32 Team = 0;
		KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		return Team;
	}

	/** UnitData.Stats.ValidTileTypes has FlyingUnitsOnly. */
	static bool CanFly(UObject* Unit)
	{
		FProperty* DataProperty = nullptr;
		void* Data = KeepersReflection::FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitData"), &DataProperty);
		const FStructProperty* DataStruct = CastField<FStructProperty>(DataProperty);
		FProperty* StatsProperty = nullptr;
		void* Stats = DataStruct ? KeepersReflection::FindValuePtr(DataStruct->Struct, Data, TEXT("Stats"), &StatsProperty) : nullptr;
		const FStructProperty* StatsStruct = CastField<FStructProperty>(StatsProperty);
		FProperty* TypesProperty = nullptr;
		void* Types = StatsStruct ? KeepersReflection::FindValuePtr(StatsStruct->Struct, Stats, TEXT("ValidTileTypes"), &TypesProperty) : nullptr;
		FArrayProperty* Array = CastField<FArrayProperty>(TypesProperty);
		if (!Array)
		{
			return false;
		}
		FScriptArrayHelper Helper(Array, Types);
		for (int32 I = 0; I < Helper.Num(); ++I)
		{
			if (*Helper.GetRawPtr(I) == 5)
			{
				return true;
			}
		}
		return false;
	}
}

double UKeepersAiLibrary::AdjustPlanScore(UObject* Unit, uint8 Spell, FIntPoint Origin, FIntPoint Target, double BaseScore)
{
	using namespace KeepersAi;
	UObject* Grid = Unit ? KeepersReflection::GetObject(Unit, TEXT("Grid")) : nullptr;
	if (!Grid || BaseScore <= 0.0)
	{
		return BaseScore;
	}
	const FGridCache& Map = ReadGrid(Grid);
	const FTile* From = Map.Tiles.Find(Origin);
	const FTile* At = Map.Tiles.Find(Target);
	if (!From)
	{
		return BaseScore;
	}
	const int32 Team = TeamOf(Unit);
	double Bonus = 0.0;
	// The battle's objective changes what the enemies want (only the enemies of the heroes read it).
	const AKeepersBattleObjective* Objective = Team != 0 ? AKeepersBattleObjective::Find(Unit->GetWorld()) : nullptr;
	// Against the clock the enemies rush: no time to keep safe.
	const bool bRush = Objective && Objective->IsTimed();
	if (bRush)
	{
		Bonus += 1.0;
	}

	// 1. Edges: water, pits and drops next to the spot — a gust or a shield bash ends a walker there.
	if (!CanFly(Unit))
	{
		static const FIntPoint Hex[] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }, { 0, 2 }, { 0, -2 } };
		static const FIntPoint Square[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		int32 Edges = 0;
		const int32 Count = Map.bHex ? 6 : 4;
		for (int32 I = 0; I < Count; ++I)
		{
			const FTile* Next = Map.Tiles.Find(Origin + (Map.bHex ? Hex[I] : Square[I]));
			Edges += !Next || Next->Type == 0 || Next->Type == 5 || From->Z - Next->Z > Map.Step * 1.5f ? 1 : 0;
		}
		Bonus -= (bRush ? 0.3 : 0.6) * FMath::Min(Edges, 3);
	}

	// 2. High ground over the target.
	if (At)
	{
		Bonus += 0.4 * FMath::Clamp((From->Z - At->Z) / FMath::Max(Map.Step, 1.f), -1.f, 2.f);
	}

	// 3. Spread the threats: a tile another enemy will already hit is worth less.
	if (const UWorld* World = Unit->GetWorld())
	{
		if (const UKeepersIntentSubsystem* Intents = World->GetSubsystem<UKeepersIntentSubsystem>())
		{
			Bonus -= Intents->GetThreatenedIndexes().Contains(Target) ? 1.0 : 0.0;
		}
	}

	// 4. Distance to the nearest hero: ranged attackers and the badly wounded keep away.
	int32 Nearest = 99;
	for (const TPair<FIntPoint, FTile>& Pair : Map.Tiles)
	{
		if (Pair.Value.Unit && Pair.Value.Unit != Unit && TeamOf(Pair.Value.Unit) != Team)
		{
			Nearest = FMath::Min(Nearest, StepsBetween(Map.bHex, Origin, Pair.Key));
		}
	}
	const bool bRanged = StepsBetween(Map.bHex, Origin, Target) > 1;
	int32 Hp = 1;
	int32 HpMax = 1;
	KeepersReflection::GetUnitStat(Unit, TEXT("HP_Current"), Hp);
	KeepersReflection::GetUnitStat(Unit, TEXT("HP_Max"), HpMax);
	const bool bWounded = !bRush && Hp * 3 < HpMax;
	if (Nearest < 99 && (bRanged || bWounded))
	{
		Bonus += (bWounded ? 0.5 : 0.3) * FMath::Min(Nearest - 1, 3);
	}

	// 5. A hero low on health is the target: finishing one off matters more than spreading damage.
	UObject* Victim = At ? At->Unit : nullptr;
	if (Victim && TeamOf(Victim) != Team)
	{
		int32 VictimHp = 1;
		int32 VictimMax = 1;
		KeepersReflection::GetUnitStat(Victim, TEXT("HP_Current"), VictimHp);
		KeepersReflection::GetUnitStat(Victim, TEXT("HP_Max"), VictimMax);
		Bonus += 1.0 - static_cast<double>(VictimHp) / FMath::Max(VictimMax, 1);
	}
	// A shield bash pushes the target one cell away from the striker: into water or off a cliff it is a kill.
	static uint8 ShieldBash = MAX_uint8;
	if (ShieldBash == MAX_uint8)
	{
		ShieldBash = MAX_uint8 - 1;
		if (const UEnum* Spells = LoadObject<UEnum>(nullptr, TEXT("/Game/Blueprints/Core/Combat/Spells/Utilities/E_Spell.E_Spell")))
		{
			for (int32 I = 0; I < Spells->NumEnums(); ++I)
			{
				if (Spells->GetDisplayNameTextByIndex(I).ToString() == TEXT("ShieldBash"))
				{
					ShieldBash = static_cast<uint8>(Spells->GetValueByIndex(I));
					UE_LOG(LogTemp, Log, TEXT("KeepersAi: ShieldBash is spell %d"), ShieldBash);
				}
			}
		}
	}
	if (Spell == ShieldBash && Victim && TeamOf(Victim) != Team && At && !CanFly(Victim))
	{
		const FIntPoint Landing = Target + (Target - Origin);
		const FTile* To = Map.Tiles.Find(Landing);
		if (!To || To->Type == 0 || To->Type == 5 || At->Z - To->Z > Map.Step * 1.5f)
		{
			Bonus += 6.0;
		}
		else if (To->Unit || To->Type == 2)
		{
			Bonus += 1.0;
		}
	}
	if (Objective)
	{
		// The unit the heroes protect is the prey.
		if (Victim && Victim == Objective->GetProtected())
		{
			Bonus += 3.0;
		}
		// The beacon is guarded: spots near it are worth more.
		FIntPoint Beacon;
		if (Objective->GetBeacon(Beacon))
		{
			Bonus += 0.6 * FMath::Max(0, 3 - StepsBetween(Map.bHex, Origin, Beacon));
		}
		// The leader keeps back; the others stand by it.
		if (const AActor* Leader = Objective->GetLeader())
		{
			FIntPoint LeaderIndex;
			if (Leader == Unit)
			{
				Bonus += Nearest < 99 ? 0.4 * FMath::Min(Nearest - 1, 3) : 0.0;
			}
			else if (KeepersReflection::GetIntPoint(Leader->GetClass(), Leader, TEXT("Index"), LeaderIndex) && StepsBetween(Map.bHex, Origin, LeaderIndex) <= 2)
			{
				Bonus += 0.8;
			}
		}
	}
	return BaseScore + Bonus;
}
