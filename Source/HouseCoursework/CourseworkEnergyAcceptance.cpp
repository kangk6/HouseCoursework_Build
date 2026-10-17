#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include <limits>

void ACourseworkHUD::TickEnergyAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Stage=0;double At=0,Started=0;uint64 Frame=0;TArray<bool> On;TArray<float> Brightness;float Watts=0;FVector Position;FString Save;};
    static FRun R;const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    auto Record=[&](const TCHAR* Name,bool Passed){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Passed?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("ENERGY_CHECK %s"),*TestResults.Last());};
    auto Click=[&](float X,float Y){return Overlay->ExerciseControlPointer({X,Y});};
    auto Key=[&](FKey K,bool Down){PC->InputKey(FInputKeyParams(K,Down?IE_Pressed:IE_Released,Down?1.:0.));};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.15f,false);};
    auto Physical=[&](){bool OK=true;for(const auto& L:ControlledLights){const float Expected=GetRoomWatts(L.Room)/GetRoomRatedWatts(L.Room);OK &=L.Component.IsValid()&&FMath::IsNearlyEqual(L.Component->Intensity,L.Intensity*Expected,.01f);}for(const auto& G:ControlledGlows){const float Expected=GetRoomWatts(G.Room)/GetRoomRatedWatts(G.Room);OK &=G.Material.IsValid()&&G.Mesh.IsValid()&&G.Mesh->GetMaterial(G.Slot)==G.Material.Get()&&FMath::IsNearlyEqual(G.Material->K2_GetScalarParameterValue(G.Parameter),Expected,.001f);}return OK;};
    auto Original=[&](){bool OK=true;for(int32 I=0;I<CourseworkRooms::Count;++I)OK &=IsRoomLightOn(I)==R.On[I]&&FMath::IsNearlyEqual(GetRoomBrightness(I),R.Brightness[I],.0001f);return OK&&Physical();};
    auto Finish=[&](){FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Packaged controller input and real Slate handlers; isolated save profile\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("EnergyAcceptance.json")));TestMode.Empty();QuitApplication();};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.Started=R.At=Now;RestoreEnergyComparison();ResetKitchenAlarm();
        for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,I%3!=0,.25f+.07f*I);
        R.On=RoomLightOn;R.Brightness=RoomBrightness;R.Watts=0;
        const float Ratings[]={48,32,36,24,12,12,12,24,16};
        for(int32 I=0;I<CourseworkRooms::Count;++I)R.Watts+=R.On[I]?Ratings[I]*R.Brightness[I]:0;
        SaveRoomLighting();FFileHelper::LoadFileToString(R.Save,*LightingSavePath);
        Record(TEXT("ratings_are_nine_logical_circuits"),FMath::IsNearlyEqual(GetBaselineWatts(),216.f,.001f));
        Record(TEXT("live_power_matches_actual_room_states"),FMath::IsNearlyEqual(GetTotalWatts(),R.Watts,.001f)&&Physical());
        SetPanel(1);Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();return;
    }
    if(Now-R.Started>160){Record(TEXT("complete_before_timeout"),false);Finish();return;}
    if(Now-R.At<.35||GFrameCounter-R.Frame<3)return;
    R.At=Now;R.Frame=GFrameCounter;
    switch(R.Stage++)
    {
    case 0:Record(TEXT("energy_tab_pointer"),Click(570,45)&&PanelMode==8);Shot(TEXT("Energy_current.png"));break;
    case 1:Record(TEXT("duration_8h_pointer"),Click(1350,212)&&GetEnergyHours()==8);Record(TEXT("same_duration_kwh_formula"),FMath::IsNearlyEqual(GetEstimatedKWh(),R.Watts*8/1000,.0001f));break;
    case 2:Record(TEXT("all_on_pointer_captures_original"),Click(380,680)&&bEnergyComparisonActive&&EnergySavedOn==R.On&&EnergySavedBrightness==R.Brightness);break;
    case 3:Record(TEXT("all_on_real_lights_and_216w"),FMath::IsNearlyEqual(GetTotalWatts(),216.f,.001f)&&Physical());Shot(TEXT("Energy_all_on.png"));break;
    case 4:Record(TEXT("eco_pointer_lowers_actual_power"),Click(560,680)&&FMath::IsNearlyEqual(GetTotalWatts(),98.2f,.001f)&&Physical());Record(TEXT("switching_scenario_keeps_first_snapshot"),EnergySavedOn==R.On&&EnergySavedBrightness==R.Brightness);Click(1260,212);Shot(TEXT("Energy_eco.png"));break;
    case 5:Record(TEXT("four_hour_baseline_is_0864kwh"),FMath::IsNearlyEqual(GetEstimatedKWh(true),.864f,.0001f));Record(TEXT("four_hour_eco_is_03928kwh"),FMath::IsNearlyEqual(GetEstimatedKWh(),.3928f,.0001f));R.Position=Player->GetActorLocation();Record(TEXT("energy_room_choice_no_teleport"),Click(150,295)&&FVector::Dist(R.Position,Player->GetActorLocation())<1);break;
    case 6:Record(TEXT("flow_map_pointer"),Click(1160,680)&&PanelMode==5&&IsEnergyMapLayer());Shot(TEXT("Energy_map_ground.png"));break;
    case 7:Record(TEXT("map_layer_real_pointer_toggle"),Overlay->ExerciseEnergyMapButton()&&!IsEnergyMapLayer());Key(EKeys::G,true);Key(EKeys::G,false);break;
    case 8:Record(TEXT("map_layer_keyboard_toggle"),IsEnergyMapLayer());SelectMapFloor(1);Shot(TEXT("Energy_map_upper.png"));break;
    case 9:
    {
        SetPanel(0);Player->TeleportTo(CourseworkRooms::Get(2).Position,FRotator::ZeroRotator,false,false);
        FVector Center(-1841.305,9366.689,275.831),Eye=Player->GetPawnViewLocation();FRotator View;
        for(int32 I=0;I<4;++I){const FRotator Aim=(Center-Eye).Rotation();PC->SetControlRotation(Aim);Player->SetActorRotation(FRotator(0,Aim.Yaw,0));PC->PlayerCameraManager->UpdateCamera(0);PC->GetPlayerViewPoint(Eye,View);}break;
    }
    case 10:UpdateInteraction();Record(TEXT("kitchen_fixture_actual_target"),TargetLightRoom==2);Key(EKeys::E,true);R.At=Now+.35;break;
    case 11:PC->InputKey(FInputKeyParams(EKeys::MouseWheelAxis,-1.,1.f/60,1));break;
    case 12:Record(TEXT("e_wheel_updates_energy_live"),FMath::IsNearlyEqual(GetTotalWatts(),96.4f,.001f)&&FMath::IsNearlyEqual(GetRoomBrightness(2),.60f,.001f)&&Physical());Record(TEXT("manual_edits_keep_restorable_snapshot"),EnergyScenario==3&&EnergySavedBrightness==R.Brightness);Key(EKeys::E,false);SetPanel(8);Shot(TEXT("Energy_manual.png"));break;
    case 13:Record(TEXT("restore_pointer_exact_original"),Click(750,680)&&!bEnergyComparisonActive&&Original());Shot(TEXT("Energy_restored.png"));break;
    case 14:ApplyEnergyScenario(true);SetPanel(0);Record(TEXT("comparison_survives_menu_close"),bEnergyComparisonActive&&FMath::IsNearlyEqual(GetTotalWatts(),98.2f,.001f));break;
    case 15:SetPanel(8);Click(380,680);Record(TEXT("switch_again_preserves_snapshot"),EnergySavedBrightness==R.Brightness);Record(TEXT("restore_after_multiple_switches"),RestoreEnergyComparison()&&Original());break;
    case 16:ApplyEnergyScenario(true);OpenEnergyMap();TriggerKitchenAlarm();Record(TEXT("fire_restores_precomparison_lighting"),IsSafetyDrillRunning()&&!bEnergyComparisonActive&&Original());Record(TEXT("fire_disables_energy_layer"),!IsEnergyMapLayer());Shot(TEXT("Energy_fire_interrupts_map.png"));break;
    case 17:SetPanel(8);Click(380,680);Click(560,680);Record(TEXT("fire_blocks_scenario_buttons"),!bEnergyComparisonActive&&Original());break;
    case 18:OpenEnergyMap();Record(TEXT("fire_blocks_energy_map_entry"),PanelMode==8&&!IsEnergyMapLayer());ToggleMap();Shot(TEXT("Energy_safety_priority.png"));break;
    case 19:ResetKitchenAlarm();SetPanel(8);Record(TEXT("scenarios_work_after_drill_reset"),ApplyEnergyScenario(true)&&bEnergyComparisonActive);RestoreEnergyComparison();break;
    case 20:SetRoomLighting(2,false,.61f);Record(TEXT("off_room_power_is_zero"),GetRoomWatts(2)==0&&GetRoomBrightness(2)>.6f);Record(TEXT("other_room_power_is_independent"),FMath::IsNearlyEqual(GetRoomWatts(1),32.f*R.Brightness[1],.001f));break;
    case 21:SetRoomLighting(2,R.On[2],R.Brightness[2]);SaveRoomLighting();Record(TEXT("manual_state_restored"),Original());break;
    case 22:SetEnergyHours(0);Record(TEXT("duration_lower_bound"),GetEnergyHours()==1);SetEnergyHours(100);Record(TEXT("duration_upper_bound"),GetEnergyHours()==24);SetEnergyHours(std::numeric_limits<float>::quiet_NaN());Record(TEXT("nonfinite_duration_ignored"),GetEnergyHours()==24);SetEnergyHours(4);break;
    case 23:ApplyEnergyScenario(true);SetAllRoomLighting(false);Record(TEXT("all_off_zero_power_and_energy"),GetTotalWatts()==0&&GetEstimatedKWh()==0&&Physical());Record(TEXT("baseline_stays_fixed_when_lights_off"),FMath::IsNearlyEqual(GetEstimatedKWh(true),.864f,.0001f));OpenEnergyMap();SelectMapFloor(0);Shot(TEXT("Energy_map_all_off.png"));break;
    case 24:SetPanel(8);Record(TEXT("restore_retains_off_and_last_brightness"),RestoreEnergyComparison()&&Original());SaveRoomLighting();FFileHelper::LoadFileToString(R.Save,*LightingSavePath);ApplyEnergyScenario(true);break;
    case 25:{SaveRoomLighting();FString Current;FFileHelper::LoadFileToString(Current,*LightingSavePath);Record(TEXT("preview_never_writes_saved_configuration"),!R.Save.IsEmpty()&&Current==R.Save);Shot(TEXT("Energy_before_quit.png"));break;}
    default:Record(TEXT("quit_during_preview_is_exercised"),bEnergyComparisonActive);Finish();break;
    }
}
