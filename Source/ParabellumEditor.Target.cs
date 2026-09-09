using UnrealBuildTool;

public class ParabellumEditorTarget : TargetRules
{
	public ParabellumEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Parabellum");
	}
}
