#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

void ACourseworkHUD::TickSafetyResponseAcceptance()
{
 if(!bLightingReady)return;
 struct FRun{int Stage=0;double At=0;float Cut=0;TArray<bool> On;TArray<float> Levels;};static FRun R;
 auto* PC=GetOwningPlayerController();auto* P=Cast<ACharacter>(PC->GetPawn());const double Now=GetWorld()->GetRealTimeSeconds();
 auto Check=[&](const TCHAR* Name,bool Good){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Good?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("SAFETY_RESPONSE %s"),*TestResults.Last());};
 auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.3f,false);};
 if(TestCase<0)
 {
  TestCase=0;R=FRun();R.At=Now;SetPanel(0);R.On=RoomLightOn;R.Levels=RoomBrightness;
  FCourseworkKitchenIncident Early;Early.Advance(1);const float Before=Early.Smoke;Early.Isolate();Early.Advance(12);
  Check(TEXT("early_isolation_prevents_ignition"),!Early.Ignited&&Early.Fire==0);
  Check(TEXT("early_smoke_and_heat_clear_over_time"),Before>0&&Early.Cleared());
  FCourseworkKitchenIncident Late;Late.Advance(18);Check(TEXT("sustained_heating_ignites_and_alarms"),Late.Ignited&&Late.Sensor>=30&&Late.Fire>0);
  const float Flame=Late.Fire;Late.Isolate();Late.Advance(15);Check(TEXT("cutting_power_does_not_extinguish_burning_fuel"),Late.Ignited&&Late.Fire>=Flame&&!Late.Cleared());
  FCourseworkKitchenIncident A,B;A.Advance(20);for(int I=0;I<600;I++)B.Advance(1.f/30);
  Check(TEXT("incident_independent_of_frame_partition"),FMath::Abs(A.Sensor-B.Sensor)<.2f&&FMath::Abs(A.Fire-B.Fire)<.01f);
  Check(TEXT("sensor_response_lags_actual_smoke"),A.Sensor<A.Smoke);
  Check(TEXT("switch_asset_and_signal_ready"),IsSafetyReady()&&SafetyIsolatorLever&&SafetyIsolatorSignal);
  P->GetCharacterMovement()->DisableMovement();const FVector Target=SafetyIsolator->GetActorLocation();
  Check(TEXT("hall_switch_standing_position_reachable"),P->TeleportTo(FVector(Target.X,Target.Y,150)+SafetyIsolator->GetActorForwardVector()*125,FRotator::ZeroRotator,false,false));
  PC->SetControlRotation((Target-P->GetPawnViewLocation()).Rotation());SetPanel(7);return;
 }
 const double Wait=R.Stage==2?12:R.Stage==5?18:R.Stage==6?3:1.2;
 if(Now-R.At<Wait)return;R.At=Now;
 switch(R.Stage++)
 {
 case 0:Check(TEXT("native_start_button_starts_heating"),Overlay->ExerciseControlPointer({400,613})&&SafetyState==1&&!KitchenIncident.Isolated);SetPanel(0);break;
 case 1:
 {
  const FVector Target=SafetyIsolator->GetActorLocation(),Normal=SafetyIsolator->GetActorForwardVector();
  FHitResult Wall;FCollisionQueryParams Q(SCENE_QUERY_STAT(SafetySwitchMounted),true);Q.AddIgnoredActor(SafetyIsolator);Q.AddIgnoredActor(P);
  Check(TEXT("switch_backplate_mounted_on_wall"),GetWorld()->LineTraceSingleByChannel(Wall,Target,Target-Normal*25,ECC_Visibility,Q)&&FVector::DotProduct(Wall.ImpactNormal,Normal)>.9f);
  const FVector Near=P->GetActorLocation();
  P->SetActorLocation(FVector(Target.X,Target.Y,150)+Normal*400,false);PC->SetControlRotation((Target-P->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);UpdateInteraction();BeginInteract();EndInteract();
  Check(TEXT("distant_switch_cannot_be_operated"),!KitchenIncident.Isolated);
  P->SetActorLocation(Near,false);PC->SetControlRotation((Target-P->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);
  auto* Barrier=GetWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Barrier);Barrier->SetRootComponent(Box);Barrier->AddInstanceComponent(Box);Box->SetBoxExtent({20,20,60});Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Box->RegisterComponent();Box->SetWorldLocation(Target+Normal*60);
  UpdateInteraction();BeginInteract();EndInteract();Check(TEXT("occluded_switch_cannot_be_operated"),!KitchenIncident.Isolated);Barrier->Destroy();
  UpdateInteraction();Check(TEXT("switch_has_native_e_target"),bTargetSafetyIsolator&&TargetLightRoom==INDEX_NONE);
  BeginInteract();EndInteract();R.Cut=KitchenIncident.IsolationAt;
  Check(TEXT("native_e_isolates_heating"),KitchenIncident.Isolated&&R.Cut>=0);
  Check(TEXT("switch_lever_physically_changes_position"),SafetyIsolatorLever->GetRelativeLocation().Z<0);Shot(TEXT("SafetyResponse_isolated.png"));break;
 }
 case 2:
  Check(TEXT("early_native_action_prevents_fire"),!KitchenIncident.Ignited&&(!SafetyFire||!SafetyFire->IsActive()));
  Check(TEXT("native_residual_smoke_clears"),KitchenIncident.Cleared()&&SmokeReading<3);
  Check(TEXT("early_clear_can_finish_without_fake_evacuation"),CanFinishSafety()&&!EvacuatingResident);SetPanel(7);Shot(TEXT("SafetyResponse_early_clear.png"));break;
 case 3:
  Check(TEXT("native_finish_button_accepts_early_clear"),Overlay->ExerciseControlPointer({400,613})&&SafetyState==0);
  Check(TEXT("early_result_records_prevention"),LastEvacuationResult.Contains(TEXT("未发生起火"))&&LastEvacuationResult.Contains(TEXT("报警前")));
  Check(TEXT("reset_rearms_lever_and_source"),!KitchenIncident.Isolated&&SafetyIsolatorLever->GetRelativeLocation().Z>0);Shot(TEXT("SafetyResponse_early_result.png"));break;
 case 4:SetPanel(0);TriggerKitchenAlarm();break;
 case 5:
  Check(TEXT("native_alarm_and_fire_develop_without_intervention"),SafetyState>=2&&KitchenIncident.Ignited&&SafetyFire->IsActive()&&SafetySiren->IsPlaying());
  Check(TEXT("resident_still_evacuates"),ResidentEvacuationState!=0);
  UpdateInteraction();BeginInteract();EndInteract();Check(TEXT("late_e_isolates_without_clearing_fire"),KitchenIncident.Isolated&&KitchenIncident.Ignited&&SafetyFire->IsActive());R.Cut=KitchenIncident.IsolationAt;break;
 case 6:
  Check(TEXT("late_cut_does_not_silence_alarm"),SafetySiren->IsPlaying()&&SmokeReading>=30);
  Check(TEXT("isolation_is_latched_until_reset"),!IsolateKitchenHeat()&&KitchenIncident.IsolationAt==R.Cut);
  Check(TEXT("unassembled_people_cannot_claim_completion"),!CanFinishSafety());SetPanel(7);Shot(TEXT("SafetyResponse_late_panel.png"));break;
 case 7:ResetKitchenAlarm();Check(TEXT("interrupted_result_keeps_uncontrolled_fire"),LastEvacuationResult.Contains(TEXT("火源未控制"))&&LastEvacuationResult.Contains(TEXT("未完成")));Shot(TEXT("SafetyResponse_late_result.png"));break;
 case 8:
  Check(TEXT("reset_clears_effects_and_preserves_daily_preferences"),SafetyState==0&&!SafetyFire->IsActive()&&!SafetySmoke->IsActive()&&RoomLightOn==R.On&&RoomBrightness==R.Levels);
  FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Relative model checks plus native Slate start/finish and real E hit-target dispatch; separate hall placement, not a complete walking route\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("SafetyResponse.json")));TestMode.Empty();QuitApplication();return;
 }
}
