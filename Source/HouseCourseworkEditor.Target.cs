using UnrealBuildTool;
public class HouseCourseworkEditorTarget : TargetRules
{
    public HouseCourseworkEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V3;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
        ExtraModuleNames.Add("HouseCoursework");
    }
}
