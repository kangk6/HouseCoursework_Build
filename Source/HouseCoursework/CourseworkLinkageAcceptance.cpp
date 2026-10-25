#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

void ACourseworkHUD::TickLinkageAcceptance()
{
    if(!bLightingReady||!Daylight)return;
    struct FRun
    {
        int32 Stage=0,Case=0,Phase=0;double At=0,Started=0,MaxReadError=0,ClearPV=0,ClearRead=0,Hour=0,ApplianceSeconds=0,MeterSeconds=0,Stamp=0;
        bool DarkMatches=true;FCourseworkWeather SavedWeather;TArray<bool> DeviceOn;TArray<bool> Rooms;TArray<float> Levels;TArray<FString> Samples;
    };static FRun R;const double Now=GetWorld()->GetRealTimeSeconds();const FName Desk(TEXT("SM_TableLamp_A1_32"));
    auto Check=[&](FString Name,bool Good){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Good?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("LINKAGE_CHECK %s"),*TestResults.Last());};
    auto Finish=[&](){FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Actual rendered sun sampled during seeks; active appliances/weather and native pause/fire restoration matrix\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("],\"samples\":[")+FString::Join(R.Samples,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("LinkageAcceptance.json")));TestMode.Empty();QuitApplication();};
    auto Capture=[&](){R.SavedWeather=Weather;R.Hour=Clock.Hour;R.ApplianceSeconds=ApplianceSeconds;R.MeterSeconds=SolarMeter.Seconds;R.Stamp=GetWorld()->GetTimeSeconds();R.DeviceOn.Empty();for(const auto& D:Appliances)R.DeviceOn.Add(D.On);R.Rooms=RoomLightOn;R.Levels=RoomBrightness;};
    auto SameWeather=[&](){const auto& W=R.SavedWeather;return Weather.Kind==W.Kind&&Weather.Automatic==W.Automatic&&Weather.Cover==W.Cover&&Weather.BlendAge==W.BlendAge&&Weather.Phase==W.Phase&&Weather.Hold==W.Hold&&Weather.Random.GetCurrentSeed()==W.Random.GetCurrentSeed();};
    auto SameDevices=[&](){if(Appliances.Num()!=R.DeviceOn.Num())return false;for(int32 I=0;I<Appliances.Num();++I)if(Appliances[I].On!=R.DeviceOn[I])return false;return true;};
    auto Balance=[&](){return FMath::Abs(SolarFlow.Generation+SolarFlow.Import-SolarFlow.Load-SolarFlow.Export)<.000001&&FMath::Abs(SolarFlow.Load-GetHouseholdWatts())<.001;};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=R.Started=Now;SetPanel(0);auto* PC=GetOwningPlayerController();auto* P=Cast<ACharacter>(PC->GetPawn());P->GetCharacterMovement()->DisableMovement();P->SetActorEnableCollision(false);P->SetActorLocation(FVector(-1840,10530,413));PC->SetControlRotation((FVector(-1734,10661,440)-P->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);
        SetTimeFlowing(false);SetDaylightHour(12);Daylight->Snap(12);Weather.Initialize(2471,false);SetAllRoomLighting(true);for(int32 I=0;I<Appliances.Num();++I)if(!Appliances[I].On)ToggleAppliance(I);SetSmartLightingScene(2);return;
    }
    if(Now-R.Started>170){Check(TEXT("completed_before_timeout"),false);Finish();return;}
    if(R.Stage==1||R.Stage==3)
    {
        const float Actual=Daylight->GetRenderedElevation();const float Expected=FMath::Lerp(.85f,.30f,FMath::Clamp(Actual/50.f,0.f,1.f)*GetWeatherDaylight());
        R.MaxReadError=FMath::Max(R.MaxReadError,double(FMath::Abs(GetSmartTargetLevel(Desk)-Expected)));R.DarkMatches&=NeedsNightLighting()==(Actual<6);
        if(R.Samples.Num()<200)R.Samples.Add(FString::Printf(TEXT("{\"target_hour\":%.4f,\"rendered_hour\":%.4f,\"actual_elevation\":%.4f,\"reading_target\":%.4f,\"expected_reading\":%.4f}"),Clock.Hour,Daylight->GetRenderedHour(),Actual,GetSmartTargetLevel(Desk),Expected));
        if(Now-R.At<2.5)return;
        Check(R.Stage==1?TEXT("reading_tracks_rendered_sunset_during_seek"):TEXT("reading_tracks_rendered_sunrise_during_seek"),R.MaxReadError<.015);
        Check(R.Stage==1?TEXT("night_detection_tracks_rendered_sunset"):TEXT("night_detection_tracks_rendered_sunrise"),R.DarkMatches);R.MaxReadError=0;R.DarkMatches=true;++R.Stage;R.At=Now;return;
    }
    if(R.Stage<7)
    {
        if(Now-R.At<(R.Stage==6?12.:1.5))return;R.At=Now;
        switch(R.Stage)
        {
        case 0:Check(TEXT("reading_presence_active"),bSmartReadingOccupied);Check(TEXT("all_appliances_counted_with_reading"),GetApplianceWatts()>=362&&Balance());SetDaylightHour(0);R.Stage=1;break;
        case 2:Check(TEXT("night_has_grid_supply_and_reading_supplement"),SolarFlow.Generation==0&&SolarFlow.Import>0&&GetSmartFixtureLevel(Desk)>.83);SetDaylightHour(12);R.Stage=3;break;
        case 4:R.ClearPV=SolarFlow.Generation;R.ClearRead=GetSmartFixtureLevel(Desk);ChooseWeather(2);R.Stage=6;break;
        case 6:Check(TEXT("overcast_increases_actual_reading_supplement"),GetSmartFixtureLevel(Desk)>R.ClearRead+.2);Check(TEXT("overcast_decreases_pv_at_same_time"),SolarFlow.Generation<R.ClearPV*.5&&SolarFlow.Generation>0&&Clock.Hour==12&&Balance());R.Stage=7;R.Phase=0;break;
        }return;
    }
    if(R.Case>=8){Check(TEXT("integrated_energy_remains_balanced"),FMath::Abs(SolarMeter.Generation+SolarMeter.Import-SolarMeter.Load-SolarMeter.Export)<.000001);Finish();return;}
    const bool Flow=(R.Case&1)!=0,Auto=(R.Case&2)!=0;const int32 Scene=R.Case<4?2:1;
    auto C=[&](const TCHAR* Name,bool Good){Check(FString::Printf(TEXT("case_%02d_%s"),R.Case,Name),Good);};
    if(Now-R.At<(R.Phase==1||R.Phase==5?2.:1.))return;R.At=Now;
    switch(R.Phase++)
    {
    case 0:SetPanel(0);SetSmartLightingScene(0);SetTimeFlowing(Flow);SetDaylightHour(Scene==2?12:21);Daylight->Snap(Scene==2?12:21);Weather.Initialize(2471+R.Case,false);Weather.Choose(1);Weather.SetAutomatic(Auto);SetSmartLightingScene(Scene);SetSmartFixtureManual(Desk,.42f);break;
    case 1:Capture();SetPanel(3);break;
    case 2:C(TEXT("help_freezes_weather_clock_and_meter"),SameWeather()&&Clock.Hour==R.Hour&&ApplianceSeconds==R.ApplianceSeconds&&SolarMeter.Seconds==R.MeterSeconds);SetPanel(0);TriggerKitchenAlarm();Capture();break;
    case 3:C(TEXT("fire_preserves_active_weather_and_devices"),IsSafetyDrillRunning()&&SameWeather()&&SameDevices()&&Clock.Hour==R.Hour&&ApplianceSeconds==R.ApplianceSeconds&&SolarMeter.Seconds==R.MeterSeconds);C(TEXT("fire_blocks_time_weather_and_device_controls"),!SetDaylightHour(3)&&!ToggleAppliance(0));ChooseWeather(2);ToggleAutomaticWeather();C(TEXT("blocked_weather_actions_do_not_consume_rng"),SameWeather());SetPanel(2);break;
    case 4:C(TEXT("pause_during_fire_keeps_suspended_state"),SameWeather()&&Clock.Hour==R.Hour&&SolarMeter.Seconds==R.MeterSeconds);SetPanel(7);ResetKitchenAlarm();C(TEXT("fire_restores_scene_override_and_user_configuration"),SmartLightingScene==Scene&&SmartFixtureOverrides.Contains(Desk)&&FMath::IsNearlyEqual(GetSmartFixtureLevel(Desk),.42f,.001f)&&RoomLightOn==R.Rooms&&RoomBrightness==R.Levels&&SameDevices()&&SameWeather()&&Clock.bRunning==Flow);Capture();SetPanel(8);break;
    case 5:C(TEXT("resume_has_no_clock_or_energy_backfill"),ApplianceSeconds>R.ApplianceSeconds&&FMath::Abs(ApplianceSeconds-R.ApplianceSeconds-(GetWorld()->GetTimeSeconds()-R.Stamp)*1440./Clock.DayMinutes)<.03&&Balance());C(TEXT("weather_resumes_in_original_mode"),Weather.Automatic==Auto&&(Flow?Weather.Phase>R.SavedWeather.Phase:Weather.Phase==R.SavedWeather.Phase)&&(Flow?Clock.Hour>R.Hour:Clock.Hour==R.Hour));C(TEXT("manual_override_survives_weather_resumption"),FMath::IsNearlyEqual(GetSmartFixtureLevel(Desk),.42f,.001f));++R.Case;R.Phase=0;break;
    }
}
