"""Six regions in a row: region bands, field rules (forest, barricades, marsh, ice, shifting land), region bosses,
the next region after a boss, region names on the map and the loading screens."""
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
('''	/** The battle in progress, saved at the start of its last round. */
	UPROPERTY(SaveGame)
	FKeepersBattleSnapshot Battle;
};''', '''	/** The battle in progress, saved at the start of its last round. */
	UPROPERTY(SaveGame)
	FKeepersBattleSnapshot Battle;

	/** Region of the run (KeepersRegions), from 0. */
	UPROPERTY(SaveGame)
	int32 Region = 0;
};'''),
('''	/** The battle in progress: saved every round, resumed by ContinueCampaign. */''', '''	/** The region of the run and its rules. */
	int32 GetRegion() const { return Save ? Save->Region : 0; }
	const struct FKeepersRegion& GetRegionDef() const;
	bool IsFinalRegion() const;
	/** Test hook: the run jumps to a region with a new map. */
	void TestSetRegion(int32 Region);
	/** The map shows the region's introduction once after a new region begins. */
	bool bRegionIntroPending = false;

	/** The battle in progress: saved every round, resumed by ContinueCampaign. */'''),
])

patch('Private/KeepersCampaign.cpp', [
('''#include "KeepersHeroes.h"''', '''#include "KeepersHeroes.h"
#include "KeepersRegions.h"'''),
('''	if (IsBossNode(Save->CurrentNode))
	{
		UE_LOG(LogKeepersCampaign, Log, TEXT("Campaign won"));''', '''	if (IsBossNode(Save->CurrentNode) && !IsFinalRegion())
	{
		// The next region: a new map; gold, relics, levels and the squad stay.
		const FKeepersRegion& Cleared = GetRegionDef();
		if (!Cleared.Unlocks.IsNone())
		{
			if (UKeepersPartySubsystem* Party = GetGameInstance()->GetSubsystem<UKeepersPartySubsystem>())
			{
				Party->UnlockHero(Cleared.Unlocks);
			}
		}
		++Save->Region;
		GenerateMap(FMath::Rand());
		Save->VisitedNodes.Reset();
		Save->CurrentNode = INDEX_NONE;
		bRegionIntroPending = true;
		UE_LOG(LogKeepersCampaign, Log, TEXT("Region %d: %s"), Save->Region, *GetRegionDef().Name.ToString());
		WriteSave();
		OpenLevel(MapLevel);
		return;
	}
	if (IsBossNode(Save->CurrentNode))
	{
		UE_LOG(LogKeepersCampaign, Log, TEXT("Campaign won"));'''),
('''void UKeepersCampaignSubsystem::MarkBattleWon()''', '''const FKeepersRegion& UKeepersCampaignSubsystem::GetRegionDef() const
{
	return KeepersRegions::Get(GetRegion());
}

bool UKeepersCampaignSubsystem::IsFinalRegion() const
{
	return GetRegion() >= KeepersRegions::All().Num() - 1;
}

void UKeepersCampaignSubsystem::TestSetRegion(int32 Region)
{
	if (!Save)
	{
		return;
	}
	Save->Region = FMath::Clamp(Region, 0, KeepersRegions::All().Num() - 1);
	GenerateMap(FMath::Rand());
	Save->VisitedNodes.Reset();
	Save->CurrentNode = INDEX_NONE;
	bRegionIntroPending = true;
	WriteSave();
}

void UKeepersCampaignSubsystem::MarkBattleWon()'''),
])

# The objective names the region's boss.
patch('Public/KeepersBattleObjective.h', [
('''	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bTotem = false;''', '''	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bTotem = false;

	/** The leader is the region's boss, called so in the goal ("Король слизней"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText BossName;'''),
])
patch('Private/KeepersBattleObjective.cpp', [
('''	LeaderName = TEXT("тотем кур");''', '''	LeaderName = BossName.IsEmpty() ? FString(TEXT("тотем кур")) : BossName.ToString();'''),
('''	if (bDefeatLeader && bTotem)
	{
		Goals.Add(TEXT("разрушьте тотем кур — он лечит врагов рядом"));
	}''', '''	if (bDefeatLeader && !BossName.IsEmpty())
	{
		Goals.Add(bTotem ? FString::Printf(TEXT("свалите босса — %s, он лечит врагов рядом"), *BossName.ToString())
			: FString::Printf(TEXT("победите босса — %s"), *BossName.ToString()));
	}
	else if (bDefeatLeader && bTotem)
	{
		Goals.Add(TEXT("разрушьте тотем кур — он лечит врагов рядом"));
	}'''),
])

# Surfaces follow the region.
patch('Private/KeepersSurfaces.cpp', [
('''	const EKeepersSurface Patch = Requested;
	Requested = EKeepersSurface::None;''', '''	EKeepersSurface Patch = Requested;
	Requested = EKeepersSurface::None;
	if (const UKeepersCampaignSubsystem* Region = UKeepersCampaignSubsystem::Get(this))
	{
		if (Patch == EKeepersSurface::None && Region->IsActive() && Region->IsInBattleNode())
		{
			Patch = Region->GetRegionDef().Surface;
		}
	}'''),
('''#include "KeepersReflection.h"''', '''#include "KeepersReflection.h"
#include "KeepersRegions.h"'''),
])

patch('Public/KeepersCampaignUI.h', [
('''	void ResumeBattle(UObject* CombatSystem);''', '''	bool ResumeBattle(UObject* CombatSystem);
	/** The region's rule at the start of a battle: forest, barricades. */
	void ApplyRegionField(UObject* CombatSystem);
	/** Each round: the shifting land of the Heart, the forest of the forest boss. */
	void ApplyRegionRound(UObject* CombatSystem);
	/** Empty plain cells of the field, at least MinSteps cells from every unit. */
	TArray<FIntPoint> FreeCells(UObject* Grid, float MinDistance) const;'''),
])
patch('Private/KeepersCampaignUI.cpp', [
('''#include "KeepersHeroes.h"''', '''#include "KeepersHeroes.h"
#include "KeepersRegions.h"'''),
# Bands by region.
('''		// Riders are the elite, siege machines come with the boss, shamans join later battles.
		const FKeepersMapNode& Node = Campaign->GetNodes()[Campaign->GetCurrentNode()];
		if (Node.Type == EKeepersNodeType::Elite)
		{
			Band = { Cavalry, Bandit, Archer };
		}
		else if (Node.Type == EKeepersNodeType::Boss)
		{
			Band = { Brute, Catapult, Ram, Archer };
		}
		else if (Node.Column >= 3)
		{
			Band = { Bandit, Shaman, Archer };
		}
		else if (Node.Column >= 2)
		{
			Band = { Bandit, Archer };
		}''', '''		// Every region has its own band; the first region eases in with the level's own enemies.
		const FKeepersMapNode& Node = Campaign->GetNodes()[Campaign->GetCurrentNode()];
		const FKeepersRegion& Region = Campaign->GetRegionDef();
		if (Node.Type == EKeepersNodeType::Elite)
		{
			Band = Region.EliteBand;
		}
		else if (Node.Type == EKeepersNodeType::Boss)
		{
			Band = Region.BossBand;
		}
		else if (Campaign->GetRegion() > 0 || Node.Column >= 2)
		{
			Band = Region.Band;
		}'''),
('''	constexpr uint8 Brute = 8;
	constexpr uint8 Cavalry = 12;
	constexpr uint8 Catapult = 13;
	constexpr uint8 Ram = 14;
	constexpr uint8 Shaman = 15;''', '''	constexpr uint8 Cavalry = 12;
	constexpr uint8 Shaman = 15;'''),
# The boss of the region.
('''	bObjectiveChecked = true;
	for (TActorIterator<AKeepersBattleObjective> It(GetWorld()); It; ++It)
	{
		return; // the level has its own goal
	}

	// Campaign battles get a goal picked by their node; others defeat every enemy.
	int32 Variant = 0;
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());''', '''	bObjectiveChecked = true;
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const bool bBoss = Campaign && Campaign->IsActive() && Campaign->IsInBattleNode() && Campaign->IsBossNode(Campaign->GetCurrentNode());
	for (TActorIterator<AKeepersBattleObjective> It(GetWorld()); It; ++It)
	{
		if (!bBoss)
		{
			return; // the level has its own goal
		}
		It->Destroy(); // the region's boss is the goal
	}
	if (bBoss)
	{
		SpawnBoss(CombatSystem);
		return;
	}

	// Campaign battles get a goal picked by their node; others defeat every enemy.
	int32 Variant = 0;'''),
('''void UKeepersCampaignWorldSubsystem::ShowMap()''', '''void UKeepersCampaignWorldSubsystem::SpawnBoss(UObject* CombatSystem)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const FKeepersRegion& Region = Campaign->GetRegionDef();
	AKeepersBattleObjective* Objective = GetWorld()->SpawnActorDeferred<AKeepersBattleObjective>(AKeepersBattleObjective::StaticClass(), FTransform::Identity);
	if (!Objective)
	{
		return;
	}
	Objective->bDefeatLeader = true;
	Objective->BossName = Region.BossName;
	Objective->bTotem = Region.bBossTotem;
	if (!Region.bBossTotem)
	{
		// The boss steps out next to the enemy furthest from the heroes: bigger and tougher than its kind.
		TArray<UObject*> Heroes = KeepersUI::Heroes(CombatSystem);
		UKeepersCreatureSubsystem* Creatures = GetWorld()->GetSubsystem<UKeepersCreatureSubsystem>();
		UObject* Far = nullptr;
		float FarDistance = -1.f;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			int32 Team = 0;
			if (It->GetClass()->GetName() != TEXT("BP_Unit_C") || !KeepersReflection::GetInt(It->GetClass(), *It, TEXT("TeamIndex"), Team) || Team == 0)
			{
				continue;
			}
			float Nearest = MAX_flt;
			for (UObject* Hero : Heroes)
			{
				if (const AActor* HeroActor = Cast<AActor>(Hero))
				{
					Nearest = FMath::Min(Nearest, FVector::Dist2D(HeroActor->GetActorLocation(), It->GetActorLocation()));
				}
			}
			if (Nearest > FarDistance)
			{
				FarDistance = Nearest;
				Far = *It;
			}
		}
		const AActor* HeroActor = Heroes.Num() > 0 ? Cast<AActor>(Heroes[0]) : nullptr;
		if (Far && HeroActor && Creatures)
		{
			FIntPoint Index;
			int32 Team = 1;
			KeepersReflection::GetIntPoint(Far->GetClass(), Far, TEXT("Index"), Index);
			KeepersReflection::GetInt(Far->GetClass(), Far, TEXT("TeamIndex"), Team);
			if (AActor* Boss = Creatures->SpawnUnitNear(CombatSystem, HeroActor->GetClass(), Region.BossType, Index, Team, 0, HeroActor->GetActorScale3D().X * Region.BossScale, TEXT("Boss")))
			{
				KeepersReflection::AddUnitStat(Boss, TEXT("HP_Max"), Region.BossHp);
				KeepersReflection::AddUnitStat(Boss, TEXT("HP_Current"), Region.BossHp);
				Objective->Leader = Boss;
				UKeepersBarkLibrary::Bark(Boss, FText::FromString(FString::Printf(TEXT("%s не отдаст камень!"), *Region.BossName.ToString())));
			}
		}
	}
	Objective->FinishSpawning(FTransform::Identity);
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Boss of region %d: %s"), Campaign->GetRegion(), *Region.BossName.ToString());
}

TArray<FIntPoint> UKeepersCampaignWorldSubsystem::FreeCells(UObject* Grid, float MinDistance) const
{
	TArray<FIntPoint> Cells;
	FKeepersLevelGrid Level;
	if (!Grid || !KeepersLevelKit::PrepareGrid(GetWorld(), Level))
	{
		return Cells;
	}
	TArray<FVector> Units;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("BP_Unit_C"))
		{
			Units.Add(It->GetActorLocation());
		}
	}
	for (const FIntPoint& Index : KeepersLevelKit::AllIndexes(Level))
	{
		uint8 Type = 0;
		FVector Location;
		if (UKeepersTerrainSubsystem::TileType(Grid, Index, Type) && Type == 1 && !UKeepersTerrainSubsystem::UnitOnTile(Grid, Index)
			&& UKeepersTerrainSubsystem::TileLocation(Grid, Index, Location)
			&& !Units.ContainsByPredicate([&](const FVector& Unit) { return FVector::Dist2D(Unit, Location) < MinDistance; }))
		{
			Cells.Add(Index);
		}
	}
	return Cells;
}

void UKeepersCampaignWorldSubsystem::ApplyRegionField(UObject* CombatSystem)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>();
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	if (!Campaign || !IsNodeLevel() || !Terrain || !Grid)
	{
		return;
	}
	const FKeepersRegion& Region = Campaign->GetRegionDef();
	FRandomStream Random(Campaign->GetSeed() + Campaign->GetCurrentNode() * 131);
	TArray<FIntPoint> Cells = FreeCells(Grid, 150.f);
	auto Take = [&]() { return Cells.Num() > 0 ? Cells[Random.RandRange(0, Cells.Num() - 1)] : FIntPoint(-1, -1); };
	for (int32 I = 0; I < Region.Forests && Cells.Num() > 0; ++I)
	{
		const FIntPoint Cell = Take();
		Cells.Remove(Cell);
		Terrain->GrowForest(Grid, Cell);
	}
	// Barricades keep a cell away from everyone, so nobody is walled in from the start.
	TArray<FIntPoint> Open = FreeCells(Grid, 330.f);
	for (int32 I = 0; I < Region.Barricades && Open.Num() > 0; ++I)
	{
		const FIntPoint Cell = Open[Random.RandRange(0, Open.Num() - 1)];
		Open.Remove(Cell);
		Terrain->RaiseRock(Grid, Cell, 1);
	}
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Region field: %d forest, %d barricades"), Region.Forests, Region.Barricades);
}

void UKeepersCampaignWorldSubsystem::ApplyRegionRound(UObject* CombatSystem)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	UKeepersTerrainSubsystem* Terrain = GetWorld()->GetSubsystem<UKeepersTerrainSubsystem>();
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	if (!Campaign || !IsNodeLevel() || !Terrain || !Grid || Round < 2)
	{
		return;
	}
	const FKeepersRegion& Region = Campaign->GetRegionDef();
	// The Heart: without the stones the land will not keep still.
	if (Region.bShifting && Round % 2 == 0)
	{
		TArray<FIntPoint> Cells = FreeCells(Grid, 150.f);
		if (Cells.Num() >= 2)
		{
			const FIntPoint Rock = Cells[FMath::RandRange(0, Cells.Num() - 1)];
			Cells.Remove(Rock);
			Terrain->RaiseRock(Grid, Rock, 1);
			Terrain->GrowForest(Grid, Cells[FMath::RandRange(0, Cells.Num() - 1)]);
		}
		const TArray<FIntPoint> Rocks = Terrain->GetRocks();
		if (Rocks.Num() > 2)
		{
			Terrain->CrumbleRock(Grid, Rocks[FMath::RandRange(0, Rocks.Num() - 1)]);
		}
		UE_LOG(LogKeepersCampaignUI, Log, TEXT("The land shifts"));
	}
	// The forest boss: trees close in on a hero.
	if (Region.bBossGrowsForest && Campaign->IsBossNode(Campaign->GetCurrentNode()))
	{
		TArray<UObject*> Heroes = KeepersUI::Heroes(CombatSystem);
		if (Heroes.Num() > 0)
		{
			UObject* Hero = Heroes[FMath::RandRange(0, Heroes.Num() - 1)];
			FIntPoint Index;
			KeepersReflection::GetIntPoint(Hero->GetClass(), Hero, TEXT("Index"), Index);
			FVector Center;
			if (UKeepersTerrainSubsystem::TileLocation(Grid, Index, Center))
			{
				for (const FIntPoint& Cell : FreeCells(Grid, 1.f))
				{
					FVector Location;
					if (UKeepersTerrainSubsystem::TileLocation(Grid, Cell, Location) && FVector::Dist2D(Location, Center) < 320.f)
					{
						Terrain->GrowForest(Grid, Cell);
						break;
					}
				}
			}
		}
	}
}

void UKeepersCampaignWorldSubsystem::ShowMap()'''),
('''			if (!bResumeChecked)
			{
				bResumeChecked = true;
				ResumeBattle(CombatSystem);
			}
			if (bSaveDue)
			{
				bSaveDue = false;
				SaveBattle(CombatSystem);
			}''', '''			if (!bResumeChecked)
			{
				bResumeChecked = true;
				if (!ResumeBattle(CombatSystem))
				{
					ApplyRegionField(CombatSystem);
				}
			}
			if (bSaveDue)
			{
				bSaveDue = false;
				ApplyRegionRound(CombatSystem);
				SaveBattle(CombatSystem);
			}'''),
('''void UKeepersCampaignWorldSubsystem::ResumeBattle(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const FKeepersBattleSnapshot* Saved = Campaign ? Campaign->GetBattleToResume() : nullptr;
	if (!Saved || !IsNodeLevel())
	{
		return;
	}''', '''bool UKeepersCampaignWorldSubsystem::ResumeBattle(UObject* CombatSystem)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(GetWorld());
	const FKeepersBattleSnapshot* Saved = Campaign ? Campaign->GetBattleToResume() : nullptr;
	if (!Saved || !IsNodeLevel())
	{
		return false;
	}'''),
('''	if (!Grid || !Terrain)
	{
		return;
	}
	// The field first: rocks, forest and ice where they were.''', '''	if (!Grid || !Terrain)
	{
		return false;
	}
	// The field first: rocks, forest and ice where they were.'''),
('''	bSaveDue = false;
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Battle resumed at round %d: %d units"), Snapshot.Round, Snapshot.Units.Num());
}''', '''	bSaveDue = false;
	UE_LOG(LogKeepersCampaignUI, Log, TEXT("Battle resumed at round %d: %d units"), Snapshot.Round, Snapshot.Units.Num());
	return true;
}'''),
# The boss falls: the region is free.
('''		UKeepersChoiceWidget::FOption Done;
		Done.Title = FText::FromString(TEXT("В главное меню"));
		Done.Color = FLinearColor(0.3f, 0.25f, 0.1f);
		Done.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->CompleteCurrentNode(); } };
		Options.Add(MoveTemp(Done));
		ShowChoice(FText::FromString(TEXT("Земли освобождены!")),
			FText::FromString(TEXT("Зелёные долы снова зеленеют. Даже куры притихли — ненадолго.")), MoveTemp(Options));
		return;''', '''		const FKeepersRegion& Region = Campaign->GetRegionDef();
		const bool bFinal = Campaign->IsFinalRegion();
		UKeepersChoiceWidget::FOption Done;
		Done.Title = FText::FromString(bFinal ? FString(TEXT("В главное меню"))
			: FString::Printf(TEXT("В путь: %s"), *KeepersRegions::Get(Campaign->GetRegion() + 1).Name.ToString()));
		Done.Description = bFinal ? FText::GetEmpty() : KeepersRegions::Get(Campaign->GetRegion() + 1).Intro;
		Done.Color = FLinearColor(0.3f, 0.25f, 0.1f);
		Done.Image = KeepersCards::Card(TEXT("Leave"));
		Done.OnPicked = [WeakCampaign]() { if (WeakCampaign.IsValid()) { WeakCampaign->CompleteCurrentNode(); } };
		Options.Add(MoveTemp(Done));
		ShowChoice(FText::FromString(FString::Printf(TEXT("%s освобождён!"), *Region.Name.ToString())), Region.Liberated, MoveTemp(Options));
		return;'''),
# Map title.
('''		TitleRow->AddChildToHorizontalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Зелёные долы")), KeepersStyle::Heading(38), KeepersStyle::Gold))
			->SetVerticalAlignment(VAlign_Center);
		TitleRow->AddChildToHorizontalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(TEXT("Выберите следующую точку пути. Открыты только соседние с пройденными.")), KeepersStyle::Body(14), KeepersStyle::TextDim))
			->SetPadding(FMargin(24.f, 6.f, 0.f, 0.f));''', '''		const FKeepersRegion& Region = Campaign->GetRegionDef();
		UVerticalBox* Titles = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Titles->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, Region.Numeral, KeepersStyle::Heading(14), KeepersStyle::TextDim));
		Titles->AddChildToVerticalBox(KeepersStyle::MakeText(WidgetTree, Region.Name, KeepersStyle::Heading(38), KeepersStyle::Gold));
		TitleRow->AddChildToHorizontalBox(Titles)->SetVerticalAlignment(VAlign_Center);
		TitleRow->AddChildToHorizontalBox(KeepersStyle::MakeText(WidgetTree, FText::FromString(Region.Intro.ToString() + TEXT("\\nВыберите следующую точку пути. Открыты только соседние с пройденными.")),
			KeepersStyle::Body(14), KeepersStyle::TextDim))->SetPadding(FMargin(24.f, 6.f, 0.f, 0.f));'''),
])

patch('Public/KeepersCampaignUI.h', [
('''	/** Every battle shows a goal: a campaign battle gets one picked by its node, others "defeat all". */
	void EnsureObjective();''', '''	/** Every battle shows a goal: a campaign battle gets one picked by its node, others "defeat all". */
	void EnsureObjective();
	/** The boss node: the region's boss joins the enemies and must fall. */
	void SpawnBoss(UObject* CombatSystem);'''),
])

# A rock that crumbles: the shifting land takes it back.
patch('Public/KeepersTerrainSubsystem.h', [
('''	/** Water on the tile becomes walkable ice with a plate; false if it is not water. */''', '''	/** A raised rock crumbles: the cell is plain ground again. */
	bool CrumbleRock(UObject* Grid, const FIntPoint& Index) { return ClearProp(Grid, Index); }

	/** Water on the tile becomes walkable ice with a plate; false if it is not water. */'''),
])

# Loading screens name the region.
patch('Private/KeepersLoadingScreen.cpp', [
('''	const TCHAR* Tip = KeepersLoading::Tips[TipIndex % UE_ARRAY_COUNT(KeepersLoading::Tips)];''', '''	// In the campaign the places belong to the current region.
	static FString RegionKicker;
	static FString RegionTitle;
	static FString RegionSubtitle;
	const UKeepersCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<UKeepersCampaignSubsystem>();
	if (Campaign && Campaign->IsActive())
	{
		const FKeepersRegion& Region = Campaign->GetRegionDef();
		if (FCString::Strcmp(Place.Level, TEXT("L_CampaignMap")) == 0)
		{
			RegionKicker = Region.Numeral.ToString();
			RegionTitle = Region.Name.ToString();
			RegionSubtitle = Region.Intro.ToString();
			Place = { Place.Level, *RegionKicker, *RegionTitle, *RegionSubtitle };
		}
		else
		{
			RegionKicker = FString(Place.Kicker).Replace(TEXT("Зелёные долы"), *Region.Name.ToString());
			Place.Kicker = *RegionKicker;
		}
	}
	const TCHAR* Tip = KeepersLoading::Tips[TipIndex % UE_ARRAY_COUNT(KeepersLoading::Tips)];'''),
])

patch('Private/KeepersDirector.cpp', [
('''	else if (Step.Action == TEXT("continue") && Campaign)''', '''	else if (Step.Action == TEXT("region") && Campaign && Args.Num() >= 1)
	{
		Campaign->TestSetRegion(FCString::Atoi(*Args[0]));
	}
	else if (Step.Action == TEXT("continue") && Campaign)'''),
])
print('ok')
