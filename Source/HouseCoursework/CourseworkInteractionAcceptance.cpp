#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/PlayerCameraManager.h"
#include "Math/RotationMatrix.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

void ACourseworkHUD::TickInteractionAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Device=0,Phase=0,Stage=0;double At=0,Started=0;bool Before=false;TWeakObjectPtr<AActor> Blocker;};static FRun R;
    const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    auto Check=[&](FString Name,bool Pass){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Pass?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("INTERACTION_CHECK %s"),*TestResults.Last());};
    auto Key=[&](){PC->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.));PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.));};
    auto View=[&](int32 I,float Distance=120.f)
    {
        const FVector N=I==2?FVector(-1,0,0):FVector(0,-1,0),Aim=Appliances[I].Mesh->Bounds.Origin;
        FVector Position=Aim+N*Distance;Position.Z=I==1?151.5f:413.f;Player->SetActorLocation(Position);
        for(int32 Pass=0;Pass<4;++Pass){PC->SetControlRotation((Aim-Player->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);}
        UpdateInteraction();
    };
    auto Block=[&](bool Thin)
    {
        auto* A=GetWorld()->SpawnActor<AActor>();auto* M=NewObject<UStaticMeshComponent>(A);A->AddInstanceComponent(M);A->SetRootComponent(M);
        M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Thin?TEXT("/Engine/BasicShapes/Plane.Plane"):TEXT("/Engine/BasicShapes/Cube.Cube")));
        M->SetCollisionEnabled(ECollisionEnabled::QueryOnly);M->SetCollisionResponseToAllChannels(ECR_Ignore);M->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);M->SetCanEverAffectNavigation(false);M->RegisterComponent();
        FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);M->SetWorldLocation(Eye+Rotation.Vector()*55);M->SetWorldRotation(FRotationMatrix::MakeFromZ(Rotation.Vector()).Rotator());M->SetWorldScale3D(Thin?FVector(2,2,1):FVector(.5,.5,.1));R.Blocker=A;
    };
    auto Finish=[&](){if(R.Blocker.IsValid())R.Blocker->Destroy();FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Packaged runtime, normal standing eye heights, actual visibility traces, controller E events, thin mesh and solid obstacle cases\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("InteractionAcceptance.json")));TestMode.Empty();QuitApplication();};
    if(TestCase<0){TestCase=0;R=FRun();R.At=R.Started=Now;SetPanel(0);Player->GetCharacterMovement()->DisableMovement();Player->SetActorEnableCollision(false);Check(TEXT("all_appliance_meshes_present"),Appliances.Num()==4);if(Appliances.Num()!=4)Finish();return;}
    if(Now-R.Started>100){Check(TEXT("completed_before_timeout"),false);Finish();return;}
    if(Now-R.At<(R.Phase==2?.04:.5))return;R.At=Now;
    if(R.Device<3)
    {
        const int32 I=R.Device;const FString Prefix=FString::Printf(TEXT("device_%d_"),I);
        switch(R.Phase++)
        {
        case 0:View(I);Check(Prefix+TEXT("reachable_from_standing_height"),TargetAppliance==I);Check(Prefix+TEXT("feedback_matches_next_action"),Overlay->CheckInteractionFeedback(GetApplianceName(I),IsApplianceOn(I)?TEXT("关闭"):TEXT("开启")));R.Before=IsApplianceOn(I);NextInteractionTime=0;Key();break;
        case 1:Check(Prefix+TEXT("e_changes_power_and_feedback"),IsApplianceOn(I)!=R.Before&&Overlay->CheckInteractionFeedback(GetApplianceName(I),IsApplianceOn(I)?TEXT("关闭"):TEXT("开启")));R.Before=IsApplianceOn(I);NextInteractionTime=Now+.25;break;
        case 2:Key();Check(Prefix+TEXT("rapid_repeat_is_debounced"),IsApplianceOn(I)==R.Before);break;
        case 3:View(I,370);Check(Prefix+TEXT("out_of_range_has_no_device_target"),TargetAppliance==INDEX_NONE);break;
        case 4:View(I);Block(true);break;
        case 5:
        {
            FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);FCollisionQueryParams Q(SCENE_QUERY_STAT(AuditThinFace),true,Player);FHitResult Hit;
            const bool Forward=GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Rotation.Vector()*350,ECC_Visibility,Q)&&Hit.GetActor()==R.Blocker.Get();
            UE_LOG(LogTemp,Display,TEXT("INTERACTION_THIN_FORWARD device=%d blocked=%d"),I,Forward);UpdateInteraction();Check(Prefix+TEXT("opaque_thin_backface_hides_target"),TargetAppliance==INDEX_NONE);R.Before=IsApplianceOn(I);NextInteractionTime=0;Key();break;
        }
        case 6:Check(Prefix+TEXT("cannot_toggle_through_thin_backface"),IsApplianceOn(I)==R.Before);R.Blocker->Destroy();View(I);Block(false);break;
        case 7:UpdateInteraction();Check(Prefix+TEXT("solid_obstacle_hides_target"),TargetAppliance==INDEX_NONE);R.Blocker->Destroy();View(I);SetPanel(8);R.Before=IsApplianceOn(I);Key();break;
        case 8:Check(Prefix+TEXT("panel_blocks_world_e"),IsApplianceOn(I)==R.Before);SetPanel(0);break;
        default:++R.Device;R.Phase=0;break;
        }return;
    }
    switch(R.Stage++)
    {
    case 0:View(0);TriggerKitchenAlarm();break;
    case 1:R.Before=IsApplianceOn(0);NextInteractionTime=0;Key();break;
    case 2:Check(TEXT("fire_blocks_appliance_power_change"),IsApplianceOn(0)==R.Before);Check(TEXT("fire_e_has_explanation"),Overlay->HasToastMessage(TEXT("演练期间请优先疏散")));ResetKitchenAlarm();break;
    case 3:View(0);NextInteractionTime=0;R.Before=IsApplianceOn(0);Key();break;
    case 4:Check(TEXT("appliance_e_resumes_after_fire"),IsApplianceOn(0)!=R.Before);break;
    default:Finish();break;
    }
}
