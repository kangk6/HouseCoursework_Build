#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

void ACourseworkHUD::InitializeTimeOfDay()
{
    Clock=FCourseworkClock();
    if(!Daylight)Daylight=GetWorld()->SpawnActor<ACourseworkDaylight>();
    if(Daylight)Daylight->Snap(float(Clock.Hour));
    // Keep a real opaque cover through atmosphere registration, sky capture and
    // exposure warmup. A zero-duration StartCameraFade does not hold a fade.
    if(bStartupCover)StartupRevealAt=GetWorld()->GetRealTimeSeconds()+.9;
    UE_LOG(LogTemp,Display,TEXT("CLOCK_READY hour=12 day=1 flowing=1 real_minutes_per_day=24"));
}
bool ACourseworkHUD::IsTimeSuspended()const
{
    // Suspend advancement, never rewrite the user's flowing/fixed choice.
    return PanelMode==2||PanelMode==3||(PanelMode==9&&ComfortPage==0)||IsSafetyDrillRunning();
}
FString ACourseworkHUD::GetTimeStatus()const
{
    if(!Clock.bRunning)return TEXT("时间已固定");
    if(IsSafetyDrillRunning())return TEXT("演练期间暂缓流动");
    if(PanelMode==9&&ComfortPage==0)return TEXT("调整中，关闭此页后继续");
    if(PanelMode==2||PanelMode==3)return TEXT("关闭菜单后继续");
    return TEXT("时间正在流动");
}
void ACourseworkHUD::TickTimeOfDay(float Dt)
{
    if(!bLightingReady||!IsDaylightActive())return;
    Clock.Advance(Dt,IsTimeSuspended());
    Daylight->Apply(float(Clock.Hour));
}
bool ACourseworkHUD::SetTimeFlowing(bool Running)
{
    if(IsSafetyDrillRunning()){ShowToast(TEXT("演练结束后可调整时间"));return false;}
    if(!IsDaylightActive()&&!SetDaylightHour(float(Clock.Hour)))return false;
    Clock.bRunning=Running;
    UE_LOG(LogTemp,Display,TEXT("CLOCK_FLOW enabled=%d hour=%.5f"),Running,Clock.Hour);
    return true;
}
bool ACourseworkHUD::SetTimeDayMinutes(float Minutes)
{
    if(IsSafetyDrillRunning())return false;
    return Clock.SetDayMinutes(Minutes);
}
bool ACourseworkHUD::SetDaylightHour(float Hour)
{
    if(IsSafetyDrillRunning()){ShowToast(TEXT("疏散演练期间暂停时间调整"));return false;}
    if(!FMath::IsFinite(Hour))return false;
    if(!Daylight)Daylight=GetWorld()->SpawnActor<ACourseworkDaylight>();
    const bool Applied=Daylight&&Daylight->Apply(Hour);
    if(Applied)
    {
        Clock.Seek(Hour);
        UE_LOG(LogTemp,Display,TEXT("ENVIRONMENT_TIME target=%.3f elevation=%.3f smart_scene=%d"),GetDaylightHour(),GetDaylightElevation(),SmartLightingScene);
    }
    return Applied;
}
// Internal cleanup/legacy comparison reset only. The normal UI resets to noon.
void ACourseworkHUD::RestoreDaylight(){Clock.bRunning=false;if(Daylight)Daylight->Restore();}
bool ACourseworkHUD::IsDaylightActive()const{return Daylight&&Daylight->IsActive();}
float ACourseworkHUD::GetDaylightHour()const{return float(Clock.Hour);}
float ACourseworkHUD::GetDaylightElevation()const{return Daylight?Daylight->GetElevation():0.f;}
bool ACourseworkHUD::NeedsNightLighting()const{return !IsDaylightActive()||Daylight->GetRenderedElevation()<6.f;}
void ACourseworkHUD::ObserveDaylight()
{
    if(IsSafetyDrillRunning()){ShowToast(TEXT("演练期间请步行疏散"));return;}
    if(auto* PC=GetOwningPlayerController())if(auto* C=Cast<ACharacter>(PC->GetPawn()))
    {
        C->GetCharacterMovement()->StopMovementImmediately();
        if(C->TeleportTo(FVector(-2100,9870,151.5),FRotator(0,180,0),false,false))
        {PC->SetControlRotation(FRotator(-25,180,0));SetPanel(0);}
        else ShowToast(TEXT("观察位置被占用，请靠近客厅窗边查看"));
    }
}
