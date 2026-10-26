#include "KeepersPartyBattle.h"

#include "Engine/World.h"
#include "KeepersCampaign.h"
#include "KeepersHeroes.h"
#include "KeepersLevelKit.h"
#include "KeepersReflection.h"
#include "KeepersTutorial.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersPartyBattle, Log, All);

bool UKeepersPartyBattleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UKeepersPartyBattleSubsystem::ChangeUnitType(UObject* Unit, uint8 UnitType)
{
	FByteProperty* TypeProperty = Unit ? CastField<FByteProperty>(KeepersReflection::FindMember(Unit->GetClass(), TEXT("UnitType"))) : nullptr;
	UClass* Library = LoadObject<UClass>(nullptr, TEXT("/Game/Blueprints/Core/Units/Utilities/BFL_Units.BFL_Units_C"));
	if (!TypeProperty || !Library)
	{
		return false;
	}
	TypeProperty->SetPropertyValue(TypeProperty->ContainerPtrToValuePtr<void>(Unit), UnitType);
	// The same data the unit's construction script reads for its type.
	KeepersReflection::FCall Defaults(Library->GetDefaultObject(), TEXT("GetDefaultUnitDataFromUnitType"), Unit);
	Defaults.Set<uint8>(TEXT("UnitType"), UnitType);
	if (!Defaults.Call())
	{
		return false;
	}
	const void* Data = Defaults.Param(TEXT("UnitData"));
	KeepersReflection::FCall Apply(Unit, TEXT("SetUnitData"));
	return Data && Apply.SetRaw(TEXT("Data"), Data) && Apply.Call();
}

void UKeepersPartyBattleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const UKeepersPartySubsystem* Party = UKeepersPartySubsystem::Get(&InWorld);
	FKeepersLevelGrid Grid;
	if (!Party || UKeepersTutorialSubsystem::bPendingTutorial || !KeepersLevelKit::PrepareGrid(&InWorld, Grid))
	{
		return;
	}
	const TArray<TArray<AActor*>> Teams = KeepersLevelKit::GetTeams(Grid);
	if (Teams.Num() == 0)
	{
		return;
	}
	// Heroes the level already has, and the squad's heroes it lacks.
	TArray<FName> Present;
	for (AActor* Unit : Teams[0])
	{
		const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit));
		if (Def && !Def->bEnemy)
		{
			Present.Add(Def->Id);
		}
	}
	UE_LOG(LogKeepersPartyBattle, Log, TEXT("Squad %s; the level has %d units of team 0 (%s)"), *FString::JoinBy(Party->GetParty(), TEXT(","), [](FName N) { return N.ToString(); }),
		Teams[0].Num(), *FString::JoinBy(Teams[0], TEXT(","), [](const AActor* A) { return FString::Printf(TEXT("%s=%d"), *A->GetName(), KeepersHeroes::UnitTypeOf(A)); }));
	TArray<FName> Missing;
	for (const FName& Hero : Party->GetParty())
	{
		if (!Present.Contains(Hero) && Party->IsUnlocked(Hero))
		{
			Missing.Add(Hero);
		}
	}
	for (AActor* Unit : Teams[0])
	{
		const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit));
		if (Missing.Num() == 0 || !Def || Def->bEnemy || Party->IsInParty(Def->Id))
		{
			continue;
		}
		const FKeepersHeroDef* Replacement = KeepersHeroes::Find(Missing[0]);
		if (Replacement && ChangeUnitType(Unit, static_cast<uint8>(Replacement->UnitType)))
		{
			UE_LOG(LogKeepersPartyBattle, Log, TEXT("%s: %s goes instead of %s"), *Unit->GetName(), *Replacement->Id.ToString(), *Def->Id.ToString());
			Missing.RemoveAt(0);
		}
	}
	// Abilities learned on the run's level-ups; the units build their ability lists on BeginPlay.
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(&InWorld);
	if (Campaign && Campaign->IsActive() && Campaign->IsInBattleNode())
	{
		for (AActor* Unit : Teams[0])
		{
			const FKeepersHeroDef* Def = KeepersHeroes::FindByUnitType(KeepersHeroes::UnitTypeOf(Unit));
			for (const FName& Spell : Def && !Def->bEnemy ? Campaign->GetLearnedSpells(Def->Id) : TArray<FName>())
			{
				if (KeepersReflection::AddUnitSpell(Unit, Spell.ToString()))
				{
					UE_LOG(LogKeepersPartyBattle, Log, TEXT("%s knows %s in this battle"), *Def->Id.ToString(), *Spell.ToString());
				}
			}
		}
	}
}
