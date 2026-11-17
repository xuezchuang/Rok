using UnrealBuildTool;

// Editor-only tooling that turns the C++-built Rok UI into Widget Blueprints.
// Runs headless (UnrealEditor-Cmd -NullRHI -ExecCmds=...); see Scripts/RokUi/.
public class RokUiTools : ModuleRules
{
	public RokUiTools(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UMG",
			"UMGEditor",
			"UnrealEd",
			"Kismet",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"Rok"
		});
	}
}
