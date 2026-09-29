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
    }
}
