using UnrealBuildTool;

public class HyperScaleTests : ModuleRules
{
    public HyperScaleTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "HyperScaleRuntime",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Slate",
                "SlateCore", 
                "HyperScaleRuntime",
                "UnrealEd",
                "Networking",
                "NetCore",
                "Engine"
            }
        );
        
        // Include plugin dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "QuarkAPI"
        });
    }
}