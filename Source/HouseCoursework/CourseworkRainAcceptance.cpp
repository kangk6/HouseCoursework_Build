#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkRain.h"
#include "CourseworkOverlay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void ACourseworkHUD::TickRainAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Stage=0;double At=0,Started=0,OvercastPV=0,Motion=0,Hour=0,Energy=0;float Rain=0,OutsideGain=0;int32 Seed=0;};static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();
    auto Check=[&](FString Name,bool Good){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Good?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("RAIN_CHECK %s"),*TestResults.Last());};
    auto View=[&](FVector P,FVector Aim){auto* C=Cast<ACharacter>(PC->GetPawn());C->GetCharacterMovement()->DisableMovement();C->SetActorEnableCollision(false);C->SetActorLocation(P);PC->SetControlRotation((Aim-C->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.25f,false);};
    auto Finish=[&](){FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Seeded four-state weather; real native rain instances, roof traces, audio gain, Slate input and pause/fire/energy checks\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("RainAcceptance.json")));TestMode.Empty();QuitApplication();};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=R.Started=Now;
        FCourseworkWeather A,B;A.Initialize(2471);B.Initialize(2471);bool Repeat=true,Adjacent=true,SawRain=false;
        for(int32 I=0;I<86400;++I){int32 Previous=A.Kind;A.Advance(1,60,true,false);B.Advance(1,60,true,false);Repeat&=A.Kind==B.Kind&&A.Rain==B.Rain;Adjacent&=FMath::Abs(A.Kind-Previous)<=1;SawRain|=A.Kind==3;}
        Check(TEXT("seeded_sequence_repeats_and_reaches_rain"),Repeat&&SawRain);Check(TEXT("automatic_rain_arrives_and_leaves_through_overcast"),Adjacent);
        A.Initialize(1,false);A.Choose(3);float Previous=0;bool Smooth=true;
        for(int32 I=0;I<100;++I){A.Advance(.1,0,false,false);Smooth&=A.Rain>=Previous&&A.Rain-Previous<.016;Previous=A.Rain;}
        Check(TEXT("fixed_clock_manual_rain_blends_in_ten_seconds"),Smooth&&A.Rain>.999f);const auto Saved=A;A.Advance(100,6000,true,true);Check(TEXT("suspension_preserves_rain_and_rng"),A.Rain==Saved.Rain&&A.Random.GetCurrentSeed()==Saved.Random.GetCurrentSeed());
        SetPanel(0);InitializeTimeOfDay();SetTimeFlowing(false);SetDaylightHour(12);Daylight->Snap(12);Weather.Initialize(2471,false);Weather.Choose(2);View(FVector(-3300,9350,950),FVector(-2270,9800,500));return;
    }
    if(Now-R.Started>150){Check(TEXT("completed_before_timeout"),false);Finish();return;}
    const double Delay=(R.Stage==0||R.Stage==2||R.Stage==19)?12.:R.Stage==6?3.:1.5;
    if(Now-R.At<Delay)return;R.At=Now;
    switch(R.Stage++)
    {
    case 0:R.OvercastPV=SolarFlow.Generation;SetPanel(9);SelectComfortPage(2);break;
    case 1:Check(TEXT("native_rain_control_clickable"),Overlay->ExerciseControlPointer({1200,596})&&Weather.Kind==3&&!Weather.Automatic);Shot(TEXT("Rain_panel.png"));break;
    case 2:Check(TEXT("rain_instances_created_and_visible"),RainEffect&&RainEffect->GetVisibleDropCount()>2000);Check(TEXT("rain_material_and_roof_clipping_valid"),RainEffect&&RainEffect->CheckRoofClipping());Check(TEXT("rain_pv_lower_than_overcast_but_not_zero"),SolarFlow.Generation>0&&SolarFlow.Generation<R.OvercastPV*.8);Check(TEXT("rain_increases_reading_supplement_factor"),Weather.Daylight()<.26f);R.OutsideGain=RainEffect?RainEffect->GetAudioGain():0;SetPanel(0);Shot(TEXT("Rain_exterior.png"));break;
    case 3:View(FVector(-1840,10530,413),FVector(-1734,10661,440));Shot(TEXT("Rain_study.png"));break;
    case 4:Check(TEXT("study_listener_under_roof"),RainEffect&&RainEffect->IsSheltered());Check(TEXT("indoor_audio_attenuated"),RainEffect&&RainEffect->GetAudioGain()<R.OutsideGain*.5);SetSmartLightingScene(2);break;
    case 5:Check(TEXT("rain_reading_lamp_actually_supplements"),bSmartReadingOccupied&&GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32"))>.7f);SetSmartLightingScene(0);R.Motion=RainEffect->GetAnimationSeconds();SetPanel(2);break;
    case 6:Check(TEXT("pause_freezes_rain_motion"),RainEffect&&RainEffect->GetAnimationSeconds()==R.Motion);Check(TEXT("pause_mutes_rain_audio"),RainEffect&&RainEffect->GetAudioGain()<.003f);SetPanel(0);R.Rain=Weather.Rain;R.Hour=Clock.Hour;R.Energy=SolarMeter.Seconds;TriggerKitchenAlarm();R.Seed=Weather.Random.GetCurrentSeed();ChooseWeather(0);break;
    case 7:Check(TEXT("fire_preserves_rain_clock_and_meter"),Weather.Kind==3&&Weather.Rain==R.Rain&&Clock.Hour==R.Hour&&SolarMeter.Seconds==R.Energy&&Weather.Random.GetCurrentSeed()==R.Seed);ResetKitchenAlarm();Check(TEXT("rain_restored_after_fire"),Weather.Kind==3&&Weather.Rain==R.Rain);SetDaylightHour(0);Daylight->Snap(0);break;
    case 8:Check(TEXT("night_rain_generates_no_solar"),SolarFlow.Generation==0&&SolarFlow.Import==GetHouseholdWatts());Shot(TEXT("Rain_night.png"));break;
    case 9:case 10:case 11:case 12:case 13:case 14:case 15:case 16:case 17:
    {
        const int32 Room=R.Stage-10;
        if(Room>0)Check(FString::Printf(TEXT("room_%d_listener_sheltered"),Room-1),RainEffect&&RainEffect->IsSheltered());
        if(Room<8){const auto& P=CourseworkRooms::Get(Room);View(P.Position,P.Focus);}
        else{const auto& P=CourseworkRooms::Get(8);View(P.Position,P.Focus);}break;
    }
    case 18:Check(TEXT("room_8_listener_sheltered"),RainEffect&&RainEffect->IsSheltered());ChooseWeather(0);SetPanel(0);break;
    case 19:Check(TEXT("clear_weather_removes_all_rain"),Weather.Rain<.001f&&RainEffect&&RainEffect->GetVisibleDropCount()==0);Check(TEXT("clear_weather_stops_rain_sound"),RainEffect&&RainEffect->GetAudioGain()<.003f);Check(TEXT("rain_energy_balance_preserved"),FMath::Abs(SolarMeter.Generation+SolarMeter.Import-SolarMeter.Load-SolarMeter.Export)<.000001);break;
    default:Finish();break;
    }
}
