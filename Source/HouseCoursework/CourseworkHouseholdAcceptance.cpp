#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void ACourseworkHUD::TickHouseholdAcceptance()
{
    if(!bLightingReady||bStartupCover)return;
    struct FRun{int32 Stage=0;double At=0,Start=0,Load=0,Power=0,Seconds=0,Phase=0;float Cover=0;};static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();
    auto Record=[&](const TCHAR* Name,bool OK){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,OK?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("HOUSEHOLD_CHECK %s"),*TestResults.Last());};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle Handle;GetWorld()->GetTimerManager().SetTimer(Handle,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.25f,false);};
    auto Finish=[&](){FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Packaged D3D12 runtime, seeded weather, actual appliance materials and native Slate input\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("HouseholdWeatherAcceptance.json")));TestMode.Empty();QuitApplication();};
    auto View=[&](FVector Position,FVector Aim){if(auto* C=Cast<ACharacter>(PC->GetPawn())){C->GetCharacterMovement()->DisableMovement();C->SetActorEnableCollision(false);C->SetActorLocation(Position);PC->SetControlRotation((Aim-C->GetPawnViewLocation()).Rotation());}};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=R.Start=Now;
        FCourseworkWeather A,B,C;A.Initialize(2471);B.Initialize(2471);C.Initialize(2741);bool Repeat=true,Adjacent=true,Holds=true,Different=false,Changed=false;
        for(int32 I=0;I<86400;++I){int32 Previous=A.Kind;A.Advance(1,60,true,false);B.Advance(1,60,true,false);C.Advance(1,60,true,false);Repeat&=A.Kind==B.Kind&&A.Cover==B.Cover&&A.Hold==B.Hold;Adjacent&=FMath::Abs(A.Kind-Previous)<=1;Holds&=A.Hold<=14400&&A.Hold>0;Different|=A.Kind!=C.Kind;Changed|=A.Kind>0;}
        Record(TEXT("same_seed_repeats_60_day_sequence"),Repeat&&Changed);Record(TEXT("different_seeds_produce_different_sequence"),Different);Record(TEXT("automatic_never_jumps_clear_to_overcast"),Adjacent);Record(TEXT("scheduled_hold_bounded_by_four_hours"),Holds);
        const auto Before=A;A.Advance(300,18000,false,false);Record(TEXT("fixed_clock_freezes_automatic_weather"),A.Cover==Before.Cover&&A.Hold==Before.Hold&&A.Phase==Before.Phase);
        A.Advance(300,18000,true,true);Record(TEXT("suspended_weather_has_no_catchup"),A.Cover==Before.Cover&&A.Hold==Before.Hold);
        A.Initialize(1);A.Choose(2);float Old=A.Cover;bool Smooth=true;for(int32 I=0;I<100;++I){A.Advance(.1,0,false,false);Smooth&=A.Cover>=Old&&A.Cover-Old<.013;Old=A.Cover;}
        Record(TEXT("manual_transition_smooth_and_works_with_fixed_sun"),Smooth&&FMath::IsNearlyEqual(A.Cover,.92f,.0001f)&&!A.Automatic);
        const double Clear=CourseworkSolar::Power(FVector::UpVector,FVector::UpVector,360,1,1),Cloudy=CourseworkSolar::Power(FVector::UpVector,FVector::UpVector,360,.56,1.6),Overcast=CourseworkSolar::Power(FVector::UpVector,FVector::UpVector,360,.12,1.4);
        Record(TEXT("same_sun_clear_cloudy_overcast_generation_order"),Clear>Cloudy&&Cloudy>Overcast&&Overcast>0);Record(TEXT("weather_cannot_generate_solar_at_night"),CourseworkSolar::Power(-FVector::UpVector,FVector::UpVector,360,.12,1.4)==0);
        Record(TEXT("four_actual_devices_registered"),Appliances.Num()==4);bool Screens=true;for(const auto& D:Appliances)Screens&=D.Mesh.IsValid()&&(D.Fridge||D.Screen.IsValid());Record(TEXT("actual_meshes_and_screen_materials_loaded"),Screens);
        bool FridgePreserved=Appliances.Num()==4&&Appliances[3].Mesh.IsValid()&&Appliances[3].Mesh->GetOwner()->FindFunction(TEXT("FurnitureAction"));for(const auto& Pair:ApplianceTargets)FridgePreserved&=Pair.Value!=3;Record(TEXT("fridge_door_interaction_not_replaced_by_power_toggle"),FridgePreserved);
        Record(TEXT("default_fridge_on_other_devices_off"),Appliances.Num()==4&&IsApplianceOn(3)&&!IsApplianceOn(0)&&!IsApplianceOn(1)&&!IsApplianceOn(2));
        SetTimeFlowing(false);SetDaylightHour(12);Daylight->Snap(12);SetAllRoomLighting(false);View(FVector(-3300,9350,1470),FVector(-2270,9800,830));SetPanel(0);return;
    }
    if(Now-R.Start>160){Record(TEXT("completed_before_timeout"),false);Finish();return;}
    const double Delay=(R.Stage==8||R.Stage==10||R.Stage==12)?13.:1.5;
    if(Now-R.At<Delay)return;R.At=Now;
    switch(R.Stage++)
    {
    case 0:Record(TEXT("fridge_contributes_to_whole_home_demand"),SolarFlow.Load==GetApplianceWatts()&&SolarFlow.Load>0);R.Power=SolarFlow.Generation;Shot(TEXT("Weather_clear.png"));SetPanel(8);break;
    case 1:Record(TEXT("native_appliance_page_opens"),Overlay->ExerciseControlPointer({1030,145})&&EnergyPage==1);Record(TEXT("native_pc_power_control"),Overlay->ExerciseControlPointer({1390,325})&&IsApplianceOn(0));Shot(TEXT("Appliances_pc_on.png"));break;
    case 2:Record(TEXT("computer_adds_180w_to_actual_flow"),FMath::IsNearlyEqual(SolarFlow.Load,GetApplianceWatts(),.01)&&GetAppliancePower(0)==180);Record(TEXT("native_tv_power_control"),Overlay->ExerciseControlPointer({1390,425})&&IsApplianceOn(1));break;
    case 3:{float Value=0;Appliances[1].Screen->GetScalarParameterValue(FMaterialParameterInfo(TEXT("EmmisivePower")),Value);Record(TEXT("tv_screen_material_is_actually_on"),Value>.1f);Record(TEXT("room_demand_includes_local_appliances"),GetRoomDemandWatts(0)==90&&GetRoomDemandWatts(4)==180);Shot(TEXT("Appliances_list.png"));SetPanel(0);View(FVector(-1832,10540,356),Appliances[0].Mesh->Bounds.Origin+FVector(0,0,8));break;}
    case 4:UpdateInteraction();Record(TEXT("computer_detected_by_actual_camera_trace"),TargetAppliance==0);BeginInteract();EndInteract();Record(TEXT("e_toggles_computer_power"),!IsApplianceOn(0));Shot(TEXT("Computer_off.png"));break;
    case 5:NextInteractionTime=0;BeginInteract();EndInteract();Record(TEXT("second_e_turns_computer_back_on"),IsApplianceOn(0));Shot(TEXT("Computer_on.png"));break;
    case 6:View(FVector(-2265,9880,98),Appliances[1].Mesh->Bounds.Origin);SetPanel(0);break;
    case 7:UpdateInteraction();Record(TEXT("tv_detected_by_actual_camera_trace"),TargetAppliance==1);Shot(TEXT("TV_on.png"));View(FVector(-3300,9350,1470),FVector(-2270,9800,830));ChooseWeather(2);SetPanel(0);break;
    case 8:Record(TEXT("overcast_transition_completed"),Weather.BlendAge==10&&Weather.Cover>.9f);Record(TEXT("same_noon_weather_reduces_actual_pv"),SolarFlow.Generation<R.Power*.5&&SolarFlow.Generation>0);Record(TEXT("overcast_sun_reduces_actual_light"),Daylight->GetSunIntensity()<2);Shot(TEXT("Weather_overcast.png"));SetPanel(9);SelectComfortPage(2);break;
    case 9:Record(TEXT("native_weather_page_selected"),Overlay->ExerciseControlPointer({140,322})&&ComfortPage==2);Record(TEXT("native_partly_cloudy_control"),Overlay->ExerciseControlPointer({675,596})&&Weather.Kind==1&&!Weather.Automatic);Shot(TEXT("Weather_panel.png"));break;
    case 10:Record(TEXT("partly_cloudy_uses_intermediate_cover"),Weather.Cover>.2f&&Weather.Cover<.7f);Record(TEXT("native_automatic_weather_toggle"),Overlay->ExerciseControlPointer({1400,145})&&Weather.Automatic);R.Cover=Weather.Cover;R.Phase=Weather.Phase;R.Seconds=ApplianceSeconds;break;
    case 11:Record(TEXT("fixed_sun_holds_weather_but_not_appliance_operation"),Weather.Cover==R.Cover&&Weather.Phase==R.Phase&&ApplianceSeconds>R.Seconds);SetTimeFlowing(true);Weather.Hold=1;break;
    case 12:Record(TEXT("automatic_schedule_advances_with_clock"),Weather.Phase>R.Phase);R.Cover=Weather.Cover;R.Phase=Weather.Phase;R.Seconds=ApplianceSeconds;SetPanel(2);break;
    case 13:Record(TEXT("pause_freezes_weather_and_appliance_meter"),Weather.Cover==R.Cover&&Weather.Phase==R.Phase&&ApplianceSeconds==R.Seconds);SetPanel(0);TriggerKitchenAlarm();R.Cover=Weather.Cover;R.Phase=Weather.Phase;R.Seconds=ApplianceSeconds;break;
    case 14:Record(TEXT("fire_preserves_weather_and_freezes_accounting"),IsSafetyDrillRunning()&&Weather.Cover==R.Cover&&Weather.Phase==R.Phase&&ApplianceSeconds==R.Seconds);Record(TEXT("fire_rejects_device_state_changes"),!ToggleAppliance(0));ResetKitchenAlarm();SetTimeFlowing(false);SetDaylightHour(0);Daylight->Snap(0);break;
    case 15:Record(TEXT("night_grid_supplies_lights_and_devices"),SolarFlow.Generation==0&&SolarFlow.Import==GetHouseholdWatts());Shot(TEXT("Weather_night.png"));ApplianceSeconds=599;Record(TEXT("fridge_compressor_120w"),GetAppliancePower(3)==120);ApplianceSeconds=601;Record(TEXT("fridge_idle_2w"),GetAppliancePower(3)==2);ApplianceSeconds=1801;Record(TEXT("fridge_repeats_30_minute_duty_cycle"),GetAppliancePower(3)==120);SetPanel(8);SelectEnergyPage(0);break;
    case 16:Record(TEXT("integrated_meter_conserves_energy"),FMath::Abs(SolarMeter.Generation+SolarMeter.Import-SolarMeter.Load-SolarMeter.Export)<.000001);Shot(TEXT("Household_energy.png"));break;
    default:Finish();break;
    }
}
