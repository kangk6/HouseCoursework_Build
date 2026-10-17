#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
// Aggregate fixture/circuit ratings, educational estimates rather than measurements.
constexpr float Watts[]={48,32,36,24,12,12,12,24,16};
constexpr float EcoLevels[]={.55f,.30f,.65f,.35f,.60f,.35f,.35f,.45f,.25f};
static_assert(UE_ARRAY_COUNT(Watts)==CourseworkRooms::Count,"Each lighting circuit needs a rating");
}
float ACourseworkHUD::GetRoomRatedWatts(int32 Room)const{return Watts[CourseworkRooms::Clamp(Room)];}
float ACourseworkHUD::GetRoomWatts(int32 Room)const{return IsRoomLightOn(Room)?GetRoomRatedWatts(Room)*GetRoomBrightness(Room):0;}
float ACourseworkHUD::GetTotalWatts()const{float Total=0;for(int32 I=0;I<CourseworkRooms::Count;++I)Total+=GetRoomWatts(I);return Total;}
float ACourseworkHUD::GetBaselineWatts()const{float Total=0;for(float W:Watts)Total+=W;return Total;}
void ACourseworkHUD::SetEnergyHours(float Hours){if(FMath::IsFinite(Hours))EnergyHours=FMath::Clamp(Hours,1.f,24.f);}
float ACourseworkHUD::GetEstimatedKWh(bool Baseline)const{return (Baseline?GetBaselineWatts():GetTotalWatts())*EnergyHours/1000.f;}
FString ACourseworkHUD::GetEnergyScenarioName()const
{
    if(!bEnergyComparisonActive)return TEXT("当前配置");
    return EnergyScenario==1?TEXT("全开对比"):EnergyScenario==2?TEXT("节能对比"):TEXT("手动调整");
}
bool ACourseworkHUD::ApplyEnergyScenario(bool Eco)
{
    if(!bLightingReady||IsSafetyDrillRunning()){ShowToast(TEXT("疏散演练期间暂停能源方案切换"));return false;}
    SetSmartLightingScene(0);
    if(!bEnergyComparisonActive)
    {
        // Flush the real settings before touching preview state. A forced exit
        // during comparison must never replace the user's normal configuration.
        SaveRoomLighting();if(bLightingSaveFailed){ShowToast(TEXT("无法保存原配置，未开始对比"));return false;}
        EnergySavedOn=RoomLightOn;EnergySavedBrightness=RoomBrightness;
        GetWorld()->GetTimerManager().ClearTimer(LightingSaveTimer);bEnergyComparisonActive=true;
    }
    bApplyingEnergyScene=true;
    for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,true,Eco?EcoLevels[I]:1.f);
    bApplyingEnergyScene=false;EnergyScenario=Eco?2:1;
    return true;
}
bool ACourseworkHUD::RestoreEnergyComparison()
{
    if(!bEnergyComparisonActive)return false;
    bEnergyComparisonActive=false;EnergyScenario=0;
    for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,EnergySavedOn[I],EnergySavedBrightness[I]);
    EnergySavedOn.Empty();EnergySavedBrightness.Empty();return true;
}
void ACourseworkHUD::ToggleEnergyMapLayer()
{
    if(PanelMode!=5)return;
    if(IsSafetyDrillRunning()){bEnergyMapLayer=false;ShowToast(TEXT("演练期间优先显示疏散路线"));return;}
    bEnergyMapLayer=!bEnergyMapLayer;if(Overlay)Overlay->RefreshPanel();
}
void ACourseworkHUD::OpenEnergyMap()
{
    if(IsSafetyDrillRunning()){ShowToast(TEXT("演练期间优先显示疏散路线"));return;}
    bEnergyMapLayer=true;SelectedMapFloor=GetPlayerFloor();SetPanel(5);
}
