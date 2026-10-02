using System.IO;
using UnrealBuildTool;

public class RenderDocMetalCapture : ModuleRules
{
    public RenderDocMetalCapture(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "RenderDoc"));
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "Projects", "RenderCore", "RHI", "Slate", "SlateCore",
            "ToolMenus", "UnrealEd"
        });
        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            // The layout probe uses this installed UE version's Metal RHI headers.
            PrivateDependencyModuleNames.AddRange(new[] { "MetalRHI", "RHICore" });
            PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "Apple", "MetalRHI", "Private"));
            AddEngineThirdPartyPrivateStaticDependencies(Target, "MetalCPP", "MetalShaderConverter");
        }
    }
}
