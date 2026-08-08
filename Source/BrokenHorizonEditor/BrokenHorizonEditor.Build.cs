using UnrealBuildTool;

public class BrokenHorizonEditor : ModuleRules
{
    public BrokenHorizonEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "BrokenHorizon",
            "Landscape",
            "Foliage"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd"
        });
    }
}
