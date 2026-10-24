#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "CourseworkVentilation.h"
#include "CourseworkDaylight.h"
#include "Misc/CommandLine.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"

// Actual packaged CharacterMovement and Slate handlers, with a single initial
// player placement. No mid-route teleports, collision disabling or OS-mouse claim.
void ACourseworkHUD::TickEvacuationAcceptance()
{
    if(!bLightingReady)return;
    struct FRun
    {
        int32 Stage=0;double Started=0,StageAt=0,LastSample=0,DoorAt=-10;
        bool RunSeen=false,NoTeleport=true,CollisionFree=true,Grounded=true,BlockedSeen=false,Shot=false,GatheredShot=false,MapShot=false,DetectorShot=false;
        double MapCloseAt=0,DetectorAt=0,KitchenAt=0;
        float HipMin=MAX_flt,HipMax=-MAX_flt;
        bool KitchenShot=false;
        FVector Previous=FVector::ZeroVector,MinRotation=FVector(MAX_flt),MaxRotation=FVector(-MAX_flt);
        TWeakObjectPtr<AActor> Barrier;
        TWeakObjectPtr<ACameraActor> Camera;
        TWeakObjectPtr<ACameraActor> KitchenCamera;
        double FireHour=0,FireElapsed=0;float Hour=21;bool Flow=false;int32 Scene=1;
    };
    static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();
    auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    auto Record=[&](const TCHAR* Name,bool Passed){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Passed?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("EVAC_CHECK %s"),*TestResults.Last());};
    auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name,true,false);};
    auto Click=[&](float X,float Y){return Overlay->ExerciseControlPointer(FVector2D(X,Y));};
    auto Finish=[&]()
    {
        if(R.Barrier.IsValid())R.Barrier->Destroy();
        if(R.Camera.IsValid()){PC->SetViewTarget(Player);R.Camera->Destroy();}
        if(R.KitchenCamera.IsValid()){PC->SetViewTarget(Player);R.KitchenCamera->Destroy();}
        float Total=0,Maximum=0;for(float Ms:AcceptanceFrameTimes){Total+=Ms;Maximum=FMath::Max(Maximum,Ms);}
        const float FPS=1000.f*AcceptanceFrameTimes.Num()/FMath::Max(Total,1.f);
        const FVector Span=R.MaxRotation-R.MinRotation;
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"mode\":\"%s\",\"checks\":[%s],\"average_fps\":%.3f,\"max_frame_ms\":%.3f,\"animation_rotation_span\":[%.3f,%.3f,%.3f],\"samples\":[%s]}"),*TestMode,*FString::Join(TestResults,TEXT(",")),FPS,Maximum,Span.X,Span.Y,Span.Z,*FString::Join(EvacuationSamples,TEXT(","))),*(FPaths::ProjectSavedDir()/TEXT("EvacuationAcceptance.json")));
        ResetKitchenAlarm();TestMode.Empty();QuitApplication();
    };
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.Started=R.StageAt=Now;
        FParse::Value(FCommandLine::Get(),TEXT("HouseEvacHour="),R.Hour);R.Flow=FParse::Param(FCommandLine::Get(),TEXT("HouseEvacFlow"));R.Scene=R.Hour<18?2:1;
        SetDaylightHour(R.Hour);SetTimeFlowing(R.Flow);
        for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,I%2==0,I==2?1.f:.2f+.07f*I);
        SetSmartLightingScene(R.Scene);SetSmartFixtureManual(TEXT("SM_TableLamp_A1_32"),.42f);
        SetRoomWindows(0,true);Ventilation->SetDisplay(true);
        if(SafetyEntryDoor.IsValid()&&IsFurnitureOpen(SafetyEntryDoor.Get()))InvokeFurniture(SafetyEntryDoor->GetOwner(),SafetyEntryDoor.Get());
        const bool Upper=TestMode==TEXT("evacuation_upper");
        Player->TeleportTo(Upper?FVector(-2280,9470,413):FVector(-1990,10228,150),FRotator::ZeroRotator,false,false);
        Player->GetCharacterMovement()->StopMovementImmediately();
        if(Upper)EvacuationPlayerRoute={FVector(-2114,9470,413),FVector(-2114,9760,413),FVector(-2114,10070,413),FVector(-2190,10070,413),FVector(-2190,10520,150),FVector(-1990,10520,150)};
        EvacuationPlayerRoute.Append({FVector(-1990,9170,150),FVector(-2205,9170,150),FVector(-2205,8890,150),FVector(-2205,8700,AssemblyPoint.Z),AssemblyPoint});
        EvacuationPlayerIndex=0;SetPanel(7);PC->SetControlRotation((GetResidentPosition()-Player->GetPawnViewLocation()).Rotation());
        if(SmokeSensor){const FVector Eye=SmokeSensor->GetActorLocation()-FVector(0,0,80);R.Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,FRotator(90,0,0));if(R.Camera.IsValid())PC->SetViewTarget(R.Camera.Get());}
        Record(TEXT("resources_and_matching_animation"),IsSafetyReady()&&ResidentRunAnimation!=nullptr&&SafetyEntryDoor.IsValid());return;
    }
    if(Now-R.Started>105){Record(TEXT("complete_before_timeout"),false);Shot(TEXT("Evacuation_failure.png"));Finish();return;}
    if(R.Stage>=2&&R.Stage<=3)
    {
        if(AcceptanceLastFrame>0)AcceptanceFrameTimes.Add(float((Now-AcceptanceLastFrame)*1000));AcceptanceLastFrame=Now;
        if(EvacuatingResident&&Now-R.LastSample>.2)
        {
            const FVector P=EvacuatingResident->GetActorLocation();auto* Mesh=EvacuatingResident->GetMesh();auto* Instance=Mesh->GetSingleNodeInstance();
            const bool Running=bResidentRunning&&Instance&&Instance->GetCurrentAsset()==ResidentLocomotion;
            if(Running)
            {
                R.RunSeen=true;const FRotator Bone=Mesh->GetSocketTransform(TEXT("thigh_l"),RTS_Component).Rotator();const FVector V(Bone.Pitch,Bone.Yaw,Bone.Roll);
                R.MinRotation=R.MinRotation.ComponentMin(V);R.MaxRotation=R.MaxRotation.ComponentMax(V);
                const float Hip=Mesh->GetSocketLocation(TEXT("pelvis")).Z-Mesh->GetComponentLocation().Z;
                R.HipMin=FMath::Min(R.HipMin,Hip);R.HipMax=FMath::Max(R.HipMax,Hip);
            }
            if(R.LastSample>0)R.NoTeleport &=FVector::Dist(P,R.Previous)<270.f*float(Now-R.LastSample)+20;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(EvacuationBodyCheck),false,EvacuatingResident);
            const bool Overlap=GetWorld()->OverlapBlockingTestByChannel(P,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(18,82),Query);
            R.CollisionFree &=!Overlap;
            Query.AddIgnoredActor(Player);FHitResult Floor;
            if(GetWorld()->LineTraceSingleByChannel(Floor,P,P-FVector(0,0,160),ECC_Visibility,Query))R.Grounded &=P.Z-88>=Floor.ImpactPoint.Z-3;
            if(P.X<-2100&&P.Y>9900&&P.Y<9970)R.Grounded &=P.Z<166;
            EvacuationSamples.Add(FString::Printf(TEXT("{\"time\":%.3f,\"npc\":[%.3f,%.3f,%.3f],\"speed\":%.3f,\"waypoint\":%d,\"state\":%d,\"run_animation\":%s,\"overlap\":%s}"),AlarmElapsed,P.X,P.Y,P.Z,EvacuatingResident->GetVelocity().Size2D(),ResidentRouteIndex,ResidentEvacuationState,Running?TEXT("true"):TEXT("false"),Overlap?TEXT("true"):TEXT("false")));
            R.Previous=P;R.LastSample=Now;
            if(!R.Shot&&Running&&AlarmElapsed>1.6f){Shot(TEXT("Evacuation_running.png"));R.Shot=true;}
        }
        if(TestMode==TEXT("evacuation_blocked")&&ResidentEvacuationState==4&&!R.BlockedSeen)
        {
            R.BlockedSeen=true;Record(TEXT("blocked_reports_no_false_arrival"),!HasResidentGathered()&&SafetyState==2);
            if(R.Barrier.IsValid())R.Barrier->Destroy();
        }
    }
    auto MovePlayer=[&](FVector Target)
    {
        FVector D=Target-Player->GetActorLocation();D.Z=0;
        if(D.Size()<22&&FMath::Abs(Player->GetActorLocation().Z-Target.Z)<45){Player->GetCharacterMovement()->StopMovementImmediately();return true;}
        if(EvacuatingResident&&FVector::Dist2D(Player->GetActorLocation(),EvacuatingResident->GetActorLocation())<115&&FVector::DotProduct(D.GetSafeNormal(),(EvacuatingResident->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D())>.3f)return false;
        Player->AddMovementInput(D.GetSafeNormal(),1.f,true);return false;
    };
    switch(R.Stage)
    {
    case 0:
        if(Now-R.StageAt<1.7)return;
        R.FireHour=Clock.Hour;R.FireElapsed=Clock.ElapsedSimulationSeconds;
        Record(TEXT("start_pointer_enters_sensor_monitoring"),Click(400,613)&&SafetyState==1&&!SafetySiren->IsPlaying());R.Stage=1;break;
    case 1:
        if(SafetyState!=2)return;
        if(!R.DetectorShot){if(R.DetectorAt==0){SetPanel(0);R.DetectorAt=Now;return;}if(Now-R.DetectorAt<.2)return;Shot(TEXT("Evacuation_detector.png"));R.DetectorShot=true;return;}
        if(AlarmElapsed<.4f)return;SetPanel(7);
        Record(TEXT("sensor_threshold_triggers_alarm"),SmokeReading>=30&&SafetySiren->IsPlaying());
        Record(TEXT("visible_fire_and_local_light"),SafetyFire&&SafetyFire->IsActive()&&SafetyFire->GetNumActiveParticles()>0&&SafetyFireLight&&SafetyFireLight->Intensity>200);
        HandleKitchenAlarm();Record(TEXT("early_finish_rejected"),SafetyState==2);
        Record(TEXT("environment_held_through_alarm"),Clock.Hour==R.FireHour&&Clock.ElapsedSimulationSeconds==R.FireElapsed&&SmartLightingScene==0&&!Ventilation->IsDisplayed());
        {const FVector Before=Player->GetActorLocation();GoToLocation(0);Record(TEXT("navigation_cannot_fake_evacuation"),FVector::Dist(Before,Player->GetActorLocation())<2);}
        SetPanel(0);
        if(R.Camera.IsValid())R.Camera->Destroy();
        R.Camera=GetWorld()->SpawnActor<ACameraActor>(FVector(-2050,9940,220),(GetResidentPosition()+FVector(0,0,30)-FVector(-2050,9940,220)).Rotation());
        if(R.Camera.IsValid())PC->SetViewTarget(R.Camera.Get());
        if(TestMode==TEXT("evacuation_blocked"))
        {
            auto* Barrier=GetWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Barrier);Barrier->SetRootComponent(Box);Barrier->AddInstanceComponent(Box);
            Box->SetBoxExtent(FVector(15,120,90));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();Box->SetWorldLocation(FVector(-2150,9951,150));R.Barrier=Barrier;
        }
        R.Stage=2;R.StageAt=Now;break;
    case 2:
        if(R.Camera.IsValid()&&ResidentRouteIndex>=3){PC->SetViewTarget(Player);R.Camera->Destroy();R.Camera.Reset();}
        if(AlarmElapsed>3.2f&&!R.MapShot&&R.MapCloseAt==0){ToggleMap();SelectMapFloor(TestMode==TEXT("evacuation_upper")?1:0);R.MapCloseAt=Now+.5;return;}
        if(PanelMode==5){if(!R.MapShot&&Now>R.MapCloseAt-.3){Shot(TEXT("Evacuation_map.png"));R.MapShot=true;}if(Now<R.MapCloseAt)return;SetPanel(0);}
        if(AlarmElapsed>7&&!R.KitchenShot)
        {
            if(R.KitchenAt==0){const FVector Eye(-1990,9450,210);R.KitchenCamera=GetWorld()->SpawnActor<ACameraActor>(Eye,(AlarmLocation+FVector(0,0,55)-Eye).Rotation());if(R.KitchenCamera.IsValid())PC->SetViewTarget(R.KitchenCamera.Get());R.KitchenAt=Now;return;}
            if(Now-R.KitchenAt<.3)return;Shot(TEXT("Evacuation_kitchen_fire.png"));R.KitchenShot=true;return;
        }
        if(R.KitchenCamera.IsValid()){if(Now-R.KitchenAt<.8)return;PC->SetViewTarget(Player);R.KitchenCamera->Destroy();R.KitchenCamera.Reset();}
        if(ResidentRouteIndex>=3||HasResidentGathered())
        {
            bool Waiting=false;
            if(TestMode==TEXT("evacuation_upper")&&EvacuationPlayerIndex==1)
            {
                for(const auto& Pair:ClosedTransforms)if(auto* Door=Cast<UStaticMeshComponent>(Pair.Key.Get()))
                    if(Door->GetOwner()->GetName()==TEXT("BP_Door_Inside_A3_8")&&Door->GetName()==TEXT("Door"))
                    {if(!IsFurnitureOpen(Door)&&Now-R.DoorAt>2){InvokeFurniture(Door->GetOwner(),Door);R.DoorAt=Now;}Waiting=Now-R.DoorAt<1.5;}
            }
            if(!Waiting&&EvacuationPlayerIndex<EvacuationPlayerRoute.Num()&&MovePlayer(EvacuationPlayerRoute[EvacuationPlayerIndex]))++EvacuationPlayerIndex;
        }
        if(SafetyState!=3)return;
        Player->GetCharacterMovement()->StopMovementImmediately();
        Record(TEXT("resident_reaches_assembly"),HasResidentGathered()&&FVector::Dist2D(GetResidentPosition(),AssemblyPoint)<170);
        Record(TEXT("player_reaches_assembly"),bPlayerGathered&&GetPlayerFloor()==0&&FVector::Dist2D(Player->GetActorLocation(),AssemblyPoint)<150);
        Record(TEXT("gathering_does_not_clear_alarm"),SafetySmoke->IsActive()&&SafetySiren->IsPlaying());
        Record(TEXT("real_locomotion_animation_seen"),R.RunSeen&&(R.MaxRotation-R.MinRotation).GetMax()>8);
        UE_LOG(LogTemp,Display,TEXT("LOCOMOTION_HIP_VERTICAL_SPAN %.3f cm"),R.HipMax-R.HipMin);
        Record(TEXT("locomotion_hip_bob_under_14cm"),R.HipMax-R.HipMin<14);
        Record(TEXT("npc_collision_free"),R.CollisionFree);Record(TEXT("npc_no_midroute_teleport"),R.NoTeleport);Record(TEXT("npc_not_below_floor"),R.Grounded);
        PC->SetControlRotation((GetResidentPosition()+FVector(0,0,30)-Player->GetPawnViewLocation()).Rotation());
        SetPanel(7);R.Stage=3;R.StageAt=Now;break;
    case 3:
        if(Now-R.StageAt<.8)return;
        if(!R.GatheredShot){Shot(TEXT("Evacuation_gathered.png"));R.GatheredShot=true;return;}
        if(Now-R.StageAt<1.5)return;
        Record(TEXT("end_pointer_after_both_arrive"),Click(420,613)&&SafetyState==0);R.Stage=4;R.StageAt=Now;break;
    case 4:
        if(Now-R.StageAt<2)return;
        Record(TEXT("reset_restores_original_idle"),!EvacuatingResident&&OriginalResident.IsValid()&&!OriginalResident->IsHidden()&&OriginalResident->GetActorTransform().Equals(OriginalResidentTransform,.1f));
        Record(TEXT("reset_clears_fire_and_light"),SafetyFire&&!SafetyFire->IsActive()&&SafetyFireLight&&SafetyFireLight->Intensity==0);
        Record(TEXT("entry_door_restored"),!IsFurnitureOpen(SafetyEntryDoor.Get()));
        Record(TEXT("completed_drill_restores_selected_scene"),SmartLightingScene==R.Scene&&SmartFixtureOverrides.Contains(TEXT("SM_TableLamp_A1_32"))&&FMath::IsNearlyEqual(GetSmartFixtureLevel(TEXT("SM_TableLamp_A1_32")),.42f,.001f));
        Record(TEXT("completed_drill_restores_time_mode"),Clock.bRunning==R.Flow&&(R.Flow?Clock.Hour>R.FireHour:Clock.Hour==R.FireHour));
        Record(TEXT("completed_drill_restores_windows_and_airflow"),GetRoomOpenWindowCount(0)==GetRoomWindowCount(0)&&Ventilation->IsDisplayed());
        {bool Lights=true;for(int32 I=0;I<CourseworkRooms::Count;++I)Lights &=RoomLightOn[I]==(I%2==0)&&FMath::IsNearlyEqual(RoomBrightness[I],I==2?1.f:.2f+.07f*I,.001f);Record(TEXT("normal_lighting_unchanged"),Lights);}
        Record(TEXT("repeat_start_pointer"),Click(400,613)&&SafetyState==1);R.Stage=5;break;
    case 5:
        if(!EvacuatingResident||ResidentRouteIndex<1)return;
        {int32 Count=0;for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("CourseworkEvacuatingResident")))++Count;Record(TEXT("repeat_has_one_resident"),Count==1);}
        Record(TEXT("abort_midrun_restores_idle"),Click(420,613)&&SafetyState==0&&!EvacuatingResident&&!OriginalResident->IsHidden());
        Record(TEXT("repeated_drill_restores_scene_again"),SmartLightingScene==R.Scene&&SmartFixtureOverrides.Contains(TEXT("SM_TableLamp_A1_32"))&&Ventilation->IsDisplayed());R.Stage=6;R.StageAt=Now;break;
    default:
        if(Now-R.StageAt<1)return;Finish();break;
    }
}
