#include "KeepersReflection.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeepersReflection, Log, All);

namespace KeepersReflection
{
	static bool MatchesReadableName(const FProperty* Property, const FString& Name)
	{
		const FString PropertyName = Property->GetName();
		if (PropertyName == Name)
		{
			return true;
		}
		// User-defined struct members: "<Name>_<Number>_<Guid>"
		if (PropertyName.Len() > Name.Len() + 1 && PropertyName.StartsWith(Name) && PropertyName[Name.Len()] == TEXT('_'))
		{
			const TCHAR Next = PropertyName.Len() > Name.Len() + 1 ? PropertyName[Name.Len() + 1] : 0;
			return FChar::IsDigit(Next);
		}
		return false;
	}

	FProperty* FindMember(const UStruct* Struct, const FString& Name)
	{
		if (!Struct)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (MatchesReadableName(*It, Name))
			{
				return *It;
			}
		}
		return nullptr;
	}

	void* FindValuePtr(const UStruct* Struct, void* Container, const FString& Name, FProperty** OutProperty)
	{
		FProperty* Property = FindMember(Struct, Name);
		if (OutProperty)
		{
			*OutProperty = Property;
		}
		return Property && Container ? Property->ContainerPtrToValuePtr<void>(Container) : nullptr;
	}

	UObject* GetObject(const UObject* Object, const FString& Name)
	{
		if (!Object)
		{
			return nullptr;
		}
		FProperty* Property = nullptr;
		void* Ptr = FindValuePtr(Object->GetClass(), const_cast<UObject*>(Object), Name, &Property);
		const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
		return ObjectProperty && Ptr ? ObjectProperty->GetObjectPropertyValue(Ptr) : nullptr;
	}

	static int32* FindUnitStat(UObject* Unit, const FString& Stat)
	{
		if (!Unit)
		{
			return nullptr;
		}
		FProperty* DataProperty = nullptr;
		void* Data = FindValuePtr(Unit->GetClass(), Unit, TEXT("UnitData"), &DataProperty);
		const FStructProperty* DataStruct = CastField<FStructProperty>(DataProperty);
		FProperty* StatsProperty = nullptr;
		void* Stats = DataStruct ? FindValuePtr(DataStruct->Struct, Data, TEXT("Stats"), &StatsProperty) : nullptr;
		const FStructProperty* StatsStruct = CastField<FStructProperty>(StatsProperty);
		FProperty* StatProperty = nullptr;
		void* Value = StatsStruct ? FindValuePtr(StatsStruct->Struct, Stats, Stat, &StatProperty) : nullptr;
		return CastField<FIntProperty>(StatProperty) ? static_cast<int32*>(Value) : nullptr;
	}

	bool GetUnitStat(UObject* Unit, const FString& Stat, int32& OutValue)
	{
		if (const int32* Value = FindUnitStat(Unit, Stat))
		{
			OutValue = *Value;
			return true;
		}
		return false;
	}

	bool AddUnitStat(UObject* Unit, const FString& Stat, int32 Delta)
	{
		if (int32* Value = FindUnitStat(Unit, Stat))
		{
			*Value += Delta;
			return true;
		}
		return false;
	}

	bool SetObject(UObject* Object, const FString& Name, UObject* Value)
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
		Delegate->ProcessDelegate<UObject>(Params);
		for (TFieldIterator<FProperty> It(Signature); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		FMemory::Free(Params);
		return true;
	}

	bool GetInt(const UStruct* Struct, const void* Container, const FString& Name, int32& OutValue)
	{
		FProperty* Property = nullptr;
		void* Ptr = FindValuePtr(Struct, const_cast<void*>(Container), Name, &Property);
		if (const FIntProperty* IntProperty = CastField<FIntProperty>(Property))
		{
			OutValue = IntProperty->GetPropertyValue(Ptr);
			return true;
		}
		return false;
	}

	bool GetBool(const UStruct* Struct, const void* Container, const FString& Name, bool& OutValue)
	{
		FProperty* Property = nullptr;
		void* Ptr = FindValuePtr(Struct, const_cast<void*>(Container), Name, &Property);
		if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			OutValue = BoolProperty->GetPropertyValue(Ptr);
			return true;
		}
		return false;
	}

	bool GetIntPoint(const UStruct* Struct, const void* Container, const FString& Name, FIntPoint& OutValue)
	{
		FProperty* Property = nullptr;
		void* Ptr = FindValuePtr(Struct, const_cast<void*>(Container), Name, &Property);
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (StructProperty && StructProperty->Struct == TBaseStructure<FIntPoint>::Get())
		{
			OutValue = *static_cast<FIntPoint*>(Ptr);
			return true;
		}
		return false;
	}

	bool SetIntPoint(const UStruct* Struct, void* Container, const FString& Name, const FIntPoint& Value)
	{
		FProperty* Property = nullptr;
		void* Ptr = FindValuePtr(Struct, Container, Name, &Property);
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (StructProperty && StructProperty->Struct == TBaseStructure<FIntPoint>::Get())
		{
			*static_cast<FIntPoint*>(Ptr) = Value;
			return true;
		}
		return false;
	}

	bool GetEnumByte(const UStruct* Struct, const void* Container, const FString& Name, uint8& OutValue)
	{
		FProperty* Property = nullptr;
		void* Ptr = FindValuePtr(Struct, const_cast<void*>(Container), Name, &Property);
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			OutValue = ByteProperty->GetPropertyValue(Ptr);
			return true;
		}
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			OutValue = static_cast<uint8>(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Ptr));
			return true;
		}
		return false;
	}

	FCall::FCall(UObject* InTarget, FName FunctionName, UObject* WorldContext)
		: Target(InTarget)
	{
		if (!Target)
		{
			return;
		}
		Function = Target->FindFunction(FunctionName);
		if (!Function)
		{
			UE_LOG(LogKeepersReflection, Warning, TEXT("%s has no function %s"), *Target->GetClass()->GetName(), *FunctionName.ToString());
			return;
		}
		Buffer = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(Function->ParmsSize, 1), Function->GetMinAlignment()));
		FMemory::Memzero(Buffer, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buffer);
		}
		if (WorldContext)
		{
			if (FObjectPropertyBase* ContextProperty = CastField<FObjectPropertyBase>(Function->FindPropertyByName(TEXT("__WorldContext"))))
			{
				ContextProperty->SetObjectPropertyValue_InContainer(Buffer, WorldContext);
			}
		}
	}

	FCall::~FCall()
	{
		if (Function && Buffer)
		{
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->DestroyValue_InContainer(Buffer);
			}
		}
		FMemory::Free(Buffer);
	}

	void* FCall::Param(const FString& Name, FProperty** OutProperty) const
	{
		if (!Function)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (MatchesReadableName(*It, Name))
			{
				if (OutProperty)
				{
					*OutProperty = *It;
				}
				return It->ContainerPtrToValuePtr<void>(Buffer);
			}
		}
		UE_LOG(LogKeepersReflection, Warning, TEXT("%s has no parameter %s"), *Function->GetName(), *Name);
		return nullptr;
	}

	bool FCall::SetRaw(const FString& Name, const void* Value)
	{
		FProperty* Property = nullptr;
		void* Ptr = Param(Name, &Property);
		if (!Ptr)
		{
			return false;
		}
		Property->CopyCompleteValue(Ptr, Value);
		return true;
	}

	bool FCall::Call()
	{
		if (!Function || !Target)
		{
			return false;
		}
		Target->ProcessEvent(Function, Buffer);
		return true;
	}
}
