using UnrealBuildTool;
using System.Collections.Generic;

public class RokTarget : TargetRules
{
	public RokTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;
		ExtraModuleNames.Add("Rok");
	}
}
