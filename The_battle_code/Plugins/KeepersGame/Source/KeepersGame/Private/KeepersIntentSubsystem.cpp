#include "KeepersIntentSubsystem.h"

#include "Engine/World.h"
#include "KeepersIntentMarkers.h"
#include "KeepersReflection.h"

namespace KeepersIntents
{
	/**
	 * An intent stays only while the combat goes on and its AI's unit is alive.
	 * (BP_Unit.IsActiveInCombat only means "it is this unit's turn", so it is not used here.)
	 */
	static bool IsStillValid(UObject* UnitAi)
	{
		UObject* Unit = KeepersReflection::GetObject(UnitAi, TEXT("Unit"));
		const UObject* CombatSystem = KeepersReflection::GetObject(UnitAi, TEXT("CombatSystem"));
		if (!Unit || !CombatSystem)
		{
			return false;
		}
		bool bInCombat = false;
		bool bFinishing = false;
		KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsInCombat"), bInCombat);
		KeepersReflection::GetBool(CombatSystem->GetClass(), CombatSystem, TEXT("IsFinishingCombat"), bFinishing);
		if (!bInCombat || bFinishing)
		{
			return false;
		}
		bool bAlive = true;
		KeepersReflection::FCall IsAlive(Unit, TEXT("IsAlive"));
		if (IsAlive.Call())
		{
			IsAlive.Get<bool>(TEXT("IsAlive"), bAlive);
		}
		return bAlive;
	}
}

void UKeepersIntentSubsystem::SetIntent(UObject* UnitAi, FKeepersIntent&& Intent)
{
	Intents.Add(UnitAi, MoveTemp(Intent));
	bMarkersDirty = true;
}

const FKeepersIntent* UKeepersIntentSubsystem::FindIntent(const UObject* UnitAi) const
{
	return Intents.Find(const_cast<UObject*>(UnitAi));
}

void UKeepersIntentSubsystem::RemoveIntent(const UObject* UnitAi)
{
	if (Intents.Remove(const_cast<UObject*>(UnitAi)) > 0)
	{
		bMarkersDirty = true;
	}
}

TArray<FIntPoint> UKeepersIntentSubsystem::GetThreatenedIndexes() const
{
	TArray<FIntPoint> Result;
	for (const TPair<TWeakObjectPtr<UObject>, FKeepersIntent>& Pair : Intents)
	{
		for (const FIntPoint& Index : Pair.Value.AttackedIndexes)
		{
			Result.AddUnique(Index);
		}
	}
	return Result;
}

void UKeepersIntentSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (auto It = Intents.CreateIterator(); It; ++It)
	{
		UObject* UnitAi = It->Key.Get();
		if (!UnitAi || !KeepersIntents::IsStillValid(UnitAi))
		{
			It.RemoveCurrent();
			bMarkersDirty = true;
		}
	}

	if (bMarkersDirty)
	{
		RefreshMarkers();
	}
}

void UKeepersIntentSubsystem::RefreshMarkers()
{
	bMarkersDirty = false;

	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	if (!Markers)
	{
		if (Intents.Num() == 0)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Markers = World->SpawnActor<AKeepersIntentMarkers>(Params);
	}

	TArray<FVector> Locations;
	for (const TPair<TWeakObjectPtr<UObject>, FKeepersIntent>& Pair : Intents)
	{
		Locations.Append(Pair.Value.AttackedLocations);
	}
	Markers->SetMarkerLocations(Locations);
}

TStatId UKeepersIntentSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeepersIntentSubsystem, STATGROUP_Tickables);
}

bool UKeepersIntentSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
