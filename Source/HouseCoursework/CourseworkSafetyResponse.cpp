#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"

void ACourseworkHUD::InitializeSafetyIsolator()
{
 SafetyIsolator=GetWorld()->SpawnActor<AActor>();
 auto* Root=NewObject<USceneComponent>(SafetyIsolator);SafetyIsolator->SetRootComponent(Root);SafetyIsolator->AddInstanceComponent(Root);Root->RegisterComponent();
 FVector Point(-1900,10080,215),Normal(-1,0,0);FHitResult Wall;FCollisionQueryParams Q(SCENE_QUERY_STAT(SafetySwitchWall),true);
 if(GetWorld()->LineTraceSingleByChannel(Wall,{-1990,10080,215},{-1750,10080,215},ECC_Visibility,Q)&&Wall.ImpactNormal.X<-.7f){Point=Wall.ImpactPoint+Wall.ImpactNormal*2;Normal=Wall.ImpactNormal;}
 Root->SetWorldLocation(Point);Root->SetWorldRotation(Normal.Rotation());
 auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto* White=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 auto Add=[&](FVector P,FVector Scale,UMaterialInterface* M)
 {
  auto* C=NewObject<UStaticMeshComponent>(SafetyIsolator);SafetyIsolator->AddInstanceComponent(C);C->SetupAttachment(Root);C->SetStaticMesh(Cube);C->SetMaterial(0,M);C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);C->SetCollisionResponseToAllChannels(ECR_Ignore);C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);C->SetCanEverAffectNavigation(false);C->SetCastShadow(false);C->SetRelativeLocation(P);C->SetRelativeScale3D(Scale);C->RegisterComponent();return C;
 };
 Add({0,0,0},{.04f,.24f,.34f},White);
 SafetyIsolatorLever=Add({5,0,3},{.05f,.075f,.15f},White);
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/Safety/M_SensorSignal.M_SensorSignal"));
 SafetyIsolatorSignal=UMaterialInstanceDynamic::Create(Parent,this);Add({3,0,12},{.012f,.05f,.025f},SafetyIsolatorSignal);
 UpdateSafetyIsolator();
 UE_LOG(LogTemp,Display,TEXT("SAFETY_ISOLATOR position=%s normal=%s"),*Point.ToString(),*Normal.ToString());
}
void ACourseworkHUD::UpdateSafetyIsolator()
{
 if(SafetyIsolatorLever){SafetyIsolatorLever->SetRelativeLocation({5,0,KitchenIncident.Isolated?-3.f:3.f});SafetyIsolatorLever->SetRelativeRotation({KitchenIncident.Isolated?25.f:-25.f,0,0});}
 if(SafetyIsolatorSignal)SafetyIsolatorSignal->SetVectorParameterValue(TEXT("SignalColor"),KitchenIncident.Isolated?FLinearColor(.015f,.18f,.065f):IsSafetyDrillRunning()?FLinearColor(.6f,.12f,.01f):FLinearColor(.015f,.045f,.02f));
}
bool ACourseworkHUD::SetSafetyTarget(const FHitResult& Hit)
{
 if(!SafetyIsolator||Hit.GetActor()!=SafetyIsolator||Hit.Distance>230||!IsSafetyDrillRunning()||KitchenIncident.Isolated)return false;
 bTargetSafetyIsolator=true;TargetActor=SafetyIsolator;TargetLabel=TEXT("厨房加热回路");
 if(Overlay)Overlay->SetInteraction(TEXT("厨房加热回路  ·  按 E 断开"));return true;
}
bool ACourseworkHUD::IsolateKitchenHeat()
{
 if(!IsSafetyDrillRunning()||KitchenIncident.Isolated)return false;
 KitchenIncident.Isolate();UpdateSafetyIsolator();
 UE_LOG(LogTemp,Display,TEXT("SAFETY_ISOLATED elapsed=%.2f ignited=%d"),KitchenIncident.Elapsed,KitchenIncident.Ignited);return true;
}
bool ACourseworkHUD::CanFinishSafety()const
{return SafetyState==3||(SafetyState==1&&KitchenIncident.Cleared());}
FString ACourseworkHUD::GetSafetyDeviceStatus()const
{return KitchenIncident.Isolated?TEXT("已断开"):IsSafetyDrillRunning()?TEXT("持续加热"):TEXT("待机");}
FString ACourseworkHUD::GetSafetyHazardStatus()const
{
 if(!IsSafetyDrillRunning())return TEXT("正常");
 if(KitchenIncident.Ignited)return TEXT("火情持续");
 if(KitchenIncident.Cleared())return TEXT("预警已解除");
 return KitchenIncident.Isolated?TEXT("余烟消散"):TEXT("过热冒烟");
}
