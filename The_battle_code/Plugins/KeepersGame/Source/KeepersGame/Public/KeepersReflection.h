#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

/**
 * Helpers for reading Blueprint variables and calling Blueprint functions from C++.
 *
 * The game's core systems (grid, units, combat, AI) are Blueprints, so C++ reaches them by name.
 * Members of user-defined structs are stored as "Name_12_GUID"; FindMember matches them by the
 * readable name before the first underscore-number suffix.
 */
namespace KeepersReflection
{
	/** Finds a property by its readable name, also matching user-defined struct members ("Spell_10_ABC..."). */
	KEEPERSGAME_API FProperty* FindMember(const UStruct* Struct, const FString& Name);

	/** Pointer to a property's value inside Container, or nullptr. */
	KEEPERSGAME_API void* FindValuePtr(const UStruct* Struct, void* Container, const FString& Name, FProperty** OutProperty = nullptr);

	/** Reads an object variable (BP_Unit "Grid", BP_UnitAi "Unit", ...). */
	KEEPERSGAME_API UObject* GetObject(const UObject* Object, const FString& Name);

	/** Reads an int / bool / FIntPoint variable from an object or a struct value. */
	KEEPERSGAME_API bool GetInt(const UStruct* Struct, const void* Container, const FString& Name, int32& OutValue);
	KEEPERSGAME_API bool GetBool(const UStruct* Struct, const void* Container, const FString& Name, bool& OutValue);
	KEEPERSGAME_API bool GetIntPoint(const UStruct* Struct, const void* Container, const FString& Name, FIntPoint& OutValue);
	KEEPERSGAME_API bool SetIntPoint(const UStruct* Struct, void* Container, const FString& Name, const FIntPoint& Value);

	/** Reads a byte-sized enum value (Blueprint enums) from a struct value. */
	KEEPERSGAME_API bool GetEnumByte(const UStruct* Struct, const void* Container, const FString& Name, uint8& OutValue);

	/**
	 * One call to a Blueprint function. Parameters are set and read by name.
	 * Works for member functions and for static functions of Blueprint function libraries
	 * (pass the library class default object and a world context object).
	 */
	class KEEPERSGAME_API FCall
	{
	public:
		FCall(UObject* InTarget, FName FunctionName, UObject* WorldContext = nullptr);
		~FCall();

		FCall(const FCall&) = delete;
		FCall& operator=(const FCall&) = delete;

		bool IsValid() const { return Function != nullptr; }

		/** Copies Value into the parameter; the caller guarantees the parameter has type T. */
		template <typename T>
		bool Set(const FString& Name, const T& Value)
		{
			FProperty* Property = nullptr;
			void* Ptr = Param(Name, &Property);
			if (!Ptr || Property->GetElementSize() != sizeof(T))
			{
				return false;
			}
			Property->CopyCompleteValue(Ptr, &Value);
			return true;
		}

		/** Copies a value of the parameter's own type (for Blueprint structs held as raw memory). */
		bool SetRaw(const FString& Name, const void* Value);

		bool Call();

		template <typename T>
		bool Get(const FString& Name, T& OutValue) const
		{
			FProperty* Property = nullptr;
			const void* Ptr = Param(Name, &Property);
			if (!Ptr || Property->GetElementSize() != sizeof(T))
			{
				return false;
			}
			Property->CopyCompleteValue(&OutValue, Ptr);
			return true;
		}

		/** Raw access to a parameter, for struct and array outputs. */
		void* Param(const FString& Name, FProperty** OutProperty = nullptr) const;

	private:
		UObject* Target = nullptr;
		UFunction* Function = nullptr;
		uint8* Buffer = nullptr;
	};
}
