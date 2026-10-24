#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkVentilation.h"
#include "CourseworkOverlay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"

// Full rendered day at the existing six-real-minutes/day test speed. No seeks,
// synthetic clock jumps, world time dilation or direct lighting ticks in the run.
void ACourseworkHUD::TickDayCycleAcceptance()
{
    if(!bLightingReady||!Daylight)return;
    struct FRun
    {
        double At=0,LastSample=-1,LastShot=-30,LastHour=12,LastSim=0;
        int64 InitialDay=1;int32 Shot=0;bool Manual=false,Probe=false;float Start=12,Span=24;double InitialSim=0;
        bool Forward=true,RenderFinite=true,PhysicalLights=true,ManualHeld=true,RoomPrefs=true,DaySun=true,NightSun=true;
        bool SolarBalance=true,SolarNight=true;double SolarPeak=0;
        float MaxLag=0,MinDesk=1,MaxDesk=0;
        TArray<bool> On;TArray<float> Brightness;TArray<FString> Samples;
        TSet<int32> Hours;
    };static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();
    auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    const FName Desk(TEXT("SM_TableLamp_A1_32")),Ambient(TEXT("PointLight19"));
    auto Check=[&](const TCHAR* Name,bool Passed){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Passed?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("DAY_CYCLE_CHECK %s"),*TestResults.Last());};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=Now;SetPanel(0);SetSmartLightingScene(0);
        R.On=RoomLightOn;R.Brightness=RoomBrightness;R.InitialDay=Clock.Day;R.LastHour=Clock.Hour;R.LastSim=Clock.ElapsedSimulationSeconds;
        Check(TEXT("normal_startup_noon"),Clock.Hour>=12&&Clock.Hour<12.1&&Clock.bRunning&&Clock.DayMinutes==24);
        SetTimeDayMinutes(6);SetTimeFlowing(true);
        R.Probe=FParse::Value(FCommandLine::Get(),TEXT("HouseCycleStart="),R.Start);
        if(R.Probe){FParse::Value(FCommandLine::Get(),TEXT("HouseCycleHours="),R.Span);R.Span=FMath::Clamp(R.Span,.5f,4.f);SetDaylightHour(R.Start);Daylight->Snap(R.Start);}
        R.InitialSim=Clock.ElapsedSimulationSeconds;
        Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();Player->SetActorEnableCollision(false);
        Player->SetActorLocation(FVector(-1840,10530,413),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation((FVector(-1734,10661,440)-Player->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);
        SetSmartLightingScene(2);
        IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Screenshots/DayCycle")),true);
        return;
    }
    const double Elapsed=Now-R.At;
    const float Warmup=R.Probe?2.f:8.f;
    if(Elapsed<=Warmup){R.LastHour=Clock.Hour;R.LastSim=Clock.ElapsedSimulationSeconds;}
    if(Elapsed>Warmup)
    {
        const double Advance=FCourseworkClock::Wrap(Clock.Hour-R.LastHour);
        R.Forward&=Advance<.1&&Clock.ElapsedSimulationSeconds>R.LastSim;
        R.LastHour=Clock.Hour;R.LastSim=Clock.ElapsedSimulationSeconds;
        const float H=GetDaylightHour(),Rendered=Daylight->GetRenderedHour();
        R.SolarBalance&=FMath::Abs(SolarFlow.Generation+SolarFlow.Import-SolarFlow.Load-SolarFlow.Export)<.00001;
        R.SolarPeak=FMath::Max(R.SolarPeak,SolarFlow.Generation);
        if(Rendered<5||Rendered>19)R.SolarNight&=SolarFlow.Generation==0;
        R.MaxLag=FMath::Max(R.MaxLag,FMath::Abs(float(FCourseworkClock::Wrap(H-Rendered+12)-12)));
        R.RenderFinite&=FMath::IsFinite(Rendered)&&FMath::IsFinite(Daylight->GetSunIntensity())&&FMath::IsFinite(Daylight->GetSkyIntensity());
        R.Hours.Add(FMath::FloorToInt(H));
        R.RoomPrefs&=RoomLightOn==R.On&&RoomBrightness==R.Brightness;
        if(H>9&&H<15)R.DaySun&=Daylight->GetSunIntensity()>9;
        if(H<5||H>19)R.NightSun&=Daylight->GetSunIntensity()<=.00002f;
        const float DeskLevel=GetSmartFixtureLevel(Desk);
        R.MinDesk=FMath::Min(R.MinDesk,DeskLevel);R.MaxDesk=FMath::Max(R.MaxDesk,DeskLevel);
        const float Expected=FMath::Lerp(.85f,.30f,FMath::Clamp(GetDaylightElevation()/50.f,0.f,1.f));
        R.PhysicalLights&=bSmartReadingOccupied&&FMath::Abs(DeskLevel-Expected)<.025f;
        for(const auto& L:ControlledLights)if(auto* C=L.Component.Get())
            R.PhysicalLights&=FMath::IsNearlyEqual(C->Intensity,L.Intensity*GetSmartFixtureLevel(C->GetOwner()->GetFName()),.02f);
        if(!R.Manual&&H>16&&Clock.Day==R.InitialDay)
        {SetSmartFixtureManual(Ambient,.42f);R.Manual=true;}
        if(R.Manual)R.ManualHeld&=SmartFixtureOverrides.Contains(Ambient)&&FMath::IsNearlyEqual(GetSmartFixtureLevel(Ambient),.42f,.001f);
        if(Now-R.LastSample>.15)
        {
            R.LastSample=Now;double Luma=-1,WindowLuma=-1;
            TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;auto Window=GetWorld()->GetGameViewport()->GetWindow();
            if(Window.IsValid()&&FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)&&Pixels.Num())
            {
                double Sum=0,Outside=0;int32 Count=0,OutsideCount=0;
                for(int32 Y=Size.Y/5;Y<Size.Y*4/5;Y+=4)for(int32 X=Size.X/5;X<Size.X*4/5;X+=4)
                {const auto P=Pixels[Y*Size.X+X];const double V=.2126*P.R+.7152*P.G+.0722*P.B;Sum+=V;++Count;if(Y<Size.Y*2/5){Outside+=V;++OutsideCount;}}
                Luma=Sum/FMath::Max(1,Count);WindowLuma=Outside/FMath::Max(1,OutsideCount);
                const bool Boundary=(H>5.97f&&H<6.12f)||(H>17.88f&&H<18.03f)||(H<.05f||H>23.95f);
                if(Now-R.LastShot>(Boundary?.2:30))
                {
                    R.LastShot=Now;TArray<FColor> Small;FImageUtils::ImageResize(Size.X,Size.Y,Pixels,960,540,Small,true);TArray<uint8> PNG;FImageUtils::CompressImageArray(960,540,Small,PNG);
                    FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/DayCycle/Frame_%03d_%02d%02d.png"),R.Shot++,int32(H),int32(H*60)%60)));
                }
            }
            R.Samples.Add(FString::Printf(TEXT("{\"elapsed\":%.5f,\"hour\":%.6f,\"day\":%lld,\"rendered\":%.6f,\"sun\":%.5f,\"sky\":%.5f,\"desk\":%.5f,\"ambient\":%.5f,\"luma\":%.5f,\"window_luma\":%.5f,\"pv_w\":%.5f,\"load_w\":%.5f,\"import_w\":%.5f,\"export_w\":%.5f}"),Elapsed,Clock.Hour,Clock.Day,Rendered,Daylight->GetSunIntensity(),Daylight->GetSkyIntensity(),DeskLevel,GetSmartFixtureLevel(Ambient),Luma,WindowLuma,SolarFlow.Generation,SolarFlow.Load,SolarFlow.Import,SolarFlow.Export));
        }
    }
    if(R.Probe?(Clock.ElapsedSimulationSeconds-R.InitialSim>=R.Span*3600):(Clock.Day==R.InitialDay+1&&Clock.Hour>=12.02))
    {
        if(R.Probe)Check(TEXT("horizon_probe_naturally_advanced"),Elapsed>R.Span*14&&Elapsed<R.Span*17&&R.Hours.Num()>=2);
        else Check(TEXT("completed_full_natural_day_without_seeking"),Elapsed>350&&Elapsed<390&&R.Hours.Num()==24);
        Check(TEXT("clock_advances_continuously_and_wraps_once"),R.Forward&&Clock.Day==R.InitialDay+(R.Probe?int32((R.Start+R.Span)/24):1));
        Check(TEXT("rendered_sun_tracks_clock_without_midnight_detour"),R.RenderFinite&&R.MaxLag<.03f);
        Check(TEXT("day_has_sun_and_night_has_none"),R.DaySun&&R.NightSun);
        Check(TEXT("reading_automatically_follows_full_day"),R.PhysicalLights&&(R.Probe||(R.MinDesk<.31f&&R.MaxDesk>.84f)));
        Check(TEXT("manual_override_survives_sunset_midnight_and_dawn"),R.Probe||R.Manual&&R.ManualHeld);
        Check(TEXT("daily_room_preferences_unchanged"),R.RoomPrefs);
        Check(TEXT("no_unbuilt_lighting_after_full_cycle"),GetWorld()->NumLightingUnbuiltObjects==0);
        Check(TEXT("pixel_samples_available"),R.Samples.Num()>(R.Probe?100:1000));
        Check(TEXT("solar_supply_demand_balanced_throughout_cycle"),R.SolarBalance&&IsSolarReady());
        Check(TEXT("solar_night_zero_and_day_generates"),R.SolarNight&&(R.Probe||R.SolarPeak>1000));
        Check(TEXT("solar_cumulative_energy_conserved"),FMath::Abs(SolarMeter.Generation+SolarMeter.Import-SolarMeter.Load-SolarMeter.Export)<.000001&&(R.Probe||SolarMeter.Generation>1));
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"probe_only\":%s,\"test_minutes_per_day\":6,\"default_minutes_per_day\":24,\"checks\":["),R.Probe?TEXT("true"):TEXT("false"))+FString::Join(TestResults,TEXT(","))+TEXT("],\"samples\":[")+FString::Join(R.Samples,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("DayCycleAcceptance.json")));
        SetSmartLightingScene(0);SetTimeDayMinutes(24);TestMode.Empty();QuitApplication();
    }
}

// Twelve independent day/night x fixed/flow x manual/night/reading scenarios.
// Abort both before and after the detector; real completed evacuation is covered
// separately by TickEvacuationAcceptance with the same restoration assertions.
void ACourseworkHUD::TickDrillRestoreAcceptance()
{
    if(!bLightingReady||!Daylight||!Ventilation)return;
    struct FRun
    {
        int32 Case=0,Stage=0,Scene=0;double At=0,Hour=0,Elapsed=0;bool Flow=false,Display=false;
        TArray<bool> On,Windows;TArray<float> Brightness;float Sun=0,OverrideLevel=.42f;bool LatestRoom=false;
    };static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();
    auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    const FName Override(TEXT("SM_TableLamp_A1_32"));
    auto Check=[&](const TCHAR* Name,bool Passed){const FString Key=FString::Printf(TEXT("case_%02d_%s"),R.Case,Name);TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Key,Passed?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("DRILL_RESTORE_CHECK %s"),*TestResults.Last());};
    auto WindowSame=[&](){for(int32 I=0;I<R.Windows.Num();++I)if(Ventilation->IsOpen(I)!=R.Windows[I])return false;return true;};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=Now;SetPanel(0);
        Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();Player->SetActorEnableCollision(false);
        Player->SetActorLocation(FVector(-1840,10530,413),false,nullptr,ETeleportType::TeleportPhysics);
        return;
    }
    if(R.Stage==0)
    {
        if(R.Case>=12)
        {
            FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("DrillRestoreAcceptance.json")));
            SetSmartLightingScene(0);TestMode.Empty();QuitApplication();return;
        }
        SetSmartLightingScene(0);SetDaylightHour(R.Case<6?12:21);SetTimeFlowing(false);
        R.Scene=R.Case%3;R.Flow=(R.Case/3)%2==1;R.Display=R.Case%2==0;
        R.OverrideLevel=(R.Case==2||R.Case==8)?0.f:.42f;R.LatestRoom=R.Case==4||R.Case==10;
        SetSmartLightingScene(R.Scene);
        if(R.Scene){SetSmartFixtureManual(Override,.42f);if(R.OverrideLevel==0)SetSmartFixtureManual(Override,0);SetRoomLighting(0,true,.37f);}
        SetRoomWindows(0,true);SetRoomWindows(4,R.Case%2==0);Ventilation->SetDisplay(R.Display);
        R.At=Now;R.Stage=1;return;
    }
    if(R.Stage==1&&Now-R.At>2.2)
    {
        SetTimeFlowing(R.Flow);R.Hour=Clock.Hour;R.Elapsed=Clock.ElapsedSimulationSeconds;R.Sun=Daylight->GetSunIntensity();
        R.On=RoomLightOn;R.Brightness=RoomBrightness;R.Windows.Empty();for(int32 I=0;I<Ventilation->GetWindowCount();++I)R.Windows.Add(Ventilation->IsOpen(I));
        Check(TEXT("baseline_scene_selected"),SmartLightingScene==R.Scene);
        TriggerKitchenAlarm();TriggerKitchenAlarm();SetPanel(7);
        if(R.LatestRoom){SetRoomLighting(4,true,.66f);R.On=RoomLightOn;R.Brightness=RoomBrightness;}
        Check(TEXT("fire_suspends_scene_and_clock"),IsSafetyDrillRunning()&&SmartLightingScene==0&&IsTimeSuspended());
        Check(TEXT("early_finish_rejected"),SafetyState==1);HandleKitchenAlarm();Check(TEXT("early_finish_does_not_reset"),SafetyState==1);
        R.At=Now;R.Stage=2;return;
    }
    if(R.Stage==2&&Now-R.At>(R.Case%2==0?1.2:5.0))
    {
        Check(TEXT("time_and_mode_held_during_drill"),Clock.Hour==R.Hour&&Clock.bRunning==R.Flow&&Clock.ElapsedSimulationSeconds==R.Elapsed);
        Check(TEXT("rendered_environment_held_during_drill"),FMath::Abs(Daylight->GetSunIntensity()-R.Sun)<.02f);
        Check(TEXT("airflow_hidden_during_drill"),!Ventilation->IsDisplayed());
        Check(TEXT("window_openings_preserved_during_drill"),WindowSame());
        Check(TEXT("manual_time_edits_blocked"),!SetDaylightHour(6)&&!SetTimeFlowing(!R.Flow)&&!SetSmartLightingScene(2));
        SetPanel(7);Check(TEXT("current_single_action_aborts"),Overlay->ExerciseControlPointer({420,610})&&!IsSafetyDrillRunning());
        Check(TEXT("scene_selection_resumes"),SmartLightingScene==R.Scene);
        Check(TEXT("airflow_choice_resumes"),Ventilation->IsDisplayed()==R.Display);
        Check(TEXT("overrides_and_room_priority_resume"),!R.Scene||(SmartManualRooms.Contains(0)&&(R.LatestRoom?SmartManualRooms.Contains(4)&&!SmartFixtureOverrides.Contains(Override):SmartFixtureOverrides.Contains(Override)&&FMath::IsNearlyEqual(SmartFixtureOverrides.FindRef(Override),R.OverrideLevel))));
        Check(TEXT("room_configuration_preserved"),RoomLightOn==R.On&&RoomBrightness==R.Brightness);
        ResetKitchenAlarm();Check(TEXT("second_reset_is_idempotent"),SmartLightingScene==R.Scene&&Ventilation->IsDisplayed()==R.Display);
        SetPanel(0);R.At=Now;R.Stage=3;return;
    }
    if(R.Stage==3&&Now-R.At>2.2)
    {
        Check(TEXT("clock_resumes_original_mode"),Clock.bRunning==R.Flow&&(R.Flow?Clock.Hour>R.Hour:Clock.Hour==R.Hour));
        Check(TEXT("windows_and_airflow_remain_restored"),WindowSame()&&Ventilation->IsDisplayed()==R.Display);
        Check(TEXT("manual_fixture_still_held_after_delay"),!R.Scene||FMath::IsNearlyEqual(GetSmartFixtureLevel(Override),R.LatestRoom?.66f:R.OverrideLevel,.001f));
        if(R.Scene&&R.OverrideLevel==0)
        {
            Check(TEXT("off_fixture_remembers_previous_nonzero_brightness"),SmartFixtures.Contains(Override)&&FMath::IsNearlyEqual(SmartFixtures[Override].Restore,.42f,.001f));
            SetSmartFixtureManual(Override,SmartFixtures[Override].Restore);
            Check(TEXT("reopening_fixture_restores_previous_dimming"),FMath::IsNearlyEqual(GetSmartFixtureLevel(Override),.42f,.001f));
        }
        Check(TEXT("resident_restored"),!EvacuatingResident&&OriginalResident.IsValid()&&!OriginalResident->IsHidden());
        Check(TEXT("no_unbuilt_lighting"),GetWorld()->NumLightingUnbuiltObjects==0);
        ++R.Case;R.Stage=0;
    }
}
