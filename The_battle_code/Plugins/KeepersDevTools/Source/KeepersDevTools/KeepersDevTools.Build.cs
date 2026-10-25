using UnrealBuildTool;

public class KeepersDevTools : ModuleRules
{
	public KeepersDevTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"BlueprintGraph",
			"Kismet",
			"AssetRegistry",
			"Projects",
			"Slate",
			"SlateCore",
			"InputCore",
			"UMG",
			"RenderCore",
			"ToolMenus",
			"KeepersGame"
		});
	}
}
