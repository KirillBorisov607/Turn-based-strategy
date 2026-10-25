"""Balance: one chicken call per call-round (3 per battle), and small slimes cannot attack (0 AP)."""
import os

ROOT = r"C:\ProjectsUE\Turn-based-strategy\The_battle_code\Plugins\KeepersGame\Source\KeepersGame"


def patch(rel, old, new):
    path = os.path.join(ROOT, rel)
    text = open(path, encoding="utf-8-sig").read()
    if new in text:
        return
    assert old in text, f"{rel}: anchor not found: {old[:70]}"
    open(path, "w", encoding="utf-8-sig").write(text.replace(old, new, 1))


H = "Public/KeepersCreatureSubsystem.h"
C = "Private/KeepersCreatureSubsystem.cpp"

patch(H, """ *  - splits into two small slimes when it dies (small slimes do not split again).""",
      """ *  - splits into two small slimes when it dies (small slimes do not split again). Small slimes have
 *    no AP: they cannot attack, they crawl, block tiles and leave puddles.""")
patch(H, """ *  - every ChickenCallEvery rounds it calls for help: a slime appears next to it (ChickenMaxCalls times at most).""",
      """ *  - every ChickenCallEvery rounds one enemy chicken calls for help: a slime appears next to it
 *    (ChickenMaxCalls per battle).""")
patch(H, """	static constexpr int32 ChickenMaxCalls = 2;""", """	static constexpr int32 ChickenMaxCalls = 3;""")
patch(H, """	AActor* SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag);""",
      """	AActor* SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag, bool bCanAct = true);""")
patch(H, """	/** Calls made by each chicken. */
	TMap<TWeakObjectPtr<AActor>, int32> ChickenCalls;""", """	/** Chicken calls made this battle. */
	int32 ChickenCalls = 0;""")

patch(C, """		if (AActor* Small = SpawnUnitNear(CombatSystem, Slime->GetClass(), KeepersCreatures::SlimeType, Index, SlimeTeam, SmallSlimeHp, 0.65f, KeepersCreatures::SmallSlimeTag))""",
      """		if (AActor* Small = SpawnUnitNear(CombatSystem, Slime->GetClass(), KeepersCreatures::SlimeType, Index, SlimeTeam, SmallSlimeHp, 0.65f, KeepersCreatures::SmallSlimeTag, false))""")

patch(C, """	for (UObject* UnitObject : KeepersCreatures::UnitsInCombat(CombatSystem))
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
	}""", """	if (ChickenCalls >= ChickenMaxCalls)
	{
		return;
	}
	TArray<AActor*> Chickens;
	for (UObject* UnitObject : KeepersCreatures::UnitsInCombat(CombatSystem))
	{
		AActor* Chicken = Cast<AActor>(UnitObject);
		int32 Team = 0;
		if (Chicken && KeepersCreatures::UnitType(Chicken) == KeepersCreatures::ChickenType && KeepersCreatures::IsAlive(Chicken)
			&& KeepersReflection::GetInt(Chicken->GetClass(), Chicken, TEXT("TeamIndex"), Team) && Team != 0)
		{
			Chickens.Add(Chicken);
		}
	}
	if (Chickens.Num() == 0)
	{
		return;
	}
	AActor* Chicken = Chickens[FMath::RandRange(0, Chickens.Num() - 1)];
	int32 Team = 0;
	KeepersReflection::GetInt(Chicken->GetClass(), Chicken, TEXT("TeamIndex"), Team);
	FIntPoint Index;
	KeepersReflection::GetIntPoint(Chicken->GetClass(), Chicken, TEXT("Index"), Index);
	if (AActor* Help = SpawnUnitNear(CombatSystem, Chicken->GetClass(), KeepersCreatures::SlimeType, Index, Team, 0, 1.f, NAME_None))
	{
		++ChickenCalls;
		UKeepersBarkLibrary::Bark(Chicken, FText::FromString(TEXT("Ко-ко-КО! На помощь!")));
		UE_LOG(LogKeepersCreatures, Log, TEXT("%s called %s"), *Chicken->GetName(), *Help->GetName());
	}""")

patch(C, """AActor* UKeepersCreatureSubsystem::SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag)""",
      """AActor* UKeepersCreatureSubsystem::SpawnUnitNear(UObject* CombatSystem, UClass* UnitClass, uint8 UnitType, const FIntPoint& Index, int32 Team, int32 Hp, float Scale, FName Tag, bool bCanAct)""")

patch(C, """	if (Hp > 0)
	{
		KeepersReflection::FCall SetHp(Unit, TEXT("SetCurrentHp"));""", """	if (!bCanAct)
	{
		KeepersCreatures::SetStat(Unit, TEXT("AP_Max"), 0);
		KeepersCreatures::SetStat(Unit, TEXT("AP_Current"), 0);
	}
	if (Hp > 0)
	{
		KeepersReflection::FCall SetHp(Unit, TEXT("SetCurrentHp"));""")

patch(C, """	static UObject* FindCombatSystem(UWorld* World)
	{""", """	/** Writes an int in BP_Unit.UnitData.Stats (AP_Max, MP_Max, ...). */
	static void SetStat(UObject* Unit, const FString& Name, int32 Value)
	{
		FProperty* DataProperty = nullptr;
		void* Data = KeepersReflection::FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitData"), &DataProperty);
		const FStructProperty* DataStruct = CastField<FStructProperty>(DataProperty);
		FProperty* StatsProperty = nullptr;
		void* Stats = DataStruct ? KeepersReflection::FindValuePtr(DataStruct->Struct, Data, TEXT("Stats"), &StatsProperty) : nullptr;
		const FStructProperty* StatsStruct = CastField<FStructProperty>(StatsProperty);
		FProperty* StatProperty = nullptr;
		void* Stat = StatsStruct ? KeepersReflection::FindValuePtr(StatsStruct->Struct, Stats, Name, &StatProperty) : nullptr;
		if (FIntProperty* IntProperty = CastField<FIntProperty>(StatProperty))
		{
			IntProperty->SetPropertyValue(Stat, Value);
		}
	}

	static UObject* FindCombatSystem(UWorld* World)
	{""")
print("patched")
