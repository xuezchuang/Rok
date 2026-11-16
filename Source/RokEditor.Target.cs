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
		// Allow the Rok gameplay module to compile to a suffixed DLL while the editor is open.
		// RokRendering is a non-game module; global shader changes still require a full restart.
		bAllowHotReload = true;
		ExtraModuleNames.Add("Rok");
	}
}
