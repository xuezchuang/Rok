using UnrealBuildTool;
using System.Collections.Generic;

public class RokEditorTarget : TargetRules
{
	public RokEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bOverrideBuildEnvironment = true;
		ExtraModuleNames.Add("Rok");
	}
}
