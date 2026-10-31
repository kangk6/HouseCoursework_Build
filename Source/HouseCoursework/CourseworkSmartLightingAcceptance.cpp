#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ACourseworkHUD::TickSmartLightingAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Step=0;double At=0;TArray<bool> On;TArray<float> Brightness;TMap<TWeakObjectPtr<ULightComponent>,FLinearColor> Colors;};static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    auto Record=[&](const TCHAR* Name,bool Passed){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Passed?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("SMART_CHECK %s"),*TestResults.Last());};
    auto Place=[&](FVector P,FVector Focus){Player->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation((Focus-Player->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.2f,false);};
    auto Unchanged=[&](){return RoomLightOn==R.On&&RoomBrightness==R.Brightness;};
    auto OriginalLights=[&](){bool Pass=true;for(const auto& L:ControlledLights)if(auto* C=L.Component.Get()){Pass&=FMath::IsNearlyEqual(C->Intensity,L.Intensity*(RoomLightOn[L.Room]?RoomBrightness[L.Room]:0),.01f);if(const auto* Color=R.Colors.Find(C))Pass&=C->GetLightColor().Equals(*Color,.01f);}return Pass;};
    auto RenderMatches=[&](){bool Pass=true;for(const auto& L:ControlledLights)if(auto* C=L.Component.Get())Pass&=FMath::IsNearlyEqual(C->Intensity,L.Intensity*GetSmartFixtureLevel(C->GetOwner()->GetFName()),.01f);for(const auto& G:ControlledGlows)if(G.Material.IsValid()&&G.Mesh.IsValid()){float Value=0;G.Material->GetScalarParameterValue(FMaterialParameterInfo(G.Parameter),Value);Pass&=FMath::IsNearlyEqual(Value,G.Strength*GetSmartFixtureLevel(G.Mesh->GetOwner()->GetFName()),.01f);}return Pass;};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=Now;SetSmartLightingScene(0);RestoreEnergyComparison();RestoreDaylight();ResetKitchenAlarm();
        R.On=RoomLightOn;R.Brightness=RoomBrightness;for(const auto& L:ControlledLights)if(L.Component.IsValid())R.Colors.Add(L.Component.Get(),L.Component->GetLightColor());
        Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();Player->SetActorEnableCollision(false);
        Place(FVector(-2114,9980,413),FVector(-2008,10177,508));SetPanel(6);
        R.Step=-1;return; // Slate needs a rendered geometry before pointer input.
    }
    const double Delay=R.Step==3||R.Step==7?8.:R.Step==15?7.:2.;
    if(Now-R.At<Delay)return;R.At=Now;
    switch(R.Step++)
    {
    case -1:Record(TEXT("scene_pointer_enters_night"),Overlay->ExerciseControlPointer({1040,380})&&SmartLightingScene==1);break;
    case 0:
        Record(TEXT("upper_route_lights_on"),GetSmartFixtureLevel(TEXT("SM_Lamp_E3"))>.15f&&GetSmartFixtureLevel(TEXT("SM_Lamp_C1_2"))>.15f);
        Record(TEXT("lower_route_not_whole_circuit"),GetSmartFixtureLevel(TEXT("SM_Lamp_E2"))<.001f);
        Record(TEXT("bedroom_main_light_sleeps"),GetSmartFixtureLevel(TEXT("SM_Lamp_D_6"))<.001f);
        Record(TEXT("resident_living_room_preserved"),FMath::IsNearlyEqual(GetSmartRoomLevel(0),R.On[0]?R.Brightness[0]:0,.01f));
        Record(TEXT("independent_lights_and_glows_match"),RenderMatches());Record(TEXT("automatic_night_does_not_change_saved_values"),Unchanged());
        Record(TEXT("stair_fill_ready"),SmartStairFills.Num()==4);
        {bool Visible=true;for(const auto& L:SmartStairFills)Visible&=L.IsValid()&&!L->GetOwner()->IsHidden();Record(TEXT("stair_fill_owner_is_visible"),Visible);}
        SetPanel(0);Shot(TEXT("Smart_night_route.png"));break;
    case 1:Place(FVector(-2280,9200,413),FVector(-2300,9290,480));break;
    case 2:Record(TEXT("route_has_departure_delay"),GetSmartFixtureLevel(TEXT("SM_Lamp_E3"))>.15f);break;
    case 3:Record(TEXT("route_fades_out_after_delay"),GetSmartFixtureLevel(TEXT("SM_Lamp_E3"))<.001f);{bool Off=true;for(const auto& L:SmartStairFills)Off&=L.IsValid()&&L->Intensity<.001f;Record(TEXT("stair_render_fill_also_fades_out"),Off);}SetDaylightHour(12);Place(FVector(-2114,9980,413),FVector(-2008,10177,508));break;
    case 4:Record(TEXT("daylight_preserves_daily_lighting"),FMath::IsNearlyEqual(GetSmartTargetLevel(TEXT("SM_Lamp_E3")),R.On[8]?R.Brightness[8]:0,.001f));RestoreDaylight();Place(FVector(-2190,10300,280),FVector(-2300,10030,472));Shot(TEXT("Smart_night_stairs.png"));break;
    case 5:Record(TEXT("stairs_light_current_and_next_landing"),GetSmartFixtureLevel(TEXT("SM_Lamp_E2"))>.15f&&GetSmartFixtureLevel(TEXT("SM_Lamp_E3"))>.15f);{bool Lit=false;for(const auto& L:SmartStairFills)Lit|=L.IsValid()&&L->Intensity>.5f;Record(TEXT("stairs_have_real_local_render_fill"),Lit);}Record(TEXT("manual_fixture_can_take_over"),SetSmartFixtureManual(TEXT("SM_Lamp_E3"),.65f));Place(FVector(-2280,9200,413),FVector(-2300,9290,480));break;
    case 6:Record(TEXT("manual_override_has_no_immediate_fight"),FMath::IsNearlyEqual(GetSmartFixtureLevel(TEXT("SM_Lamp_E3")),.65f,.001f));break;
    case 7:
        Record(TEXT("manual_override_survives_route_timeout"),FMath::IsNearlyEqual(GetSmartFixtureLevel(TEXT("SM_Lamp_E3")),.65f,.001f));SetPanel(6);
        Overlay->ShowLightingRoom(1);Overlay->ExerciseControlPointer({510,600});
        Record(TEXT("night_tab_can_turn_off_occupied_bedroom"),SmartManualRooms.Contains(1)&&GetSmartRoomLevel(1)<.001f);
        Overlay->ExerciseControlPointer({620,600});
        Record(TEXT("night_tab_can_reopen_manual_room"),SmartManualRooms.Contains(1)&&GetSmartRoomLevel(1)>.99f);
        SetRoomLighting(1,R.On[1],R.Brightness[1]);
        Record(TEXT("exit_scene_pointer"),Overlay->ExerciseControlPointer({1040,380})&&SmartLightingScene==0);Record(TEXT("exit_restores_colors_and_levels"),OriginalLights()&&Unchanged());Record(TEXT("reading_scene_pointer"),Overlay->ExerciseControlPointer({1350,380})&&SmartLightingScene==2);break;
    case 8:Record(TEXT("reading_armed_without_remote_activity"),!bSmartReadingOccupied&&FMath::IsNearlyEqual(GetSmartRoomLevel(4),R.On[4]?R.Brightness[4]:0,.01f));Place(FVector(-1840,10530,413),FVector(-1734,10661,440));SetPanel(0);break;
    case 9:
        Record(TEXT("desk_proximity_starts_task_light"),bSmartReadingOccupied&&GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32"))>.8f);
        Record(TEXT("reading_keeps_soft_ambient_below_task_level"),FMath::IsNearlyEqual(GetSmartFixtureLevel(TEXT("PointLight19")),.10f,.001f)&&GetSmartFixtureLevel(TEXT("PointLight19"))<GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32")));
        Record(TEXT("reading_actual_emitters_match"),RenderMatches());Record(TEXT("reading_keeps_other_rooms"),FMath::IsNearlyEqual(GetSmartRoomLevel(0),R.On[0]?R.Brightness[0]:0,.01f));Shot(TEXT("Smart_reading_night.png"));
        {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[this](){SetDaylightHour(12);},.65f,false);}break;
    case 10:
        Record(TEXT("daylight_reduces_desk_supplement"),GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32"))<.5f&&GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32"))>=.29f);Shot(TEXT("Smart_reading_day.png"));
        Place(FVector(-1840,10530,413),FVector(-1734,10661,440));
        {FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[this](){
            UpdateInteraction();const bool Aim=TargetLightFixture.IsValid()&&TargetLightFixture->GetOwner()->GetFName()==FName(TEXT("SM_TableLamp_A1_32"));
            TestResults.Add(FString::Printf(TEXT("{\"case\":\"reading_lamp_aim_uses_real_e_target\",\"passed\":%s}"),Aim?TEXT("true"):TEXT("false")));
            GetOwningPlayerController()->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.));GetOwningPlayerController()->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.));
        },.65f,false);}break;
    case 11:Record(TEXT("manual_e_switches_only_reading_lamp"),SmartFixtureOverrides.Contains(TEXT("SM_TableLamp_A1_32"))&&GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32"))<.001f);SetDaylightHour(17);break;
    case 12:Record(TEXT("daylight_does_not_undo_manual_e"),GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32"))<.001f);SetSmartFixtureManual(TEXT("SM_TableLamp_A1_32"),.42f);break;
    case 13:
        Record(TEXT("manual_dimming_held_until_exit"),FMath::IsNearlyEqual(GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32")),.42f,.001f));Record(TEXT("energy_reads_physical_desk_lamp_rating"),FMath::IsNearlyEqual(GetRoomWatts(4),12.f*.42f,.001f));
        for(int32 I=0;I<2;++I){PC->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.));PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.));}
        Record(TEXT("manual_e_roundtrip_preserves_dimming"),FMath::IsNearlyEqual(GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32")),.42f,.001f));
        Record(TEXT("automatic_reading_does_not_change_saved_values"),Unchanged());Place(FVector(-2280,9200,413),FVector(-2300,9290,480));break;
    case 14:Record(TEXT("reading_departure_has_grace_period"),bSmartReadingOccupied);break;
    case 15:Record(TEXT("leaving_desk_restores_ambient"),!bSmartReadingOccupied&&FMath::IsNearlyEqual(GetSmartFixtureLevel(TEXT("PointLight19")),R.On[4]?R.Brightness[4]:0,.01f));Record(TEXT("manual_lamp_preserved_after_leaving"),FMath::IsNearlyEqual(GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32")),.42f,.001f));SetSmartLightingScene(0);RestoreDaylight();Record(TEXT("reading_exit_restores_exact_baseline"),OriginalLights()&&Unchanged());SetSmartLightingScene(2);SetRoomLighting(4,true,.37f);break;
    case 16:Record(TEXT("tab_room_control_takes_priority"),SmartManualRooms.Contains(4)&&FMath::IsNearlyEqual(GetSmartRoomLevel(4),.37f,.001f));SetSmartLightingScene(0);SetRoomLighting(4,R.On[4],R.Brightness[4]);SetSmartLightingScene(1);TriggerKitchenAlarm();Record(TEXT("fire_exits_lifestyle_scene"),IsSafetyDrillRunning()&&SmartLightingScene==0);Record(TEXT("fire_blocks_scene_reentry"),!SetSmartLightingScene(2));ResetKitchenAlarm();break;
    case 17:SetSmartLightingScene(2);Record(TEXT("energy_comparison_exits_lifestyle"),ApplyEnergyScenario(true)&&SmartLightingScene==0);RestoreEnergyComparison();Record(TEXT("all_baseline_values_restored_at_end"),Unchanged()&&OriginalLights());SetPanel(6);Shot(TEXT("Smart_panel_final.png"));break;
    case 18:SetPanel(0);SetSmartLightingScene(1);Place(FVector(-2260,9455,410),FVector(-2114,9470,335));break;
    case 19:
        Record(TEXT("bedroom_exit_is_not_misclassified_as_hall"),SmartPlayerZone==1);
        Record(TEXT("occupied_bedroom_has_bedside_guide"),FMath::Max(GetSmartFixtureLevel(TEXT("BP_Lamp_C2_ON2")),GetSmartFixtureLevel(TEXT("BP_Lamp_C2_ON_2")))>=.21f);
        Record(TEXT("bedroom_ceiling_stays_asleep"),GetSmartFixtureLevel(TEXT("SM_Lamp_D_6"))<.001f);
        {bool Lit=false,Owners=true;for(const auto& F:SmartPathFills)if(auto* L=F.Light.Get()){Owners&=!L->GetOwner()->IsHidden();if(F.Room==1&&L->Intensity>1)Lit=true;}Record(TEXT("bedroom_has_rendered_path_fill"),Lit);Record(TEXT("path_fill_owners_are_visible"),Owners);}
        Shot(TEXT("Smart_bedroom_exit.png"));break;
    case 20:Place(FVector(-2114,9777,415),FVector(-2114,9970,335));break;
    case 21:
        {bool Lit=false;for(const auto& F:SmartPathFills)if(F.Light.IsValid()&&F.Room==1&&F.Light->Intensity>1)Lit=true;Record(TEXT("bedroom_door_has_path_coverage"),Lit);}
        Shot(TEXT("Smart_bedroom_door.png"));break;
    case 22:Place(FVector(-1793,9117,413),FVector(-2010,9130,335));break;
    case 23:
        {bool Lit=false;for(const auto& F:SmartPathFills)if(F.Light.IsValid()&&F.Room==3&&F.Light->Intensity>1)Lit=true;Record(TEXT("child_exit_has_path_coverage"),Lit);}
        Shot(TEXT("Smart_child_exit.png"));break;
    case 24:Place(FVector(-1990,10020,150),FVector(-1990,10300,80));break;
    case 25:
        Record(TEXT("ground_connection_is_hall"),SmartPlayerZone==8);
        {bool Ground=false,Upper=false;for(const auto& F:SmartPathFills)if(F.Light.IsValid()&&F.Light->Intensity>1){Ground|=F.Light->GetComponentLocation().Z<200&&F.Room==8;Upper|=F.Light->GetComponentLocation().Z>300&&F.Until>GetWorld()->GetTimeSeconds()+4;}Record(TEXT("ground_corridor_has_real_fill"),Ground);Record(TEXT("presence_does_not_refresh_floor_above"),!Upper);}
        Shot(TEXT("Smart_ground_connection.png"));break;
    case 26:SetRoomLighting(8,false,0);break;
    case 27:
        {bool Off=true;for(const auto& F:SmartPathFills)if(F.Room==8)Off&=F.Light.IsValid()&&F.Light->Intensity<.001f;Record(TEXT("manual_room_off_disables_local_path_fill"),Off);}
        SetSmartLightingScene(0);SetRoomLighting(8,R.On[8],R.Brightness[8]);SetSmartLightingScene(1);SetDaylightHour(12);break;
    case 28:
        {bool Off=true;for(const auto& F:SmartPathFills)Off&=F.Light.IsValid()&&F.Light->Intensity<.001f;Record(TEXT("daytime_disables_local_night_fill"),Off);}
        Record(TEXT("daytime_restores_room_baselines"),OriginalLights());SetSmartLightingScene(0);RestoreDaylight();break;
    case 29:
        {bool Off=true;for(const auto& F:SmartPathFills)Off&=F.Light.IsValid()&&F.Light->Intensity<.001f;Record(TEXT("scene_exit_clears_every_path_fill"),Off);}
        Record(TEXT("path_fix_preserves_saved_configuration"),Unchanged()&&OriginalLights());break;
    default:
        FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Actual Slate and E handlers, fixed test placements; not a continuous walking claim\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("SmartLightingAcceptance.json")));
        TestMode.Empty();QuitApplication();break;
    }
}
