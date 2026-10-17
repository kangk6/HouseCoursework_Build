using UnrealBuildTool;
public class HouseCourseworkTarget : TargetRules
{
    public HouseCourseworkTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V3;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
        ExtraModuleNames.Add("HouseCoursework");
    }
}
