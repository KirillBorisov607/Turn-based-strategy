#include "BlueprintExportCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/UserDefinedEnum.h"
#include "HAL/FileManager.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintExport, Log, All);

namespace BlueprintExport
{
	static FString OneLine(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("\r"), TEXT(""));
		Out.ReplaceInline(TEXT("\n"), TEXT(" | "));
		return Out;
	}

	static FString Clip(const FString& In, int32 Max = 300)
	{
		return In.Len() > Max ? In.Left(Max) + TEXT("...") : In;
	}

	static FString PinTypeText(const FEdGraphPinType& Type)
	{
		return UEdGraphSchema_K2::TypeToText(Type).ToString();
	}

	static FString PinDefault(const UEdGraphPin* Pin)
	{
		if (Pin->DefaultObject)
		{
			return Pin->DefaultObject->GetPathName();
		}
		if (!Pin->DefaultTextValue.IsEmpty())
		{
			return Pin->DefaultTextValue.ToString();
		}
		return Pin->DefaultValue;
	}

	/** Writes one graph as a readable list of nodes, their pins and links. */
	static void WriteGraph(const UEdGraph* Graph, const FString& Kind, FString& Out)
	{
		TMap<const UEdGraphNode*, int32> Ids;
		for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
		{
			Ids.Add(Graph->Nodes[Index], Index);
		}

		Out += FString::Printf(TEXT("\n== %s: %s (%d nodes)\n"), *Kind, *Graph->GetName(), Graph->Nodes.Num());

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			const int32 Id = Ids.FindChecked(Node);

			if (const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
			{
				Out += FString::Printf(TEXT("[N%d] // %s\n"), Id, *OneLine(Comment->NodeComment));
				continue;
			}

			Out += FString::Printf(TEXT("[N%d] %s  <%s>\n"), Id,
				*OneLine(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString()),
				*Node->GetClass()->GetName());
			if (!Node->NodeComment.IsEmpty())
			{
				Out += FString::Printf(TEXT("      // %s\n"), *OneLine(Node->NodeComment));
			}

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->bHidden)
				{
					continue;
				}
				const bool bInput = Pin->Direction == EGPD_Input;
				const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;

				TArray<FString> Links;
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNodeUnchecked())
					{
						const int32* LinkedId = Ids.Find(Linked->GetOwningNode());
						Links.Add(FString::Printf(TEXT("N%s.%s"),
							LinkedId ? *FString::FromInt(*LinkedId) : TEXT("?"),
							*Linked->PinName.ToString()));
					}
				}

				FString Line = FString::Printf(TEXT("      %s %s"),
					bInput ? TEXT("in ") : TEXT("out"), *Pin->PinName.ToString());
				if (!bExec)
				{
					Line += FString::Printf(TEXT(" : %s"), *PinTypeText(Pin->PinType));
				}
				if (Links.Num() > 0)
				{
					Line += FString::Printf(TEXT(" %s %s"), bInput ? TEXT("<-") : TEXT("->"), *FString::Join(Links, TEXT(", ")));
				}
				else if (bInput && !bExec)
				{
					const FString Default = PinDefault(Pin);
					if (!Default.IsEmpty())
					{
						Line += FString::Printf(TEXT(" = %s"), *Clip(OneLine(Default)));
					}
				}
				else if (bExec && !bInput)
				{
					// Unconnected exec outputs are noise.
					continue;
				}
				Out += Line + TEXT("\n");
			}
		}
	}

	static FString DescribeBlueprint(UBlueprint* Blueprint, bool bWriteT3D, TMap<FString, FString>& OutT3D)
	{
		FString Out;
		Out += FString::Printf(TEXT("Blueprint: %s\n"), *Blueprint->GetPathName());
		Out += FString::Printf(TEXT("Parent: %s\n"), Blueprint->ParentClass ? *Blueprint->ParentClass->GetPathName() : TEXT("None"));
		Out += FString::Printf(TEXT("Type: %s\n"), *StaticEnum<EBlueprintType>()->GetNameStringByValue(Blueprint->BlueprintType));

		if (Blueprint->ImplementedInterfaces.Num() > 0)
		{
			Out += TEXT("Interfaces:");
			for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
			{
				Out += FString::Printf(TEXT(" %s"), Interface.Interface ? *Interface.Interface->GetName() : TEXT("?"));
			}
			Out += TEXT("\n");
		}

		if (Blueprint->SimpleConstructionScript)
		{
			Out += TEXT("Components:\n");
			for (const USCS_Node* SCSNode : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (SCSNode)
				{
					Out += FString::Printf(TEXT("  %s : %s (parent %s)\n"),
						*SCSNode->GetVariableName().ToString(),
						SCSNode->ComponentClass ? *SCSNode->ComponentClass->GetName() : TEXT("?"),
						*SCSNode->ParentComponentOrVariableName.ToString());
				}
			}
		}

		UObject* DefaultObject = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject(false) : nullptr;
		Out += TEXT("Variables:\n");
		for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
		{
			FString Default;
			if (DefaultObject)
			{
				if (const FProperty* Property = Blueprint->GeneratedClass->FindPropertyByName(Variable.VarName))
				{
					Property->ExportTextItem_Direct(Default, Property->ContainerPtrToValuePtr<void>(DefaultObject), nullptr, nullptr, PPF_None);
				}
			}
			Out += FString::Printf(TEXT("  %s : %s%s%s\n"),
				*Variable.VarName.ToString(),
				*PinTypeText(Variable.VarType),
				Variable.Category.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" [%s]"), *Variable.Category.ToString()),
				Default.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" = %s"), *Clip(OneLine(Default))));
		}

		auto WriteGraphs = [&](const TArray<TObjectPtr<UEdGraph>>& Graphs, const TCHAR* Kind)
		{
			for (const UEdGraph* Graph : Graphs)
			{
				if (!Graph)
				{
					continue;
				}
				WriteGraph(Graph, Kind, Out);
				if (bWriteT3D)
				{
					TSet<UObject*> Nodes;
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						Nodes.Add(Node);
					}
					FString Text;
					FEdGraphUtilities::ExportNodesToText(Nodes, Text);
					OutT3D.Add(FString::Printf(TEXT("%s_%s"), Kind, *Graph->GetName()), Text);
				}
			}
		};
		WriteGraphs(Blueprint->UbergraphPages, TEXT("EventGraph"));
		WriteGraphs(Blueprint->FunctionGraphs, TEXT("Function"));
		WriteGraphs(Blueprint->MacroGraphs, TEXT("Macro"));
		for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			WriteGraphs(Interface.Graphs, TEXT("InterfaceFunction"));
		}
		return Out;
	}

	static FString DescribeStruct(UUserDefinedStruct* Struct)
	{
		FString Out = FString::Printf(TEXT("Struct: %s\n"), *Struct->GetPathName());
		for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(Struct))
		{
			Out += FString::Printf(TEXT("  %s : %s%s\n"),
				*Var.FriendlyName,
				*PinTypeText(Var.ToPinType()),
				Var.DefaultValue.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" = %s"), *Clip(Var.DefaultValue)));
		}
		return Out;
	}

	static FString DescribeEnum(UUserDefinedEnum* Enum)
	{
		FString Out = FString::Printf(TEXT("Enum: %s\n"), *Enum->GetPathName());
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			Out += FString::Printf(TEXT("  %d %s = %s\n"), Index,
				*Enum->GetNameStringByIndex(Index), *Enum->GetDisplayNameTextByIndex(Index).ToString());
		}
		return Out;
	}
}

UBlueprintExportCommandlet::UBlueprintExportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UBlueprintExportCommandlet::Main(const FString& Params)
{
	FString OutDir;
	if (!FParse::Value(*Params, TEXT("out="), OutDir))
	{
		UE_LOG(LogBlueprintExport, Error, TEXT("Pass -out=<folder>"));
		return 1;
	}
	FString ContentPath = TEXT("/Game");
	FParse::Value(*Params, TEXT("path="), ContentPath);
	const bool bWriteT3D = FParse::Param(*Params, TEXT("t3d"));

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);

	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*ContentPath), Assets, true);

	int32 Written = 0;
	for (const FAssetData& AssetData : Assets)
	{
		const FString PackagePath = AssetData.PackageName.ToString();
		const FString RelativePath = PackagePath.RightChop(ContentPath.Len());
		const FString BaseFile = FPaths::Combine(OutDir, RelativePath);

		UObject* Asset = nullptr;
		const FTopLevelAssetPath ClassPath = AssetData.AssetClassPath;
		const FString ClassName = ClassPath.GetAssetName().ToString();
		const bool bInteresting = ClassName.Contains(TEXT("Blueprint")) || ClassName == TEXT("UserDefinedStruct")
			|| ClassName == TEXT("UserDefinedEnum") || ClassName == TEXT("DataTable");
		if (!bInteresting)
		{
			continue;
		}
		Asset = AssetData.GetAsset();
		if (!Asset)
		{
			UE_LOG(LogBlueprintExport, Warning, TEXT("Could not load %s"), *PackagePath);
			continue;
		}

		FString Text;
		TMap<FString, FString> T3D;
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			Text = BlueprintExport::DescribeBlueprint(Blueprint, bWriteT3D, T3D);
		}
		else if (UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(Asset))
		{
			Text = BlueprintExport::DescribeStruct(Struct);
		}
		else if (UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(Asset))
		{
			Text = BlueprintExport::DescribeEnum(Enum);
		}
		else if (UDataTable* Table = Cast<UDataTable>(Asset))
		{
			Text = FString::Printf(TEXT("DataTable: %s\nRowStruct: %s\n\n%s"), *Table->GetPathName(),
				Table->GetRowStruct() ? *Table->GetRowStruct()->GetPathName() : TEXT("?"), *Table->GetTableAsCSV());
		}
		else
		{
			continue;
		}

		FFileHelper::SaveStringToFile(Text, *(BaseFile + TEXT(".txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		for (const TPair<FString, FString>& Pair : T3D)
		{
			FFileHelper::SaveStringToFile(Pair.Value, *(BaseFile + TEXT(".") + Pair.Key + TEXT(".t3d")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
		++Written;
	}

	UE_LOG(LogBlueprintExport, Display, TEXT("BlueprintExport: wrote %d assets to %s"), Written, *OutDir);
	return 0;
}
