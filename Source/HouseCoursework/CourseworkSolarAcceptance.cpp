#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include <limits>

void ACourseworkHUD::TickSolarAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Stage=0;double At=0,Started=0,Seconds=0,Energy=0,Stamp=0;FCourseworkSolarMeter Before;};
    static FRun R;const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();
    auto Record=[&](const TCHAR* Name,bool OK){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,OK?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("SOLAR_CHECK %s"),*TestResults.Last());};
    auto Close=[&](double A,double B,double E=.000001){return FMath::Abs(A-B)<=E;};
    auto Balance=[&](){return Close(SolarFlow.Generation+SolarFlow.Import,SolarFlow.Load+SolarFlow.Export)&&Close(SolarFlow.Generation,SolarFlow.SelfUse+SolarFlow.Export)&&Close(SolarFlow.Load,SolarFlow.SelfUse+SolarFlow.Import);};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle Handle;GetWorld()->GetTimerManager().SetTimer(Handle,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.2f,false);};
    auto Hour=[&](float H){SetDaylightHour(H);Daylight->Snap(H);};
    auto Finish=[&](){FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Packaged D3D12 runtime, actual roof traces, physical lighting states, native Slate input and integrated meter\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/(TestMode==TEXT("energy")?TEXT("EnergyAcceptance.json"):TEXT("SolarAcceptance.json"))));TestMode.Empty();QuitApplication();};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=R.Started=Now;
        const auto Demand=FCourseworkSolarFlow::Balance(100,216),Surplus=FCourseworkSolarFlow::Balance(1000,216),Equal=FCourseworkSolarFlow::Balance(216,216);
        Record(TEXT("known_deficit_116w"),Close(Demand.Import,116)&&Close(Demand.SelfUse,100)&&Demand.Export==0);
        Record(TEXT("known_surplus_784w"),Close(Surplus.Export,784)&&Close(Surplus.SelfUse,216)&&Surplus.Import==0);
        Record(TEXT("equal_supply_no_grid_exchange"),Equal.Import==0&&Equal.Export==0);
        const auto Invalid=FCourseworkSolarFlow::Balance(std::numeric_limits<double>::quiet_NaN(),-1);
        Record(TEXT("invalid_power_cannot_corrupt_meter"),Invalid.Generation==0&&Invalid.Load==0);
        FCourseworkSolarMeter M;M.Add(Demand,3600);
        Record(TEXT("one_hour_100w_is_0100kwh"),Close(M.Generation,.1)&&Close(M.Load,.216)&&Close(M.Import,.116));
        const double Before=M.Seconds;M.Add(Demand,-1);M.Add(Demand,std::numeric_limits<double>::infinity());
        Record(TEXT("invalid_duration_is_ignored"),M.Seconds==Before);
        Record(TEXT("night_and_horizon_generate_zero"),CourseworkSolar::Power(FVector(0,0,-1),FVector::UpVector,360)==0&&CourseworkSolar::Power(FVector(1,0,0),FVector::UpVector,360)==0);
        Record(TEXT("normal_incidence_uses_nameplate_once"),Close(CourseworkSolar::Power(FVector::UpVector,FVector::UpVector,360),324));
        Record(TEXT("zero_rating_or_normal_produces_zero"),CourseworkSolar::Power(FVector::UpVector,FVector::ZeroVector,360)==0&&CourseworkSolar::Power(FVector::UpVector,FVector::UpVector,0)==0);
        bool Sweep=true;for(int32 I=0;I<=1440;++I){const double A=I*2*PI/1440.;const double P=CourseworkSolar::Power(FVector(FMath::Cos(A),0,FMath::Sin(A)),FVector(-.7,0,.7),2160);Sweep&=FMath::IsFinite(P)&&P>=0&&P<=2160;}
        Record(TEXT("full_angle_sweep_finite_and_bounded"),Sweep);
        Record(TEXT("six_real_panels_match_2160w_nameplate"),IsSolarReady()&&Close(GetSolarRatedWatts(),2160));
        bool RoofOK=IsSolarReady();
        for(const auto& Panel:SolarPanels)for(double X:{-85.,85.})for(double Y:{-55.,55.})
        {
            const FVector Corner=Panel.Center+Panel.Rotation.RotateVector(FVector(X,Y,0));bool Supported=false;
            for(TActorIterator<AActor> It(GetWorld());It;++It){TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* C:Parts)if(C->GetStaticMesh()&&C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_Roof_C")))
            {FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(SolarCorner),true);if(C->LineTraceComponent(Hit,Corner+Panel.Normal*8,Corner-Panel.Normal*20,Q)&&FVector::DotProduct(Hit.ImpactNormal,Panel.Normal)>.97)Supported=true;}}
            RoofOK&=Supported;
        }
        Record(TEXT("all_24_corners_supported_by_matching_roof_slope"),RoofOK);
        bool Geometry=SolarInstallation!=nullptr;int32 Instances=0;
        if(SolarInstallation){TArray<UInstancedStaticMeshComponent*> Groups;SolarInstallation->GetComponents(Groups);for(auto* G:Groups){Instances+=G->GetInstanceCount();Geometry&=G->GetStaticMesh()!=nullptr&&G->GetCollisionEnabled()==ECollisionEnabled::NoCollision;}Geometry&=Groups.Num()==3;}
        Record(TEXT("instanced_cooked_geometry_has_no_gameplay_collision"),Geometry&&Instances==108);
        if(auto* C=Cast<ACharacter>(PC->GetPawn())){C->GetCharacterMovement()->DisableMovement();C->SetActorEnableCollision(false);C->SetActorLocation(FVector(-3300,9350,1470));PC->SetControlRotation((FVector(-2270,9800,830)-C->GetPawnViewLocation()).Rotation());}
        SetTimeFlowing(false);Hour(12);SetAllRoomLighting(true);SetPanel(0);return;
    }
    if(Now-R.Started>130){Record(TEXT("completed_before_timeout"),false);Finish();return;}
    if(Now-R.At<1.)return;R.At=Now;
    switch(R.Stage++)
    {
    case 0:Record(TEXT("noon_roof_produces_surplus"),SolarFlow.Generation>216&&SolarFlow.Export>0&&SolarFlow.Import==0&&Balance());Shot(TEXT("Solar_roof.png"));R.Seconds=SolarMeter.Seconds;R.Energy=SolarMeter.Generation;R.Stamp=Now;break;
    case 1:Record(TEXT("fixed_sun_keeps_meter_running_at_60x"),Close((SolarMeter.Seconds-R.Seconds)/(Now-R.Stamp),60,4)&&SolarMeter.Generation>R.Energy);SetPanel(8);Shot(TEXT("Solar_day.png"));break;
    case 2:Record(TEXT("native_green_tab_and_room_selection"),Overlay->ExerciseControlPointer({570,45})&&PanelMode==8&&Overlay->ExerciseControlPointer({150,295}));Record(TEXT("obsolete_scenario_and_duration_buttons_removed"),!Overlay->ExerciseControlPointer({380,680})&&!Overlay->ExerciseControlPointer({1350,212}));R.Before=SolarMeter;Hour(0);Record(TEXT("seeking_does_not_bill_skipped_hours"),SolarMeter.Seconds==R.Before.Seconds&&SolarMeter.Generation==R.Before.Generation);break;
    case 3:Record(TEXT("midnight_grid_supplies_actual_216w_load"),SolarFlow.Generation==0&&Close(SolarFlow.Import,216)&&SolarFlow.Export==0&&Balance());Shot(TEXT("Solar_night.png"));SetAllRoomLighting(false);break;
    case 4:Record(TEXT("lights_off_stops_grid_import"),SolarFlow.Load==0&&SolarFlow.Import==0);Hour(12);break;
    case 5:Record(TEXT("no_load_exports_all_generation"),SolarFlow.Generation>0&&SolarFlow.SelfUse==0&&Close(SolarFlow.Export,SolarFlow.Generation)&&Balance());Shot(TEXT("Solar_export.png"));SetRoomLighting(0,true,.5);break;
    case 6:Record(TEXT("manual_dimming_updates_load_to_24w"),Close(SolarFlow.Load,24)&&Close(SolarFlow.SelfUse,24)&&Balance());Hour(6.02f);SetAllRoomLighting(true);break;
    case 7:Record(TEXT("dawn_supplements_pv_with_grid"),SolarFlow.Generation>0&&SolarFlow.Generation<216&&SolarFlow.Import>0&&SolarFlow.SelfUse>0&&Balance());Shot(TEXT("Solar_dawn.png"));SetPanel(2);R.Before=SolarMeter;break;
    case 8:Record(TEXT("pause_stops_all_energy_accumulation"),SolarMeter.Seconds==R.Before.Seconds&&SolarMeter.Import==R.Before.Import);SetPanel(3);break;
    case 9:Record(TEXT("help_stops_energy_accumulation"),SolarMeter.Seconds==R.Before.Seconds);SetPanel(9);SelectComfortPage(0);break;
    case 10:Record(TEXT("time_editor_stops_energy_accumulation"),SolarMeter.Seconds==R.Before.Seconds);SetPanel(8);break;
    case 11:Record(TEXT("green_page_resumes_energy_accumulation"),SolarMeter.Seconds>R.Before.Seconds);Hour(12);TriggerKitchenAlarm();R.Before=SolarMeter;break;
    case 12:Record(TEXT("fire_drill_suspends_meter"),IsSafetyDrillRunning()&&SolarMeter.Seconds==R.Before.Seconds);ResetKitchenAlarm();break;
    case 13:Record(TEXT("ending_drill_resumes_meter_without_backfill"),!IsSafetyDrillRunning()&&SolarMeter.Seconds>R.Before.Seconds&&SolarMeter.Seconds-R.Before.Seconds<100);Hour(0);if(auto* C=Cast<ACharacter>(PC->GetPawn())){C->SetActorLocation(FVector(-2320,10350,430));PC->SetControlRotation(FRotator(0,0,0));}SetSmartLightingScene(2);break;
    case 14:Record(TEXT("reading_scene_counts_physical_fixture_outputs"),SmartLightingScene==2&&SolarFlow.Load>0&&Close(SolarFlow.Load,GetTotalWatts(),.001)&&Balance());Record(TEXT("single_fixture_manual_override_accepted"),SetSmartFixtureManual(TEXT("SM_TableLamp_A1_32"),.2f));break;
    case 15:Record(TEXT("single_fixture_override_reaches_energy_load"),Close(GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32")),.2,.01)&&Close(SolarFlow.Load,GetTotalWatts(),.001));SetSmartLightingScene(1);break;
    case 16:Record(TEXT("night_guidance_counts_actual_outputs"),SmartLightingScene==1&&Close(SolarFlow.Load,GetTotalWatts(),.001)&&Balance());SetSmartLightingScene(0);SetAllRoomLighting(true);Hour(12);SetTimeFlowing(true);SetPanel(8);R.Seconds=Clock.Hour;break;
    case 17:Record(TEXT("flowing_clock_advances_with_energy"),Clock.Hour>R.Seconds&&SolarFlow.Generation>0&&Balance());Record(TEXT("meter_conserves_total_energy"),Close(SolarMeter.Generation+SolarMeter.Import,SolarMeter.Load+SolarMeter.Export)&&Close(SolarMeter.Generation,SolarMeter.SelfUse+SolarMeter.Export)&&Close(SolarMeter.Load,SolarMeter.SelfUse+SolarMeter.Import));Record(TEXT("floor_distribution_pointer_opens_real_map"),Overlay->ExerciseControlPointer({1400,758})&&PanelMode==5&&IsEnergyMapLayer());Shot(TEXT("Solar_distribution.png"));break;
    default:Finish();break;
    }
}
