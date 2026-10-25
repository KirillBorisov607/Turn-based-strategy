#include "KeepersMapGenerator.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "KeepersBattleScenery.h"
#include "KeepersReflection.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersMapGen, Log, All);

namespace KeepersMapGen
{
	static int32 NextSeed = 0;
	static EKeepersBiome NextBiome = EKeepersBiome::Meadow;
	static bool bNextBiomeRandom = true;

	/** Field size in cells (hexagon grid: Y indexes are doubled). */
	static constexpr int32 CountX = 34;
	static constexpr int32 CountY = 26;

	static const TCHAR* Checker1 = TEXT("/Game/Art/Materials/ColorsWithTexture/MI_Hexagon_Checker_01.MI_Hexagon_Checker_01");
	static const TCHAR* Checker2 = TEXT("/Game/Art/Materials/ColorsWithTexture/MI_Hexagon_Checker_02.MI_Hexagon_Checker_02");
	static const TCHAR* Checker3 = TEXT("/Game/Art/Materials/ColorsWithTexture/MI_Hexagon_Checker_03.MI_Hexagon_Checker_03");

	struct FBiome
	{
		/** Chance of a plateau on a middle island. */
		float Plateau;
		float Forest;
		float Rocks;
		float Puddles;
		/** Houses in the hamlet (0: none); ruins instead of houses on cursed land. */
		int32 Houses;
		/** Island top patterns, one per island in turn. */
		const TCHAR* Tops[3];
	};

	static const FBiome& BiomeOf(EKeepersBiome Biome)
	{
		static const FBiome Biomes[] =
		{
			// Plateau Forest Rocks Puddles Houses Tops
			{ 0.55f, 0.16f, 0.10f, 0.00f, 3, { Checker1, Checker2, Checker3 } },
			{ 0.45f, 0.30f, 0.08f, 0.00f, 2, { Checker2, Checker3, Checker1 } },
			{ 0.25f, 0.12f, 0.06f, 0.30f, 2, { TEXT("/Game/Art/Materials/Colors/MI_Green_04.MI_Green_04"), Checker3, TEXT("/Game/Art/Materials/Colors/MI_Brown_02.MI_Brown_02") } },
			{ 1.00f, 0.08f, 0.20f, 0.00f, 1, { TEXT("/Game/Art/Materials/Colors/MI_Grey_04.MI_Grey_04"), Checker3, TEXT("/Game/Art/Materials/Colors/MI_Grey_06.MI_Grey_06") } },
			{ 0.60f, 0.10f, 0.12f, 0.00f, 2, { TEXT("/Game/Art/Materials/Colors/MI_Grey_01.MI_Grey_01"), TEXT("/Game/Art/Materials/Colors/MI_Blue_02.MI_Blue_02"), TEXT("/Game/Art/Materials/Colors/MI_Grey_02.MI_Grey_02") } },
			{ 0.50f, 0.05f, 0.14f, 0.12f, 3, { TEXT("/Game/Art/Materials/Colors/MI_Purple_02.MI_Purple_02"), TEXT("/Game/Art/Materials/Colors/MI_Grey_06.MI_Grey_06"), TEXT("/Game/Art/Materials/Colors/MI_Purple_03.MI_Purple_03") } },
		};
		return Biomes[FMath::Clamp(static_cast<int32>(Biome), 0, UE_ARRAY_COUNT(Biomes) - 1)];
	}

	/** Houses of the Toony set; cursed land gets the half-built ones (ruins). */
	static const TCHAR* Houses[] = { TEXT("SM_House"), TEXT("SM_Granary"), TEXT("SM_Tower_A"), TEXT("SM_Farm"), TEXT("SM_LumberMill"), TEXT("SM_Blacksmith") };
	static const TCHAR* StackColors[] =
	{
		TEXT("/Game/Art/Materials/Colors/MI_Orange_01.MI_Orange_01"),
		TEXT("/Game/Art/Materials/Colors/MI_Yellow_01.MI_Yellow_01"),
		TEXT("/Game/Art/Materials/Colors/MI_Red_02.MI_Red_02"),
	};

	static const FIntPoint Neighbors[] = { FIntPoint(1, 1), FIntPoint(1, -1), FIntPoint(-1, 1), FIntPoint(-1, -1), FIntPoint(0, 2), FIntPoint(0, -2) };

	static bool Walkable(const FKeepersGenCell& Cell)
	{
		return Cell.Type == 1 || Cell.Type == 3 || Cell.Type == 4;
	}

	/** Cells reachable on foot from Start (height steps of one level at most). */
	static TSet<FIntPoint> Reach(const TMap<FIntPoint, int32>& ByIndex, const TArray<FKeepersGenCell>& Cells, const FIntPoint& Start)
	{
		TSet<FIntPoint> Seen;
		TArray<FIntPoint> Queue = { Start };
		Seen.Add(Start);
		while (Queue.Num() > 0)
		{
			const FIntPoint Here = Queue.Pop(EAllowShrinking::No);
			const FKeepersGenCell& Cell = Cells[ByIndex[Here]];
			for (const FIntPoint& Step : Neighbors)
			{
				const FIntPoint Next = Here + Step;
				const int32* Slot = ByIndex.Find(Next);
				if (!Slot || Seen.Contains(Next))
				{
					continue;
				}
				const FKeepersGenCell& Other = Cells[*Slot];
				if (Walkable(Other) && FMath::Abs(Other.Level - Cell.Level) <= 1)
				{
					Seen.Add(Next);
					Queue.Add(Next);
				}
			}
		}
		return Seen;
	}

	struct FIsland
	{
		FVector2D Center;
		double Radius;
		bool bMain;
	};
}

// ---------------------------------------------------------------------------------------------------------------------

const FName UKeepersMapGenerator::Level = TEXT("/Game/Maps/Generated/L_Generated");

void UKeepersMapGenerator::SetNextMap(int32 Seed, EKeepersBiome Biome)
{
	KeepersMapGen::NextSeed = Seed;
	KeepersMapGen::NextBiome = Biome;
	KeepersMapGen::bNextBiomeRandom = false;
}

FText UKeepersMapGenerator::BiomeBlurb(EKeepersBiome Biome)
{
	static const TCHAR* Lines[] =
	{
		TEXT("Острова среди лугов, мосты и хутора. Никто ещё не видел этих мест — и врагов, что там прячутся."),
		TEXT("Лесные острова: идти тяжело, стрелы вязнут в ветвях. Держитесь опушек и мостов."),
		TEXT("Лужи замедляют, вода отрезает пути. Смотрите под ноги."),
		TEXT("Плато и обрывы. Порыв ветра здесь страшнее меча."),
		TEXT("Снег по колено, ветер в лицо. Укрытий мало, ошибок не прощают."),
		TEXT("Земля здесь больна, на островах одни руины. Не задерживайтесь."),
	};
	return FText::FromString(Lines[FMath::Clamp(static_cast<int32>(Biome), 0, 5)]);
}

bool UKeepersMapGenerator::GetPendingBiome(EKeepersBiome& OutBiome)
{
	OutBiome = KeepersMapGen::NextBiome;
	return !KeepersMapGen::bNextBiomeRandom;
}

FText UKeepersMapGenerator::BiomeName(EKeepersBiome Biome)
{
	static const TCHAR* Names[] = { TEXT("Луга"), TEXT("Лес"), TEXT("Болота"), TEXT("Горы"), TEXT("Зима"), TEXT("Проклятые земли") };
	return FText::FromString(Names[FMath::Clamp(static_cast<int32>(Biome), 0, 5)]);
}

bool UKeepersMapGenerator::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UKeepersMapGenerator::Layout(int32 Seed, EKeepersBiome Biome, TFunctionRef<FVector(const FIntPoint&)> LocationOf, int32 CountX, int32 CountY, TArray<FKeepersGenCell>& OutCells)
{
	using namespace KeepersMapGen;
	const FBiome& Params = BiomeOf(Biome);

	// All cells of the grid, in BP_Grid.SpawnGrid's hexagon order: Y is doubled, odd columns start at 1.
	TArray<FKeepersGenCell> Base;
	FVector Min(TNumericLimits<float>::Max());
	FVector Max(-TNumericLimits<float>::Max());
	for (int32 X = 0; X < CountX; ++X)
	{
		for (int32 Y = (X % 2 == 0 ? 0 : 1); Y <= CountY * 2 - 1; Y += 2)
		{
			FKeepersGenCell& Cell = Base.AddDefaulted_GetRef();
			Cell.Index = FIntPoint(X, Y);
			Cell.Location = LocationOf(Cell.Index);
			Min = Min.ComponentMin(Cell.Location);
			Max = Max.ComponentMax(Cell.Location);
		}
	}
	const FVector Center = (Min + Max) * 0.5f;
	const FVector Half = FVector::Max((Max - Min) * 0.5f, FVector(1.f));
	// Field position in -1..1, X across from the player to the enemy.
	auto Normalized = [&](const FKeepersGenCell& Cell) { return FVector2D((Cell.Location.X - Center.X) / Half.X, (Cell.Location.Y - Center.Y) / Half.Y); };

	for (int32 Attempt = 0; Attempt < 60; ++Attempt)
	{
		FRandomStream Random(Seed * 7919 + Attempt * 104729);
		const FVector2D Phase(Random.FRandRange(0.f, 1000.f), Random.FRandRange(0.f, 1000.f));
		auto Noise = [&](const FKeepersGenCell& Cell, float Scale, const FVector2D& Offset)
		{
			return FMath::PerlinNoise2D(FVector2D(Cell.Location.X, Cell.Location.Y) / Scale + Phase + Offset) * 0.5f + 0.5f;
		};
		TArray<FKeepersGenCell> Cells = Base;
		TMap<FIntPoint, int32> ByIndex;
		for (int32 I = 0; I < Cells.Num(); ++I)
		{
			ByIndex.Add(Cells[I].Index, I);
			Cells[I].Type = 0;
		}

		// 1. Islands: the player's on the left, the enemy's on the right, one to three in the middle,
		//    and a few small islets off the paths.
		TArray<FIsland> Islands;
		Islands.Add({ FVector2D(-0.7f, Random.FRandRange(-0.3f, 0.3f)), Random.FRandRange(0.24f, 0.28f), true });
		const int32 Middle = Random.RandRange(1, 3);
		for (int32 M = 0; M < Middle; ++M)
		{
			const float X = Middle == 1 ? Random.FRandRange(-0.1f, 0.1f) : FMath::Lerp(-0.3f, 0.3f, static_cast<float>(M) / (Middle - 1));
			const float Y = Middle == 1 ? Random.FRandRange(-0.3f, 0.3f) : (M % 2 == 0 ? -1.f : 1.f) * Random.FRandRange(0.3f, 0.5f);
			Islands.Add({ FVector2D(X, Y), Random.FRandRange(0.2f, 0.26f), true });
		}
		Islands.Add({ FVector2D(0.7f, Random.FRandRange(-0.3f, 0.3f)), Random.FRandRange(0.24f, 0.28f), true });
		const int32 Islets = Random.RandRange(2, 3);
		for (int32 S = 0; S < Islets; ++S)
		{
			Islands.Add({ FVector2D(Random.FRandRange(-0.75f, 0.75f), (S % 2 == 0 ? -1.f : 1.f) * Random.FRandRange(0.75f, 0.9f)), Random.FRandRange(0.09f, 0.13f), false });
		}
		for (FKeepersGenCell& Cell : Cells)
		{
			const FVector2D P = Normalized(Cell);
			const float Wobble = (Noise(Cell, 1300.f, FVector2D::ZeroVector) - 0.5f) * 0.3f;
			float Best = 1.f;
			for (int32 I = 0; I < Islands.Num(); ++I)
			{
				const float Inside = (P - Islands[I].Center).Size() / Islands[I].Radius + Wobble;
				if (Inside < Best)
				{
					Best = Inside;
					Cell.Island = I;
					Cell.Type = 1;
				}
			}
		}

		// 2. Bridges along a chain from the player's island to the enemy's, sometimes one more cross link.
		TArray<TPair<int32, int32>> Links;
		TArray<int32> Chain;
		for (int32 I = 0; I < Islands.Num(); ++I)
		{
			if (Islands[I].bMain)
			{
				Chain.Add(I);
			}
		}
		Chain.Sort([&](int32 A, int32 B) { return Islands[A].Center.X < Islands[B].Center.X; });
		for (int32 I = 0; I + 1 < Chain.Num(); ++I)
		{
			Links.Add({ Chain[I], Chain[I + 1] });
		}
		if (Chain.Num() >= 4 && Random.FRand() < 0.6f)
		{
			Links.Add({ Chain[0], Chain[2] });
		}
		for (const TPair<int32, int32>& Link : Links)
		{
			const FVector2D From = Islands[Link.Key].Center;
			const FVector2D To = Islands[Link.Value].Center;
			const FVector2D Delta((To.X - From.X) * Half.X, (To.Y - From.Y) * Half.Y);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
			for (float T = 0.f; T <= 1.f; T += 0.01f)
			{
				const FVector2D Point = FMath::Lerp(From, To, T);
				int32 Nearest = INDEX_NONE;
				float NearestDistance = TNumericLimits<float>::Max();
				for (int32 I = 0; I < Cells.Num(); ++I)
				{
					const float Distance = FVector2D::DistSquared(Normalized(Cells[I]), Point);
					if (Distance < NearestDistance)
					{
						NearestDistance = Distance;
						Nearest = I;
					}
				}
				FKeepersGenCell& Cell = Cells[Nearest];
				if (Cell.Type == 0)
				{
					Cell.Type = 1;
					Cell.bBridge = true;
					Cell.Yaw = Yaw;
					Cell.Level = 0;
				}
			}
		}

		// 3. Plateaus with cliffs on the middle islands, a slope of one level around them.
		for (int32 I = 0; I < Islands.Num(); ++I)
		{
			const bool bSide = I == Chain[0] || I == Chain.Last();
			if (!Islands[I].bMain || bSide || Random.FRand() > Params.Plateau)
			{
				continue;
			}
			const FVector2D Peak = Islands[I].Center + FVector2D(Random.FRandRange(-0.08f, 0.08f), Random.FRandRange(-0.08f, 0.08f));
			for (FKeepersGenCell& Cell : Cells)
			{
				if (Cell.Island != I || Cell.bBridge)
				{
					continue;
				}
				const float Inside = (Normalized(Cell) - Peak).Size() / Islands[I].Radius + (Noise(Cell, 1000.f, FVector2D(19.f, 7.f)) - 0.5f) * 0.4f;
				Cell.Level = Inside < 0.45f ? 2 : Inside < 0.7f ? 1 : 0;
			}
			// The cliff has gaps: part of the ring stays at one level, so the top can be climbed.
			for (FKeepersGenCell& Cell : Cells)
			{
				if (Cell.Island == I && Cell.Level == 1 && Noise(Cell, 700.f, FVector2D(3.f, 77.f)) < 0.35f)
				{
					Cell.Level = 0;
				}
			}
		}
		if (Biome == EKeepersBiome::Mountains)
		{
			// Mountains: the low ground of every island has hills too.
			for (FKeepersGenCell& Cell : Cells)
			{
				if (Cell.Type == 1 && !Cell.bBridge && Cell.Level == 0 && Noise(Cell, 1100.f, FVector2D(41.f, 5.f)) > 0.62f)
				{
					Cell.Level = 1;
				}
			}
		}

		// 4. Starts: the cells nearest to the middle of the side islands, flat and clear.
		auto PickStarts = [&](int32 Island, int32 Team, int32 Count)
		{
			TArray<int32> Candidates;
			for (int32 I = 0; I < Cells.Num(); ++I)
			{
				if (Cells[I].Island == Island && Cells[I].Type != 0 && !Cells[I].bBridge)
				{
					Candidates.Add(I);
				}
			}
			const FVector2D Anchor = Islands[Island].Center;
			Candidates.Sort([&](int32 A, int32 B) { return FVector2D::DistSquared(Normalized(Cells[A]), Anchor) < FVector2D::DistSquared(Normalized(Cells[B]), Anchor); });
			for (int32 I = 0; I < Count && I < Candidates.Num(); ++I)
			{
				FKeepersGenCell& Cell = Cells[Candidates[I]];
				Cell.Type = 1;
				Cell.Level = 0;
				Cell.Start = Team;
			}
			return Candidates.Num() >= Count + 3;
		};
		if (!PickStarts(Chain[0], 1, 6) || !PickStarts(Chain.Last(), 2, 6))
		{
			continue;
		}
		auto NearStart = [&](const FKeepersGenCell& Cell, float Range)
		{
			for (const FKeepersGenCell& Start : Cells)
			{
				if (Start.Start != 0 && FVector::Dist2D(Start.Location, Cell.Location) < Range)
				{
					return true;
				}
			}
			return false;
		};
		// Cells next to bridges stay clear, so nothing blocks the crossings; late attempts keep the field plain.
		TSet<FIntPoint> Reserved;
		for (const FKeepersGenCell& Cell : Cells)
		{
			if (Cell.bBridge)
			{
				for (const FIntPoint& Step : Neighbors)
				{
					Reserved.Add(Cell.Index + Step);
					Reserved.Add(Cell.Index + Step + Step);
				}
			}
		}
		// Bridge landings are on low ground.
		for (FKeepersGenCell& Cell : Cells)
		{
			if (Reserved.Contains(Cell.Index) && Cell.Type != 0 && !Cell.bBridge)
			{
				Cell.Level = 0;
			}
		}
		const bool bPlain = Attempt >= 50;
		auto Free = [&](const FKeepersGenCell& Cell) { return !bPlain && Cell.Type == 1 && !Cell.bBridge && Cell.Start == 0 && !Reserved.Contains(Cell.Index); };

		// 5. Groves, rock outcrops, marshes: clusters from low-frequency noise.
		for (FKeepersGenCell& Cell : Cells)
		{
			if (!Free(Cell) || NearStart(Cell, 260.f))
			{
				continue;
			}
			if (Noise(Cell, 1300.f, FVector2D(5.f, 91.f)) > 0.9f - Params.Forest * 1.6f)
			{
				Cell.Type = 3;
				Cell.bTree = Random.FRand() < 0.75f;
			}
			else if (Noise(Cell, 800.f, FVector2D(53.f, 17.f)) > 0.95f - Params.Rocks * 1.8f && Random.FRand() < 0.6f)
			{
				Cell.Type = 2;
				Cell.bRock = true;
			}
			else if (Noise(Cell, 1100.f, FVector2D(13.f, 61.f)) > 0.95f - Params.Puddles * 2.f)
			{
				Cell.Type = 3;
				Cell.bPuddle = true;
			}
			else if (Random.FRand() < 0.08f)
			{
				Cell.bBush = true;
			}
			else if (Random.FRand() < 0.04f)
			{
				// A lone tree on open ground: wood, but most of the cell stays clear.
				Cell.Type = 3;
				Cell.bTree = true;
			}
		}

		// Coloured stacks on the middle islands: two or three raised blocks side by side.
		for (int32 Group = 0; Group < Random.RandRange(1, 2); ++Group)
		{
			TArray<int32> Spots;
			for (int32 I = 0; I < Cells.Num(); ++I)
			{
				const FKeepersGenCell& Cell = Cells[I];
				if (Free(Cell) && Cell.Island != Chain[0] && Cell.Island != Chain.Last() && Islands.IsValidIndex(Cell.Island) && Islands[Cell.Island].bMain)
				{
					Spots.Add(I);
				}
			}
			if (Spots.Num() == 0)
			{
				break;
			}
			const FKeepersGenCell First = Cells[Spots[Random.RandRange(0, Spots.Num() - 1)]];
			const int32 Color = Random.RandRange(0, UE_ARRAY_COUNT(StackColors) - 1);
			int32 Placed = 0;
			for (const FIntPoint& Step : Neighbors)
			{
				const int32* Slot = ByIndex.Find(First.Index + Step);
				if (Placed >= 3 || !Slot || !Free(Cells[*Slot]))
				{
					continue;
				}
				Cells[*Slot].Type = 2;
				Cells[*Slot].Stack = Color;
				++Placed;
			}
		}

		// A hamlet: houses close together on an island, a fence beside them.
		if (Params.Houses > 0)
		{
			TArray<int32> Spots;
			for (int32 I = 0; I < Cells.Num(); ++I)
			{
				if (Free(Cells[I]) && !NearStart(Cells[I], 700.f) && Islands.IsValidIndex(Cells[I].Island) && Islands[Cells[I].Island].bMain)
				{
					Spots.Add(I);
				}
			}
			if (Spots.Num() > 0)
			{
				const FKeepersGenCell Heart = Cells[Spots[Random.RandRange(0, Spots.Num() - 1)]];
				int32 Built = 0;
				for (int32 I : Spots)
				{
					FKeepersGenCell& Cell = Cells[I];
					if (Built < Params.Houses && Cell.Island == Heart.Island && FVector::Dist2D(Cell.Location, Heart.Location) < 520.f && Free(Cell) && Random.FRand() < 0.5f)
					{
						Cell.Type = 2;
						Cell.House = Random.RandRange(0, UE_ARRAY_COUNT(Houses) - 1);
						Cell.Yaw = Random.RandRange(0, 5) * 60.f;
						++Built;
					}
				}
				// Fence: a short straight run next to the hamlet.
				const FIntPoint Step = Neighbors[Random.RandRange(0, 5)];
				const int32* Start = ByIndex.Find(Heart.Index + Step + Step);
				for (int32 Piece = 0; Start && Piece < 3; ++Piece)
				{
					const int32* Slot = ByIndex.Find(Cells[*Start].Index + Step * Piece);
					if (Slot && Free(Cells[*Slot]))
					{
						FKeepersGenCell& Cell = Cells[*Slot];
						Cell.Type = 2;
						Cell.bFence = true;
						Cell.Yaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Step.Y) * 100.f, static_cast<float>(Step.X) * 173.f));
					}
				}
			}
		}

		// 6. The enemy start must be reachable from the player's; islets may stay apart.
		int32 PlayerStart = INDEX_NONE;
		int32 EnemyStart = INDEX_NONE;
		for (int32 I = 0; I < Cells.Num(); ++I)
		{
			PlayerStart = Cells[I].Start == 1 && PlayerStart == INDEX_NONE ? I : PlayerStart;
			EnemyStart = Cells[I].Start == 2 && EnemyStart == INDEX_NONE ? I : EnemyStart;
		}
		const TSet<FIntPoint> Reached = Reach(ByIndex, Cells, Cells[PlayerStart].Index);
		bool bJoined = true;
		for (const FKeepersGenCell& Cell : Cells)
		{
			bJoined &= Cell.Start == 0 || Reached.Contains(Cell.Index);
		}
		if (!bJoined || Reached.Num() < CountX * CountY / 7)
		{
			continue;
		}
		OutCells = MoveTemp(Cells);
		return true;
	}
	return false;
}

void UKeepersMapGenerator::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetMapName().Contains(TEXT("L_Generated")))
	{
		Generate(InWorld);
	}
}

void UKeepersMapGenerator::Generate(UWorld& World)
{
	using namespace KeepersMapGen;
	AActor* Grid = nullptr;
	AActor* Scenario = nullptr;
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		const FString Class = It->GetClass()->GetName();
		Grid = Class == TEXT("BP_Grid_C") ? *It : Grid;
		Scenario = Class == TEXT("BP_CombatScenario_C") ? *It : Scenario;
	}
	FProperty* GridDataProperty = nullptr;
	void* GridData = Scenario ? KeepersReflection::FindValuePtr(Scenario->GetClass(), Scenario, TEXT("GridData"), &GridDataProperty) : nullptr;
	const FStructProperty* GridDataStruct = CastField<FStructProperty>(GridDataProperty);
	if (!Grid || !GridData || !GridDataStruct)
	{
		UE_LOG(LogKeepersMapGen, Warning, TEXT("No grid or scenario in %s"), *World.GetMapName());
		return;
	}

	UsedSeed = NextSeed != 0 ? NextSeed : FMath::RandRange(1, 999999);
	UsedBiome = bNextBiomeRandom ? static_cast<EKeepersBiome>(FMath::RandRange(0, 5)) : NextBiome;
	NextSeed = 0;
	bNextBiomeRandom = true;

	// The scenario builds the grid when combat starts: give it our size, and the grid the same numbers
	// now, so its own functions tell where each cell is.
	FVector TileSize(200.f, 200.f, 100.f);
	if (const FVector* ScenarioTile = static_cast<const FVector*>(KeepersReflection::FindValuePtr(GridDataStruct->Struct, GridData, TEXT("TileSize"))))
	{
		TileSize = *ScenarioTile;
	}
	const FVector Center(0.f, 0.f, 0.f);
	const FIntPoint Count(CountX, CountY);
	auto SetStructMember = [](const UStruct* Struct, void* Container, const TCHAR* Name, const void* Value)
	{
		FProperty* Property = nullptr;
		if (void* Ptr = KeepersReflection::FindValuePtr(Struct, Container, Name, &Property))
		{
			Property->CopyCompleteValue(Ptr, Value);
		}
	};
	const bool bTrue = true;
	SetStructMember(GridDataStruct->Struct, GridData, TEXT("GenerateGridBeforeStartingCombat"), &bTrue);
	SetStructMember(GridDataStruct->Struct, GridData, TEXT("CenterLocation"), &Center);
	SetStructMember(GridDataStruct->Struct, GridData, TEXT("TileCount"), &Count);
	SetStructMember(GridDataStruct->Struct, GridData, TEXT("UseEnvironment"), &bTrue);
	SetStructMember(Grid->GetClass(), Grid, TEXT("GridCenterLocation"), &Center);
	SetStructMember(Grid->GetClass(), Grid, TEXT("GridTileSize"), &TileSize);
	SetStructMember(Grid->GetClass(), Grid, TEXT("GridTileCount"), &Count);
	{
		KeepersReflection::FCall Corner(Grid, TEXT("CalculateCenterAndBottomLeft"));
		if (Corner.Call())
		{
			FVector BottomLeft;
			if (Corner.Get<FVector>(TEXT("BottomLeft"), BottomLeft))
			{
				SetStructMember(Grid->GetClass(), Grid, TEXT("GridBottomLeftCornerLocation"), &BottomLeft);
			}
		}
	}
	auto LocationOf = [Grid](const FIntPoint& Index)
	{
		KeepersReflection::FCall Where(Grid, TEXT("GetTileLocationFromGridIndex"));
		Where.Set<FIntPoint>(TEXT("GridIndex"), Index);
		FVector Location = FVector::ZeroVector;
		if (Where.Call())
		{
			Where.Get<FVector>(TEXT("TileLocation"), Location);
		}
		return Location;
	};

	TArray<FKeepersGenCell> Cells;
	if (!Layout(UsedSeed, UsedBiome, LocationOf, CountX, CountY, Cells))
	{
		UE_LOG(LogKeepersMapGen, Error, TEXT("Seed %d (%d): no valid field"), UsedSeed, static_cast<int32>(UsedBiome));
		return;
	}

	// Actors: a hexagon column per land cell, bridges over water, grid modifiers for the terrain types,
	// and what stands on the cells.
	const FBiome& Params = BiomeOf(UsedBiome);
	UStaticMesh* Column = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Meshes/SM_Hexagon_Platform_01.SM_Hexagon_Platform_01"));
	UMaterialInterface* Tops[3] = { LoadObject<UMaterialInterface>(nullptr, Params.Tops[0]), LoadObject<UMaterialInterface>(nullptr, Params.Tops[1]), LoadObject<UMaterialInterface>(nullptr, Params.Tops[2]) };
	UMaterialInterface* Side = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/Colors/MI_Brown_08.MI_Brown_08"));
	UMaterialInterface* Puddle = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/Colors/MI_Brown_05.MI_Brown_05"));
	UMaterialInterface* RockColor = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/Colors/MI_Grey_05.MI_Grey_05"));
	UStaticMesh* Rock = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Meshes/SM_Cube_Chamfer.SM_Cube_Chamfer"));
	UStaticMesh* Bridge = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Meshes/SM_Bridge_01.SM_Bridge_01"));
	UStaticMesh* Fence = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Toony_Tiny_RTS_Set/Meshes/Buildings/SM_Wall_B_wall.SM_Wall_B_wall"));
	UMaterialInterface* BuildingColors[] =
	{
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Toony_Tiny_RTS_Set/Materials/Buildings/MI_Buildings_Brown.MI_Buildings_Brown")),
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Toony_Tiny_RTS_Set/Materials/Buildings/MI_Buildings_Blue.MI_Buildings_Blue")),
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Toony_Tiny_RTS_Set/Materials/Buildings/MI_Buildings_Red.MI_Buildings_Red")),
	};
	UStaticMesh* Trees[] =
	{
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Meshes/SM_Tree_01.SM_Tree_01")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Meshes/SM_Tree_02.SM_Tree_02")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Meshes/SM_Tree_03.SM_Tree_03")),
	};
	UStaticMesh* Bushes[] =
	{
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Meshes/SM_Bush_01.SM_Bush_01")),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Meshes/SM_Bush_03.SM_Bush_03")),
	};
	UClass* Modifier = LoadObject<UClass>(nullptr, TEXT("/Game/Blueprints/Core/Grid/BP_GridModifier.BP_GridModifier_C"));
	if (!Column)
	{
		return;
	}
	const FBoxSphereBounds ColumnBounds = Column->GetBounds();
	const float ColumnScale = TileSize.X / FMath::Max(ColumnBounds.BoxExtent.X * 2.f, 1.f);
	const float ColumnTop = ColumnBounds.Origin.Z + ColumnBounds.BoxExtent.Z;
	FRandomStream Random(UsedSeed);

	auto SpawnMesh = [&World](UStaticMesh* Mesh, const FTransform& Transform, bool bGround, bool bBlocksSight, bool bCollision = true) -> UStaticMeshComponent*
	{
		AStaticMeshActor* Actor = Mesh ? World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform) : nullptr;
		if (!Actor)
		{
			return nullptr;
		}
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		if (bCollision)
		{
			Component->SetCollisionProfileName(TEXT("BlockAll"));
			// GroundAndGridModifiers: the grid finds its tiles on the ground; LineOfSight: what blocks shots.
			Component->SetCollisionResponseToChannel(ECC_GameTraceChannel1, bGround ? ECR_Block : ECR_Ignore);
			Component->SetCollisionResponseToChannel(ECC_GameTraceChannel4, bBlocksSight ? ECR_Block : ECR_Ignore);
		}
		else
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Actor->SetActorScale3D(Transform.GetScale3D());
		return Component;
	};
	// Scale that fits a mesh into one cell.
	auto FitScale = [&TileSize](const UStaticMesh* Mesh, float Fill)
	{
		const FVector Extent = Mesh ? Mesh->GetBounds().BoxExtent : FVector(100.f);
		return TileSize.X * Fill / FMath::Max(FMath::Max(Extent.X, Extent.Y) * 2.f, 1.f);
	};

	for (const FKeepersGenCell& Cell : Cells)
	{
		if (Cell.Type == 0)
		{
			continue;
		}
		const float Height = Cell.Level * TileSize.Z;
		const FVector Surface(Cell.Location.X, Cell.Location.Y, Height);
		if (Cell.bBridge)
		{
			// Planks over the water; the grid finds the bridge as ground.
			const float Scale = FitScale(Bridge, 1.f);
			SpawnMesh(Bridge, FTransform(FRotator(0.f, Cell.Yaw + 90.f, 0.f), Surface - FVector(0.f, 0.f, 100.f * Scale), FVector(Scale)), true, false);
		}
		else
		{
			// Coloured stacks rise one or two levels above the ground around them.
			const float Raise = Cell.Stack != INDEX_NONE ? TileSize.Z * (1 + (Cell.Index.X + Cell.Index.Y) % 2) : 0.f;
			const FVector Base(Cell.Location.X, Cell.Location.Y, Height + Raise - ColumnTop);
			if (UStaticMeshComponent* Ground = SpawnMesh(Column, FTransform(FRotator::ZeroRotator, Base, FVector(ColumnScale, ColumnScale, 1.f)), true, true))
			{
				UMaterialInterface* TopMaterial = Tops[FMath::Max(Cell.Island, 0) % 3];
				if (Cell.Level > 0)
				{
					// Higher ground in the next pattern, so terraces read at a glance.
					TopMaterial = Tops[(FMath::Max(Cell.Island, 0) + Cell.Level) % 3];
				}
				if (Cell.bPuddle)
				{
					TopMaterial = Puddle;
				}
				if (Cell.Stack != INDEX_NONE)
				{
					TopMaterial = LoadObject<UMaterialInterface>(nullptr, StackColors[Cell.Stack]);
				}
				Ground->SetMaterial(0, TopMaterial);
				Ground->SetMaterial(1, Cell.Stack != INDEX_NONE ? TopMaterial : Side);
			}
		}
		if (Cell.Type != 1 && Modifier)
		{
			const FTransform ModifierTransform(Surface + FVector(0.f, 0.f, 20.f));
			if (AActor* Mod = World.SpawnActorDeferred<AActor>(Modifier, ModifierTransform))
			{
				if (FByteProperty* TypeProperty = CastField<FByteProperty>(KeepersReflection::FindMember(Mod->GetClass(), TEXT("Type"))))
				{
					TypeProperty->SetPropertyValue_InContainer(Mod, Cell.Type);
				}
				UGameplayStatics::FinishSpawningActor(Mod, ModifierTransform);
				Mod->SetActorHiddenInGame(true);
			}
		}
		if (Cell.bTree)
		{
			UStaticMesh* Tree = Trees[Random.RandRange(0, UE_ARRAY_COUNT(Trees) - 1)];
			SpawnMesh(Tree, FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Surface, FVector(Random.FRandRange(0.75f, 1.f))), false, true);
		}
		if (Cell.bBush)
		{
			UStaticMesh* Bush = Bushes[Random.RandRange(0, UE_ARRAY_COUNT(Bushes) - 1)];
			SpawnMesh(Bush, FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Surface + FVector(Random.FRandRange(-50.f, 50.f), Random.FRandRange(-50.f, 50.f), 0.f), FVector(Random.FRandRange(0.8f, 1.3f))), false, false, false);
		}
		if (Cell.bRock && Rock)
		{
			// A little heap: a big stone and one or two small ones.
			for (int32 Stone = 0; Stone < Random.RandRange(2, 3); ++Stone)
			{
				const float Big = Stone == 0 ? 1.f : 0.55f;
				const FVector Size(Random.FRandRange(0.5f, 0.7f) * Big, Random.FRandRange(0.5f, 0.7f) * Big, Random.FRandRange(0.6f, 1.1f) * Big);
				const FVector Offset = Stone == 0 ? FVector::ZeroVector : FVector(Random.FRandRange(-55.f, 55.f), Random.FRandRange(-55.f, 55.f), 0.f);
				if (UStaticMeshComponent* Piece = SpawnMesh(Rock, FTransform(FRotator(Random.FRandRange(-8.f, 8.f), Random.FRandRange(0.f, 90.f), 0.f), Surface + Offset + FVector(0.f, 0.f, Size.Z * 45.f), Size), false, true))
				{
					Piece->SetMaterial(0, RockColor);
				}
			}
		}
		if (Cell.House != INDEX_NONE)
		{
			const FString Name = UsedBiome == EKeepersBiome::Cursed ? FString::Printf(TEXT("Construction/%s_0"), Houses[Cell.House]) : FString(Houses[Cell.House]);
			const FString Leaf = FPaths::GetCleanFilename(Name);
			UStaticMesh* House = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Toony_Tiny_RTS_Set/Meshes/Buildings/%s.%s"), *Name, *Leaf));
			if (UStaticMeshComponent* Building = SpawnMesh(House, FTransform(FRotator(0.f, Cell.Yaw, 0.f), Surface, FVector(FitScale(House, 0.95f))), false, true))
			{
				for (int32 Slot = 0; Slot < Building->GetNumMaterials(); ++Slot)
				{
					Building->SetMaterial(Slot, BuildingColors[(Cell.Index.X + Cell.Index.Y) % UE_ARRAY_COUNT(BuildingColors)]);
				}
			}
		}
		if (Cell.bFence && Fence)
		{
			// A low wall along the run: blocks the way, not the view.
			const FVector Extent = Fence->GetBounds().BoxExtent;
			const FVector Scale(0.35f, TileSize.X / FMath::Max(Extent.Y * 2.f, 1.f), 0.17f);
			SpawnMesh(Fence, FTransform(FRotator(0.f, Cell.Yaw + 90.f, 0.f), Surface, Scale), false, false);
		}
	}

	// Units start on the chosen cells: the scenario's UnitDesiredIndexes, team by team.
	FProperty* UnitsProperty = nullptr;
	void* UnitsData = KeepersReflection::FindValuePtr(Scenario->GetClass(), Scenario, TEXT("UnitsData"), &UnitsProperty);
	const FStructProperty* UnitsStruct = CastField<FStructProperty>(UnitsProperty);
	FProperty* TeamsProperty = nullptr;
	void* Teams = UnitsStruct ? KeepersReflection::FindValuePtr(UnitsStruct->Struct, UnitsData, TEXT("UnitTeams"), &TeamsProperty) : nullptr;
	FProperty* DesiredProperty = nullptr;
	void* Desired = UnitsStruct ? KeepersReflection::FindValuePtr(UnitsStruct->Struct, UnitsData, TEXT("UnitDesiredIndexes"), &DesiredProperty) : nullptr;
	FArrayProperty* TeamsArray = CastField<FArrayProperty>(TeamsProperty);
	FMapProperty* DesiredMap = CastField<FMapProperty>(DesiredProperty);
	if (TeamsArray && DesiredMap)
	{
		FScriptArrayHelper TeamHelper(TeamsArray, Teams);
		FScriptMapHelper DesiredHelper(DesiredMap, Desired);
		DesiredHelper.EmptyValues();
		const FStructProperty* TeamStruct = CastField<FStructProperty>(TeamsArray->Inner);
		for (int32 Team = 0; Team < TeamHelper.Num() && TeamStruct; ++Team)
		{
			TArray<const FKeepersGenCell*> Starts;
			for (const FKeepersGenCell& Cell : Cells)
			{
				if (Cell.Start == (Team == 0 ? 1 : 2))
				{
					Starts.Add(&Cell);
				}
			}
			FProperty* UnitArrayProperty = nullptr;
			void* UnitArray = KeepersReflection::FindValuePtr(TeamStruct->Struct, TeamHelper.GetRawPtr(Team), TEXT("UnitArray"), &UnitArrayProperty);
			FArrayProperty* Units = CastField<FArrayProperty>(UnitArrayProperty);
			FObjectPropertyBase* UnitInner = Units ? CastField<FObjectPropertyBase>(Units->Inner) : nullptr;
			if (!UnitInner)
			{
				continue;
			}
			FScriptArrayHelper UnitHelper(Units, UnitArray);
			for (int32 I = 0; I < UnitHelper.Num() && I < Starts.Num(); ++I)
			{
				UObject* Unit = UnitInner->GetObjectPropertyValue(UnitHelper.GetRawPtr(I));
				if (AActor* UnitActor = Cast<AActor>(Unit))
				{
					UnitActor->SetActorLocation(FVector(Starts[I]->Location.X, Starts[I]->Location.Y, Starts[I]->Level * TileSize.Z + 100.f));
					const int32 Pair = DesiredHelper.AddDefaultValue_Invalid_NeedsRehash();
					*reinterpret_cast<UObject**>(DesiredHelper.GetKeyPtr(Pair)) = Unit;
					*reinterpret_cast<FIntPoint*>(DesiredHelper.GetValuePtr(Pair)) = Starts[I]->Index;
				}
			}
		}
		DesiredHelper.Rehash();
	}

	// The camera starts over the player's island: the player pawn spawns at the player start.
	FVector PlayerIsland = FVector::ZeroVector;
	int32 StartCount = 0;
	for (const FKeepersGenCell& Cell : Cells)
	{
		if (Cell.Start == 1)
		{
			PlayerIsland += Cell.Location;
			++StartCount;
		}
	}
	if (StartCount > 0)
	{
		PlayerIsland /= StartCount;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("PlayerStart"))
			{
				It->SetActorLocation(FVector(FMath::Lerp(PlayerIsland.X, 0.f, 0.35f), FMath::Lerp(PlayerIsland.Y, 0.f, 0.35f), 200.f));
			}
		}
	}

	UKeepersBattleScenerySubsystem::SurroundField(World);
	// The land around the field takes the biome's colours.
	static const TCHAR* Hills[] =
	{
		TEXT("/Game/Art/Materials/Colors/MI_Green_02.MI_Green_02"), TEXT("/Game/Art/Materials/Colors/MI_Green_01.MI_Green_01"),
		TEXT("/Game/Art/Materials/Colors/MI_Green_04.MI_Green_04"), TEXT("/Game/Art/Materials/Colors/MI_Grey_04.MI_Grey_04"),
		TEXT("/Game/Art/Materials/Colors/MI_Grey_01.MI_Grey_01"), TEXT("/Game/Art/Materials/Colors/MI_Purple_04.MI_Purple_04"),
	};
	if (UsedBiome != EKeepersBiome::Meadow)
	{
		UMaterialInterface* HillTop = LoadObject<UMaterialInterface>(nullptr, Hills[static_cast<int32>(UsedBiome)]);
		for (TActorIterator<AKeepersBattleScenery> It(&World); It && HillTop; ++It)
		{
			TArray<UStaticMeshComponent*> Parts;
			It->GetComponents(Parts);
			for (UStaticMeshComponent* Part : Parts)
			{
				const FString Mesh = Part->GetStaticMesh() ? Part->GetStaticMesh()->GetName() : FString();
				if (Mesh.StartsWith(TEXT("SM_Hexagon")) || Mesh == TEXT("Plane"))
				{
					Part->SetMaterial(0, HillTop);
				}
			}
		}
	}
	UE_LOG(LogKeepersMapGen, Log, TEXT("Generated %s, seed %d: %d cells"), *BiomeName(UsedBiome).ToString(), UsedSeed, Cells.Num());
}

// ---------------------------------------------------------------------------------------------------------------------

void UKeepersMapGenLibrary::SetNextGeneratedMap(int32 Seed, EKeepersBiome Biome)
{
	UKeepersMapGenerator::SetNextMap(Seed, Biome);
}

int32 UKeepersMapGenLibrary::CheckSeeds(EKeepersBiome Biome, int32 FirstSeed, int32 Count)
{
	// A plain hexagon layout with the same proportions as BP_Grid's (Y indexes doubled).
	auto LocationOf = [](const FIntPoint& Index) { return FVector(Index.X * 173.2f, Index.Y * 100.f, 0.f); };
	int32 Passed = 0;
	for (int32 Seed = FirstSeed; Seed < FirstSeed + Count; ++Seed)
	{
		TArray<FKeepersGenCell> Cells;
		if (UKeepersMapGenerator::Layout(Seed, Biome, LocationOf, KeepersMapGen::CountX, KeepersMapGen::CountY, Cells))
		{
			++Passed;
		}
		else
		{
			UE_LOG(LogKeepersMapGen, Warning, TEXT("CheckSeeds: %s seed %d failed"), *UKeepersMapGenerator::BiomeName(Biome).ToString(), Seed);
		}
	}
	return Passed;
}
