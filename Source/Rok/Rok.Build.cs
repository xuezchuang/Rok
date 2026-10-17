using UnrealBuildTool;

public class Rok : ModuleRules
{
	public Rok(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

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
			"RokRendering",
			"ProceduralMeshComponent"
		});
	}
}
