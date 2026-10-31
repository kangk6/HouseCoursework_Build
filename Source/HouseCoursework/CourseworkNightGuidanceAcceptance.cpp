#include "CourseworkHUD.h"
#include "CourseworkNightGuidance.h"
#include "CourseworkDaylight.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void ACourseworkHUD::TickNightGuidanceAcceptance()
{
 if(!bLightingReady)return;
 struct FRun{int32 Stage=0;double At=0;FVector Start,Heading;TArray<bool> On;TArray<float> Levels;float Speed=0;TWeakObjectPtr<UStaticMeshComponent> Door;};static FRun R;
 auto* PC=GetOwningPlayerController();auto* P=Cast<ACharacter>(PC->GetPawn());auto* Move=P->GetCharacterMovement();const double Now=GetWorld()->GetRealTimeSeconds();
 auto Check=[&](const TCHAR* Name,bool Good){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Good?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("NIGHT_GUIDE_CHECK %s"),*TestResults.Last());};
 auto Fill=[&](float Y)->FSmartPathFill*{for(auto& F:SmartPathFills)if(F.Light.IsValid()&&F.Light->GetComponentLocation().Z<200&&FMath::Abs(F.Light->GetComponentLocation().Y-Y)<2)return &F;return nullptr;};
 auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle Handle;GetWorld()->GetTimerManager().SetTimer(Handle,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.2f,false);};
 auto Place=[&](FVector Location,FRotator View){Move->StopMovementImmediately();const bool Placed=P->TeleportTo(Location,FRotator::ZeroRotator,false,false);PC->SetControlRotation(View);return Placed;};
 if(TestCase<0)
 {
  TestCase=0;R=FRun();R.At=Now;R.On=RoomLightOn;R.Levels=RoomBrightness;R.Speed=Move->MaxWalkSpeed;
  SetPanel(0);SetSmartLightingScene(0);SetTimeFlowing(false);SetDaylightHour(0);Daylight->Snap(0);Move->SetMovementMode(MOVE_Walking);Move->MaxWalkSpeed=100;
  Check(TEXT("place_ground_corridor"),Place({-1990,10020,150},{-15,90,0}));SetSmartLightingScene(1);
  using namespace CourseworkNightGuidance;
  Check(TEXT("forward_node_outranks_side_node"),AheadScore({0,250,0},{0,1,0},true)<AheadScore({200,150,0},{0,1,0},true));
  Check(TEXT("stationary_has_no_directional_prediction"),AheadScore({0,250,0},{0,1,0},false)==MAX_flt);
  Check(TEXT("behind_node_not_predicted"),AheadScore({0,-250,0},{0,1,0},true)==MAX_flt);
  Check(TEXT("gentle_fade_has_fast_rise_and_slow_complete_fall"),Fade(0,1,.1f,true)>1-Fade(1,0,.1f,true)&&Fade(.2f,0,1.f,true)==0);
  for(const auto& Pair:ClosedTransforms)if(auto* C=Cast<UStaticMeshComponent>(Pair.Key.Get()))if(C->GetOwner()->GetFName()==FName(TEXT("BP_Door_Inside_A3_8")))R.Door=C;
  return;
 }
 if(R.Stage==1)P->AddMovementInput({0,1,0},1);
 if(R.Stage==3)P->AddMovementInput({0,-1,0},1);
 const double Wait=R.Stage==1||R.Stage==3?.9:R.Stage==2?.7:R.Stage==4?10.:2.;
 if(Now-R.At<Wait)return;R.At=Now;
 const double GameNow=GetWorld()->GetTimeSeconds();
 switch(R.Stage++)
 {
  case 0:R.Start=P->GetActorLocation();Check(TEXT("stationary_keeps_local_floor_light"),Fill(10020)&&Fill(10020)->Level>.8f);PC->SetControlRotation({-15,180,0});break;
  case 1:
   Check(TEXT("real_character_moved_forward"),P->GetActorLocation().Y>R.Start.Y+20);
   Check(TEXT("heading_uses_body_velocity_not_camera"),bSmartNightWalking&&SmartNightHeading.GetSafeNormal2D().Y>.9f);
   Check(TEXT("next_forward_segment_is_prepared"),Fill(10240)&&Fill(10240)->Until>GameNow+5);
   Check(TEXT("distant_rear_segment_not_refreshed"),Fill(9800)&&Fill(9800)->Until<GameNow);
   Move->StopMovementImmediately();R.Heading=SmartNightHeading;PC->SetControlRotation({-15,90,0});Shot(TEXT("NightGuide_forward.png"));break;
  case 2:Check(TEXT("turning_camera_while_stopped_keeps_heading"),!bSmartNightWalking&&SmartNightHeading.Equals(R.Heading,.01f));PC->SetControlRotation({-15,-90,0});break;
  case 3:
   Check(TEXT("real_turnaround_changes_prediction_direction"),bSmartNightWalking&&SmartNightHeading.GetSafeNormal2D().Y<-.9f);
   Check(TEXT("turnaround_prepares_return_segment"),Fill(9800)&&Fill(9800)->Until>GameNow+5);
   Check(TEXT("old_forward_segment_has_departure_grace"),Fill(10240)&&Fill(10240)->Level>.1f);Move->StopMovementImmediately();break;
  case 4:
   Check(TEXT("occupied_local_segment_does_not_black_out"),Fill(10020)&&Fill(10020)->Level>.8f);
   Check(TEXT("unused_forward_segment_fades_out"),Fill(10240)&&Fill(10240)->Level<.001f);Shot(TEXT("NightGuide_stopped.png"));SetRoomLighting(8,false,0);break;
  case 5:
   {bool Off=true;for(const auto& F:SmartPathFills)if(F.Room==8)Off&=F.Level==0;Check(TEXT("manual_hall_off_overrides_prediction"),Off);}
   SetSmartLightingScene(0);SetRoomLighting(8,R.On[8],R.Levels[8]);Check(TEXT("place_bedroom"),Place({-2260,9455,413},{-15,0,0}));SetSmartLightingScene(1);break;
  case 6:
   Check(TEXT("only_one_bedside_family_selected"),(GetSmartTargetLevel(TEXT("BP_Lamp_C2_ON2"))>.15f)+(GetSmartTargetLevel(TEXT("BP_Lamp_C2_ON_2"))>.15f)==1);
   Check(TEXT("bedroom_ceiling_not_automatically_enabled"),GetSmartTargetLevel(TEXT("SM_Lamp_D_6"))==0);Shot(TEXT("NightGuide_bedroom.png"));
   SetSmartLightingScene(0);if(R.Door.IsValid()&&IsFurnitureOpen(R.Door.Get()))InvokeFurniture(R.Door->GetOwner(),R.Door.Get());Check(TEXT("place_inside_closed_bedroom_door"),Place({-2114,9760,415},{-15,90,0}));SetSmartLightingScene(1);break;
  case 7:
   Check(TEXT("closed_door_blocks_hall_prediction"),R.Door.IsValid()&&GetSmartTargetLevel(TEXT("SM_Lamp_E3"))==0&&GetSmartTargetLevel(TEXT("BP_Lamp_E2_ON_2"))==0);
   if(R.Door.IsValid())InvokeFurniture(R.Door->GetOwner(),R.Door.Get());break;
  case 8:Check(TEXT("opening_door_allows_exit_prelight"),GetSmartTargetLevel(TEXT("SM_Lamp_E3"))>.15f);{bool ExitLit=false;for(const auto& F:SmartPathFills)if(F.Room==8&&F.Light.IsValid()&&F.Light->GetComponentLocation().Z>300&&F.Light->GetComponentLocation().Y>9950&&F.Level>.8f)ExitLit=true;Check(TEXT("open_exit_has_actual_floor_coverage"),ExitLit);}Shot(TEXT("NightGuide_open_door.png"));break;
  case 9:SetSmartLightingScene(0);if(R.Door.IsValid()&&IsFurnitureOpen(R.Door.Get()))InvokeFurniture(R.Door->GetOwner(),R.Door.Get());
   Place({-1990,10020,150},{-15,90,0});SetSmartLightingScene(1);break;
  case 10:
   Check(TEXT("ground_presence_does_not_trigger_upper_floor"),GetSmartTargetLevel(TEXT("SM_Lamp_E3"))==0&&GetSmartTargetLevel(TEXT("BP_Lamp_E2_ON_2"))==0);
   SetSmartLightingScene(0);Check(TEXT("daily_saved_values_preserved"),RoomLightOn==R.On&&RoomBrightness==R.Levels);Move->MaxWalkSpeed=R.Speed;
   FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Two short native CharacterMovement walks and a real stop/camera turn/turnaround; separate test placements for bedroom and closed door, not a continuous house walkthrough\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("NightGuidance.json")));TestMode.Empty();QuitApplication();return;
 }
}
