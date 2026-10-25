using UnrealBuildTool;

public class KeepersGame : ModuleRules
{
	public KeepersGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"Slate",
			"SlateCore",
			"InputCore",
			"MoviePlayer",
			"ImageCore"
		});
	}
}
