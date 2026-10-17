using UnrealBuildTool;

public class RokRendering : ModuleRules
{
	public RokRendering(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "RHI" });
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CoreUObject", "Engine", "RenderCore", "Renderer", "Projects"
		});
	}
}
