#include "CourseworkHUD.h"
#include "CourseworkRain.h"
#include "CourseworkDaylight.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "EngineGlobals.h"
#include "Misc/CommandLine.h"

void ACourseworkHUD::TickRainPolishAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Stage=0;double At=0,Started=0,Motion=0;float Wet=0;TArray<double> FrameMs,EffectMs,GpuMs;TArray<TWeakObjectPtr<UStaticMeshComponent>> Interiors;TArray<TArray<TWeakObjectPtr<UMaterialInterface>>> Materials;};static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();
    auto Check=[&](FString Name,bool Good){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Good?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("RAIN_POLISH_CHECK %s"),*TestResults.Last());};
    auto View=[&](FVector P,FVector Aim){auto* C=Cast<ACharacter>(PC->GetPawn());C->GetCharacterMovement()->DisableMovement();C->SetActorEnableCollision(false);C->SetActorLocation(P);PC->SetControlRotation((Aim-C->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.25f,false);};
    auto InteriorSame=[&](){if(R.Interiors.Num()<5)return false;for(int32 I=0;I<R.Interiors.Num();++I){auto* M=R.Interiors[I].Get();if(!M)return false;for(int32 J=0;J<R.Materials[I].Num();++J)if(M->GetMaterial(J)!=R.Materials[I][J].Get())return false;}return true;};
    const bool Profile=TestMode==TEXT("rain_profile");
    auto Finish=[&]()
    {
        FString Extra;
        if(Profile)
        {
            auto Stats=[](TArray<double> Values){if(!Values.Num())return FString(TEXT("{\"samples\":0}"));double Sum=0;for(auto V:Values)Sum+=V;Values.Sort();return FString::Printf(TEXT("{\"samples\":%d,\"mean_ms\":%.5f,\"p95_ms\":%.5f}"),Values.Num(),Sum/Values.Num(),Values[FMath::Min(Values.Num()-1,int32(Values.Num()*.95))]);};
            Extra=TEXT(",\"frame\":")+Stats(R.FrameMs)+TEXT(",\"rain_cpu\":")+Stats(R.EffectMs)+TEXT(",\"gpu\":")+Stats(R.GpuMs)+FString::Printf(TEXT(",\"polished\":%s"),RainEffect&&RainEffect->IsPolished()?TEXT("true"):TEXT("false"));
        }
        FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]")+Extra+TEXT("}"),*(FPaths::ProjectSavedDir()/(Profile?TEXT("RainProfile.json"):TEXT("RainPolish.json"))));TestMode.Empty();QuitApplication();
    };
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=R.Started=Now;SetPanel(0);InitializeTimeOfDay();SetTimeFlowing(false);SetDaylightHour(12);Daylight->Snap(12);Weather.Initialize(2471,false);
        View(FVector(-1460,11800,165),FVector(-1500,11180,-10));
        int32 ProfileView=0;FParse::Value(FCommandLine::Get(),TEXT("HouseRainProfileView="),ProfileView);if(Profile&&ProfileView==1)View(FVector(-1410,11500,90),FVector(-1410,11350,-15));
        for(TActorIterator<AActor> It(GetWorld());It;++It){TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* M:Parts)if(M->GetStaticMesh()&&(M->GetStaticMesh()->GetName().Contains(TEXT("Floor"))||M->GetStaticMesh()->GetName().Contains(TEXT("Curtain")))){R.Interiors.Add(M);TArray<TWeakObjectPtr<UMaterialInterface>> Mats;for(int32 J=0;J<M->GetNumMaterials();++J)Mats.Add(M->GetMaterial(J));R.Materials.Add(Mats);}}
        if(Profile){Weather.Choose(3);Weather.Advance(10,0,false,false);TickHousehold(0);}return;
    }
    if(Now-R.Started>145){Check(TEXT("completed_before_timeout"),false);Finish();return;}
    if(Profile)
    {
        const double Elapsed=Now-R.Started;if(Elapsed>10&&RainEffect){R.FrameMs.Add(GetWorld()->GetDeltaSeconds()*1000.);R.EffectMs.Add(RainEffect->GetEffectMilliseconds());R.GpuMs.Add(FPlatformTime::ToMilliseconds(GGPUFrameTime));}
        if(Elapsed>40){Check(TEXT("matched_view_profile_has_samples"),R.FrameMs.Num()>200);Check(TEXT("profile_rain_visible"),RainEffect&&RainEffect->GetVisibleDropCount()>500);Finish();}return;
    }
    const double Delay=R.Stage==1?35.:R.Stage==5||R.Stage==8?12.:1.5;
    if(Now-R.At<Delay)return;R.At=Now;
    switch(R.Stage++)
    {
    case 0:Shot(TEXT("RainPolish_road_initial.png"));Weather.Choose(3);break;
    case 1:Check(TEXT("rain_materials_load_in_packaged_runtime"),RainEffect&&RainEffect->IsPolished()&&RainEffect->GetVisibleDropCount()>500);Check(TEXT("natural_rain_gradually_wets_surfaces"),RainEffect&&RainEffect->GetWetness()>.8f);Check(TEXT("road_roof_paving_retain_textures_and_normals"),RainEffect&&RainEffect->CheckWetSurfaces());Check(TEXT("nearby_hard_surfaces_receive_splashes"),RainEffect&&RainEffect->GetSplashCount()>0&&RainEffect->CheckSplashes());Shot(TEXT("RainPolish_road_wet.png"));break;
    case 2:Check(TEXT("slanted_rain_remains_clipped_above_first_hit"),RainEffect&&RainEffect->CheckRoofClipping());Check(TEXT("interior_floor_and_curtains_unchanged"),InteriorSame());View(FVector(-1410,11500,90),FVector(-1410,11350,-15));R.Wet=RainEffect->GetWetness();R.Motion=RainEffect->GetAnimationSeconds();SetPanel(2);break;
    case 3:Check(TEXT("pause_freezes_surface_water_and_motion"),RainEffect&&RainEffect->GetWetness()==R.Wet&&RainEffect->GetAnimationSeconds()==R.Motion);SetPanel(0);Shot(TEXT("RainPolish_splash_close.png"));break;
    case 4:View(FVector(-3300,9350,950),FVector(-2270,9800,500));Shot(TEXT("RainPolish_roof_wet.png"));R.Wet=RainEffect->GetWetness();Weather.Choose(0);break;
    case 5:Check(TEXT("rain_stop_keeps_residual_wetness"),Weather.Rain<.001f&&RainEffect->GetWetness()>R.Wet*.7f&&RainEffect->GetWetSurfaceCount()>0);Check(TEXT("rain_stop_clears_splashes"),RainEffect->GetSplashCount()==0);Shot(TEXT("RainPolish_roof_after_rain.png"));break;
    case 6:RainEffect->Update(0,181);Check(TEXT("three_minutes_clear_restores_dry_materials"),RainEffect->GetWetness()==0&&RainEffect->GetWetSurfaceCount()==0);Check(TEXT("dry_restore_does_not_change_interior_materials"),InteriorSame());Shot(TEXT("RainPolish_roof_dry.png"));break;
    case 7:Weather.Choose(3);break;
    case 8:Check(TEXT("second_rain_recreates_wet_materials_without_stacking"),RainEffect->GetWetSurfaceCount()>6&&RainEffect->CheckWetSurfaces());Check(TEXT("second_rain_preserves_shelter_clipping"),RainEffect->CheckRoofClipping());Check(TEXT("energy_accounting_stays_balanced"),FMath::Abs(SolarMeter.Generation+SolarMeter.Import-SolarMeter.Load-SolarMeter.Export)<.000001);break;
    default:Finish();break;
    }
}
