#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"

void ACourseworkHUD::TickFixtureAcceptance()
{
    if(!bLightingReady)return;
    struct FRun {int32 Room=0,Phase=0,Stage=0,FixtureIndex=0,FixturePhase=0,ReachPhase=0;double At=0,Started=0;uint64 Frame=0;bool BackfaceChecked=false;TWeakObjectPtr<UBoxComponent> Fixture;TWeakObjectPtr<AActor> Blocker;};
    static FRun R;const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    auto Record=[&](FString Name,bool Passed){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Passed?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("FIXTURE_CHECK %s"),*TestResults.Last());};
    auto Key=[&](FKey K,bool Down){PC->InputKey(FInputKeyParams(K,Down?IE_Pressed:IE_Released,Down?1.:0.));};
    auto Wheel=[&](double D){PC->InputKey(FInputKeyParams(EKeys::MouseWheelAxis,D,1.f/60,1));};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle Timer;GetWorld()->GetTimerManager().SetTimer(Timer,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.08f,false);};
    auto Physical=[&](int32 Room){const float Expected=IsRoomLightOn(Room)?GetRoomBrightness(Room):0;bool Good=true;for(const auto& L:ControlledLights)if(L.Room==Room)Good &=L.Component.IsValid()&&FMath::IsNearlyEqual(L.Component->Intensity,L.Intensity*Expected,.01f);for(const auto& G:ControlledGlows)if(G.Room==Room)Good &=G.Material.IsValid()&&FMath::IsNearlyEqual(G.Material->K2_GetScalarParameterValue(G.Parameter),Expected,.01f);return Good;};
    auto HallFixture=[](const FString& Name){return Name==TEXT("SM_Lamp_E2")||Name==TEXT("SM_Lamp_E3")||Name==TEXT("SM_Lamp_E4")||Name==TEXT("SM_Lamp_C1_2");};
    auto Aim=[&](int32 Room,int32 OnlyFixture=INDEX_NONE,float MaxRange=210.f,float MinRange=0.f,int32 Pose=0)
    {
        FVector Preset=CourseworkRooms::Get(Room).Position;const FVector EyeOffset=Player->GetPawnViewLocation()-Player->GetActorLocation();R.Fixture.Reset();const FVector PreviousPosition=Player->GetActorLocation();
        const auto* Capsule=Player->GetCapsuleComponent();const float Half=Capsule->GetScaledCapsuleHalfHeight();
        for(int32 Index=0;Index<LightFixtures.Num();++Index)
        {
            const auto& F=LightFixtures[Index];if(F.Room!=Room||!F.Proxy.IsValid()||(OnlyFixture!=INDEX_NONE&&Index!=OnlyFixture))continue;
            const FVector Center=F.Proxy->Bounds.Origin;Preset.Z=Center.Z<300?150:413;TArray<FVector> Candidates;if(Pose==0)Candidates.Add(Preset);
            for(float Radius:{40.f,60.f,75.f,90.f,110.f,145.f,190.f,210.f,280.f,300.f,330.f})for(int32 I=0;I<16;++I){const float A=I*PI/8+Pose*PI;Candidates.Add(FVector(Center.X+Radius*FMath::Cos(A),Center.Y+Radius*FMath::Sin(A),Preset.Z));}
            for(auto Position:Candidates)
            {
                if(Pose>0&&FVector::Dist2D(Position,PreviousPosition)<25)continue;
                FCollisionQueryParams Query(SCENE_QUERY_STAT(FixtureTestAim),true,Player);FHitResult Floor,Hit;
                if(!GetWorld()->LineTraceSingleByChannel(Floor,Position+FVector(0,0,40),Position-FVector(0,0,180),ECC_Visibility,Query)||Floor.ImpactNormal.Z<.8||LightFixtureRooms.Contains(Floor.GetComponent()))continue;
                Position.Z=Floor.ImpactPoint.Z+Half+1;
                if(FMath::Abs(Position.Z-Preset.Z)>40||GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Half-2),Query))continue;
                const FVector Eye=Position+EyeOffset;
                if(FVector::Dist(Eye,F.Proxy->Bounds.GetBox().GetClosestPointTo(Eye))>MaxRange||FVector::Dist(Eye,Center)<MinRange)continue;
                if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,Center,ECC_Visibility,Query)&&Hit.Distance<MaxRange+5&&Hit.GetActor()==F.Proxy->GetOwner())
                {
                    if(!Player->TeleportTo(Position,FRotator::ZeroRotator,false,false))continue;
                    Player->GetCharacterMovement()->StopMovementImmediately();R.Fixture=F.Proxy;
                    FVector ActualEye=Player->GetPawnViewLocation();FRotator View;
                    for(int32 Pass=0;Pass<4;++Pass){const FRotator AimRotation=(Center-ActualEye).Rotation();PC->SetControlRotation(AimRotation);Player->SetActorRotation(FRotator(0,AimRotation.Yaw,0));PC->PlayerCameraManager->UpdateCamera(0);PC->GetPlayerViewPoint(ActualEye,View);}
                    UpdateInteraction();if(TargetActor.Get()!=F.Proxy->GetOwner()||TargetLightRoom!=Room)continue;
                    UE_LOG(LogTemp,Display,TEXT("FIXTURE_AIM room=%d actor=%s position=%s eye=%s"),Room,*F.Proxy->GetOwner()->GetName(),*Position.ToString(),*ActualEye.ToString());return true;
                }
            }
        }
        return false;
    };
    auto Finish=[&](){CancelLightInteraction();if(R.Blocker.IsValid())R.Blocker->Destroy();FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Packaged controller E press/release and wheel axis events; real visibility traces; no physical mouse claim\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("FixtureAcceptance.json")));TestMode.Empty();QuitApplication();};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=R.Started=Now;SetPanel(0);ResetAllRoomLighting();Player->GetCharacterMovement()->DisableMovement();
        bool ProxySafe=true,Complete=true;TArray<FString> Inventory;
        for(const auto& F:LightFixtures)if(F.Proxy.IsValid()&&F.Mesh.IsValid())
        {ProxySafe &=F.Proxy->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore;const FVector C=F.Proxy->Bounds.Origin,E=F.Proxy->Bounds.BoxExtent;Inventory.Add(FString::Printf(TEXT("{\"room\":%d,\"actor\":\"%s\",\"component\":\"%s\",\"center\":[%.3f,%.3f,%.3f],\"extent\":[%.3f,%.3f,%.3f]}"),F.Room,*F.Mesh->GetOwner()->GetName(),*F.Mesh->GetName(),C.X,C.Y,C.Z,E.X,E.Y,E.Z));}
        for(int32 Room=0;Room<CourseworkRooms::Count;++Room){bool Found=false;for(const auto& F:LightFixtures)Found |=F.Room==Room;Complete &=Found;}
        Record(TEXT("all_nine_zones_have_fixture"),Complete);Record(TEXT("fixture_proxies_ignore_pawn"),ProxySafe);
        for(const TCHAR* Name:{TEXT("SM_TableLamp_A_26"),TEXT("SM_TableLamp_A1_32")})
        {
            bool Emitter=false,Bulb=false;
            for(const auto& L:ControlledLights)if(L.Component.IsValid()&&L.Component->GetOwner()->GetName()==Name&&L.Component->GetName()==TEXT("CourseworkDeskLampLight"))Emitter=L.Intensity>0&&L.Component->CastShadows;
            for(const auto& G:ControlledGlows)if(G.Mesh.IsValid()&&G.Mesh->GetOwner()->GetName()==Name)Bulb=true;
            Record(FString(TEXT("desk_has_local_emitter_"))+Name,Emitter);Record(FString(TEXT("desk_has_controlled_bulb_"))+Name,Bulb);
        }
        // A room state/array-only test can pass with a decorative lamp missing
        // both emission and a nearby real light. Require positive coverage for
        // each registered fixture, then separately inspect rendered differences.
        for(const auto& F:LightFixtures)if(F.Mesh.IsValid())
        {
            const FString Name=F.Mesh->GetOwner()->GetName();bool Emitter=false,Glow=false;int32 OwnLights=0;
            for(const auto& L:ControlledLights)if(L.Component.IsValid()&&L.Room==F.Room&&L.Intensity>0)
            {
                const bool Own=L.Component->GetOwner()==F.Mesh->GetOwner();
                Emitter|=Own||F.Mesh->Bounds.GetBox().ExpandBy(50).IsInside(L.Component->GetComponentLocation());
                if(Own&&L.Component->CastShadows)++OwnLights;
            }
            for(const auto& G:ControlledGlows)if(G.Mesh.IsValid()&&G.Material.IsValid()&&G.Mesh->GetOwner()==F.Mesh->GetOwner())Glow=true;
            Record(TEXT("positive_emitter_coverage_")+Name,Emitter);Record(TEXT("positive_glow_coverage_")+Name,Glow);
            if(HallFixture(Name))Record(TEXT("hall_two_local_shadow_lights_")+Name,OwnLights==2);
        }
        SetRoomLighting(0,true,.42f);SetRoomLighting(0,true,0);Record(TEXT("zero_api_closes_actual_lights"),!IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.42f,.0001f)&&Physical(0));ResetAllRoomLighting();
        FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(Inventory,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("FixtureInventory.json")));return;
    }
    if(Now-R.Started>200){Record(TEXT("complete_before_timeout"),false);Finish();return;}
    // Cover every fixture, including the kitchen pendant; one fixture per zone hid gaps.
    if(R.FixtureIndex<LightFixtures.Num())
    {
        const auto& F=LightFixtures[R.FixtureIndex];const FString Name=F.Mesh->GetOwner()->GetName();
        if(FParse::Param(FCommandLine::Get(),TEXT("HouseDeskProbe"))&&!Name.StartsWith(TEXT("SM_TableLamp_"))){++R.FixtureIndex;return;}
        if(FParse::Param(FCommandLine::Get(),TEXT("HouseHallProbe"))&&!HallFixture(Name)){++R.FixtureIndex;return;}
        if(Now-R.At<.08||GFrameCounter-R.Frame<2)return;
        R.At=Now;R.Frame=GFrameCounter;
        switch(R.FixturePhase++)
        {
        case 0:
            Record(TEXT("reachable_")+Name,Aim(F.Room,R.FixtureIndex,330,0));SetRoomLighting(F.Room,true,.65f);break;
        case 1:
            UpdateInteraction();Record(TEXT("selected_")+Name,TargetActor==F.Mesh->GetOwner()&&TargetLightRoom==F.Room);Key(EKeys::E,true);break;
        case 2:Key(EKeys::E,false);break;
        case 3:
            Record(TEXT("tap_")+Name,!IsRoomLightOn(F.Room)&&Physical(F.Room));
            Record(TEXT("off_feedback_")+Name,Overlay->CheckLightFeedback(F.Room,false));
            Shot(*FString::Printf(TEXT("Fixture_Inspection_%02d_off.png"),R.FixtureIndex));R.At=Now+.3;break;
        case 4:SetRoomLighting(F.Room,true,.65f);Key(EKeys::E,true);R.At=Now+.35;break;
        case 5:Wheel(-1);break;
        case 6:
            Record(TEXT("dim_")+Name,bLightDimming&&FMath::IsNearlyEqual(GetRoomBrightness(F.Room),.60f,.0001f)&&Physical(F.Room));
            Record(TEXT("dim_feedback_")+Name,Overlay->CheckLightFeedback(F.Room,true));
            Shot(*FString::Printf(TEXT("Fixture_Inspection_%02d_dim.png"),R.FixtureIndex));R.At=Now+.3;break;
        case 7:Key(EKeys::E,false);if(Name==TEXT("SM_Lamp_B1_2")){Shot(TEXT("Fixture_kitchen.png"));R.At=Now+.3;}break;
        case 8:Record(TEXT("alternate_stance_")+Name,Aim(F.Room,R.FixtureIndex,330,0,1));break;
        case 9:UpdateInteraction();Record(TEXT("alternate_selection_")+Name,TargetActor==F.Mesh->GetOwner()&&TargetLightRoom==F.Room);break;
        case 10:
            if(!HallFixture(Name)){ResetAllRoomLighting();++R.FixtureIndex;R.FixturePhase=0;break;}
            // Same camera for all three images. Other room fills and fixtures
            // are disabled so the fixture must actually illuminate surfaces.
            SetAllRoomLighting(false);SetRoomLighting(F.Room,true,1.f);
            for(const auto& L:ControlledLights)if(L.Component.IsValid()&&L.Room==F.Room&&L.Component->GetOwner()!=F.Mesh->GetOwner())L.Component->SetIntensity(0);
            for(const auto& G:ControlledGlows)if(G.Mesh.IsValid()&&G.Material.IsValid()&&G.Room==F.Room&&G.Mesh->GetOwner()!=F.Mesh->GetOwner())G.Material->SetScalarParameterValue(G.Parameter,0);
            R.At=Now+.7;break;
        case 11:Shot(*FString::Printf(TEXT("Fixture_Isolated_%s_on.png"),*Name));R.At=Now+.3;break;
        case 12:
            for(const auto& L:ControlledLights)if(L.Component.IsValid()&&L.Component->GetOwner()==F.Mesh->GetOwner())L.Component->SetIntensity(0);
            R.At=Now+.7;break;
        case 13:Shot(*FString::Printf(TEXT("Fixture_Isolated_%s_glow.png"),*Name));R.At=Now+.3;break;
        case 14:
            for(const auto& G:ControlledGlows)if(G.Mesh.IsValid()&&G.Material.IsValid()&&G.Mesh->GetOwner()==F.Mesh->GetOwner())G.Material->SetScalarParameterValue(G.Parameter,0);
            R.At=Now+.7;break;
        case 15:Shot(*FString::Printf(TEXT("Fixture_Isolated_%s_off.png"),*Name));R.At=Now+.3;break;
        default:ResetAllRoomLighting();++R.FixtureIndex;R.FixturePhase=0;break;
        }
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("HouseDeskProbe"))||FParse::Param(FCommandLine::Get(),TEXT("HouseHallProbe"))){Finish();return;}
    if(!R.BackfaceChecked)
    {
        const FVector Position(-2036.923,9612.171,409),Lamp(-1778.237,9505.020,566.734);
        Player->TeleportTo(Position,FRotator::ZeroRotator,false,false);FVector Eye=Player->GetPawnViewLocation();FRotator View;
        for(int32 I=0;I<4;++I){const FRotator AimRotation=(Lamp-Eye).Rotation();PC->SetControlRotation(AimRotation);Player->SetActorRotation(FRotator(0,AimRotation.Yaw,0));PC->PlayerCameraManager->UpdateCamera(0);PC->GetPlayerViewPoint(Eye,View);}
        UpdateInteraction();Record(TEXT("opaque_back_of_wall_blocks_ceiling_light"),TargetLightRoom==INDEX_NONE);
        R.BackfaceChecked=true;return;
    }
    if(R.ReachPhase<10)
    {
        if(Now-R.At<.1||GFrameCounter-R.Frame<2)return;
        R.At=Now;R.Frame=GFrameCounter;
        int32 Kitchen=INDEX_NONE,Ceiling=INDEX_NONE;
        for(int32 I=0;I<LightFixtures.Num();++I){const FString Name=LightFixtures[I].Mesh->GetOwner()->GetName();if(Name==TEXT("SM_Lamp_B1_2"))Kitchen=I;if(Name==TEXT("SM_Lamp_D2"))Ceiling=I;}
        switch(R.ReachPhase++)
        {
        case 0:Record(TEXT("kitchen_normal_floor_beyond_old_range"),Kitchen!=INDEX_NONE&&Aim(2,Kitchen,345,325));SetRoomLighting(2,true,.65f);break;
        case 1:
        {
            FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);FHitResult OldHit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(OldKitchenReach),true,Player);
            const bool OldHitLight=GetWorld()->LineTraceSingleByChannel(OldHit,Eye,Eye+View.Vector()*230,ECC_Visibility,Query)&&OldHit.GetActor()==R.Fixture->GetOwner();
            UpdateInteraction();Record(TEXT("old_230cm_query_missed_kitchen"),!OldHitLight);Record(TEXT("kitchen_extended_reach_selected"),TargetActor==R.Fixture->GetOwner()&&TargetLightRoom==2);Key(EKeys::E,true);break;
        }
        case 2:Key(EKeys::E,false);break;
        case 3:Record(TEXT("kitchen_extended_reach_toggle"),!IsRoomLightOn(2)&&Physical(2));SetRoomLighting(2,true,.65f);Key(EKeys::E,true);R.At=Now+.35;break;
        case 4:Wheel(-1);break;
        case 5:Record(TEXT("kitchen_extended_reach_dim"),bLightDimming&&FMath::IsNearlyEqual(GetRoomBrightness(2),.60f,.0001f)&&Physical(2));Key(EKeys::E,false);Shot(TEXT("Fixture_kitchen_reach.png"));R.At=Now+.3;break;
        case 6:Record(TEXT("small_ceiling_fixture_reachable"),Ceiling!=INDEX_NONE&&Aim(3,Ceiling));break;
        case 7:
        {
            FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);const FVector Right=FRotationMatrix(View).GetUnitAxis(EAxis::Y);
            const FVector Extent=R.Fixture->Bounds.BoxExtent;
            const float Support=FVector::DotProduct(Extent,Right.GetAbs());
            const FVector Edge=R.Fixture->Bounds.Origin+Right*(Support+4);
            PC->SetControlRotation((Edge-Eye).Rotation());PC->PlayerCameraManager->UpdateCamera(0);break;
        }
        case 8:
        {
            FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);FHitResult Precise;FCollisionQueryParams Query(SCENE_QUERY_STAT(SmallLampAim),true,Player);
            const bool HitFixture=GetWorld()->LineTraceSingleByChannel(Precise,Eye,Eye+View.Vector()*350,ECC_Visibility,Query)&&Precise.GetActor()==R.Fixture->GetOwner();
            UpdateInteraction();Record(TEXT("small_ceiling_near_miss_tolerated"),!HitFixture&&TargetActor==R.Fixture->GetOwner()&&TargetLightRoom==3);Key(EKeys::E,true);R.At=Now+.35;break;
        }
        case 9:Record(TEXT("near_miss_keeps_valid_hold"),bLightDimming&&HeldLightRoom==3);Key(EKeys::E,false);ResetAllRoomLighting();break;
        }
        return;
    }
    if(R.Room<CourseworkRooms::Count)
    {
        if(R.Phase==0){Aim(R.Room);R.At=Now;R.Phase=1;return;}
        if(Now-R.At<.35)return;UpdateInteraction();Record(FString::Printf(TEXT("visible_fixture_zone_%d"),R.Room),TargetLightRoom==R.Room);++R.Room;R.Phase=0;return;
    }
    const bool ShortRelease=R.Stage==2||(R.Stage==5&&R.Phase==0);
    if(Now-R.At<(ShortRelease?.045:.16)||GFrameCounter-R.Frame<(ShortRelease?1:3))return;
    R.At=Now;R.Frame=GFrameCounter;
    switch(R.Stage++)
    {
    case 0:Aim(0);SetRoomLighting(0,true,.65f);UpdateInteraction();Shot(TEXT("Fixture_ready.png"));break;
    case 1:UpdateInteraction();Record(TEXT("living_fixture_selected"),TargetLightRoom==0);Key(EKeys::E,true);break;
    case 2:Record(TEXT("press_does_not_toggle"),bLightKeyHeld&&IsRoomLightOn(0));Key(EKeys::E,false);break;
    case 3:Record(TEXT("short_release_off_preserves_65"),!IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.65f,.0001f)&&Physical(0));Shot(TEXT("Fixture_off.png"));R.At=Now+.25;break;
    case 4:Key(EKeys::E,true);break;
    case 5:if(R.Phase==0){Key(EKeys::E,false);R.Phase=1;--R.Stage;break;}R.Phase=0;Record(TEXT("short_release_restores_65"),IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.65f,.0001f)&&Physical(0));Key(EKeys::E,true);R.At=Now+.3;break;
    case 6:Record(TEXT("long_hold_no_accidental_toggle"),bLightDimming&&IsRoomLightOn(0));Shot(TEXT("Fixture_dimmer.png"));break;
    case 7:if(R.Phase==0){Wheel(-1);R.Phase=1;--R.Stage;break;}R.Phase=0;Record(TEXT("wheel_step_five_percent"),FMath::IsNearlyEqual(GetRoomBrightness(0),.60f,.0001f)&&Physical(0));Key(EKeys::LeftShift,true);break;
    case 8:Wheel(1);break;
    case 9:Record(TEXT("shift_wheel_step_one_percent"),FMath::IsNearlyEqual(GetRoomBrightness(0),.61f,.0001f));Key(EKeys::LeftShift,false);break;
    case 10:Wheel(20);break;
    case 11:Record(TEXT("upper_clamp_100"),IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),1.f,.0001f)&&Physical(0));Wheel(-100);break;
    case 12:Record(TEXT("zero_off_keeps_nonzero_restore"),!IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),1.f,.0001f)&&Physical(0));Shot(TEXT("Fixture_zero.png"));break;
    case 13:if(R.Phase==0){Wheel(1);R.Phase=1;--R.Stage;break;}R.Phase=0;Record(TEXT("wheel_from_zero_reopens_at_five"),IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.05f,.0001f)&&Physical(0));Key(EKeys::LeftShift,true);break;
    case 14:Wheel(-4);break;
    case 15:Record(TEXT("one_percent_is_supported"),FMath::IsNearlyEqual(GetRoomBrightness(0),.01f,.0001f)&&Physical(0));Key(EKeys::LeftShift,false);Key(EKeys::E,false);break;
    case 16:Record(TEXT("long_release_does_not_toggle"),!bLightKeyHeld&&IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.01f,.0001f));Wheel(1);break;
    case 17:Record(TEXT("wheel_without_hold_does_not_dim"),FMath::IsNearlyEqual(GetRoomBrightness(0),.01f,.0001f));SetPanel(6);Overlay->ShowLightingRoom(0);R.At=Now+.7;break;
    case 18:Record(TEXT("tab_changes_shared_state"),Overlay->ExerciseControlPointer(FVector2D(441.34f,477))&&FMath::IsNearlyEqual(GetRoomBrightness(0),.37f,.0001f)&&Physical(0));Shot(TEXT("Fixture_tab_sync.png"));R.At=Now+.25;break;
    case 19:SetPanel(0);Aim(0);break;
    case 20:UpdateInteraction();Record(TEXT("quick_state_after_tab"),TargetLightRoom==0&&FMath::IsNearlyEqual(GetRoomBrightness(0),.37f,.0001f));Key(EKeys::E,true);R.At=Now+.3;break;
    case 21:SetPanel(5);Record(TEXT("map_cancels_held_adjustment"),!bLightKeyHeld&&!bLightDimming);Key(EKeys::E,false);break;
    case 22:SetPanel(0);Aim(0);break;
    case 23:Key(EKeys::E,true);R.At=Now+.3;break;
    case 24:PC->SetControlRotation(PC->GetControlRotation()+FRotator(0,3,0));break;
    case 25:Record(TEXT("slight_look_keeps_locked_zone"),bLightDimming&&HeldLightRoom==0);Player->TeleportTo(CourseworkRooms::Get(2).Position,FRotator::ZeroRotator,false,false);break;
    case 26:Record(TEXT("walking_away_cancels_hold"),!bLightKeyHeld&&!bLightDimming);Key(EKeys::E,false);Aim(0);break;
    case 27:
        if(R.Fixture.IsValid())
        {auto* Block=GetWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Block);Block->SetRootComponent(Box);Block->AddInstanceComponent(Box);Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Box->SetBoxExtent(FVector(12));Box->RegisterComponent();Box->SetWorldLocation((Player->GetPawnViewLocation()+R.Fixture->Bounds.Origin)*.5f);R.Blocker=Block;}
        break;
    case 28:UpdateInteraction();Record(TEXT("opaque_obstacle_blocks_selection"),TargetLightRoom==INDEX_NONE);Key(EKeys::E,true);break;
    case 29:Key(EKeys::E,false);break;
    case 30:Record(TEXT("cannot_operate_light_through_obstacle"),FMath::IsNearlyEqual(GetRoomBrightness(0),.37f,.0001f)&&IsRoomLightOn(0));if(R.Blocker.IsValid())R.Blocker->Destroy();break;
    case 31:UpdateInteraction();Record(TEXT("selection_returns_after_obstacle_removed"),TargetLightRoom==0);Shot(TEXT("Fixture_final.png"));break;
    case 32:SetRoomLighting(0,false,.37f);SetPanel(6);Overlay->ShowLightingRoom(0);Shot(TEXT("Fixture_tab_off.png"));R.At=Now+.35;break;
    case 33:Record(TEXT("tab_off_still_preserves_restore"),!IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.37f,.0001f)&&Physical(0));SetPanel(0);break;
    default:Finish();break;
    }
}
