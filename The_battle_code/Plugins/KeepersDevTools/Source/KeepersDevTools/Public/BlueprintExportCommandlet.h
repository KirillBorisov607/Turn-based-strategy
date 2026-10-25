#pragma once

#include "Commandlets/Commandlet.h"
#include "BlueprintExportCommandlet.generated.h"

/**
 * Dumps every Blueprint under a content path to readable text.
 *
 * Usage:
 *   UnrealEditor-Cmd.exe Project.uproject -run=BlueprintExport -out=C:/dump [-path=/Game]
 *
 * For each Blueprint it writes <out>/<package path>/<Name>.txt with the parent class,
 * variables, and every graph as a list of nodes with their pins and links.
 * With -t3d it also writes the raw copy/paste text of each graph next to it.
 */
UCLASS()
class UBlueprintExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBlueprintExportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
