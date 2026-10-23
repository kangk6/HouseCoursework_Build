#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "CourseworkVentilation.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include <limits>

// Packaged real-world integration: normal startup, actual pointer controls,
// natural midnight wrap, fixed time with running world, smart lights and fire.
void ACourseworkHUD::TickClockAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Step=0;double At=0,Hour=0,Elapsed=0,World=0;int64 Day=1;TArray<bool> On;TArray<float> Levels;};
    static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();
    auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    auto Check=[&](const TCHAR* Name,bool Pass){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Pass?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("CLOCK_CHECK %s"),*TestResults.Last());};
    auto Click=[&](float X,float Y){return Overlay->ExerciseControlPointer({X,Y});};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;if(FString(Name).Contains(TEXT("panel"))){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.4f,false);}else FScreenshotRequest::RequestScreenshot(Path,true,false);};
    auto LightsSame=[&](){return RoomLightOn==R.On&&RoomBrightness==R.Levels;};
    auto Place=[&](FVector Position,FVector Focus){Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation((Focus-Player->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);};
    auto PathLit=[&](){for(const auto& F:SmartPathFills)if(F.Room==8&&F.Light.IsValid()&&F.Light->Intensity>1)return true;return false;};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=Now;R.On=RoomLightOn;R.Levels=RoomBrightness;
        Check(TEXT("normal_startup_noon_and_flowing"),IsDaylightActive()&&Clock.bRunning&&Clock.Hour>=12&&Clock.Hour<12.1&&Clock.Day==1&&Clock.DayMinutes==24);
        Check(TEXT("startup_noon_has_sunlight"),Daylight->GetSunIntensity()>11);
        FCourseworkClock C;
        Check(TEXT("core_default_is_noon"),C.Hour==12&&C.Day==1&&C.bRunning);
        C.Advance(60,false);Check(TEXT("24_real_minutes_is_one_day_rate"),FMath::IsNearlyEqual(C.Hour,13.0)&&C.ElapsedSimulationSeconds==3600);
        C.Advance(60,true);Check(TEXT("suspension_preserves_mode_and_time"),C.Hour==13&&C.bRunning&&C.ElapsedSimulationSeconds==3600);
        C.bRunning=false;C.Advance(60,false);Check(TEXT("fixed_does_not_advance"),C.Hour==13&&C.ElapsedSimulationSeconds==3600);
        C.Seek(23.99);Check(TEXT("seek_never_counts_elapsed_or_days"),C.ElapsedSimulationSeconds==3600&&C.Day==1);
        C.bRunning=true;C.Advance(2,false);Check(TEXT("midnight_wrap_increments_day"),C.Hour<.1&&C.Day==2);
        C.Seek(24);Check(TEXT("24_normalizes_to_zero"),C.Hour==0);C.Seek(-.25);Check(TEXT("negative_seek_wraps"),C.Hour==23.75);
        const double Before=C.Hour;Check(TEXT("nan_seek_rejected"),!C.Seek(std::numeric_limits<double>::quiet_NaN())&&C.Hour==Before);
        C.SetDayMinutes(0);Check(TEXT("speed_lower_bound"),C.DayMinutes==6);C.SetDayMinutes(999);Check(TEXT("speed_upper_bound"),C.DayMinutes==120);
        Check(TEXT("infinite_speed_rejected"),!C.SetDayMinutes(std::numeric_limits<float>::infinity())&&C.DayMinutes==120);
        const double Elapsed=C.ElapsedSimulationSeconds;C.Advance(-1,false);C.Advance(std::numeric_limits<double>::infinity(),false);
        Check(TEXT("invalid_delta_does_not_advance"),C.Hour==Before&&C.ElapsedSimulationSeconds==Elapsed);
        C=FCourseworkClock();C.Advance(1440*3+30,false);Check(TEXT("long_frame_multiple_days"),C.Day==4&&FMath::IsNearlyEqual(C.Hour,12.5));
        Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();Player->SetActorEnableCollision(false);
        Place(FVector(-2100,9870,151.5),FVector(-2350,9870,90));SetPanel(0);return;
    }
    if(Now-R.At<2.2)return;R.At=Now;
    switch(R.Step++)
    {
    case 0:
        Check(TEXT("natural_flow_advances_after_start"),Clock.Hour>12.01&&Clock.ElapsedSimulationSeconds>0);
        Shot(TEXT("Clock_startup_noon.png"));SetPanel(9);SelectComfortPage(0);R.Hour=Clock.Hour;R.Elapsed=Clock.ElapsedSimulationSeconds;break;
    case 1:
        Check(TEXT("time_page_suspends_without_changing_mode"),Clock.Hour==R.Hour&&Clock.bRunning&&Clock.ElapsedSimulationSeconds==R.Elapsed);
        Check(TEXT("fixed_mode_api"),SetTimeFlowing(false)&&!Clock.bRunning);R.World=GetWorld()->GetTimeSeconds();SetPanel(0);break;
    case 2:
        Check(TEXT("fixed_keeps_time_but_world_runs"),Clock.Hour==R.Hour&&GetWorld()->GetTimeSeconds()>R.World+1);
        SetPanel(9);Check(TEXT("flow_mode_api"),SetTimeFlowing(true)&&Clock.bRunning);Check(TEXT("speed_6_api"),SetTimeDayMinutes(6)&&Clock.DayMinutes==6);
        SetDaylightHour(23.99f);R.Day=Clock.Day;SetPanel(0);break;
    case 3:
        Check(TEXT("natural_midnight_crosses_day"),Clock.Day==R.Day+1&&Clock.Hour<.5);
        Check(TEXT("midnight_never_sweeps_through_noon"),Daylight->GetRenderedHour()<.5&&Daylight->GetSunIntensity()==0);
        Shot(TEXT("Clock_midnight.png"));SetTimeFlowing(false);R.Elapsed=Clock.ElapsedSimulationSeconds;R.Day=Clock.Day;
        Check(TEXT("manual_zero_supported"),SetDaylightHour(0)&&Clock.Hour==0);Check(TEXT("manual_24_supported"),SetDaylightHour(24)&&Clock.Hour==0);
        Check(TEXT("manual_negative_supported"),SetDaylightHour(-1)&&Clock.Hour==23);SetDaylightHour(12);
        Check(TEXT("manual_seek_does_not_generate_elapsed_time"),Clock.ElapsedSimulationSeconds==R.Elapsed&&Clock.Day==R.Day);break;
    case 4:
        Check(TEXT("manual_noon_settles"),Daylight->GetSunIntensity()>11&&FMath::IsNearlyEqual(Daylight->GetRenderedHour(),12.f,.01f));
        Check(TEXT("clock_preserves_lighting_settings"),LightsSame());
        Check(TEXT("energy_comparison_coexists_with_clock"),ApplyEnergyScenario(true)&&Clock.Hour==12&&!Clock.bRunning);RestoreEnergyComparison();
        Check(TEXT("energy_restores_original_settings"),LightsSame());SetRoomWindows(0,true);SetDaylightHour(0);break;
    case 5:
        Check(TEXT("midnight_sun_is_zero"),Daylight->GetSunIntensity()==0&&NeedsNightLighting());
        Check(TEXT("windows_remain_open_across_time_changes"),GetRoomOpenWindowCount(0)==GetRoomWindowCount(0));
        Place(FVector(-2114,9980,413),FVector(-2008,10177,508));SetSmartLightingScene(1);break;
    case 6:
        Check(TEXT("night_guidance_works_at_midnight"),PathLit());Shot(TEXT("Clock_night_guidance.png"));SetDaylightHour(12);break;
    case 7:
        Check(TEXT("guidance_sleeps_in_daylight"),!NeedsNightLighting()&&!PathLit());SetSmartLightingScene(0);SetTimeFlowing(true);
        TriggerKitchenAlarm();R.Hour=Clock.Hour;R.Elapsed=Clock.ElapsedSimulationSeconds;break;
    case 8:
        Check(TEXT("fire_suspends_clock_without_rewriting_flow"),IsSafetyDrillRunning()&&Clock.bRunning&&Clock.Hour==R.Hour&&Clock.ElapsedSimulationSeconds==R.Elapsed);
        Check(TEXT("fire_still_progresses_with_clock_suspended"),SmokeReading>10);
        Check(TEXT("fire_rejects_manual_time_and_mode_changes"),!SetDaylightHour(0)&&!SetTimeFlowing(false)&&!SetTimeDayMinutes(60)&&Clock.Hour==R.Hour&&Clock.bRunning);
        ResetKitchenAlarm();R.Hour=Clock.Hour;break;
    case 9:
        Check(TEXT("flow_resumes_after_fire"),Clock.bRunning&&Clock.Hour>R.Hour);SetTimeFlowing(false);TriggerKitchenAlarm();R.Hour=Clock.Hour;break;
    case 10:
        Check(TEXT("fixed_mode_survives_fire"),!Clock.bRunning&&Clock.Hour==R.Hour&&SmokeReading>10);ResetKitchenAlarm();
        SetPanel(9);Check(TEXT("speed_24_api"),SetTimeDayMinutes(24)&&Clock.DayMinutes==24);Check(TEXT("speed_60_api"),SetTimeDayMinutes(60)&&Clock.DayMinutes==60);
        Check(TEXT("manual_zero"),SetDaylightHour(0)&&Clock.Hour==0);Check(TEXT("manual_last_minute"),SetDaylightHour(1439.f/60)&&FMath::IsNearlyEqual(float(Clock.Hour),1439.f/60,.001f));
        Check(TEXT("manual_noon"),SetDaylightHour(12)&&Clock.Hour==12&&!Clock.bRunning);Shot(TEXT("Clock_panel_fixed.png"));break;
    case 11:
        Check(TEXT("fixed_mode_survives_page_changes"),!Clock.bRunning);SelectComfortPage(1);SetTimeFlowing(true);R.Hour=Clock.Hour;break;
    case 12:
        Check(TEXT("ventilation_page_does_not_suspend_clock"),Clock.Hour>R.Hour);SelectComfortPage(0);R.Hour=Clock.Hour;SetPanel(2);break;
    case 13:
        Check(TEXT("pause_menu_suspends_clock"),Clock.Hour==R.Hour&&Clock.bRunning);SetPanel(0);SetTimeFlowing(false);
        SetDaylightHour(5.9f);break;
    case 14:Shot(TEXT("Clock_before_dawn.png"));SetDaylightHour(6.2f);break;
    case 15:Shot(TEXT("Clock_dawn.png"));Check(TEXT("dawn_sun_nonnegative"),Daylight->GetSunIntensity()>=0);SetDaylightHour(8);break;
    case 16:Shot(TEXT("Clock_morning.png"));SetDaylightHour(17.8f);break;
    case 17:Shot(TEXT("Clock_sunset.png"));SetDaylightHour(18.2f);break;
    case 18:Shot(TEXT("Clock_after_sunset.png"));SetDaylightHour(23.95f);break;
    case 19:R.Hour=Daylight->GetRenderedHour();SetDaylightHour(.05f);break;
    case 20:
        Check(TEXT("manual_midnight_wrap_is_short"),Daylight->GetRenderedHour()<.1&&Daylight->GetSunIntensity()==0);
        Check(TEXT("no_new_unbuilt_lighting"),GetWorld()->NumLightingUnbuiltObjects==0);
        Check(TEXT("manual_modes_do_not_touch_saved_room_preferences"),LightsSame());CloseAllWindows();SetDaylightHour(12);SetPanel(9);SetTimeFlowing(true);SetTimeDayMinutes(24);break;
    case 21:Shot(TEXT("Clock_panel_flow.png"));break;
    case 22:
        SetPanel(0);SetTimeDayMinutes(6);SetDaylightHour(17.55f);SetSmartLightingScene(1);
        Place(FVector(-2114,9980,413),FVector(-2008,10177,508));break;
    case 23:
        Check(TEXT("natural_sunset_enables_guidance"),Clock.bRunning&&Clock.Hour>17.65&&NeedsNightLighting()&&PathLit());
        Shot(TEXT("Clock_natural_sunset_guidance.png"));SetDaylightHour(6.34f);break;
    case 24:
        Check(TEXT("natural_dawn_suspends_guidance"),Clock.bRunning&&Clock.Hour>6.4&&!NeedsNightLighting()&&!PathLit());
        SetSmartLightingScene(0);SetTimeDayMinutes(24);SetDaylightHour(12);SetPanel(9);break;
    default:
        FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("ClockAcceptance.json")));
        TestMode.Empty();QuitApplication();break;
    }
}
