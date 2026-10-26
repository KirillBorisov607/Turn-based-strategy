"""Abilities of the Mage, the Rogue and the Barbarian: effects in KeepersTerrainSubsystem (by spell animation)."""
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


patch('Public/KeepersTerrainSubsystem.h', [
('''	Push,
	Sprout,
	Roots
};''', '''	Push,
	Sprout,
	Roots,
	// The Mage.
	Fireball,
	IcePath,
	ChainLightning,
	// The Rogue.
	Dash,
	Backstab,
	SmokeBomb,
	// The Barbarian.
	Crush,
	Throw,
	Rage
};'''),
(''' *  - BP_SpellAnim_Roots: the target's MP (already cut by the spell) is set to exactly 0 for its next turn.
 */''', ''' *  - BP_SpellAnim_Roots: the target's MP (already cut by the spell) is set to exactly 0 for its next turn.
 *  - BP_SpellAnim_Fireball: forest on the target and around it burns down (plain ground again).
 *  - BP_SpellAnim_IcePath: water on the target and the next cell along the cast freezes into walkable ice.
 *  - BP_SpellAnim_ChainLightning: the bolt jumps to up to two more foes within two cells (3 damage each).
 *  - BP_SpellAnim_Dash: the Rogue lands on the empty target cell; foes next to the landing take 2.
 *  - BP_SpellAnim_Backstab: a foe flanked by another of the Rogue's allies takes the hit twice.
 *  - BP_SpellAnim_SmokeBomb: smoke on the target and around it blocks sight for two rounds.
 *  - BP_SpellAnim_Crush: rocks and trees raised in battle on the target and around it are smashed.
 *  - BP_SpellAnim_Throw: the target flies over the Barbarian to the cell behind it (water drowns it,
 *    a rock or a unit hurts both).
 *  - BP_SpellAnim_Rage: the Barbarian (who paid health) gets its action points back and +1 movement.
 */'''),
('''	/** Damages a unit and lets the combat system process its death. */
	static void DamageUnit(UObject* CombatSystem, UObject* Unit, int32 Amount);''', '''	/** Damages a unit and lets the combat system process its death. */
	static void DamageUnit(UObject* CombatSystem, UObject* Unit, int32 Amount);

	/** Moves a unit by force to Destination (none: the edge): bumps, falls into water, or lands. */
	bool ForcedMove(UObject* CombatSystem, UObject* Grid, UObject* Unit, const TOptional<FIntPoint>& Destination, int32 ExtraDamage);

	/** Effects of the Mage's, the Rogue's and the Barbarian's abilities. */
	void ApplyHeroAbility(EKeepersTerrainEffect Effect, UObject* Grid, const FIntPoint& Origin, const FIntPoint& Target);
	/** Removes a rock or a tree raised in battle on a cell; the cell becomes plain ground. */
	bool ClearProp(UObject* Grid, const FIntPoint& Index);'''),
])

patch('Private/KeepersTerrainSubsystem.cpp', [
('''	static const FString RootsAnimation = TEXT("BP_SpellAnim_Roots");''', '''	static const FString RootsAnimation = TEXT("BP_SpellAnim_Roots");
	/** Animations of the heroes' newer abilities and their effects. */
	static const TPair<const TCHAR*, EKeepersTerrainEffect> HeroAnimations[] =
	{
		{ TEXT("BP_SpellAnim_Fireball"), EKeepersTerrainEffect::Fireball },
		{ TEXT("BP_SpellAnim_IcePath"), EKeepersTerrainEffect::IcePath },
		{ TEXT("BP_SpellAnim_ChainLightning"), EKeepersTerrainEffect::ChainLightning },
		{ TEXT("BP_SpellAnim_Dash"), EKeepersTerrainEffect::Dash },
		{ TEXT("BP_SpellAnim_Backstab"), EKeepersTerrainEffect::Backstab },
		{ TEXT("BP_SpellAnim_SmokeBomb"), EKeepersTerrainEffect::SmokeBomb },
		{ TEXT("BP_SpellAnim_Crush"), EKeepersTerrainEffect::Crush },
		{ TEXT("BP_SpellAnim_Throw"), EKeepersTerrainEffect::Throw },
		{ TEXT("BP_SpellAnim_Rage"), EKeepersTerrainEffect::Rage },
	};'''),
('''	static FVector GetTileSize(UObject* Grid)''', '''	static int32 TeamOf(const UObject* Unit)
	{
		int32 Team = 0;
		if (Unit)
		{
			KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		}
		return Team;
	}

	static FVector GetTileSize(UObject* Grid);

	/** Cells next to Index (any grid shape): within 1.2 cell sizes. */
	static TArray<FIntPoint> Adjacent(UObject* Grid, const FIntPoint& Index, float Range = 1.2f)
	{
		TArray<FIntPoint> Result;
		FVector Center;
		if (!GetTileLocation(Grid, Index, Center))
		{
			return Result;
		}
		const float Size = GetTileSize(Grid).X;
		const int32 Reach = FMath::CeilToInt(Range * 2.f);
		for (int32 DX = -Reach; DX <= Reach; ++DX)
		{
			for (int32 DY = -Reach; DY <= Reach; ++DY)
			{
				const FIntPoint Candidate(Index.X + DX, Index.Y + DY);
				FVector Location;
				if (Candidate != Index && GetTileLocation(Grid, Candidate, Location) && (Location - Center).Size2D() <= Size * Range)
				{
					Result.Add(Candidate);
				}
			}
		}
		return Result;
	}

	/** The neighbour of Index that lies furthest along Direction (alignment above 0.5), if any. */
	static TOptional<FIntPoint> NeighbourAlong(UObject* Grid, const FIntPoint& Index, const FVector& Direction)
	{
		FVector Center;
		TOptional<FIntPoint> Best;
		if (!GetTileLocation(Grid, Index, Center))
		{
			return Best;
		}
		float BestAlignment = 0.5f;
		for (const FIntPoint& Candidate : Adjacent(Grid, Index))
		{
			FVector Location;
			GetTileLocation(Grid, Candidate, Location);
			const float Alignment = FVector::DotProduct((Location - Center).GetSafeNormal2D(), Direction);
			if (Alignment > BestAlignment)
			{
				BestAlignment = Alignment;
				Best = Candidate;
			}
		}
		return Best;
	}

	static FVector GetTileSize(UObject* Grid)'''),
('''	else if (ClassName.StartsWith(KeepersTerrain::RootsAnimation))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Roots);
	}
	else
	{
		return;
	}''', '''	else if (ClassName.StartsWith(KeepersTerrain::RootsAnimation))
	{
		PendingEffects.Add(Actor, EKeepersTerrainEffect::Roots);
	}
	else if (const TPair<const TCHAR*, EKeepersTerrainEffect>* Hero = Algo::FindByPredicate(KeepersTerrain::HeroAnimations,
		[&ClassName](const TPair<const TCHAR*, EKeepersTerrainEffect>& Entry) { return ClassName.StartsWith(Entry.Key); }))
	{
		PendingEffects.Add(Actor, Hero->Value);
	}
	else
	{
		return;
	}'''),
('''	case EKeepersTerrainEffect::Sprout:
		if (GrowForest(Grid, Target))''', '''	case EKeepersTerrainEffect::Fireball:
	case EKeepersTerrainEffect::IcePath:
	case EKeepersTerrainEffect::ChainLightning:
	case EKeepersTerrainEffect::Dash:
	case EKeepersTerrainEffect::Backstab:
	case EKeepersTerrainEffect::SmokeBomb:
	case EKeepersTerrainEffect::Crush:
	case EKeepersTerrainEffect::Throw:
	case EKeepersTerrainEffect::Rage:
		ApplyHeroAbility(Effect, Grid, Origin, Target);
		break;
	case EKeepersTerrainEffect::Sprout:
		if (GrowForest(Grid, Target))'''),
# PushUnit: the neighbour search and the move go to the helpers.
('''	const FVector PushDirection = (TargetLocation - OriginLocation).GetSafeNormal2D();

	// The neighbour of Target that lies furthest along the push direction (works for every grid shape).
	const FVector TileSize = KeepersTerrain::GetTileSize(Grid);
	TOptional<FIntPoint> Destination;
	float BestAlignment = 0.5f;
	for (int32 DX = -2; DX <= 2; ++DX)
	{
		for (int32 DY = -2; DY <= 2; ++DY)
		{
			const FIntPoint Candidate(Target.X + DX, Target.Y + DY);
			FVector CandidateLocation;
			if (Candidate == Target || !KeepersTerrain::GetTileLocation(Grid, Candidate, CandidateLocation))
			{
				continue;
			}
			const FVector Offset = CandidateLocation - TargetLocation;
			const float Distance = Offset.Size2D();
			if (Distance > TileSize.X * 1.2f)
			{
				continue; // not adjacent
			}
			const float Alignment = FVector::DotProduct(Offset.GetSafeNormal2D(), PushDirection);
			if (Alignment > BestAlignment)
			{
				BestAlignment = Alignment;
				Destination = Candidate;
			}
		}
	}

	const int32 FeatherBonus = UKeepersCampaignSubsystem::HasArtifact(this, TEXT("StormFeather")) ? 1 : 0;
	const int32 BumpDamage = KeepersTerrain::BumpDamage + FeatherBonus;''', '''	const FVector PushDirection = (TargetLocation - OriginLocation).GetSafeNormal2D();

	// The neighbour of Target that lies furthest along the push direction (works for every grid shape).
	const TOptional<FIntPoint> Destination = KeepersTerrain::NeighbourAlong(Grid, Target, PushDirection);
	return ForcedMove(CombatSystem, Grid, Unit, Destination, UKeepersCampaignSubsystem::HasArtifact(this, TEXT("StormFeather")) ? 1 : 0);
}

bool UKeepersTerrainSubsystem::ForcedMove(UObject* CombatSystem, UObject* Grid, UObject* Unit, const TOptional<FIntPoint>& Destination, int32 FeatherBonus)
{
	const int32 BumpDamage = KeepersTerrain::BumpDamage + FeatherBonus;'''),
])

s = open('Private/KeepersTerrainSubsystem.cpp', encoding='utf-8').read()
s = s.replace('#include "KeepersRockActor.h"\n', '#include "KeepersRockActor.h"\n#include "KeepersSmokeActor.h"\n#include "Algo/Find.h"\n#include "Components/StaticMeshComponent.h"\n#include "Engine/StaticMeshActor.h"\n#include "Materials/MaterialInstanceDynamic.h"\n', 1)
s += '''
bool UKeepersTerrainSubsystem::ClearProp(UObject* Grid, const FIntPoint& Index)
{
	FVector Location;
	if (!KeepersTerrain::GetTileLocation(Grid, Index, Location))
	{
		return false;
	}
	const float Reach = KeepersTerrain::GetTileSize(Grid).X * 0.45f;
	bool bCleared = false;
	for (int32 I = TerrainProps.Num() - 1; I >= 0; --I)
	{
		AActor* Prop = TerrainProps[I];
		const bool bRockOrTree = Prop && (Prop->IsA<AKeepersRockActor>() || Prop->IsA<AKeepersPlantActor>());
		if (bRockOrTree && FVector::Dist2D(Prop->GetActorLocation(), Location) < Reach)
		{
			UKeepersJuiceSubsystem::Burst(GetWorld(), Prop->GetActorLocation() + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit,
				Prop->IsA<AKeepersRockActor>() ? FLinearColor(0.5f, 0.5f, 0.52f) : FLinearColor(0.25f, 0.55f, 0.2f), 16, 1.2f);
			Prop->Destroy();
			TerrainProps.RemoveAt(I);
			bCleared = true;
		}
	}
	if (bCleared || ForestTiles.Contains(Index))
	{
		FTransform Unused;
		SetTileType(Grid, Index, 1, Unused);
		ForestTiles.Remove(Index);
		bCleared = true;
	}
	return bCleared;
}

void UKeepersTerrainSubsystem::ApplyHeroAbility(EKeepersTerrainEffect Effect, UObject* Grid, const FIntPoint& Origin, const FIntPoint& Target)
{
	UObject* CombatSystem = KeepersTerrain::FindCombatSystem(GetWorld());
	UObject* Caster = KeepersTerrain::GetUnitOnTile(Grid, Origin);
	UObject* Victim = KeepersTerrain::GetUnitOnTile(Grid, Target);
	const int32 Team = KeepersTerrain::TeamOf(Caster);
	FVector TargetLocation = FVector::ZeroVector;
	FVector OriginLocation = FVector::ZeroVector;
	KeepersTerrain::GetTileLocation(Grid, Target, TargetLocation);
	KeepersTerrain::GetTileLocation(Grid, Origin, OriginLocation);
	TArray<FIntPoint> Area = KeepersTerrain::Adjacent(Grid, Target);
	Area.Insert(Target, 0);
	UWorld* World = GetWorld();

	switch (Effect)
	{
	case EKeepersTerrainEffect::Fireball:
		// The blast; forest in it burns to the ground.
		UKeepersJuiceSubsystem::Burst(World, TargetLocation + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit, FLinearColor(1.f, 0.45f, 0.1f), 24, 1.3f);
		UKeepersJuiceSubsystem::Burst(World, TargetLocation + FVector(0.f, 0.f, 30.f), AKeepersBurst::EKind::Puff, FLinearColor(0.3f, 0.28f, 0.26f), 12, 1.f);
		UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"), 0.8f);
		for (const FIntPoint& Index : Area)
		{
			if (ForestTiles.Contains(Index))
			{
				ClearProp(Grid, Index);
			}
		}
		break;
	case EKeepersTerrainEffect::IcePath:
	{
		// The target and the next cell along the cast freeze.
		TArray<FIntPoint> Path = { Target };
		if (const TOptional<FIntPoint> Next = KeepersTerrain::NeighbourAlong(Grid, Target, (TargetLocation - OriginLocation).GetSafeNormal2D()))
		{
			Path.Add(Next.GetValue());
		}
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
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
			UE_LOG(LogKeepersTerrain, Log, TEXT("IcePath freezes (%d,%d)"), Index.X, Index.Y);
		}
		UKeepersJuiceSubsystem::Sound(this, TEXT("Splash"), 0.6f);
		break;
	}
	case EKeepersTerrainEffect::ChainLightning:
	{
		// The bolt jumps to the two nearest foes within two cells of the target.
		TArray<TPair<float, UObject*>> Foes;
		for (const FIntPoint& Index : KeepersTerrain::Adjacent(Grid, Target, 2.3f))
		{
			UObject* Unit = KeepersTerrain::GetUnitOnTile(Grid, Index);
			FVector Location;
			if (Unit && Unit != Victim && KeepersTerrain::TeamOf(Unit) != Team && KeepersTerrain::GetTileLocation(Grid, Index, Location))
			{
				Foes.Emplace(FVector::Dist2D(Location, TargetLocation), Unit);
			}
		}
		Foes.Sort([](const TPair<float, UObject*>& A, const TPair<float, UObject*>& B) { return A.Key < B.Key; });
		UKeepersJuiceSubsystem::Burst(World, TargetLocation + FVector(0.f, 0.f, 80.f), AKeepersBurst::EKind::Hit, FLinearColor(1.f, 0.95f, 0.5f), 16);
		for (int32 I = 0; I < FMath::Min(Foes.Num(), 2); ++I)
		{
			if (const AActor* Struck = Cast<AActor>(Foes[I].Value))
			{
				UKeepersJuiceSubsystem::Burst(World, Struck->GetActorLocation() + FVector(0.f, 0.f, 60.f), AKeepersBurst::EKind::Hit, FLinearColor(0.7f, 0.85f, 1.f), 14);
			}
			DamageUnit(CombatSystem, Foes[I].Value, 3);
		}
		UKeepersJuiceSubsystem::Sound(this, TEXT("Hit"));
		break;
	}
	case EKeepersTerrainEffect::Dash:
	{
		uint8 Type = 0;
		if (!Caster || Victim || !KeepersTerrain::GetTileType(Grid, Target, Type) || !KeepersTerrain::CanStandOn(Caster, Type))
		{
			break;
		}
		if (const AActor* Runner = Cast<AActor>(Caster))
		{
			UKeepersJuiceSubsystem::Burst(World, Runner->GetActorLocation(), AKeepersBurst::EKind::Puff, FLinearColor(0.8f, 0.78f, 0.72f), 10, 0.7f);
		}
		KeepersReflection::FCall Move(CombatSystem, TEXT("SetUnitIndexOnGrid"));
		Move.Set<UObject*>(TEXT("Unit"), Caster);
		Move.Set<FIntPoint>(TEXT("Index"), Target);
		Move.Set<bool>(TEXT("Force"), true);
		Move.Set<bool>(TEXT("SnapLocation"), true);
		Move.Call();
		UKeepersJuiceSubsystem::Sound(this, TEXT("Gust"));
		// Foes next to the landing get a slash on the way.
		for (const FIntPoint& Index : KeepersTerrain::Adjacent(Grid, Target))
		{
			UObject* Unit = KeepersTerrain::GetUnitOnTile(Grid, Index);
			if (Unit && KeepersTerrain::TeamOf(Unit) != Team)
			{
				DamageUnit(CombatSystem, Unit, 2);
			}
		}
		UE_LOG(LogKeepersTerrain, Log, TEXT("Dash to (%d,%d)"), Target.X, Target.Y);
		break;
	}
	case EKeepersTerrainEffect::Backstab:
	{
		if (!Victim || KeepersTerrain::TeamOf(Victim) == Team)
		{
			break;
		}
		// Flanked: another of the Rogue's side stands next to the victim.
		for (const FIntPoint& Index : KeepersTerrain::Adjacent(Grid, Target))
		{
			UObject* Ally = KeepersTerrain::GetUnitOnTile(Grid, Index);
			if (Ally && Ally != Caster && KeepersTerrain::TeamOf(Ally) == Team)
			{
				UKeepersJuiceSubsystem::Burst(World, TargetLocation + FVector(0.f, 0.f, 80.f), AKeepersBurst::EKind::Hit, FLinearColor(0.85f, 0.1f, 0.15f), 18);
				DamageUnit(CombatSystem, Victim, 4);
				UE_LOG(LogKeepersTerrain, Log, TEXT("Backstab: flanked, double hit"));
				break;
			}
		}
		break;
	}
	case EKeepersTerrainEffect::SmokeBomb:
		for (const FIntPoint& Index : Area)
		{
			FVector Location;
			if (KeepersTerrain::GetTileLocation(Grid, Index, Location))
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AKeepersSmokeActor* Smoke = World->SpawnActor<AKeepersSmokeActor>(Location, FRotator::ZeroRotator, Params))
				{
					Smoke->Setup(KeepersTerrain::GetTileSize(Grid).X, 2);
				}
			}
		}
		UKeepersJuiceSubsystem::Sound(this, TEXT("Puff"));
		break;
	case EKeepersTerrainEffect::Crush:
		for (const FIntPoint& Index : Area)
		{
			ClearProp(Grid, Index);
		}
		UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
		break;
	case EKeepersTerrainEffect::Throw:
		if (Victim && Caster && Victim != Caster)
		{
			// Over the Barbarian's head to the cell behind it.
			ForcedMove(CombatSystem, Grid, Victim, KeepersTerrain::NeighbourAlong(Grid, Origin, (OriginLocation - TargetLocation).GetSafeNormal2D()), 0);
		}
		break;
	case EKeepersTerrainEffect::Rage:
		if (Caster)
		{
			int32 ApMax = 3;
			KeepersReflection::GetUnitStat(Caster, TEXT("AP_Max"), ApMax);
			KeepersReflection::FCall Ap(Caster, TEXT("SetCurrentAp"));
			Ap.Set<int32>(TEXT("Ap"), ApMax);
			Ap.Set<bool>(TEXT("ShowNumber"), true);
			Ap.Call();
			KeepersReflection::FCall Mp(Caster, TEXT("ModifyCurrentMp"));
			Mp.Set<int32>(TEXT("Modifier"), 1);
			Mp.Set<bool>(TEXT("ShowNumber"), true);
			Mp.Call();
			UKeepersJuiceSubsystem::Burst(World, OriginLocation + FVector(0.f, 0.f, 90.f), AKeepersBurst::EKind::Puff, FLinearColor(0.9f, 0.15f, 0.1f), 12, 0.9f);
			UKeepersJuiceSubsystem::Sound(this, TEXT("Hit"));
		}
		break;
	default:
		break;
	}
}
'''
open('Private/KeepersTerrainSubsystem.cpp', 'w', encoding='utf-8', newline='\r\n').write(s.replace('\r\n', '\n'))
print('ok')
