"""Adds any-order hero switching (TrySwitchToUnitAt) and the reflection helpers it needs."""
import os

ROOT = r"C:\ProjectsUE\Turn-based-strategy\The_battle_code\Plugins\KeepersGame\Source\KeepersGame"


def patch(rel, old, new):
    path = os.path.join(ROOT, rel)
    text = open(path, encoding="utf-8-sig").read()
    if new in text:
        return
    assert old in text, f"{rel}: anchor not found"
    open(path, "w", encoding="utf-8").write(text.replace(old, new, 1))


patch("Public/KeepersReflection.h", """	/**
	 * One call to a Blueprint function.""", """	/** Sets a Blueprint object variable. */
	KEEPERSGAME_API bool SetObject(UObject* Object, const FString& Name, UObject* Value);

	/**
	 * Broadcasts a Blueprint event dispatcher (multicast delegate variable) whose only parameter is an object,
	 * e.g. BP_CombatSystem.OnUnitTurnStarted(Unit).
	 */
	KEEPERSGAME_API bool BroadcastObjectEvent(UObject* Object, const FString& DispatcherName, UObject* Argument);

	/**
	 * One call to a Blueprint function.""")

patch("Private/KeepersReflection.cpp", """	bool GetInt(const UStruct* Struct""", """	bool SetObject(UObject* Object, const FString& Name, UObject* Value)
	{
		FProperty* Property = nullptr;
		void* Ptr = Object ? FindValuePtr(Object->GetClass(), Object, Name, &Property) : nullptr;
		FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
		if (!ObjectProperty || !Ptr)
		{
			return false;
		}
		ObjectProperty->SetObjectPropertyValue(Ptr, Value);
		return true;
	}

	bool BroadcastObjectEvent(UObject* Object, const FString& DispatcherName, UObject* Argument)
	{
		FMulticastDelegateProperty* Dispatcher = Object ? CastField<FMulticastDelegateProperty>(FindMember(Object->GetClass(), DispatcherName)) : nullptr;
		UFunction* Signature = Dispatcher ? Dispatcher->SignatureFunction.Get() : nullptr;
		if (!Signature)
		{
			return false;
		}
		const FMulticastScriptDelegate* Delegate = Dispatcher->GetMulticastDelegate(Dispatcher->ContainerPtrToValuePtr<void>(Object));
		if (!Delegate)
		{
			return false;
		}

		uint8* Params = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(Signature->ParmsSize, 1), Signature->GetMinAlignment()));
		FMemory::Memzero(Params, Signature->ParmsSize);
		for (TFieldIterator<FProperty> It(Signature); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
			if (FObjectPropertyBase* ObjectParam = CastField<FObjectPropertyBase>(*It))
			{
				ObjectParam->SetObjectPropertyValue_InContainer(Params, Argument);
			}
		}
		Delegate->ProcessMulticastDelegate<UObject>(Params);
		for (TFieldIterator<FProperty> It(Signature); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		FMemory::Free(Params);
		return true;
	}

	bool GetInt(const UStruct* Struct""")

patch("Public/KeepersCombatLibrary.h", """	/**
	 * Enemy intents, called from BP_UnitAi's AiFlow graph.""", """	/**
	 * Heroes act in any order during the player phase (called from Action_Cbt_Move.ExecuteAction).
	 * Clicking a hero of the current team that has not played this phase yet hands control to it:
	 * it takes the current unit's place in UnitsInCombat, and the current unit keeps its remaining
	 * MP/AP and plays right after. Returns true when control switched (the click is not a move).
	 */
	UFUNCTION(BlueprintCallable, Category = "Keepers|Combat", meta = (DefaultToSelf = "Action"))
	static bool TrySwitchToUnitAt(UObject* Action, FIntPoint Index);

	/**
	 * Enemy intents, called from BP_UnitAi's AiFlow graph.""")

patch("Private/KeepersCombatLibrary.cpp", """void UKeepersCombatLibrary::SaveIntent(UObject* UnitAi)""", """bool UKeepersCombatLibrary::TrySwitchToUnitAt(UObject* Action, FIntPoint Index)
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

	UE_LOG(LogKeepersCombat, Log, TEXT("Control switched from %s to %s"), *Current->GetName(), *Clicked->GetName());
	return true;
}

void UKeepersCombatLibrary::SaveIntent(UObject* UnitAi)""")
print("patched")
