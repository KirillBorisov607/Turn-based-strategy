"""The "destroy the totem" objective: an enemy becomes a still chicken totem that heals the enemies around it."""
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


patch('Public/KeepersBattleObjective.h', [
('''	/** Hold out for a few rounds. */
	Survive
};''', '''	/** Hold out for a few rounds. */
	Survive,
	/** Destroy the chickens' totem: it does not move, it heals the enemies around it. */
	Totem
};'''),
('''	/** The leader; empty — the toughest enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<AActor> Leader;''', '''	/** The leader; empty — the toughest enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<AActor> Leader;

	/** With bDefeatLeader: the leader is a totem (empty Leader — the enemy furthest from the heroes). It stands still,
	 *  does not act and heals the enemies around it at the start of each of their turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bTotem = false;'''),
('''	void SetUp();''', '''	void SetUp();
	/** The leader becomes the totem: still, tough, hidden under a carved chicken pole. */
	void MakeTotem();
	/** The enemies' turn begins: the totem heals those around it. */
	void TotemHeals();'''),
('''	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BeaconGem;''', '''	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BeaconGem;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TotemParts;
	UPROPERTY()
	TObjectPtr<UPointLightComponent> TotemLight;
	bool bTotemFallen = false;'''),
])

patch('Private/KeepersBattleObjective.cpp', [
('''	Objective->bDefeatLeader = Kind == EKeepersObjectiveKind::Leader;''', '''	Objective->bDefeatLeader = Kind == EKeepersObjectiveKind::Leader || Kind == EKeepersObjectiveKind::Totem;
	Objective->bTotem = Kind == EKeepersObjectiveKind::Totem;'''),
('''	if (bDefeatLeader && !Leader)
	{
		// The toughest enemy leads.''', '''	if (bDefeatLeader && bTotem && !Leader)
	{
		// The totem stands where the enemies are, as far from the heroes as it gets.
		float Best = -1.f;
		for (const TPair<FIntPoint, KeepersObjective::FTile>& Pair : Tiles)
		{
			if (!Pair.Value.Unit || KeepersObjective::TeamOf(Pair.Value.Unit) == 0)
			{
				continue;
			}
			float Nearest = MAX_flt;
			for (const TPair<FIntPoint, KeepersObjective::FTile>& Other : Tiles)
			{
				if (Other.Value.Unit && KeepersObjective::TeamOf(Other.Value.Unit) == 0)
				{
					Nearest = FMath::Min(Nearest, FVector::Dist2D(Other.Value.Location, Pair.Value.Location));
				}
			}
			if (Nearest > Best)
			{
				Best = Nearest;
				Leader = Cast<AActor>(Pair.Value.Unit);
			}
		}
	}
	if (bDefeatLeader && !Leader)
	{
		// The toughest enemy leads.'''),
('''	if (bDefeatLeader && Leader)
	{
		const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Leader));
		LeaderName = Def ? Def->Name.ToString() : FString();''', '''	if (bDefeatLeader && bTotem && Leader)
	{
		MakeTotem();
	}
	else if (bDefeatLeader && Leader)
	{
		const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Leader));
		LeaderName = Def ? Def->Name.ToString() : FString();'''),
('''AKeepersBattleObjective::AKeepersBattleObjective()''', '''void AKeepersBattleObjective::MakeTotem()
{
	// Still and tough: no movement, no actions, 26 health whatever the unit was.
	auto SetStat = [this](const TCHAR* Stat, int32 Value)
	{
		int32 Old = 0;
		if (KeepersReflection::GetUnitStat(Leader, Stat, Old))
		{
			KeepersReflection::AddUnitStat(Leader, Stat, Value - Old);
		}
	};
	SetStat(TEXT("HP_Max"), 26);
	SetStat(TEXT("HP_Current"), 26);
	SetStat(TEXT("MP_Max"), 0);
	SetStat(TEXT("MP_Current"), 0);
	SetStat(TEXT("AP_Max"), 0);
	SetStat(TEXT("AP_Current"), 0);
	// The unit's figure hides under the pole.
	TArray<UMeshComponent*> Meshes;
	Leader->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh->IsA<UStaticMeshComponent>() || Mesh->IsA<USkeletalMeshComponent>())
		{
			Mesh->SetVisibility(false, true);
		}
	}
	TArray<AActor*> Attached;
	Leader->GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		Actor->SetActorHiddenInGame(true);
	}

	// A carved pole: a stone base, three painted rings, wings, a beak, eyes and a red comb.
	const FVector Base = Leader->GetActorLocation() - FVector(0.f, 0.f, Leader->GetSimpleCollisionHalfHeight());
	auto Part = [this, &Base](const TCHAR* Mesh, const FLinearColor& Color, const FVector& Offset, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Component = KeepersObjective::Shape(this, Mesh, Color);
		Component->SetWorldLocation(Base + Offset);
		Component->SetWorldRotation(Rotation);
		Component->SetWorldScale3D(Scale);
		TotemParts.Add(Component);
	};
	const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
	Part(Cylinder, FLinearColor(0.35f, 0.33f, 0.3f), FVector(0.f, 0.f, 10.f), FVector(0.75f, 0.75f, 0.2f));
	Part(Cylinder, FLinearColor(0.45f, 0.25f, 0.1f), FVector(0.f, 0.f, 50.f), FVector(0.45f, 0.45f, 0.6f));
	Part(Cylinder, FLinearColor(0.85f, 0.65f, 0.15f), FVector(0.f, 0.f, 100.f), FVector(0.42f, 0.42f, 0.4f));
	Part(Cylinder, FLinearColor(0.75f, 0.2f, 0.12f), FVector(0.f, 0.f, 140.f), FVector(0.4f, 0.4f, 0.4f));
	Part(Cube, FLinearColor(0.9f, 0.88f, 0.8f), FVector(0.f, 32.f, 115.f), FVector(0.1f, 0.5f, 0.3f), FRotator(0.f, 0.f, -25.f));
	Part(Cube, FLinearColor(0.9f, 0.88f, 0.8f), FVector(0.f, -32.f, 115.f), FVector(0.1f, 0.5f, 0.3f), FRotator(0.f, 0.f, 25.f));
	Part(Sphere, FLinearColor(0.95f, 0.95f, 0.9f), FVector(0.f, 0.f, 185.f), FVector(0.45f));
	Part(Cone, FLinearColor(1.f, 0.6f, 0.1f), FVector(26.f, 0.f, 185.f), FVector(0.14f, 0.14f, 0.22f), FRotator(-90.f, 0.f, 0.f));
	Part(Sphere, FLinearColor(0.05f, 0.05f, 0.05f), FVector(18.f, 10.f, 196.f), FVector(0.06f));
	Part(Sphere, FLinearColor(0.05f, 0.05f, 0.05f), FVector(18.f, -10.f, 196.f), FVector(0.06f));
	Part(Cube, FLinearColor(0.9f, 0.1f, 0.08f), FVector(0.f, 0.f, 215.f), FVector(0.3f, 0.08f, 0.18f));
	TotemLight = NewObject<UPointLightComponent>(this);
	TotemLight->SetupAttachment(RootComponent);
	TotemLight->RegisterComponent();
	TotemLight->SetWorldLocation(Base + FVector(0.f, 0.f, 160.f));
	TotemLight->SetLightColor(FLinearColor(1.f, 0.35f, 0.2f));
	TotemLight->SetIntensity(6000.f);
	TotemLight->SetAttenuationRadius(500.f);
	TotemLight->SetCastShadows(false);
	LeaderName = TEXT("тотем кур");
	UE_LOG(LogKeepersObjective, Log, TEXT("Totem %s"), *Leader->GetName());
}

void AKeepersBattleObjective::TotemHeals()
{
	if (!Leader || !KeepersObjective::IsAlive(Leader))
	{
		return;
	}
	const FVector Center = Leader->GetActorLocation();
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	float Reach = 450.f;
	if (const FVector* Size = Grid ? static_cast<const FVector*>(KeepersReflection::FindValuePtr(Grid->GetClass(), Grid, TEXT("GridTileSize"))) : nullptr)
	{
		Reach = Size->X * 2.3f;
	}
	int32 Healed = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It == Leader || It->GetClass()->GetName() != TEXT("BP_Unit_C") || KeepersObjective::TeamOf(*It) == 0 || !KeepersObjective::IsAlive(*It)
			|| FVector::Dist2D(It->GetActorLocation(), Center) > Reach)
		{
			continue;
		}
		int32 Hp = 0;
		int32 HpMax = 0;
		KeepersReflection::GetUnitStat(*It, TEXT("HP_Current"), Hp);
		KeepersReflection::GetUnitStat(*It, TEXT("HP_Max"), HpMax);
		if (Hp >= HpMax)
		{
			continue;
		}
		KeepersReflection::FCall Heal(*It, TEXT("ModifyCurrentHp"));
		Heal.Set<int32>(TEXT("Modifier"), FMath::Min(2, HpMax - Hp));
		Heal.Set<bool>(TEXT("ShowNumber"), true);
		Heal.Call();
		UKeepersJuiceSubsystem::Burst(GetWorld(), It->GetActorLocation(), AKeepersBurst::EKind::Heal, FLinearColor(1.f, 0.45f, 0.25f), 10, 0.8f);
		++Healed;
	}
	if (Healed > 0)
	{
		UKeepersJuiceSubsystem::Burst(GetWorld(), Center + FVector(0.f, 0.f, 200.f), AKeepersBurst::EKind::Heal, FLinearColor(1.f, 0.35f, 0.2f), 16, 1.2f);
		UE_LOG(LogKeepersObjective, Log, TEXT("The totem heals %d enemies"), Healed);
	}
}

AKeepersBattleObjective::AKeepersBattleObjective()'''),
('''	if (bDefeatLeader && Leader && !KeepersObjective::IsAlive(Leader))
	{
		Finish(true);
		return;
	}''', '''	if (TotemLight)
	{
		TotemLight->SetIntensity(5000.f + 2500.f * FMath::Sin(Time * 2.f));
	}
	if (bDefeatLeader && Leader && !KeepersObjective::IsAlive(Leader))
	{
		if (bTotem && !bTotemFallen)
		{
			// The pole breaks into splinters.
			bTotemFallen = true;
			for (UStaticMeshComponent* Part : TotemParts)
			{
				UKeepersJuiceSubsystem::Burst(GetWorld(), Part->GetComponentLocation(), AKeepersBurst::EKind::Hit, FLinearColor(0.6f, 0.35f, 0.15f), 4, 1.f);
				Part->SetVisibility(false);
			}
			if (TotemLight)
			{
				TotemLight->SetVisibility(false);
			}
			UKeepersJuiceSubsystem::Sound(this, TEXT("Rock"));
		}
		Finish(true);
		return;
	}'''),
('''	if (Current)
	{
		LastTeam = Team;
	}''', '''	if (Current && bTotem && Team != 0 && LastTeam == 0)
	{
		TotemHeals();
	}
	if (Current)
	{
		LastTeam = Team;
	}'''),
('''	if (bDefeatLeader)
	{
		Goals.Add(LeaderName.IsEmpty()''', '''	if (bDefeatLeader && bTotem)
	{
		Goals.Add(TEXT("разрушьте тотем кур — он лечит врагов рядом"));
	}
	else if (bDefeatLeader)
	{
		Goals.Add(LeaderName.IsEmpty()'''),
('''#include "KeepersLevelKit.h"''', '''#include "KeepersJuice.h"
#include "KeepersLevelKit.h"'''),
('''#include "Components/StaticMeshComponent.h"''', '''#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"'''),
])

# Tests and quick battles: -KeepersObjective=totem, -KeepersSurface=ice|marsh.
patch('Private/KeepersDirector.cpp', [
('''			: Goal == TEXT("leader") ? EKeepersObjectiveKind::Leader : Goal == TEXT("survive") ? EKeepersObjectiveKind::Survive : EKeepersObjectiveKind::None);''',
 '''			: Goal == TEXT("leader") ? EKeepersObjectiveKind::Leader : Goal == TEXT("survive") ? EKeepersObjectiveKind::Survive
			: Goal == TEXT("totem") ? EKeepersObjectiveKind::Totem : EKeepersObjectiveKind::None);
	}
	// Ice or marsh patches in the first battle: -KeepersSurface=ice|marsh.
	FString Surface;
	if (FParse::Value(CommandLine, TEXT("-KeepersSurface="), Surface))
	{
		UKeepersSurfaceSubsystem::RequestForNextBattle(Surface == TEXT("ice") ? EKeepersSurface::Ice : Surface == TEXT("marsh") ? EKeepersSurface::Marsh : EKeepersSurface::None);'''),
('''#include "KeepersDirector.h"''', '''#include "KeepersDirector.h"
#include "KeepersSurfaces.h"'''),
])
print('ok')
