using UnrealBuildTool;

public class ParabellumTarget : TargetRules
{
	public ParabellumTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Parabellum");
	}
}
