#include "KeepersEditorLibrary.h"

#include "Engine/UserDefinedEnum.h"
#include "Kismet2/EnumEditorUtils.h"

int32 UKeepersEditorLibrary::AddUserEnumEntry(UUserDefinedEnum* Enum, const FString& DisplayName)
{
	if (!Enum)
	{
		return INDEX_NONE;
	}

	// The last entry is the hidden _MAX.
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
	{
		if (Enum->GetDisplayNameTextByIndex(Index).ToString() == DisplayName)
		{
			return Index;
		}
	}

	FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(Enum);
	const int32 NewIndex = Enum->NumEnums() - 2;
	FEnumEditorUtils::SetEnumeratorDisplayName(Enum, NewIndex, FText::FromString(DisplayName));
	return NewIndex;
}
