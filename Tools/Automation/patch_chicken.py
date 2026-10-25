"""Chickens call for help: every 2nd round each enemy chicken summons a slime next to it (twice at most)."""
import os

ROOT = r"C:\ProjectsUE\Turn-based-strategy\The_battle_code\Plugins\KeepersGame\Source\KeepersGame"


def patch(rel, old, new):
    path = os.path.join(ROOT, rel)
    text = open(path, encoding="utf-8-sig").read()
    if new in text:
        return
    assert old in text, f"{rel}: anchor not found: {old[:70]}"
    open(path, "w", encoding="utf-8").write(text.replace(old, new, 1))


H = "Public/KeepersCreatureSubsystem.h"
C = "Private/KeepersCreatureSubsystem.cpp"

patch(H, """ *    The last slime of a battle does not split: the battle is already won.
 */""", """ *    The last slime of a battle does not split: the battle is already won.
 *
 * Chicken (enemy):
 *  - every ChickenCallEvery rounds it calls for help: a slime appears next to it (ChickenMaxCalls times at most).
 */""")
patch(H, """	/** HP of a slime born from a split. */
	static constexpr int32 SmallSlimeHp = 3;""", """	/** HP of a slime born from a split. */
	static constexpr int32 SmallSlimeHp = 3;

	static constexpr int32 ChickenCallEvery = 2;
	static constexpr int32 ChickenMaxCalls = 2;""")
patch(H, """	void Split(UObject* CombatSystem, AActor* Slime, const FIntPoint& Index);""", """	void Split(UObject* CombatSystem, AActor* Slime, const FIntPoint& Index);
	void OnRoundStarted(UObject* CombatSystem);

	/** Spawns a unit of UnitType next to Index for Team and adds it to the combat. */
	AActor* SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag);""")
patch(H, """	TWeakObjectPtr<UObject> CachedCombatSystem;""", """	TWeakObjectPtr<UObject> CachedCombatSystem;

	/** Calls made by each chicken. */
	TMap<TWeakObjectPtr<AActor>, int32> ChickenCalls;

	int32 Round = 0;
	int32 LastTeam = INDEX_NONE;""")

patch(C, """	static constexpr uint8 SlimeType = 3;   // E_UnitType::Slime""", """	static constexpr uint8 SlimeType = 3;   // E_UnitType::Slime
	static constexpr uint8 ChickenType = 4; // E_UnitType::Chicken""")

patch(C, """	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));

	for (UObject* UnitObject : KeepersCreatures::UnitsInCombat(CombatSystem))""", """	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));

	// A round starts each time play returns to team 0.
	if (const UObject* Current = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit")))
	{
		int32 Team = INDEX_NONE;
		KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), Team);
		if (Team == 0 && LastTeam != 0)
		{
			++Round;
			OnRoundStarted(CombatSystem);
		}
		LastTeam = Team;
	}

	for (UObject* UnitObject : KeepersCreatures::UnitsInCombat(CombatSystem))""")

# Split now uses the shared spawner.
patch(C, """	UKeepersBarkLibrary::BarkAbout(Slime, EKeepersBarkSituation::Falls);
	for (const TPair<FIntPoint, FVector>& Spot : Spots)
	{
		AActor* Small = GetWorld()->SpawnActorDeferred<AActor>(Slime->GetClass(), FTransform(Spot.Value + FVector(0.f, 0.f, 100.f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Small)
		{
			continue;
		}
		if (FByteProperty* TypeProperty = CastField<FByteProperty>(KeepersReflection::FindMember(Small->GetClass(), TEXT("UnitType"))))
		{
			TypeProperty->SetPropertyValue_InContainer(Small, KeepersCreatures::SlimeType);
		}
		Small->Tags.Add(KeepersCreatures::SmallSlimeTag);
		UGameplayStatics::FinishSpawningActor(Small, FTransform(Spot.Value + FVector(0.f, 0.f, 100.f)));
		Small->SetActorScale3D(FVector(0.65f));

		KeepersReflection::FCall Add(CombatSystem, TEXT("AddUnitInCombat"));
		Add.Set<UObject*>(TEXT("Unit"), Small);
		Add.Set<FIntPoint>(TEXT("Index"), Spot.Key);
		Add.Set<int32>(TEXT("TeamIndex"), SlimeTeam);
		Add.Set<bool>(TEXT("UseAi"), true);
		Add.Call();
		KeepersReflection::FCall(Small, TEXT("CombatStarted")).Call();
		if (UObject* Ai = KeepersReflection::GetObject(Small, TEXT("Ai")))
		{
			KeepersReflection::FCall(Ai, TEXT("AiCombatStarted")).Call();
		}
		KeepersReflection::FCall SetHp(Small, TEXT("SetCurrentHp"));
		SetHp.Set<int32>(TEXT("Hp"), SmallSlimeHp);
		SetHp.Set<bool>(TEXT("ShowNumber"), false);
		SetHp.Call();
		UE_LOG(LogKeepersCreatures, Log, TEXT("Slime split: small slime at (%d,%d)"), Spot.Key.X, Spot.Key.Y);
	}
}""", """	UKeepersBarkLibrary::BarkAbout(Slime, EKeepersBarkSituation::Falls);
	for (int32 Piece = 0; Piece < 2; ++Piece)
	{
		if (AActor* Small = SpawnUnitNear(CombatSystem, Slime->GetClass(), KeepersCreatures::SlimeType, Index, SlimeTeam, SmallSlimeHp, 0.65f, KeepersCreatures::SmallSlimeTag))
		{
			UE_LOG(LogKeepersCreatures, Log, TEXT("Slime split: %s"), *Small->GetName());
		}
	}
}

void UKeepersCreatureSubsystem::OnRoundStarted(UObject* CombatSystem)
{
	if (Round <= 1 || (Round - 1) % ChickenCallEvery != 0)
	{
		return;
	}
	for (UObject* UnitObject : KeepersCreatures::UnitsInCombat(CombatSystem))
	{
		AActor* Chicken = Cast<AActor>(UnitObject);
		int32 Team = 0;
		if (!Chicken || KeepersCreatures::UnitType(Chicken) != KeepersCreatures::ChickenType || !KeepersCreatures::IsAlive(Chicken)
			|| !KeepersReflection::GetInt(Chicken->GetClass(), Chicken, TEXT("TeamIndex"), Team) || Team == 0)
		{
			continue;
		}
		int32& Calls = ChickenCalls.FindOrAdd(Chicken);
		if (Calls >= ChickenMaxCalls)
		{
			continue;
		}
		FIntPoint Index;
		KeepersReflection::GetIntPoint(Chicken->GetClass(), Chicken, TEXT("Index"), Index);
		if (AActor* Help = SpawnUnitNear(CombatSystem, Chicken->GetClass(), KeepersCreatures::SlimeType, Index, Team, 0, 1.f, NAME_None))
		{
			++Calls;
			UKeepersBarkLibrary::Bark(Chicken, FText::FromString(TEXT("Ко-ко-КО! На помощь!")));
			UE_LOG(LogKeepersCreatures, Log, TEXT("%s called %s"), *Chicken->GetName(), *Help->GetName());
		}
	}
}

AActor* UKeepersCreatureSubsystem::SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag)
{
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	FIntPoint Spot;
	FVector SpotLocation;
	bool bFound = false;
	for (int32 Ring = 1; Ring <= 2 && !bFound; ++Ring)
	{
		for (int32 DX = -Ring; DX <= Ring && !bFound; ++DX)
		{
			for (int32 DY = -Ring; DY <= Ring && !bFound; ++DY)
			{
				const FIntPoint Candidate(Index.X + DX, Index.Y + DY);
				if (Candidate != Index && KeepersCreatures::IsFreeWalkable(Grid, Candidate, SpotLocation))
				{
					Spot = Candidate;
					bFound = true;
				}
			}
		}
	}
	if (!bFound || !UnitClass)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(SpotLocation + FVector(0.f, 0.f, 100.f));
	AActor* Unit = GetWorld()->SpawnActorDeferred<AActor>(UnitClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Unit)
	{
		return nullptr;
	}
	if (FByteProperty* TypeProperty = CastField<FByteProperty>(KeepersReflection::FindMember(Unit->GetClass(), TEXT("UnitType"))))
	{
		TypeProperty->SetPropertyValue_InContainer(Unit, UnitType);
	}
	if (!Tag.IsNone())
	{
		Unit->Tags.Add(Tag);
	}
	UGameplayStatics::FinishSpawningActor(Unit, SpawnTransform);
	Unit->SetActorScale3D(FVector(Scale));

	KeepersReflection::FCall Add(CombatSystem, TEXT("AddUnitInCombat"));
	Add.Set<UObject*>(TEXT("Unit"), Unit);
	Add.Set<FIntPoint>(TEXT("Index"), Spot);
	Add.Set<int32>(TEXT("TeamIndex"), Team);
	Add.Set<bool>(TEXT("UseAi"), true);
	Add.Call();
	KeepersReflection::FCall(Unit, TEXT("CombatStarted")).Call();
	if (UObject* Ai = KeepersReflection::GetObject(Unit, TEXT("Ai")))
	{
		KeepersReflection::FCall(Ai, TEXT("AiCombatStarted")).Call();
	}
	if (Hp > 0)
	{
		KeepersReflection::FCall SetHp(Unit, TEXT("SetCurrentHp"));
		SetHp.Set<int32>(TEXT("Hp"), Hp);
		SetHp.Set<bool>(TEXT("ShowNumber"), false);
		SetHp.Call();
	}
	return Unit;
}""")

# The free-spot search moved into SpawnUnitNear; drop the old one in Split.
patch(C, """	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	TArray<TPair<FIntPoint, FVector>> Spots;
	for (int32 DX = -1; DX <= 1 && Spots.Num() < 2; ++DX)
	{
		for (int32 DY = -1; DY <= 1 && Spots.Num() < 2; ++DY)
		{
			const FIntPoint Candidate(Index.X + DX, Index.Y + DY);
			FVector Location;
			if (Candidate != Index && KeepersCreatures::IsFreeWalkable(Grid, Candidate, Location))
			{
				Spots.Emplace(Candidate, Location);
			}
		}
	}

""", "")
print("patched")
