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
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "Components/LightComponent.h"

void ACourseworkHUD::TickRainAcceptance()
{
    if(!bLightingReady)return;
    if(TestMode==TEXT("rain_brightness"))
    {
        struct FSurvey{int32 Index=0,Frames=0;double At=0;bool AllPixels=true,NeutralGrade=true,LightingBuilt=true;TArray<FString> Samples;TArray<double> Luma,Dark,Bright;};static FSurvey S;
        const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();
        auto Setup=[&]()
        {
            const int32 Room=S.Index/3,Scene=S.Index%3;const auto& P=CourseworkRooms::Get(Room);
            auto* C=Cast<ACharacter>(PC->GetPawn());C->GetCharacterMovement()->DisableMovement();C->SetActorEnableCollision(false);C->SetActorLocation(P.Position);PC->SetControlRotation((P.Focus-C->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);
            SetDaylightHour(Scene==2?0:12);Daylight->Snap(Scene==2?0:12);Weather.Initialize(2471,false);Weather.Choose(Scene==0?0:3);Weather.Advance(10,0,false,false);TickHousehold(0);S.At=Now;S.Frames=0;
        };
        if(TestCase<0)
        {
            TestCase=0;S=FSurvey();SetPanel(0);InitializeTimeOfDay();SetTimeFlowing(false);SetSmartLightingScene(0);SetAllRoomLighting(true);
            for(int32 I=0;I<Appliances.Num();++I)if(Appliances[I].On)ToggleAppliance(I);
            bool Temperatures=true;int32 Warm=0;TArray<FString> Sources;
            for(const auto& Source:ControlledLights)if(auto* L=Source.Component.Get())
            {
                Temperatures&=!L->bUseTemperature||L->Temperature<=6500.f;
                Warm+=L->bUseTemperature&&L->Temperature<4000.f?1:0;
                Sources.Add(FString::Printf(TEXT("{\"actor\":\"%s\",\"component\":\"%s\",\"temperature_enabled\":%s,\"kelvin\":%.3f,\"intensity\":%.5f}"),*L->GetOwner()->GetName(),*L->GetName(),L->bUseTemperature?TEXT("true"):TEXT("false"),L->Temperature,L->Intensity));
            }
            TestResults.Add(FString::Printf(TEXT("{\"case\":\"indoor_fills_not_excessively_cool_and_warm_fixtures_retained\",\"passed\":%s}"),Temperatures&&Warm>0?TEXT("true"):TEXT("false")));
            FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(Sources,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("InteriorLightingSources.json")));
            for(TActorIterator<APostProcessVolume> It(GetWorld());It;++It)UE_LOG(LogTemp,Display,TEXT("RAIN_EXPOSURE actor=%s priority=%.2f weight=%.2f min=%.3f max=%.3f bias=%.3f method=%d"),*It->GetName(),It->Priority,It->BlendWeight,It->Settings.AutoExposureMinBrightness,It->Settings.AutoExposureMaxBrightness,It->Settings.AutoExposureBias,int32(It->Settings.AutoExposureMethod));
            Setup();return;
        }
        // Initial loading and temporal exposure need rendered frames as well as
        // wall time, especially in an offscreen/background run on battery.
        if(++S.Frames<24||Now-S.At<(S.Index==0?12.:3.))return;
        if(GetWorld()->NumLightingUnbuiltObjects>0){S.LightingBuilt=false;PC->ConsoleCommand(TEXT("DumpUnbuiltLightInteractions"),true);}
        const APostProcessVolume* Grade=nullptr;
        for(TActorIterator<APostProcessVolume> It(GetWorld());It;++It)
            if(It->bUnbound&&It->BlendWeight>.99f&&It->Settings.bOverride_WhiteTemp&&(!Grade||It->Priority>Grade->Priority))Grade=*It;
        S.NeutralGrade&=Grade&&FMath::IsNearlyEqual(Grade->Settings.WhiteTemp,6500.f)&&Grade->Settings.bOverride_WhiteTint&&Grade->Settings.WhiteTint==0&&Grade->Settings.bOverride_SceneColorTint&&Grade->Settings.SceneColorTint.Equals(FLinearColor::White);
        TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;auto Window=GetWorld()->GetGameViewport()->GetWindow();
        double Luma=0,Dark=0,Bright=0;int32 Count=0;
        if(Window.IsValid()&&FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)&&Pixels.Num())
        {
            for(int32 Y=Size.Y*45/100;Y<Size.Y*80/100;Y+=2)for(int32 X=Size.X/5;X<Size.X*4/5;X+=2){const auto& P=Pixels[Y*Size.X+X];const double L=.2126*P.R+.7152*P.G+.0722*P.B;Luma+=L;Dark+=L<8?1:0;Bright+=L>250?1:0;++Count;}
            TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/FString::Printf(TEXT("RainBrightness_%02d.png"),S.Index)));
        }
        S.AllPixels&=Count>1000;S.Luma.Add(Luma/FMath::Max(1,Count));S.Dark.Add(Dark/FMath::Max(1,Count));S.Bright.Add(Bright/FMath::Max(1,Count));
        const FString Sample=FString::Printf(TEXT("{\"room\":%d,\"scene\":%d,\"luma\":%.5f,\"dark_fraction\":%.5f,\"bright_fraction\":%.5f,\"sun\":%.5f,\"sky\":%.5f,\"pixels\":%d}"),S.Index/3,S.Index%3,S.Luma.Last(),S.Dark.Last(),S.Bright.Last(),Daylight->GetSunIntensity(),Daylight->GetSkyIntensity(),Count);
        S.Samples.Add(Sample);UE_LOG(LogTemp,Display,TEXT("RAIN_BRIGHTNESS %s"),*Sample);
        if(++S.Index<27){Setup();return;}
        auto Check=[&](FString Name,bool Good){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Good?TEXT("true"):TEXT("false")));};
        Check(TEXT("all_27_rendered_views_sampled"),S.AllPixels&&S.Samples.Num()==27);
        Check(TEXT("neutral_white_balance_in_all_27_views"),S.NeutralGrade);
        Check(TEXT("no_unbuilt_lighting_in_all_27_views"),S.LightingBuilt);
        for(int32 Room=0;Room<9;++Room)
        {
            const int32 I=Room*3;const FString Prefix=FString::Printf(TEXT("room_%d_"),Room);
            Check(Prefix+TEXT("noon_rain_not_darker_than_night"),S.Luma[I+1]>=S.Luma[I+2]*.98);
            Check(Prefix+TEXT("rain_retains_clear_day_contrast"),S.Luma[I+1]<=S.Luma[I]+2);
            Check(Prefix+TEXT("rain_interior_not_crushed"),S.Luma[I+1]>20&&S.Dark[I+1]<.25);
            Check(Prefix+TEXT("rain_interior_not_whitewashed"),S.Luma[I+1]<200&&S.Bright[I+1]<.20);
        }
        FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("],\"samples\":[")+FString::Join(S.Samples,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("RainBrightness.json")));TestMode.Empty();QuitApplication();return;
    }
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
