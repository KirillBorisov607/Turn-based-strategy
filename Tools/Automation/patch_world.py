"""The world reacts: trees fall where they are hit, pushed units knock rocks rolling, rocks raised on a cliff edge fall
on the foe below, fire melts ice, lightning runs through the marsh, ice carries the pushed on, marsh holds them."""
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


patch('Public/KeepersBarks.h', [
('''	RaisesRock,
	GrowsForest
};''', '''	RaisesRock,
	GrowsForest,
	// Reactions (KeepersReactions): who hits whom, who falls, what the world does.
	FriendlyFire,
	HitAlly,
	Kill,
	AllyDown,
	LowHealth,
	Healed,
	BattleStart,
	Victory,
	TreeFalls,
	RockRolls,
	Rockfall,
	IceMelts,
	Slides,
	StuckInMarsh,
	Shocked
};'''),
])

patch('Public/KeepersTerrainSubsystem.h', [
('''	Throw,
	Rage,''', '''	Throw,
	Rage,
	/** Any other spell's blow: the world may react (a tree by the target falls). */
	Blow,'''),
('''	TSet<FIntPoint> RockTiles;''', '''	TSet<FIntPoint> RockTiles;
	/** A push that carries on over ice: where it came from, how deep the chain is. */
	FVector LastPushFrom = FVector::ZeroVector;
	int32 SlideDepth = 0;'''),
('''	/** Removes a rock or a tree raised in battle on a cell; the cell becomes plain ground. */
	bool ClearProp(UObject* Grid, const FIntPoint& Index);''', '''	/** Removes a rock or a tree raised in battle on a cell; the cell becomes plain ground. */
	bool ClearProp(UObject* Grid, const FIntPoint& Index);

	// The world reacts.
	/** A melee blow on a unit by a tree fells the tree onto the cell behind (3 damage to whoever is there). */
	void FellTree(UObject* Grid, const FIntPoint& Origin, const FIntPoint& Target);
	/** A unit pushed into a raised rock knocks it rolling one cell on; it crushes a unit there (4) and breaks. */
	bool RollRock(UObject* CombatSystem, UObject* Grid, const FIntPoint& Rock, const FVector& Direction);
	/** A rock raised on a cliff edge above a foe topples onto it (5 damage). */
	void TryRockfall(UObject* Grid, const FIntPoint& Rock, int32 CasterTeam);
	/** Ice in the area melts back into water; who stands on it drops in. */
	void MeltIce(UObject* Grid, const TArray<FIntPoint>& Area);
	/** A prop (rock or tree) raised in battle on a cell. */
	AActor* PropAt(UObject* Grid, const FIntPoint& Index, bool bTree) const;'''),
])

patch('Private/KeepersTerrainSubsystem.cpp', [
# Every spell animation is watched: melee blows by trees fell them.
('''	else if (const TPair<const TCHAR*, EKeepersTerrainEffect>* Hero = Algo::FindByPredicate(KeepersTerrain::HeroAnimations,
		[&ClassName](const TPair<const TCHAR*, EKeepersTerrainEffect>& Entry) { return ClassName.StartsWith(Entry.Key); }))
	{
		PendingEffects.Add(Actor, Hero->Value);
	}
	else
	{
		return;
	}''', '''	else if (const TPair<const TCHAR*, EKeepersTerrainEffect>* Hero = Algo::FindByPredicate(KeepersTerrain::HeroAnimations,
		[&ClassName](const TPair<const TCHAR*, EKeepersTerrainEffect>& Entry) { return ClassName.StartsWith(Entry.Key); }))
	{
		PendingEffects.Add(Actor, Hero->Value);
	}
	else if (ClassName.StartsWith(TEXT("BP_SpellAnim_")))
	{
		// Any other blow: the world may still react (a tree by the target).
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Blow);
	}
	else
	{
		return;
	}'''),
('''	case EKeepersTerrainEffect::Push:
		PushUnit(KeepersTerrain::FindCombatSystem(GetWorld()), Origin, Target);
		break;''', '''	case EKeepersTerrainEffect::Push:
		FellTree(Grid, Origin, Target);
		PushUnit(KeepersTerrain::FindCombatSystem(GetWorld()), Origin, Target);
		break;
	case EKeepersTerrainEffect::Blow:
		FellTree(Grid, Origin, Target);
		break;'''),
('''			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Origin)), EKeepersBarkSituation::RaisesRock, 0.8f);
		}
		break;''', '''			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Origin)), EKeepersBarkSituation::RaisesRock, 0.8f);
			const UObject* Caster = KeepersTerrain::GetUnitOnTile(Grid, Origin);
			TryRockfall(Grid, Target, Caster ? KeepersTerrain::TeamOf(Caster) : 0);
		}
		break;'''),
('''	case EKeepersTerrainEffect::Crush:
	case EKeepersTerrainEffect::Throw:
	case EKeepersTerrainEffect::Rage:
		ApplyHeroAbility(Effect, Grid, Origin, Target);
		break;''', '''	case EKeepersTerrainEffect::Crush:
	case EKeepersTerrainEffect::Throw:
	case EKeepersTerrainEffect::Rage:
		if (Effect == EKeepersTerrainEffect::Backstab)
		{
			FellTree(Grid, Origin, Target);
		}
		ApplyHeroAbility(Effect, Grid, Origin, Target);
		break;'''),
# Bumping into a rock knocks it rolling.
('''	if (DestinationType == KeepersTerrain::ObstacleTileType)
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s hits a rock"), *Unit->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, BumpDamage + (UKeepersCampaignSubsystem::HasArtifact(this, TEXT("AncestorStone")) ? 1 : 0));
		return true;
	}''', '''	if (DestinationType == KeepersTerrain::ObstacleTileType)
	{
		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s hits a rock"), *Unit->GetName());
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Bumped);
		DamageUnit(CombatSystem, Unit, BumpDamage + (UKeepersCampaignSubsystem::HasArtifact(this, TEXT("AncestorStone")) ? 1 : 0));
		// A rock raised in battle is loose: the blow sends it rolling.
		FVector UnitLocation;
		FVector RockLocation;
		if (const AActor* Pushed = Cast<AActor>(Unit))
		{
			if (KeepersTerrain::GetTileLocation(Grid, Destination.GetValue(), RockLocation))
			{
				UnitLocation = Pushed->GetActorLocation();
				RollRock(CombatSystem, Grid, Destination.GetValue(), (RockLocation - UnitLocation).GetSafeNormal2D());
			}
		}
		return true;
	}'''),
# Pushed onto ice: carried on; into marsh: stuck.
('''		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s to (%d,%d)"), *Unit->GetName(), Destination->X, Destination->Y);
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Pushed, 0.7f);
		if (FeatherBonus > 0)
		{
			DamageUnit(CombatSystem, Unit, FeatherBonus);
		}''', '''		UE_LOG(LogKeepersTerrain, Log, TEXT("Push: %s to (%d,%d)"), *Unit->GetName(), Destination->X, Destination->Y);
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Pushed, 0.7f);
		if (FeatherBonus > 0)
		{
			DamageUnit(CombatSystem, Unit, FeatherBonus);
		}
		const UKeepersSurfaceSubsystem* Surfaces = UKeepersSurfaceSubsystem::Get(this);
		const EKeepersSurface Surface = Surfaces ? Surfaces->GetSurface(Destination.GetValue()) : EKeepersSurface::None;
		if (Surface == EKeepersSurface::Ice && KeepersTerrain::TeamOf(Unit) >= 0 && SlideDepth < 3)
		{
			// On ice the push carries on.
			FVector Here;
			const AActor* Pushed = Cast<AActor>(Unit);
			if (Pushed && KeepersTerrain::GetTileLocation(Grid, Destination.GetValue(), Here))
			{
				const FVector Direction = (Here - LastPushFrom).GetSafeNormal2D();
				if (const TOptional<FIntPoint> Next = KeepersTerrain::NeighbourAlong(Grid, Destination.GetValue(), Direction))
				{
					UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::Slides, 0.8f);
					++SlideDepth;
					LastPushFrom = Here;
					ForcedMove(CombatSystem, Grid, Unit, Next, 0);
					--SlideDepth;
				}
			}
		}
		else if (Surface == EKeepersSurface::Marsh)
		{
			// The marsh holds whoever lands in it: no steps next turn.
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::StuckInMarsh, 0.8f);
			KeepersReflection::FCall SetMp(Unit, TEXT("SetCurrentMp"));
			SetMp.Set<int32>(TEXT("Mp"), 0);
			SetMp.Set<bool>(TEXT("ShowNumber"), false);
			SetMp.Call();
		}'''),
# Fire melts ice.
('''		for (const FIntPoint& Index : Area)
		{
			if (ForestTiles.Contains(Index))
			{
				ClearProp(Grid, Index);
			}
		}
		break;''', '''		for (const FIntPoint& Index : Area)
		{
			if (ForestTiles.Contains(Index))
			{
				ClearProp(Grid, Index);
			}
		}
		MeltIce(Grid, Area);
		break;'''),
# Lightning runs through the marsh.
('''			DamageUnit(CombatSystem, Foes[I].Value, 3);
		}''', '''			DamageUnit(CombatSystem, Foes[I].Value, 3);
		}
		// Water carries it: foes standing in the marsh near the strike take 2 more.
		if (const UKeepersSurfaceSubsystem* Surfaces = UKeepersSurfaceSubsystem::Get(this))
		{
			for (const FIntPoint& Index : KeepersTerrain::Adjacent(Grid, Target, 2.3f))
			{
				UObject* Wet = KeepersTerrain::GetUnitOnTile(Grid, Index);
				if (Wet && KeepersTerrain::TeamOf(Wet) != Team && Surfaces->GetSurface(Index) == EKeepersSurface::Marsh)
				{
					UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Wet), EKeepersBarkSituation::Shocked);
					DamageUnit(CombatSystem, Wet, 2);
				}
			}
		}'''),
('''bool UKeepersTerrainSubsystem::FreezeWater(UObject* Grid, const FIntPoint& Index)''', '''AActor* UKeepersTerrainSubsystem::PropAt(UObject* Grid, const FIntPoint& Index, bool bTree) const
{
	FVector Location;
	if (!KeepersTerrain::GetTileLocation(Grid, Index, Location))
	{
		return nullptr;
	}
	const float Reach = KeepersTerrain::GetTileSize(Grid).X * 0.45f;
	for (AActor* Prop : TerrainProps)
	{
		if (Prop && (bTree ? Prop->IsA<AKeepersPlantActor>() : Prop->IsA<AKeepersRockActor>()) && FVector::Dist2D(Prop->GetActorLocation(), Location) < Reach)
		{
			return Prop;
		}
	}
	return nullptr;
}

void UKeepersTerrainSubsystem::FellTree(UObject* Grid, const FIntPoint& Origin, const FIntPoint& Target)
{
	// Only blows from next to the target; the tree must stand on the target's cell.
	FVector OriginLocation;
	FVector TargetLocation;
	const float Step = KeepersTerrain::GetTileSize(Grid).X;
	if (!KeepersTerrain::GetTileLocation(Grid, Origin, OriginLocation) || !KeepersTerrain::GetTileLocation(Grid, Target, TargetLocation)
		|| FVector::Dist2D(OriginLocation, TargetLocation) > Step * 1.3f || !PropAt(Grid, Target, true))
	{
		return;
	}
	UObject* CombatSystem = KeepersTerrain::FindCombatSystem(GetWorld());
	const FVector Direction = (TargetLocation - OriginLocation).GetSafeNormal2D();
	UKeepersJuiceSubsystem::Burst(GetWorld(), TargetLocation + FVector(0.f, 0.f, 120.f), AKeepersBurst::EKind::Hit, FLinearColor(0.3f, 0.6f, 0.2f), 20, 1.3f);
	UKeepersJuiceSubsystem::Sound(this, TEXT("Leaves"));
	ClearProp(Grid, Target);
	UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Origin)), EKeepersBarkSituation::TreeFalls);
	// It falls away from the blow: whoever stands there is hit, the trunk becomes a thicket.
	if (const TOptional<FIntPoint> Behind = KeepersTerrain::NeighbourAlong(Grid, Target, Direction))
	{
		FVector BehindLocation;
		if (KeepersTerrain::GetTileLocation(Grid, Behind.GetValue(), BehindLocation))
		{
			UKeepersJuiceSubsystem::Burst(GetWorld(), BehindLocation + FVector(0.f, 0.f, 30.f), AKeepersBurst::EKind::Puff, FLinearColor(0.45f, 0.35f, 0.2f), 14, 1.1f);
		}
		if (UObject* Under = KeepersTerrain::GetUnitOnTile(Grid, Behind.GetValue()))
		{
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Under), EKeepersBarkSituation::Bumped);
			DamageUnit(CombatSystem, Under, 3);
		}
		GrowForest(Grid, Behind.GetValue());
	}
	UE_LOG(LogKeepersTerrain, Log, TEXT("A tree falls at (%d,%d)"), Target.X, Target.Y);
}

bool UKeepersTerrainSubsystem::RollRock(UObject* CombatSystem, UObject* Grid, const FIntPoint& Rock, const FVector& Direction)
{
	if (!PropAt(Grid, Rock, false))
	{
		return false;
	}
	const TOptional<FIntPoint> Next = KeepersTerrain::NeighbourAlong(Grid, Rock, Direction);
	uint8 Type = 0;
	if (!Next || !KeepersTerrain::GetTileType(Grid, Next.GetValue(), Type) || Type == KeepersTerrain::ObstacleTileType)
	{
		return false;
	}
	FVector From;
	FVector To;
	KeepersTerrain::GetTileLocation(Grid, Rock, From);
	KeepersTerrain::GetTileLocation(Grid, Next.GetValue(), To);
	ClearProp(Grid, Rock);
	UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersTerrain::GetUnitOnTile(Grid, Rock)), EKeepersBarkSituation::RockRolls);
	UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
	if (UObject* Under = KeepersTerrain::GetUnitOnTile(Grid, Next.GetValue()))
	{
		// It rolls over a unit and breaks.
		UKeepersJuiceSubsystem::Burst(GetWorld(), To + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit, FLinearColor(0.5f, 0.5f, 0.52f), 22, 1.3f);
		UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Under), EKeepersBarkSituation::Rockfall);
		DamageUnit(CombatSystem, Under, 4);
	}
	else if (Type == KeepersTerrain::ChasmTileType)
	{
		// Into the water or the pit: gone with a splash.
		UKeepersJuiceSubsystem::Burst(GetWorld(), To, AKeepersBurst::EKind::Hit, FLinearColor(0.35f, 0.65f, 1.f), 20, 1.1f);
		UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"));
	}
	else
	{
		RaiseRock(Grid, Next.GetValue(), 1);
	}
	UE_LOG(LogKeepersTerrain, Log, TEXT("A rock rolls from (%d,%d) to (%d,%d)"), Rock.X, Rock.Y, Next->X, Next->Y);
	return true;
}

void UKeepersTerrainSubsystem::TryRockfall(UObject* Grid, const FIntPoint& Rock, int32 CasterTeam)
{
	FVector Top;
	if (!KeepersTerrain::GetTileLocation(Grid, Rock, Top))
	{
		return;
	}
	const float Step = KeepersTerrain::GetTileSize(Grid).Z;
	UObject* CombatSystem = KeepersTerrain::FindCombatSystem(GetWorld());
	for (const FIntPoint& Below : KeepersTerrain::Adjacent(Grid, Rock))
	{
		FVector Location;
		UObject* Foe = KeepersTerrain::GetUnitOnTile(Grid, Below);
		if (Foe && KeepersTerrain::TeamOf(Foe) != CasterTeam && KeepersTerrain::GetTileLocation(Grid, Below, Location) && Top.Z - Location.Z >= Step * 1.5f)
		{
			// The rock was raised on the edge: it topples onto the foe below.
			ClearProp(Grid, Rock);
			UKeepersJuiceSubsystem::Burst(GetWorld(), Location + FVector(0.f, 0.f, 80.f), AKeepersBurst::EKind::Hit, FLinearColor(0.55f, 0.5f, 0.45f), 26, 1.4f);
			UKeepersJuiceSubsystem::Burst(GetWorld(), Location, AKeepersBurst::EKind::Puff, FLinearColor(0.62f, 0.55f, 0.45f), 18, 1.2f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
			UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Foe), EKeepersBarkSituation::Rockfall);
			DamageUnit(CombatSystem, Foe, 5);
			UE_LOG(LogKeepersTerrain, Log, TEXT("Rockfall from (%d,%d) onto (%d,%d)"), Rock.X, Rock.Y, Below.X, Below.Y);
			return;
		}
	}
}

void UKeepersTerrainSubsystem::MeltIce(UObject* Grid, const TArray<FIntPoint>& Area)
{
	UObject* CombatSystem = KeepersTerrain::FindCombatSystem(GetWorld());
	for (const FIntPoint& Index : Area)
	{
		if (!IceTiles.Contains(Index))
		{
			continue;
		}
		// The plate goes, the water comes back.
		FVector Location;
		KeepersTerrain::GetTileLocation(Grid, Index, Location);
		for (int32 I = TerrainProps.Num() - 1; I >= 0; --I)
		{
			AActor* Prop = TerrainProps[I];
			if (Prop && Prop->IsA<AStaticMeshActor>() && FVector::Dist2D(Prop->GetActorLocation(), Location) < 30.f)
			{
				Prop->Destroy();
				TerrainProps.RemoveAt(I);
			}
		}
		FTransform Unused;
		SetTileType(Grid, Index, KeepersTerrain::ChasmTileType, Unused);
		IceTiles.Remove(Index);
		UKeepersJuiceSubsystem::Burst(GetWorld(), Location + FVector(0.f, 0.f, 20.f), AKeepersBurst::EKind::Puff, FLinearColor(0.85f, 0.92f, 1.f), 14, 1.f);
		UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"), 0.7f);
		if (UObject* Unit = KeepersTerrain::GetUnitOnTile(Grid, Index))
		{
			if (!KeepersTerrain::CanStandOn(Unit, KeepersTerrain::ChasmTileType))
			{
				UKeepersBarkLibrary::BarkAbout(Cast<AActor>(Unit), EKeepersBarkSituation::IceMelts);
				DamageUnit(CombatSystem, Unit, 9999);
			}
		}
		UE_LOG(LogKeepersTerrain, Log, TEXT("Ice melts at (%d,%d)"), Index.X, Index.Y);
	}
}

bool UKeepersTerrainSubsystem::FreezeWater(UObject* Grid, const FIntPoint& Index)'''),
# Where a push starts (for sliding on along it).
('''	const int32 BumpDamage = KeepersTerrain::BumpDamage + FeatherBonus;''', '''	const int32 BumpDamage = KeepersTerrain::BumpDamage + FeatherBonus;
	if (SlideDepth == 0)
	{
		if (const AActor* Start = Cast<AActor>(Unit))
		{
			LastPushFrom = Start->GetActorLocation();
		}
	}'''),
])
print('ok')
