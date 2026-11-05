#include "CourseworkHUD.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
void ACourseworkHUD::TickGaitPreview()
{
 if(!bLightingReady)return;
 struct FRun{ASkeletalMeshActor* Actor=nullptr;ACameraActor* Camera=nullptr;UAnimSequence* Walk=nullptr;double Start=0,Last=0;int Frame=0,Phase=0;float Min[2]={MAX_flt,MAX_flt},Max[2]={-MAX_flt,-MAX_flt};};static FRun R;
 const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();
 if(TestCase<0)
 {
  TestCase=0;R=FRun();R.Start=Now;SetPanel(0);SetDaylightHour(12);SetTimeFlowing(false);
  R.Walk=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Coursework/ResidentMocap/A_ResidentWalk.A_ResidentWalk"));
  if(!R.Walk||!OriginalResident.IsValid()){UE_LOG(LogTemp,Error,TEXT("GAIT_PREVIEW assets missing"));TestMode.Empty();QuitApplication();return;}
  FHitResult Floor;FCollisionQueryParams Query(SCENE_QUERY_STAT(GaitFloor),true);Query.AddIgnoredActor(PC->GetPawn());GetWorld()->LineTraceSingleByChannel(Floor,{-2300,8400,500},{-2300,8400,-300},ECC_Visibility,Query);
  FVector Point(-2300,8400,Floor.bBlockingHit?Floor.ImpactPoint.Z:0);
  auto* Platform=GetWorld()->SpawnActor<AStaticMeshActor>(Point+FVector(0,0,5),FRotator::ZeroRotator);Platform->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Platform->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Platform->SetActorScale3D({3,3,.10f});Platform->SetActorEnableCollision(false);Point.Z+=10;
  auto* Source=OriginalResident->FindComponentByClass<USkeletalMeshComponent>();R.Actor=GetWorld()->SpawnActor<ASkeletalMeshActor>(Point,FRotator(0,-90,0));auto* Mesh=R.Actor->GetSkeletalMeshComponent();Mesh->SetSkeletalMesh(Source->GetSkeletalMeshAsset());for(int I=0;I<Source->GetNumMaterials();I++)Mesh->SetMaterial(I,Source->GetMaterial(I));Mesh->SetWorldScale3D(Source->GetComponentScale());Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;Mesh->PlayAnimation(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Coursework/ResidentBrisk/harper-brisk-linear_Anim.harper-brisk-linear_Anim")),true);
  FVector Eye=Point+FVector(240,210,105);R.Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,(Point+FVector(0,0,80)-Eye).Rotation());PC->SetViewTarget(R.Camera);return;
 }
 if(R.Phase==0&&Now-R.Start>3.5){R.Phase=1;R.Start=Now;R.Frame=0;R.Actor->GetSkeletalMeshComponent()->PlayAnimation(R.Walk,true);}
 auto* Mesh=R.Actor->GetSkeletalMeshComponent();const float H=FMath::Min(Mesh->GetSocketLocation(TEXT("ball_l")).Z,Mesh->GetSocketLocation(TEXT("ball_r")).Z)-R.Actor->GetActorLocation().Z;R.Min[R.Phase]=FMath::Min(R.Min[R.Phase],H);R.Max[R.Phase]=FMath::Max(R.Max[R.Phase],H);
 if(Now-R.Last>.08&&Now-R.Start>.3&&Now-R.Start<3.25){R.Last=Now;FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/GaitPreview/%s_%03d.png"),R.Phase?TEXT("mocap"):TEXT("old"),R.Frame++),false,false);}
 if(R.Phase==1&&Now-R.Start>3.5)
 {
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"old_lower_toe_height_cm\":[%.3f,%.3f],\"mocap_lower_toe_height_cm\":[%.3f,%.3f],\"method\":\"Both raw animation clips on same mesh and ground, no actor translation\"}"),R.Min[0],R.Max[0],R.Min[1],R.Max[1]),*(FPaths::ProjectSavedDir()/TEXT("GaitPreview.json")));
  PC->SetViewTarget(PC->GetPawn());R.Camera->Destroy();R.Actor->Destroy();TestMode.Empty();QuitApplication();
 }
}
