using UnrealBuildTool;

public class RenderDocMetalBootstrap : ModuleRules
{
    public RenderDocMetalBootstrap(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });
        PrivateDependencyModuleNames.Add("Projects");
    }
}
