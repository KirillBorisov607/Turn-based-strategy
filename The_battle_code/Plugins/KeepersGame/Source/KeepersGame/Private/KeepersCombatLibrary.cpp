#include "KeepersCombatLibrary.h"

#include "Algo/StableSort.h"
#include "Engine/World.h"
#include "KeepersBarks.h"
#include "KeepersIntentSubsystem.h"
#include "KeepersReflection.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersCombat, Log, All);

namespace KeepersCombat
{
	static const TCHAR* SpellLibraryPath = TEXT("/Game/Blueprints/Core/Combat/Spells/Utilities/BFL_Spells.BFL_Spells_C");

	/** BP_UnitAi.AiSpellCasting: its struct type and value. */
	static bool FindSpellCasting(UObject* UnitAi, const UScriptStruct*& OutStruct, void*& OutValue)
	{
		FProperty* Property = nullptr;
		OutValue = KeepersReflection::FindValuePtr(UnitAi->GetClass(), UnitAi, TEXT("AiSpellCasting"), &Property);
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		OutStruct = StructProperty ? StructProperty->Struct : nullptr;
		return OutStruct && OutValue;
	}

	static UKeepersIntentSubsystem* GetIntents(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UKeepersIntentSubsystem>() : nullptr;
	}

	/** Tiles a spell cast at TargetIndex will hit, via BFL_Spells and BP_CombatSystem.GetAttackedIndexes. */
	static TArray<FIntPoint> GetAttackedIndexes(UObject* UnitAi, UObject* CombatSystem, uint8 Spell, const FIntPoint& TargetIndex)
	{
		TArray<FIntPoint> Result;
		UClass* SpellLibrary = LoadObject<UClass>(nullptr, SpellLibraryPath);
		if (!SpellLibrary || !CombatSystem)
		{
			Result.Add(TargetIndex);
			return Result;
		}

		KeepersReflection::FCall GetSpellData(SpellLibrary->GetDefaultObject(), TEXT("GetSpellDataFromSpell"), UnitAi);
		GetSpellData.Set<uint8>(TEXT("Spell"), Spell);
		GetSpellData.Call();
		void* SpellData = GetSpellData.Param(TEXT("SpellData"));

		KeepersReflection::FCall GetAttacked(CombatSystem, TEXT("GetAttackedIndexes"));
		if (SpellData)
		{
			GetAttacked.SetRaw(TEXT("Spell"), SpellData);
		}
		GetAttacked.Set<FIntPoint>(TEXT("TargetIndex"), TargetIndex);
		GetAttacked.Call();
		if (const TArray<FIntPoint>* Attacked = static_cast<const TArray<FIntPoint>*>(GetAttacked.Param(TEXT("AttackedIndexes"))))
		{
			Result = *Attacked;
		}
		if (Result.Num() == 0)
		{
			Result.Add(TargetIndex);
		}
		return Result;
	}

	/** World location of a tile's surface: BP_Grid.GridTiles[Index].Transform (includes the tile height). */
	static bool GetTileLocation(UObject* Grid, const FIntPoint& Index, FVector& OutLocation)
	{
		FMapProperty* TilesProperty = CastField<FMapProperty>(KeepersReflection::FindMember(Grid->GetClass(), TEXT("GridTiles")));
		const FStructProperty* TileStruct = TilesProperty ? CastField<FStructProperty>(TilesProperty->ValueProp) : nullptr;
		if (!TileStruct)
		{
			return false;
		}
		FScriptMapHelper Tiles(TilesProperty, TilesProperty->ContainerPtrToValuePtr<void>(Grid));
		const uint8* Tile = nullptr;
		for (FScriptMapHelper::FIterator It(Tiles); It; ++It)
		{
			if (*reinterpret_cast<const FIntPoint*>(Tiles.GetKeyPtr(It)) == Index)
			{
				Tile = Tiles.GetValuePtr(It);
				break;
			}
		}
		if (!Tile)
		{
			return false;
		}
		FProperty* TransformProperty = nullptr;
		const void* Transform = KeepersReflection::FindValuePtr(TileStruct->Struct, const_cast<uint8*>(Tile), TEXT("Transform"), &TransformProperty);
		if (!Transform)
		{
			return false;
		}
		OutLocation = static_cast<const FTransform*>(Transform)->GetLocation();
		return true;
	}
}

void UKeepersCombatLibrary::SortUnitsInCombatByTeam(UObject* CombatSystem)
{
	if (!CombatSystem)
	{
		return;
	}
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(KeepersReflection::FindMember(CombatSystem->GetClass(), TEXT("UnitsInCombat")));
	FObjectPropertyBase* Inner = ArrayProperty ? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
	if (!Inner)
	{
		UE_LOG(LogKeepersCombat, Warning, TEXT("SortUnitsInCombatByTeam: %s has no UnitsInCombat array"), *CombatSystem->GetName());
		return;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(CombatSystem));
	TArray<UObject*> Units;
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		Units.Add(Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index)));
	}

	auto TeamOf = [](const UObject* Unit)
	{
		int32 Team = MAX_int32;
		if (Unit)
		{
			KeepersReflection::GetInt(Unit->GetClass(), Unit, TEXT("TeamIndex"), Team);
		}
		return Team;
	};
	Algo::StableSortBy(Units, TeamOf);

	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		Inner->SetObjectPropertyValue(Helper.GetRawPtr(Index), Units[Index]);
	}
}

void UKeepersCombatLibrary::EndPlayerTurn(UObject* CombatSystem)
{
	const UObject* First = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
	if (!First)
	{
		return;
	}
	int32 Team = INDEX_NONE;
	KeepersReflection::GetInt(First->GetClass(), First, TEXT("TeamIndex"), Team);
	for (int32 Guard = 0; Guard < 64; ++Guard)
	{
		KeepersReflection::FCall(CombatSystem, TEXT("NextUnit")).Call();
		const UObject* Next = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
		int32 NextTeam = INDEX_NONE;
		if (Next)
		{
			KeepersReflection::GetInt(Next->GetClass(), Next, TEXT("TeamIndex"), NextTeam);
		}
		if (!Next || Next == First || NextTeam != Team || KeepersReflection::GetObject(Next, TEXT("Ai")))
		{
			break;
		}
	}
}

bool UKeepersCombatLibrary::TrySwitchToUnitAt(UObject* Action, FIntPoint Index)
{
	UObject* PlayerActions = KeepersReflection::GetObject(Action, TEXT("PlayerActions"));
	UObject* CombatSystem = KeepersReflection::GetObject(PlayerActions, TEXT("CombatSystem"));
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));
	UObject* Current = KeepersReflection::GetObject(CombatSystem, TEXT("CurrentUnit"));
	if (!Grid || !Current)
	{
		return false;
	}

	// The unit standing on the clicked tile.
	UObject* Clicked = nullptr;
	FMapProperty* TilesProperty = CastField<FMapProperty>(KeepersReflection::FindMember(Grid->GetClass(), TEXT("GridTiles")));
	const FStructProperty* TileProperty = TilesProperty ? CastField<FStructProperty>(TilesProperty->ValueProp) : nullptr;
	if (TileProperty)
	{
		FScriptMapHelper Tiles(TilesProperty, TilesProperty->ContainerPtrToValuePtr<void>(Grid));
		for (FScriptMapHelper::FIterator It(Tiles); It; ++It)
		{
			if (*reinterpret_cast<const FIntPoint*>(Tiles.GetKeyPtr(It)) == Index)
			{
				FProperty* UnitProperty = nullptr;
				void* UnitPtr = KeepersReflection::FindValuePtr(TileProperty->Struct, Tiles.GetValuePtr(It), TEXT("UnitOnTile"), &UnitProperty);
				if (const FObjectPropertyBase* UnitObject = CastField<FObjectPropertyBase>(UnitProperty))
				{
					Clicked = UnitObject->GetObjectPropertyValue(UnitPtr);
				}
				break;
			}
		}
	}
	if (!Clicked || Clicked == Current)
	{
		return false;
	}

	// Only between player-controlled units of the same team.
	int32 ClickedTeam = -1;
	int32 CurrentTeam = -2;
	KeepersReflection::GetInt(Clicked->GetClass(), Clicked, TEXT("TeamIndex"), ClickedTeam);
	KeepersReflection::GetInt(Current->GetClass(), Current, TEXT("TeamIndex"), CurrentTeam);
	if (ClickedTeam != CurrentTeam || KeepersReflection::GetObject(Clicked, TEXT("Ai")) || KeepersReflection::GetObject(Current, TEXT("Ai")))
	{
		return false;
	}

	// The current unit must not be walking.
	if (FArrayProperty* PathProperty = CastField<FArrayProperty>(KeepersReflection::FindMember(Current->GetClass(), TEXT("CurrentPathToFollow"))))
	{
		FScriptArrayHelper Path(PathProperty, PathProperty->ContainerPtrToValuePtr<void>(Current));
		if (Path.Num() > 0)
		{
			return false;
		}
	}

	// Units after the current one have not played this phase yet.
	FArrayProperty* UnitsProperty = CastField<FArrayProperty>(KeepersReflection::FindMember(CombatSystem->GetClass(), TEXT("UnitsInCombat")));
	FObjectPropertyBase* Inner = UnitsProperty ? CastField<FObjectPropertyBase>(UnitsProperty->Inner) : nullptr;
	if (!Inner)
	{
		return false;
	}
	FScriptArrayHelper Helper(UnitsProperty, UnitsProperty->ContainerPtrToValuePtr<void>(CombatSystem));
	TArray<UObject*> Units;
	for (int32 I = 0; I < Helper.Num(); ++I)
	{
		Units.Add(Inner->GetObjectPropertyValue(Helper.GetRawPtr(I)));
	}
	const int32 CurrentIndex = Units.IndexOfByKey(Current);
	const int32 ClickedIndex = Units.IndexOfByKey(Clicked);
	if (CurrentIndex == INDEX_NONE || ClickedIndex <= CurrentIndex)
	{
		return false;
	}
	bool bAlive = true;
	KeepersReflection::FCall IsAlive(Clicked, TEXT("IsAlive"));
	if (IsAlive.Call())
	{
		IsAlive.Get<bool>(TEXT("IsAlive"), bAlive);
	}
	if (!bAlive)
	{
		return false;
	}

	Units.RemoveAt(ClickedIndex);
	Units.Insert(Clicked, CurrentIndex);
	for (int32 I = 0; I < Units.Num(); ++I)
	{
		Inner->SetObjectPropertyValue(Helper.GetRawPtr(I), Units[I]);
	}

	KeepersReflection::FCall Deactivate(Current, TEXT("SetIsActiveInCombat"));
	Deactivate.Set<bool>(TEXT("IsActiveInCombat"), false);
	Deactivate.Call();

	KeepersReflection::SetObject(CombatSystem, TEXT("CurrentUnit"), Clicked);
	KeepersReflection::FCall(Clicked, TEXT("TurnStarted")).Call();
	KeepersReflection::BroadcastObjectEvent(CombatSystem, TEXT("OnUnitTurnStarted"), Clicked);
	// Rebuilds the turn order bar.
	KeepersReflection::BroadcastObjectEvent(CombatSystem, TEXT("OnUnitTeamsChanged"), nullptr);

	UE_LOG(LogKeepersCombat, Log, TEXT("Control switched from %s to %s"), *Current->GetName(), *Clicked->GetName());
	return true;
}

void UKeepersCombatLibrary::SaveIntent(UObject* UnitAi)
{
	const UScriptStruct* CastingStruct = nullptr;
	void* Casting = nullptr;
	UKeepersIntentSubsystem* Intents = KeepersCombat::GetIntents(UnitAi);
	if (!UnitAi || !Intents || !KeepersCombat::FindSpellCasting(UnitAi, CastingStruct, Casting))
	{
		return;
	}

	bool bValid = false;
	KeepersReflection::GetBool(CastingStruct, Casting, TEXT("IsSpellValid"), bValid);
	if (!bValid)
	{
		Intents->RemoveIntent(UnitAi);
		return;
	}

	uint8 Spell = 0;
	FIntPoint TargetIndex;
	KeepersReflection::GetEnumByte(CastingStruct, Casting, TEXT("Spell"), Spell);
	KeepersReflection::GetIntPoint(CastingStruct, Casting, TEXT("TargetIndex"), TargetIndex);

	UObject* CombatSystem = KeepersReflection::GetObject(UnitAi, TEXT("CombatSystem"));
	UObject* Grid = KeepersReflection::GetObject(CombatSystem, TEXT("Grid"));

	FKeepersIntent Intent;
	Intent.SpellCasting.InitializeAs(CastingStruct, static_cast<const uint8*>(Casting));
	Intent.AttackedIndexes = KeepersCombat::GetAttackedIndexes(UnitAi, CombatSystem, Spell, TargetIndex);
	if (Grid)
	{
		for (const FIntPoint& Index : Intent.AttackedIndexes)
		{
			FVector Location;
			if (KeepersCombat::GetTileLocation(Grid, Index, Location))
			{
				Intent.AttackedLocations.Add(Location);
			}
		}
	}

	UE_LOG(LogKeepersCombat, Log, TEXT("%s plans spell %d at (%d,%d), %d tiles, %d marked"), *UnitAi->GetName(), Spell, TargetIndex.X, TargetIndex.Y,
		Intent.AttackedIndexes.Num(), Intent.AttackedLocations.Num());
	Intents->SetIntent(UnitAi, MoveTemp(Intent));
	UKeepersBarkLibrary::BarkAbout(Cast<AActor>(KeepersReflection::GetObject(UnitAi, TEXT("Unit"))), EKeepersBarkSituation::PlansAttack, 0.35f);
}

bool UKeepersCombatLibrary::RestoreIntent(UObject* UnitAi)
{
	const UScriptStruct* CastingStruct = nullptr;
	void* Casting = nullptr;
	UKeepersIntentSubsystem* Intents = KeepersCombat::GetIntents(UnitAi);
	if (!UnitAi || !Intents || !KeepersCombat::FindSpellCasting(UnitAi, CastingStruct, Casting))
	{
		return false;
	}

	const FKeepersIntent* Intent = Intents->FindIntent(UnitAi);
	if (!Intent || Intent->SpellCasting.GetScriptStruct() != CastingStruct)
	{
		return false;
	}

	CastingStruct->CopyScriptStruct(Casting, Intent->SpellCasting.GetMemory());

	// The unit may have been pushed since it planned: it attacks the same tile from where it stands.
	FIntPoint UnitIndex;
	const UObject* Unit = KeepersReflection::GetObject(UnitAi, TEXT("Unit"));
	if (Unit && KeepersReflection::GetIntPoint(Unit->GetClass(), Unit, TEXT("Index"), UnitIndex))
	{
		KeepersReflection::SetIntPoint(CastingStruct, Casting, TEXT("OriginIndex"), UnitIndex);
	}

	Intents->RemoveIntent(UnitAi);
	return true;
}

void UKeepersCombatLibrary::PrepareToPlanNextIntent(UObject* UnitAi)
{
	UObject* Unit = KeepersReflection::GetObject(UnitAi, TEXT("Unit"));
	if (!Unit)
	{
		return;
	}
	KeepersReflection::FCall(Unit, TEXT("ResetAp")).Call();
	KeepersReflection::FCall(Unit, TEXT("ResetSpellsCastCountThisTurn")).Call();
}

void UKeepersCombatLibrary::ClearIntent(UObject* UnitAi)
{
	if (UKeepersIntentSubsystem* Intents = KeepersCombat::GetIntents(UnitAi))
	{
		Intents->RemoveIntent(UnitAi);
	}
}
