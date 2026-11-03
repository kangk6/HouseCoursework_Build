#include "CourseworkHUD.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ACourseworkHUD::TickFireSpreadAcceptance()
{
 if(!bLightingReady)return;
 struct FRun{int Stage=0;double At=0;ACameraActor* Camera=nullptr;float Prior=0;};static FRun R;
 auto* PC=GetOwningPlayerController();const double Now=GetWorld()->GetRealTimeSeconds();
 auto Check=[&](const TCHAR* Name,bool Good){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Good?TEXT("true"):TEXT("false")));};
 auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name,true,false);};
 if(TestCase<0)
 {
  TestCase=0;R=FRun();R.At=Now;SetPanel(0);
  FCourseworkKitchenIncident Early;Early.Advance(1);Early.Isolate();Early.Advance(110);Check(TEXT("early_isolation_prevents_all_spread"),Early.BurnAge==0&&Early.Spread(0)==0&&Early.Spread(1)==0&&Early.Spread(2)==0);
  FCourseworkKitchenIncident A,B;A.Advance(50);for(int I=0;I<1500;I++)B.Advance(1.f/30);Check(TEXT("spread_independent_of_frame_partition"),FMath::Abs(A.Spread(2)-B.Spread(2))<.01f);
  FCourseworkKitchenIncident Late;Late.Advance(18);Late.Isolate();Late.Advance(60);Check(TEXT("burning_fuel_spreads_after_electrical_isolation"),Late.Spread(2)>.99f);
  Check(TEXT("spread_assets_ready_and_bounded"),SpreadFires.Num()==3&&SpreadFires[0]&&SpreadFires[1]&&SpreadFires[2]&&SpreadSmoke&&SpreadLights.Num()==2);
  PC->GetPawn()->TeleportTo(AssemblyPoint,FRotator::ZeroRotator,false,false);
  FVector Eye(-1930,9585,220);R.Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,(AlarmLocation+FVector(-35,0,40)-Eye).Rotation());PC->SetViewTarget(R.Camera);TriggerKitchenAlarm();return;
 }
 const double Wait=R.Stage==0?18:R.Stage==1?11:R.Stage==2?16:R.Stage==3?18:1;
 if(Now-R.At<Wait)return;R.At=Now;
 switch(R.Stage++)
 {
 case 0:
  Check(TEXT("first_fire_stays_at_stove"),KitchenIncident.Ignited&&SafetyFire->IsActive()&&!SpreadFires[0]->IsActive()&&!SpreadFires[1]->IsActive()&&!SpreadFires[2]->IsActive());
  Check(TEXT("fire_light_visible_in_world"),SafetyFireLight->Intensity>0&&!SafetyFireLight->GetOwner()->IsHidden());Shot(TEXT("FireSpread_01_stove.png"));break;
 case 1:
  Check(TEXT("first_neighbor_grows_before_upper_cabinet"),SpreadFires[0]->IsActive()&&!SpreadFires[2]->IsActive()&&KitchenIncident.Spread(0)>KitchenIncident.Spread(1));R.Prior=SpreadFires[0]->GetComponentScale().X;Shot(TEXT("FireSpread_02_counter.png"));break;
 case 2:
  Check(TEXT("upper_cabinet_and_smoke_join_later"),SpreadFires[1]->IsActive()&&SpreadSmoke->IsActive()&&SpreadFires[0]->GetComponentScale().X>=R.Prior);Shot(TEXT("FireSpread_03_cabinet.png"));break;
 case 3:
  Check(TEXT("final_spread_capped_inside_kitchen"),KitchenIncident.Spread(2)>.99f&&SpreadFires[2]->IsActive()&&SpreadFires[2]->GetComponentScale().X<.7f&&SpreadFires.Num()==3);
  Check(TEXT("resident_reaches_assembly_despite_spread"),HasResidentGathered());Shot(TEXT("FireSpread_04_developed.png"));break;
 case 4:ResetKitchenAlarm();break;
 case 5:
  Check(TEXT("reset_removes_all_spread_and_glow"),!SafetyFire->IsActive()&&!SpreadFires[0]->IsActive()&&!SpreadFires[1]->IsActive()&&!SpreadFires[2]->IsActive()&&!SpreadSmoke->IsActive()&&SpreadLights[0]->Intensity==0&&SpreadLights[1]->Intensity==0&&KitchenIncident.BurnAge==0);Shot(TEXT("FireSpread_05_reset.png"));break;
 case 6:
  PC->SetViewTarget(PC->GetPawn());R.Camera->Destroy();
  FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Native timed progression with fixed kitchen fuel sites, separate model checks, no unrestricted fire simulation\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("FireSpread.json")));TestMode.Empty();QuitApplication();return;
 }
}
