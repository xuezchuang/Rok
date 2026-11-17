using UnrealBuildTool;

public class Rok : ModuleRules
{
	public Rok(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		// Flat module layout: expose its headers to RokUiTools (editor WBP tooling).
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"HTTP",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"Slate",
			"SlateCore",
			"RHI",
			"RenderCore",
			"ImageWrapper",
			"ImageCore",
			"MediaAssets",
			"RokRendering",
			"ProceduralMeshComponent",
			"Sockets",
			"Networking"
		});
	}
}
