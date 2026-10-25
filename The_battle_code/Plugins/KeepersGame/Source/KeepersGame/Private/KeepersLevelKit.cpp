#include "KeepersLevelKit.h"

#include "EngineUtils.h"
#include "KeepersReflection.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersLevelKit, Log, All);

const TCHAR* KeepersLevelKit::TablePath = TEXT("/Game/Keepers/Levels/DT_KeepersLevels.DT_KeepersLevels");

namespace
{
	FString ShortName(const FString& Path)
	{
		FString Name = FPackageName::GetShortName(Path);
		Name.Split(TEXT("."), nullptr, &Name);
		return Name.IsEmpty() ? FPackageName::GetShortName(Path) : Name;
	}

	void* UnitsMember(const FKeepersLevelGrid& Grid, const TCHAR* Name, FProperty** OutProperty)
	{
		FProperty* UnitsProperty = nullptr;
		void* UnitsData = Grid.Scenario ? KeepersReflection::FindValuePtr(Grid.Scenario->GetClass(), Grid.Scenario, TEXT("UnitsData"), &UnitsProperty) : nullptr;
		const FStructProperty* UnitsStruct = CastField<FStructProperty>(UnitsProperty);
		return UnitsStruct ? KeepersReflection::FindValuePtr(UnitsStruct->Struct, UnitsData, Name, OutProperty) : nullptr;
	}

	/** The UnitArray of each team, as array helpers' raw parts. */
	template <typename FVisit>
	void ForEachTeamArray(const FKeepersLevelGrid& Grid, FVisit&& Visit)
	{
		FProperty* TeamsProperty = nullptr;
		void* Teams = UnitsMember(Grid, TEXT("UnitTeams"), &TeamsProperty);
		FArrayProperty* TeamsArray = CastField<FArrayProperty>(TeamsProperty);
		const FStructProperty* TeamStruct = TeamsArray ? CastField<FStructProperty>(TeamsArray->Inner) : nullptr;
		if (!TeamStruct)
		{
			return;
		}
		FScriptArrayHelper TeamHelper(TeamsArray, Teams);
		for (int32 Team = 0; Team < TeamHelper.Num(); ++Team)
		{
			FProperty* UnitArrayProperty = nullptr;
			void* UnitArray = KeepersReflection::FindValuePtr(TeamStruct->Struct, TeamHelper.GetRawPtr(Team), TEXT("UnitArray"), &UnitArrayProperty);
			FArrayProperty* Units = CastField<FArrayProperty>(UnitArrayProperty);
			FObjectPropertyBase* Inner = Units ? CastField<FObjectPropertyBase>(Units->Inner) : nullptr;
			if (Inner)
			{
				Visit(Team, Units, Inner, UnitArray);
			}
		}
	}
}

UDataTable* KeepersLevelKit::GetTable()
{
	return LoadObject<UDataTable>(nullptr, TablePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

TArray<const FKeepersLevelRow*> KeepersLevelKit::GetRows()
{
	TArray<const FKeepersLevelRow*> Rows;
	if (UDataTable* Table = GetTable())
	{
		Table->ForeachRow<FKeepersLevelRow>(TEXT("KeepersLevelKit"), [&Rows](const FName&, const FKeepersLevelRow& Row)
		{
			if (!Row.Level.IsNull())
			{
				Rows.Add(&Row);
			}
		});
	}
	return Rows;
}

const FKeepersLevelRow* KeepersLevelKit::FindRow(FName Level)
{
	const FString Wanted = ShortName(UWorld::RemovePIEPrefix(Level.ToString()));
	for (const FKeepersLevelRow* Row : GetRows())
	{
		if (ShortName(Row->Level.ToSoftObjectPath().GetLongPackageName()) == Wanted)
		{
			return Row;
		}
	}
	return nullptr;
}

bool KeepersLevelKit::PrepareGrid(UWorld* World, FKeepersLevelGrid& Out)
{
	Out = FKeepersLevelGrid();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const FString Class = It->GetClass()->GetName();
		Out.Grid = Class == TEXT("BP_Grid_C") ? *It : Out.Grid;
		Out.Scenario = Class == TEXT("BP_CombatScenario_C") ? *It : Out.Scenario;
	}
	FProperty* GridDataProperty = nullptr;
	void* GridData = Out.Scenario ? KeepersReflection::FindValuePtr(Out.Scenario->GetClass(), Out.Scenario, TEXT("GridData"), &GridDataProperty) : nullptr;
	const FStructProperty* GridDataStruct = CastField<FStructProperty>(GridDataProperty);
	if (!Out.Grid || !GridDataStruct)
	{
		return false;
	}
	// Copy the scenario's grid settings into the grid, the way the scenario does when combat starts.
	static const TCHAR* Pairs[][2] =
	{
		{ TEXT("CenterLocation"), TEXT("GridCenterLocation") },
		{ TEXT("TileSize"), TEXT("GridTileSize") },
		{ TEXT("TileCount"), TEXT("GridTileCount") },
		{ TEXT("Shape"), TEXT("GridShape") },
	};
	for (const auto& Pair : Pairs)
	{
		FProperty* From = nullptr;
		FProperty* To = nullptr;
		const void* Value = KeepersReflection::FindValuePtr(GridDataStruct->Struct, GridData, Pair[0], &From);
		void* Target = KeepersReflection::FindValuePtr(Out.Grid->GetClass(), Out.Grid, Pair[1], &To);
		if (Value && Target && From->GetElementSize() == To->GetElementSize())
		{
			To->CopyCompleteValue(Target, Value);
		}
	}
	if (const FVector* Size = static_cast<const FVector*>(KeepersReflection::FindValuePtr(Out.Grid->GetClass(), Out.Grid, TEXT("GridTileSize"))))
	{
		Out.TileSize = *Size;
	}
	KeepersReflection::GetIntPoint(Out.Grid->GetClass(), Out.Grid, TEXT("GridTileCount"), Out.Count);
	if (const uint8* Shape = static_cast<const uint8*>(KeepersReflection::FindValuePtr(Out.Grid->GetClass(), Out.Grid, TEXT("GridShape"))))
	{
		Out.Shape = *Shape;
	}
	KeepersReflection::FCall Corner(Out.Grid, TEXT("CalculateCenterAndBottomLeft"));
	FVector BottomLeft;
	FProperty* CornerProperty = nullptr;
	if (Corner.Call() && Corner.Get<FVector>(TEXT("BottomLeft"), BottomLeft))
	{
		if (void* Target = KeepersReflection::FindValuePtr(Out.Grid->GetClass(), Out.Grid, TEXT("GridBottomLeftCornerLocation"), &CornerProperty))
		{
			CornerProperty->CopyCompleteValue(Target, &BottomLeft);
		}
	}
	return true;
}

FVector KeepersLevelKit::TileLocation(const FKeepersLevelGrid& Grid, const FIntPoint& Index)
{
	KeepersReflection::FCall Where(Grid.Grid, TEXT("GetTileLocationFromGridIndex"));
	Where.Set<FIntPoint>(TEXT("GridIndex"), Index);
	FVector Location = FVector::ZeroVector;
	if (Where.Call())
	{
		Where.Get<FVector>(TEXT("TileLocation"), Location);
	}
	return Location;
}

TArray<FIntPoint> KeepersLevelKit::AllIndexes(const FKeepersLevelGrid& Grid)
{
	TArray<FIntPoint> Indexes;
	for (int32 X = 0; X < Grid.Count.X; ++X)
	{
		// Hexagons: Y is doubled, odd columns start at 1 (the order of BP_Grid.SpawnGrid).
		const int32 Step = Grid.IsHex() ? 2 : 1;
		for (int32 Y = Grid.IsHex() ? X % 2 : 0; Y < Grid.Count.Y * Step; Y += Step)
		{
			Indexes.Emplace(X, Y);
		}
	}
	return Indexes;
}

bool KeepersLevelKit::TileIndex(const FKeepersLevelGrid& Grid, const FVector& Location, FIntPoint& OutIndex)
{
	KeepersReflection::FCall Which(Grid.Grid, TEXT("GetTileIndexFromWorldLocation"));
	Which.Set<FVector>(TEXT("Location"), Location);
	if (!Which.Call() || !Which.Get<FIntPoint>(TEXT("Index"), OutIndex))
	{
		return false;
	}
	const int32 MaxY = Grid.IsHex() ? Grid.Count.Y * 2 : Grid.Count.Y;
	return OutIndex.X >= 0 && OutIndex.Y >= 0 && OutIndex.X < Grid.Count.X && OutIndex.Y < MaxY
		&& (!Grid.IsHex() || (OutIndex.X + OutIndex.Y) % 2 == 0);
}

TArray<FIntPoint> KeepersLevelKit::Neighbors(const FKeepersLevelGrid& Grid, const FIntPoint& Index)
{
	static const FIntPoint Hex[] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }, { 0, 2 }, { 0, -2 } };
	static const FIntPoint Square[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	TArray<FIntPoint> Result;
	if (Grid.IsHex())
	{
		for (const FIntPoint& Step : Hex)
		{
			Result.Add(Index + Step);
		}
	}
	else
	{
		for (const FIntPoint& Step : Square)
		{
			Result.Add(Index + Step);
		}
	}
	return Result;
}

uint8 KeepersLevelKit::TraceTile(const FKeepersLevelGrid& Grid, const FIntPoint& Index, FVector& OutGround)
{
	KeepersReflection::FCall Trace(Grid.Grid, TEXT("TraceForGround"));
	Trace.Set<FVector>(TEXT("Location"), TileLocation(Grid, Index));
	uint8 Type = 0;
	if (Trace.Call())
	{
		Trace.Get<uint8>(TEXT("TileType"), Type);
		Trace.Get<FVector>(TEXT("OutLocation"), OutGround);
	}
	return Type;
}

TArray<TArray<AActor*>> KeepersLevelKit::GetTeams(const FKeepersLevelGrid& Grid)
{
	TArray<TArray<AActor*>> Teams;
	ForEachTeamArray(Grid, [&Teams](int32 Team, FArrayProperty* Units, FObjectPropertyBase* Inner, void* UnitArray)
	{
		TArray<AActor*>& Members = Teams.AddDefaulted_GetRef();
		FScriptArrayHelper Helper(Units, UnitArray);
		for (int32 I = 0; I < Helper.Num(); ++I)
		{
			if (AActor* Unit = Cast<AActor>(Inner->GetObjectPropertyValue(Helper.GetRawPtr(I))))
			{
				Members.Add(Unit);
			}
		}
	});
	return Teams;
}

TArray<AActor*> KeepersLevelKit::GetLevelUnits(UWorld* World)
{
	TArray<AActor*> Units;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("BP_Unit_C"))
		{
			Units.Add(*It);
		}
	}
	return Units;
}

int32 KeepersLevelKit::CollectUnits(const FKeepersLevelGrid& Grid)
{
	if (!Grid.Scenario)
	{
		return 0;
	}
	TSet<AActor*> Known;
	for (const TArray<AActor*>& Team : GetTeams(Grid))
	{
		Known.Append(Team);
	}
	int32 Added = 0;
	for (AActor* Unit : GetLevelUnits(Grid.Scenario->GetWorld()))
	{
		if (Known.Contains(Unit))
		{
			continue;
		}
		const int32 Wanted = Unit->ActorHasTag(TEXT("Enemy")) ? 1 : 0;
		ForEachTeamArray(Grid, [&](int32 Team, FArrayProperty* Units, FObjectPropertyBase* Inner, void* UnitArray)
		{
			if (Team == Wanted)
			{
				FScriptArrayHelper Helper(Units, UnitArray);
				Inner->SetObjectPropertyValue(Helper.GetRawPtr(Helper.AddValue()), Unit);
				++Added;
			}
		});
	}
	return Added;
}

int32 KeepersLevelKit::StartUnitsWhereTheyStand(const FKeepersLevelGrid& Grid)
{
	FProperty* DesiredProperty = nullptr;
	void* Desired = UnitsMember(Grid, TEXT("UnitDesiredIndexes"), &DesiredProperty);
	FMapProperty* DesiredMap = CastField<FMapProperty>(DesiredProperty);
	if (!DesiredMap)
	{
		return 0;
	}
	FScriptMapHelper Helper(DesiredMap, Desired);
	Helper.EmptyValues();
	int32 Placed = 0;
	for (const TArray<AActor*>& Team : GetTeams(Grid))
	{
		for (AActor* Unit : Team)
		{
			FIntPoint Index;
			if (!TileIndex(Grid, Unit->GetActorLocation(), Index))
			{
				UE_LOG(LogKeepersLevelKit, Warning, TEXT("%s stands outside the grid"), *Unit->GetName());
				continue;
			}
			const int32 Pair = Helper.AddDefaultValue_Invalid_NeedsRehash();
			*reinterpret_cast<UObject**>(Helper.GetKeyPtr(Pair)) = Unit;
			*reinterpret_cast<FIntPoint*>(Helper.GetValuePtr(Pair)) = Index;
			++Placed;
		}
	}
	Helper.Rehash();
	return Placed;
}

bool UKeepersLevelSetupSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKeepersLevelSetupSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FKeepersLevelRow* Row = KeepersLevelKit::FindRow(InWorld.GetOutermost()->GetFName());
	FKeepersLevelGrid Grid;
	if (!Row || !Row->bStartsFromPlacement || !KeepersLevelKit::PrepareGrid(&InWorld, Grid))
	{
		return;
	}
	const int32 Added = KeepersLevelKit::CollectUnits(Grid);
	const int32 Placed = KeepersLevelKit::StartUnitsWhereTheyStand(Grid);
	UE_LOG(LogKeepersLevelKit, Log, TEXT("%s: %d units joined the teams, %d start where they stand"), *InWorld.GetMapName(), Added, Placed);
}
