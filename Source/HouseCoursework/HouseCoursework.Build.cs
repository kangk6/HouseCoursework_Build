using UnrealBuildTool;
public class HouseCoursework : ModuleRules
{
    public HouseCoursework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "Json", "RenderCore" });
        PrivateDependencyModuleNames.Add("ApplicationCore");
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "BlueprintGraph", "MeshDescription", "StaticMeshDescription" });
    }
}
