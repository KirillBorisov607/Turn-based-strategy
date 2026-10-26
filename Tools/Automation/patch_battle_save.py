"""Saving in the middle of a region: a campaign battle is saved at the start of every round (units, health,
the fallen, rocks, forest, ice, the round) and "Продолжить" brings the player back into it."""
import os
os.chdir(r'C:/ProjectsUE/Turn-based-strategy/The_battle_code/Plugins/KeepersGame/Source/KeepersGame')


def patch(p, pairs):
    s = open(p, 'rb').read().decode('utf-8-sig')
    crlf = '\r\n' in s
    s = s.replace('\r\n', '\n')
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='\r\n' if crlf else '\n').write(s)


patch('Public/KeepersCampaign.h', [
('''/** A relic found during the campaign; it changes a rule for the rest of the run. */''', '''/** A unit of a saved battle. */
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
};

/** A relic found during the campaign; it changes a rule for the rest of the run. */'''),
('''	UPROPERTY(SaveGame)
	TArray<FString> RecruitNames;
};''', '''	UPROPERTY(SaveGame)
	TArray<FString> RecruitNames;

	/** The battle in progress, saved at the start of its last round. */
	UPROPERTY(SaveGame)
	FKeepersBattleSnapshot Battle;
};'''),
('''	/** Enemies (E_UnitType) who join the next battle, whatever it is (quick battles, tests). */''', '''	/** The battle in progress: saved every round, resumed by ContinueCampaign. */
	void SaveBattle(const FKeepersBattleSnapshot& Snapshot);
	/** The saved battle to put back after "Продолжить", or nullptr. */
	const FKeepersBattleSnapshot* GetBattleToResume() const;
	void ForgetBattle();
	/** Set by ContinueCampaign when it opens an interrupted battle. */
	bool bResumeBattle = false;

	/** Enemies (E_UnitType) who join the next battle, whatever it is (quick battles, tests). */'''),
])

patch('Private/KeepersCampaign.cpp', [
('''	Save = Loaded;
	bActive = true;
	// A battle that was interrupted is played again.
	if (Save->CurrentNode != INDEX_NONE && !Save->VisitedNodes.Contains(Save->CurrentNode))
	{
		Save->CurrentNode = Save->VisitedNodes.Num() > 0 ? Save->VisitedNodes.Last() : INDEX_NONE;
	}
	OpenLevel(MapLevel);
	return true;''', '''	Save = Loaded;
	bActive = true;
	// An interrupted battle goes on from its last round (or from the start when it had none yet).
	if (Save->Nodes.IsValidIndex(Save->CurrentNode) && !Save->VisitedNodes.Contains(Save->CurrentNode) && !Save->Nodes[Save->CurrentNode].Level.IsNone())
	{
		bResumeBattle = Save->Battle.Node == Save->CurrentNode;
		UE_LOG(LogKeepersCampaign, Log, TEXT("Continue the battle of node %d%s"), Save->CurrentNode, bResumeBattle ? *FString::Printf(TEXT(" from round %d"), Save->Battle.Round) : TEXT(""));
		PrepareGeneratedField(Save->CurrentNode);
		OpenLevel(Save->Nodes[Save->CurrentNode].Level);
		return true;
	}
	// A camp or an event that was left open: back on the map.
	if (Save->CurrentNode != INDEX_NONE && !Save->VisitedNodes.Contains(Save->CurrentNode))
	{
		Save->CurrentNode = Save->VisitedNodes.Num() > 0 ? Save->VisitedNodes.Last() : INDEX_NONE;
	}
	OpenLevel(MapLevel);
	return true;'''),
('''void UKeepersCampaignSubsystem::CompleteCurrentNode()
{
	if (!Save->Nodes.IsValidIndex(Save->CurrentNode))
	{
		return;
	}''', '''void UKeepersCampaignSubsystem::SaveBattle(const FKeepersBattleSnapshot& Snapshot)
{
	if (!Save || !bActive)
	{
		return;
	}
	Save->Battle = Snapshot;
	WriteSave();
}

const FKeepersBattleSnapshot* UKeepersCampaignSubsystem::GetBattleToResume() const
{
	return bResumeBattle && Save && Save->Battle.Node != INDEX_NONE && Save->Battle.Node == Save->CurrentNode ? &Save->Battle : nullptr;
}

void UKeepersCampaignSubsystem::ForgetBattle()
{
	bResumeBattle = false;
	if (Save && Save->Battle.Node != INDEX_NONE)
	{
		Save->Battle = FKeepersBattleSnapshot();
		WriteSave();
	}
}

void UKeepersCampaignSubsystem::CompleteCurrentNode()
{
	if (!Save->Nodes.IsValidIndex(Save->CurrentNode))
	{
		return;
	}
	Save->Battle = FKeepersBattleSnapshot();'''),
('''void UKeepersCampaignSubsystem::RetryCurrentNode()
{''', '''void UKeepersCampaignSubsystem::RetryCurrentNode()
{
	ForgetBattle();'''),
])

# The terrain remembers what battle changed, and can put it back.
patch('Public/KeepersTerrainSubsystem.h', [
('''	/** Was this tile turned into forest by Sprout? */
	bool IsForest(const FIntPoint& Index) const { return ForestTiles.Contains(Index); }''', '''	/** Was this tile turned into forest by Sprout? */
	bool IsForest(const FIntPoint& Index) const { return ForestTiles.Contains(Index); }

	/** What the battle changed: raised rocks, grown forest, frozen water (for saving a battle). */
	TArray<FIntPoint> GetRocks() const { return RockTiles.Array(); }
	TArray<FIntPoint> GetForests() const { return ForestTiles.Array(); }
	TArray<FIntPoint> GetIce() const { return IceTiles.Array(); }

	/** Water on the tile becomes walkable ice with a plate; false if it is not water. */
	bool FreezeWater(UObject* Grid, const FIntPoint& Index);'''),
('''	TSet<FIntPoint> ForestTiles;''', '''	TSet<FIntPoint> ForestTiles;
	TSet<FIntPoint> RockTiles;
	TSet<FIntPoint> IceTiles;'''),
])
patch('Private/KeepersTerrainSubsystem.cpp', [
('''		Rock->Rise(TileTransform.GetLocation(), TileTransform.GetLocation().Z + TileSize.Z * Levels);
		TerrainProps.Add(Rock);
	}
''', '''		Rock->Rise(TileTransform.GetLocation(), TileTransform.GetLocation().Z + TileSize.Z * Levels);
		TerrainProps.Add(Rock);
	}
	RockTiles.Add(Index);
'''),
('''		SetTileType(Grid, Index, 1, Unused);
		ForestTiles.Remove(Index);
		bCleared = true;''', '''		SetTileType(Grid, Index, 1, Unused);
		ForestTiles.Remove(Index);
		RockTiles.Remove(Index);
		bCleared = true;'''),
('''		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		const float Size = KeepersTerrain::GetTileSize(Grid).X;
		for (const FIntPoint& Index : Path)
		{
			uint8 Type = 0;
			FTransform Tile;
			if (!KeepersTerrain::GetTileType(Grid, Index, Type) || Type != KeepersTerrain::ChasmTileType || !SetTileType(Grid, Index, 1, Tile))
			{
				continue;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* Ice = World->SpawnActor<AStaticMeshActor>(Tile.GetLocation() + FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, Params))
			{
				Ice->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Ice->GetStaticMeshComponent()->SetStaticMesh(Cube);
				Ice->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Ice->SetActorScale3D(FVector(Size / 100.f * 0.48f, Size / 100.f * 0.48f, 0.08f));
				if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
					LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), Ice))
				{
					Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.9f, 1.f));
					Ice->GetStaticMeshComponent()->SetMaterial(0, Material);
				}
				TerrainProps.Add(Ice);
			}
			UKeepersJuiceSubsystem::Burst(World, Tile.GetLocation() + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Hit, FLinearColor(0.8f, 0.95f, 1.f), 14);
			// Frozen water is ice: who stops on it slides.
			if (UKeepersSurfaceSubsystem* Surfaces = UKeepersSurfaceSubsystem::Get(this))
			{
				Surfaces->Paint(Index, EKeepersSurface::Ice, false);
			}
			UE_LOG(LogKeepersTerrain, Log, TEXT("IcePath freezes (%d,%d)"), Index.X, Index.Y);
		}''', '''		for (const FIntPoint& Index : Path)
		{
			FVector Location;
			if (FreezeWater(Grid, Index) && KeepersTerrain::GetTileLocation(Grid, Index, Location))
			{
				UKeepersJuiceSubsystem::Burst(World, Location + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Hit, FLinearColor(0.8f, 0.95f, 1.f), 14);
				UE_LOG(LogKeepersTerrain, Log, TEXT("IcePath freezes (%d,%d)"), Index.X, Index.Y);
			}
		}'''),
('''bool UKeepersTerrainSubsystem::ClearProp(UObject* Grid, const FIntPoint& Index)''', '''bool UKeepersTerrainSubsystem::FreezeWater(UObject* Grid, const FIntPoint& Index)
{
	uint8 Type = 0;
	FTransform Tile;
	if (!KeepersTerrain::GetTileType(Grid, Index, Type) || Type != KeepersTerrain::ChasmTileType || !SetTileType(Grid, Index, 1, Tile))
	{
		return false;
	}
	const float Size = KeepersTerrain::GetTileSize(Grid).X;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AStaticMeshActor* Ice = GetWorld()->SpawnActor<AStaticMeshActor>(Tile.GetLocation() + FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, Params))
	{
		Ice->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Ice->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Ice->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ice->SetActorScale3D(FVector(Size / 100.f * 0.48f, Size / 100.f * 0.48f, 0.08f));
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), Ice))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.9f, 1.f));
			Ice->GetStaticMeshComponent()->SetMaterial(0, Material);
		}
		TerrainProps.Add(Ice);
	}
	IceTiles.Add(Index);
	// Frozen water is ice: who stops on it slides.
	if (UKeepersSurfaceSubsystem* Surfaces = UKeepersSurfaceSubsystem::Get(this))
	{
		Surfaces->Paint(Index, EKeepersSurface::Ice, false);
	}
	return true;
}

bool UKeepersTerrainSubsystem::ClearProp(UObject* Grid, const FIntPoint& Index)'''),
])

patch('Public/KeepersBattleObjective.h', [
('''	/** Current round (1 = the first player phase). */''', '''	/** A saved battle goes on from this round. */
	void RestoreRound(int32 InRound);

	/** Current round (1 = the first player phase). */'''),
])
patch('Private/KeepersBattleObjective.cpp', [
('''void AKeepersBattleObjective::UpdateWidget()
{''', '''void AKeepersBattleObjective::RestoreRound(int32 InRound)
{
	Round = InRound;
	LastTeam = 0;
	UpdateWidget();
}

void AKeepersBattleObjective::UpdateWidget()
{'''),
])

# The campaign's battle screen saves every round and puts a saved battle back.
patch('Public/KeepersCampaignUI.h', [
('''	void OnRoundStarted(UObject* CombatSystem);''', '''	void OnRoundStarted(UObject* CombatSystem);
	/** Saves the battle (once the mercenaries and the objective are in place). */
	void SaveBattle(UObject* CombatSystem);
	/** Puts the saved battle back: the field, the fallen, positions, health, the round. */
	void ResumeBattle(UObject* CombatSystem);
	bool bSaveDue = false;
	bool bResumeChecked = false;'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''			if (Team == 0 && LastTeam != 0)
			{
				++Round;
				OnRoundStarted(CombatSystem);
			}
			LastTeam = Team;
		}
	}''', '''			if (Team == 0 && LastTeam != 0)
			{
				++Round;
				OnRoundStarted(CombatSystem);
				bSaveDue = true;
			}
			LastTeam = Team;
		}
		// The field is ready once the mercenaries have come and the objective is set.
		if (bMercenariesDone && bObjectiveChecked)
		{
			if (!bResumeChecked)
			{
				bResumeChecked = true;
				ResumeBattle(CombatSystem);
			}
			if (bSaveDue)
			{
				bSaveDue = false;
				SaveBattle(CombatSystem);
			}
		}
	}'''),
('''void UKeepersCampaignWorldSubsystem::OnBattleFinished(UObject* CombatSystem, int32 Winner)
{''', '''void UKeepersCampaignWorldSubsystem::SaveBattle(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	if (!Campaign || !Campaign->IsInBattleNode())
	{
		return;
	}
	FKeepersBattleSnapshot Snapshot;
	Snapshot.Node = Campaign->GetCurrentNode();
	Snapshot.Round = Round;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		bool bAlive = false;
		KeepersReflection::FCall IsAlive(*It, TEXT("IsAlive"));
		if (It->GetClass()->GetName() != TEXT("BP_Unit_C") || !IsAlive.Call() || !IsAlive.Get<bool>(TEXT("IsAlive"), bAlive) || !bAlive)
		{
			continue;
		}
		FKeepersUnitSnapshot& Unit = Snapshot.Units.AddDefaulted_GetRef();
		Unit.Name = It->GetName();
		KeepersReflection::GetIntPoint(It->GetClass(), *It, TEXT("Index"), Unit.Index);
		KeepersReflection::GetUnitStat(*It, TEXT("HP_Current"), Unit.Hp);
	}
	if (const UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>())
	{
		Snapshot.Rocks = Terrain->GetRocks();
		Snapshot.Forests = Terrain->GetForests();
		Snapshot.Ice = Terrain->GetIce();
	}
	Campaign->SaveBattle(Snapshot);
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Battle saved: round %d, %d units, %d rocks, %d forest"), Round, Snapshot.Units.Num(), Snapshot.Rocks.Num(), Snapshot.Forests.Num());
}

void UKeepersCampaignWorldSubsystem::ResumeBattle(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const FKeepersBattleSnapshot* Saved = Campaign ? Campaign->GetBattleToResume() : nullptr;
	if (!Saved)
	{
		return;
	}
	const FKeepersBattleSnapshot Snapshot = *Saved;
	Campaign->bResumeBattle = false;
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>();
	if (!Grid || !Terrain)
	{
		return;
	}
	// The field first: rocks, forest and ice where they were.
	for (const FIntPoint& Index : Snapshot.Ice)
	{
		Terrain->FreezeWater(Grid, Index);
	}
	for (const FIntPoint& Index : Snapshot.Forests)
	{
		Terrain->GrowForest(Grid, Index);
	}
	TMap<FString, const FKeepersUnitSnapshot*> ByName;
	for (const FKeepersUnitSnapshot& Unit : Snapshot.Units)
	{
		ByName.Add(Unit.Name, &Unit);
	}
	// The fallen fall again; the living get their health.
	TArray<TPair<AActor*, FIntPoint>> Moves;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() != TEXT("BP_Unit_C"))
		{
			continue;
		}
		const FKeepersUnitSnapshot* const* Found = ByName.Find(It->GetName());
		if (!Found)
		{
			UKeepersTerrainSubsystem::DamageUnit(CombatSystem, *It, 9999);
			continue;
		}
		int32 Hp = 0;
		KeepersReflection::GetUnitStat(*It, TEXT("HP_Current"), Hp);
		if (Hp != (*Found)->Hp)
		{
			KeepersReflection::FCall Modify(*It, TEXT("ModifyCurrentHp"));
			Modify.Set<int32>(TEXT("Modifier"), (*Found)->Hp - Hp);
			Modify.Set<bool>(TEXT("ShowNumber"), false);
			Modify.Call();
		}
		FIntPoint Index;
		KeepersReflection::GetIntPoint(It->GetClass(), *It, TEXT("Index"), Index);
		if (Index != (*Found)->Index)
		{
			Moves.Emplace(*It, (*Found)->Index);
		}
	}
	// Moves onto free cells first, so that no unit lands where another still stands.
	for (int32 Pass = 0; Pass < 4 && Moves.Num() > 0; ++Pass)
	{
		for (int32 I = Moves.Num() - 1; I >= 0; --I)
		{
			if (UKeepersTerrainSubsystem::UnitOnTile(Grid, Moves[I].Value) && Pass < 3)
			{
				continue;
			}
			KeepersReflection::FCall Move(CombatSystem, TEXT("SetUnitIndexOnGrid"));
			Move.Set<UObject*>(TEXT("Unit"), Moves[I].Key);
			Move.Set<FIntPoint>(TEXT("Index"), Moves[I].Value);
			Move.Set<bool>(TEXT("Force"), true);
			Move.Set<bool>(TEXT("SnapLocation"), true);
			Move.Call();
			Moves.RemoveAt(I);
		}
	}
	for (const FIntPoint& Index : Snapshot.Rocks)
	{
		Terrain->RaiseRock(Grid, Index, 2);
	}
	Round = Snapshot.Round;
	if (AKeepersBattleObjective* Objective = AKeepersBattleObjective::Find(GetWorld()))
	{
		Objective->RestoreRound(Snapshot.Round);
	}
	bSaveDue = false;
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Battle resumed at round %d: %d units"), Snapshot.Round, Snapshot.Units.Num());
}

void UKeepersCampaignWorldSubsystem::OnBattleFinished(UObject* CombatSystem, int32 Winner)
{'''),
])
print('ok')
