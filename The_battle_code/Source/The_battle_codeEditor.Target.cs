using UnrealBuildTool;

public class The_battle_codeEditorTarget : TargetRules
{
	public The_battle_codeEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("The_battle_code");
	}
}
