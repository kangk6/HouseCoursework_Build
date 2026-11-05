#include "CourseworkHUD.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
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
 struct FRun{ASkeletalMeshActor* Actor=nullptr;ACameraActor* Camera=nullptr;FVector Point;double Start=0,Last=0;int Frame=0,Phase=0;float Min=MAX_flt,Max=-MAX_flt;};static FRun R;
 const TCHAR* Labels[]={TEXT("idle"),TEXT("front"),TEXT("side"),TEXT("back"),TEXT("turn_positive"),TEXT("turn_negative")};
 const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();
 auto BeginPhase=[&]()
 {
  R.Start=Now;R.Frame=0;R.Actor->SetActorRotation(FRotator(0,-90,0));
  auto* Mesh=R.Actor->GetSkeletalMeshComponent();
  Mesh->PlayAnimation(R.Phase==0?ResidentIdleAnimation.Get():R.Phase<4?ResidentRunAnimation.Get():R.Phase==4?ResidentTurnPositive.Get():ResidentTurnNegative.Get(),R.Phase<4);
  if(R.Phase>=4)Mesh->GetSingleNodeInstance()->SetPlaying(false);
  const FVector Offset=R.Phase==2?FVector(0,310,100):R.Phase==3?FVector(-310,0,100):R.Phase>=4?FVector(250,220,105):FVector(310,0,100);
  const FVector Eye=R.Point+Offset;R.Camera->SetActorLocation(Eye);R.Camera->SetActorRotation((R.Point+FVector(0,0,85)-Eye).Rotation());
 };
 if(TestCase<0)
 {
  TestCase=0;R=FRun();SetPanel(0);SetDaylightHour(12);SetTimeFlowing(false);
  if(!ResidentRunAnimation||!ResidentTurnPositive||!ResidentTurnNegative||!OriginalResident.IsValid()){UE_LOG(LogTemp,Error,TEXT("GAIT_PREVIEW assets missing"));TestMode.Empty();QuitApplication();return;}
  FHitResult Floor;FCollisionQueryParams Query(SCENE_QUERY_STAT(GaitFloor),true);Query.AddIgnoredActor(PC->GetPawn());GetWorld()->LineTraceSingleByChannel(Floor,{-2300,8400,500},{-2300,8400,-300},ECC_Visibility,Query);
  R.Point=FVector(-2300,8400,Floor.bBlockingHit?Floor.ImpactPoint.Z:0);
  auto* Platform=GetWorld()->SpawnActor<AStaticMeshActor>(R.Point+FVector(0,0,5),FRotator::ZeroRotator);Platform->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Platform->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Platform->SetActorScale3D({3,3,.10f});Platform->SetActorEnableCollision(false);R.Point.Z+=10;
  auto* Source=OriginalResident->FindComponentByClass<USkeletalMeshComponent>();R.Actor=GetWorld()->SpawnActor<ASkeletalMeshActor>(R.Point,FRotator(0,-90,0));auto* Mesh=R.Actor->GetSkeletalMeshComponent();Mesh->SetSkeletalMesh(Source->GetSkeletalMeshAsset());for(int I=0;I<Source->GetNumMaterials();I++)Mesh->SetMaterial(I,Source->GetMaterial(I));Mesh->SetWorldScale3D(Source->GetComponentScale());Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  R.Camera=GetWorld()->SpawnActor<ACameraActor>();PC->SetViewTarget(R.Camera);BeginPhase();return;
 }
 if(Now-R.Start>3.5)
 {
  if(++R.Phase==6)
  {
   FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"views\":[\"idle\",\"front\",\"side\",\"back\",\"turn_positive\",\"turn_negative\"],\"walk_lower_toe_height_cm\":[%.3f,%.3f],\"method\":\"Native skeletal rendering on same mesh; three walk angles and synchronized two-step turns; sampled images, not a foreground FPS test\"}"),R.Min,R.Max),*(FPaths::ProjectSavedDir()/TEXT("GaitPreview.json")));
   PC->SetViewTarget(PC->GetPawn());R.Camera->Destroy();R.Actor->Destroy();TestMode.Empty();QuitApplication();return;
  }
  BeginPhase();
 }
 auto* Mesh=R.Actor->GetSkeletalMeshComponent();const float Elapsed=Now-R.Start;
 if(R.Phase>=4)
 {
  const float T=FMath::Clamp(Elapsed-.4f,0.f,1.f);const float Yaw=(R.Phase==4?90.f:-90.f)*T*T*(3-2*T);
  R.Actor->SetActorRotation(FRotator(0,-90+Yaw,0));Mesh->GetSingleNodeInstance()->SetPosition(T,false);Mesh->TickAnimation(0,false);Mesh->RefreshBoneTransforms();
 }
 if(R.Phase>0&&R.Phase<4)
 {
  const float H=FMath::Min(Mesh->GetSocketLocation(TEXT("ball_l")).Z,Mesh->GetSocketLocation(TEXT("ball_r")).Z)-R.Point.Z;R.Min=FMath::Min(R.Min,H);R.Max=FMath::Max(R.Max,H);
 }
 if(Now-R.Last>.08&&Elapsed>.1&&Elapsed<3.3){R.Last=Now;FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/GaitPreview/%s_%03d.png"),Labels[R.Phase],R.Frame++),false,false);}
}
