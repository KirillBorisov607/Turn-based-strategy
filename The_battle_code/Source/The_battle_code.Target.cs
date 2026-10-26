using UnrealBuildTool;

public class The_battle_codeTarget : TargetRules
{
	public The_battle_codeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("The_battle_code");
	}
}
