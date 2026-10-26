using UnrealBuildTool;

// The game's own module is empty: the gameplay code lives in Plugins/KeepersGame. The module exists so that
// a packaged game is built as a project executable that includes the project's code plugins.
public class The_battle_code : ModuleRules
{
	public The_battle_code(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
