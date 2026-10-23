#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"

void ACourseworkHUD::TickDialAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Step=0;double At=0,Hour=0,World=0;TArray<bool> On;TArray<float> Levels;};static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();
    auto Check=[&](const TCHAR* Name,bool Pass){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Pass?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("DIAL_CHECK %s"),*TestResults.Last());};
    auto Click=[&](float X,float Y){return Overlay->ExerciseControlPointer({X,Y});};
    auto Point=[](float H){const float A=H*2*PI/24+PI/2;return FVector2D(930+220*FMath::Cos(A),440+220*FMath::Sin(A));};
    auto Seek=[&](float H){return Overlay->ExerciseControlPointer(Point(H));};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.4f,false);};
    auto Same=[&](){return R.On==RoomLightOn&&R.Levels==RoomBrightness;};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=Now;R.On=RoomLightOn;R.Levels=RoomBrightness;
        Check(TEXT("noon_startup_retained"),IsDaylightActive()&&Clock.Hour>=12&&Clock.Hour<12.1&&Clock.bRunning);
        SetPanel(9);SelectComfortPage(0);R.Hour=Clock.Hour;return;
    }
    if(Now-R.At<2.1)return;R.At=Now;
    switch(R.Step++)
    {
    case 0:
        Check(TEXT("opening_dial_suspends_time"),Clock.Hour==R.Hour&&Clock.bRunning);
        Check(TEXT("dial_pointer_accepts_dawn"),Seek(6));Check(TEXT("dial_preview_does_not_seek_scene"),Clock.Hour==R.Hour);Shot(TEXT("Dial_preview.png"));break;
    case 1:
        Check(TEXT("apply_pointer_seeks_selected_time"),Click(1350,750)&&FMath::IsNearlyEqual(float(Clock.Hour),6.f,.001f));
        Check(TEXT("apply_keeps_flow_mode"),Clock.bRunning);SetPanel(0);break;
    case 2:
        Check(TEXT("clock_resumes_after_dial_close"),Clock.Hour>6);SetPanel(9);
        Check(TEXT("fixed_checkbox_is_draft_only"),Click(900,750)&&Clock.bRunning);Seek(12);Click(1350,750);
        Check(TEXT("apply_commits_fixed_mode"),!Clock.bRunning&&Clock.Hour==12);R.World=GetWorld()->GetTimeSeconds();SetPanel(0);break;
    case 3:
        Check(TEXT("fixed_time_does_not_pause_world"),Clock.Hour==12&&GetWorld()->GetTimeSeconds()>R.World+1);
        SetPanel(9);Seek(21);Click(900,750);SetPanel(0);
        Check(TEXT("cancel_drops_time_and_mode_edits"),Clock.Hour==12&&!Clock.bRunning);SetPanel(9);break;
    case 4:
        Click(1350,750);Check(TEXT("reopen_resets_discarded_draft"),Clock.Hour==12&&!Clock.bRunning);Shot(TEXT("Dial_fixed.png"));break;
    case 5:
        Check(TEXT("dial_pointer_accepts_midnight"),Seek(0));Click(1350,750);Check(TEXT("midnight_applied"),Clock.Hour==0);SetPanel(0);break;
    case 6:
        Check(TEXT("midnight_has_no_sun"),Daylight->GetSunIntensity()==0&&NeedsNightLighting());SetPanel(9);
        Check(TEXT("drag_begin"),Overlay->ExerciseControlDrag(Point(23.75f),0));
        Check(TEXT("drag_across_midnight"),Overlay->ExerciseControlDrag(Point(.25f),1));
        Check(TEXT("drag_release"),Overlay->ExerciseControlDrag(Point(.25f),2));
        Check(TEXT("drag_is_preview_only"),Clock.Hour==0);Click(1350,750);Check(TEXT("drag_applies_quarter_hour"),FMath::IsNearlyEqual(float(Clock.Hour),.25f,.001f));SetPanel(0);break;
    case 7:
        Check(TEXT("midnight_drag_does_not_flash_day"),Daylight->GetSunIntensity()==0&&Daylight->GetRenderedHour()<.3f);SetPanel(6);break;
    case 8:
        {const bool Before=IsRoomLightOn(0);Overlay->ShowLightingRoom(0);Check(TEXT("single_light_toggle"),Click(560,600)&&IsRoomLightOn(0)!=Before);Click(560,600);Check(TEXT("light_toggle_restores_state"),IsRoomLightOn(0)==Before);}
        Check(TEXT("night_card_enters_scene"),Click(1040,380)&&SmartLightingScene==1);Check(TEXT("night_card_toggles_off"),Click(1040,380)&&SmartLightingScene==0);
        Check(TEXT("reading_card_enters_scene"),Click(1350,380)&&SmartLightingScene==2);Check(TEXT("reading_card_toggles_off"),Click(1350,380)&&SmartLightingScene==0);Shot(TEXT("Clean_smart.png"));break;
    case 9:SetPanel(8);break;
    case 10:
        Check(TEXT("energy_eco_button"),Click(560,680)&&bEnergyComparisonActive);Check(TEXT("energy_restore_button"),Click(750,680)&&!bEnergyComparisonActive&&Same());Shot(TEXT("Clean_energy.png"));break;
    case 11:SetPanel(7);break;
    case 12:Check(TEXT("single_safety_action_starts_drill"),Click(420,610)&&IsSafetyDrillRunning());break;
    case 13:
        Check(TEXT("fixed_time_survives_fire"),Clock.Hour==.25&&!Clock.bRunning);Shot(TEXT("Clean_safety_active.png"));break;
    case 14:
        Check(TEXT("route_button_works_during_drill"),Click(420,720)&&PanelMode==5);SetPanel(7);break;
    case 15:
        Check(TEXT("same_safety_action_aborts_drill"),Click(420,610)&&!IsSafetyDrillRunning());Shot(TEXT("Clean_safety_idle.png"));break;
    case 16:CloseAllWindows();SetPanel(9);SelectComfortPage(1);Overlay->ShowLightingRoom(0);break;
    case 17:
        Check(TEXT("single_window_button_opens"),Click(420,580)&&GetRoomOpenWindowCount(0)==GetRoomWindowCount(0));
        Check(TEXT("airflow_button"),Click(1200,580)&&IsAirflowDisplayed());Shot(TEXT("Clean_ventilation.png"));break;
    case 18:
        Check(TEXT("same_window_button_closes"),Click(420,580)&&GetRoomOpenWindowCount(0)==0);Click(1200,580);SetPanel(4);Shot(TEXT("Clean_settings.png"));break;
    case 19:
        SetPanel(9);SelectComfortPage(0);TriggerKitchenAlarm();R.Hour=Clock.Hour;break;
    case 20:
        Seek(12);Click(900,750);Click(1350,750);Check(TEXT("fire_blocks_dial_apply"),Clock.Hour==R.Hour&&!Clock.bRunning);
        ResetKitchenAlarm();SetPanel(0);SetPanel(9);break;
    case 21:
        Seek(12);Click(900,750);Click(1350,750);Check(TEXT("flow_checkbox_can_resume"),Clock.bRunning&&Clock.Hour==12);Shot(TEXT("Dial_final.png"));break;
    default:
        Check(TEXT("original_lighting_preferences_preserved"),Same());
        FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("DialAcceptance.json")));
        TestMode.Empty();QuitApplication();break;
    }
}

// Capture startup from the first HUD tick, not after the usual 3-second test
// warmup. This uses the normal startup cover and normal noon initialization.
void ACourseworkHUD::TickStartupAcceptance()
{
    struct FRun{int32 Covered=0,Visible=0;float MaxCovered=0;bool WrongReveal=false;TArray<FString> Frames;};static FRun R;
    auto Window=GetWorld()->GetGameViewport()->GetWindow();TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
    if(Window.IsValid()&&FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)&&Pixels.Num())
    {
        float Peak=0;double Mean=0;int32 Count=0;
        for(int32 Y=Size.Y/8;Y<Size.Y*7/8;Y+=12)for(int32 X=Size.X/8;X<Size.X*7/8;X+=12)
        {const FColor P=Pixels[Y*Size.X+X];Peak=FMath::Max(Peak,float(FMath::Max3(P.R,P.G,P.B)));Mean+=.2126*P.R+.7152*P.G+.0722*P.B;++Count;}
        if(bStartupCover){++R.Covered;R.MaxCovered=FMath::Max(R.MaxCovered,Peak);}
        else {++R.Visible;R.WrongReveal|=!bLightingReady||!Daylight||Daylight->GetSunIntensity()<11||GetDaylightElevation()<64;}
        R.Frames.Add(FString::Printf(TEXT("{\"covered\":%s,\"peak\":%.1f,\"mean\":%.3f,\"hour\":%.4f}"),bStartupCover?TEXT("true"):TEXT("false"),Peak,Mean/FMath::Max(1,Count),GetDaylightHour()));
    }
    if(GetWorld()->GetRealTimeSeconds()<5)return;
    const bool Pass=R.Covered>0&&R.Visible>0&&R.MaxCovered<=2&&!R.WrongReveal;
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"checks\":[{\"case\":\"startup_covered_until_noon_ready\",\"passed\":%s}],\"covered_frames\":%d,\"visible_frames\":%d,\"max_covered_pixel\":%.1f,\"frames\":["),Pass?TEXT("true"):TEXT("false"),R.Covered,R.Visible,R.MaxCovered)+FString::Join(R.Frames,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("StartupAcceptance.json")));
    TestMode.Empty();QuitApplication();
}
