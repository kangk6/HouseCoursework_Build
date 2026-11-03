#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Components/WidgetComponent.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/Paths.h"

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
 // A recessed lever, protective rim and equipment name distinguish this
 // isolator from the house's ordinary lighting switches.
 auto Paint=[&](FLinearColor Color){auto* M=UMaterialInstanceDynamic::Create(White,SafetyIsolator);M->SetVectorParameterValue(TEXT("Color"),Color);return M;};
 auto* Shell=Paint({.18f,.21f,.22f});auto* Face=Paint({.57f,.60f,.58f});auto* Dark=Paint({.025f,.032f,.035f});auto* Handle=Paint({.55f,.055f,.025f});
 Add({1,0,0},{.06f,.28f,.38f},Shell);
 Add({4.3f,0,0},{.012f,.25f,.35f},Face);
 Add({5.1f,0,-2},{.012f,.115f,.215f},Dark);
 SafetyIsolatorLever=Add({7,0,3},{.065f,.085f,.10f},Handle);
 for(float Y:{-11.f,11.f})for(float Z:{-15.5f,15.5f})Add({5.1f,Y,Z},{.008f,.012f,.012f},Dark);
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/Safety/M_SensorSignal.M_SensorSignal"));
 SafetyIsolatorSignal=UMaterialInstanceDynamic::Create(Parent,this);Add({5.3f,0,8},{.012f,.035f,.018f},SafetyIsolatorSignal);
 auto* Label=NewObject<UWidgetComponent>(SafetyIsolator);SafetyIsolator->AddInstanceComponent(Label);Label->SetupAttachment(Root);
 Label->SetWidgetSpace(EWidgetSpace::World);Label->SetDrawSize({512,96});Label->SetPivot({.5f,.5f});Label->SetRelativeLocation({5.5f,0,13});Label->SetRelativeScale3D({.037f,.037f,.037f});
 Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);Label->SetCastShadow(false);Label->SetTickMode(ETickMode::Disabled);Label->SetManuallyRedraw(true);Label->RegisterComponent();
 Label->SetSlateWidget(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.07f,.085f,.09f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
 [SNew(STextBlock).Text(FText::FromString(TEXT("厨房加热"))).Font(FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Slate/Fonts/SourceHanSansCN-Regular.otf"),34)).ColorAndOpacity(FLinearColor(.86f,.88f,.84f))]);Label->RequestRenderUpdate();
 UpdateSafetyIsolator();
 UE_LOG(LogTemp,Display,TEXT("SAFETY_ISOLATOR position=%s normal=%s"),*Point.ToString(),*Normal.ToString());
}
void ACourseworkHUD::UpdateSafetyIsolator()
{
 if(SafetyIsolatorLever){SafetyIsolatorLever->SetRelativeLocation({7,0,KitchenIncident.Isolated?-5.f:3.f});SafetyIsolatorLever->SetRelativeRotation({KitchenIncident.Isolated?25.f:-25.f,0,0});}
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
