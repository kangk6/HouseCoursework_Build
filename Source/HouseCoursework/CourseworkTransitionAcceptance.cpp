#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"

// Exercises the same HUD handlers bound to Tab/M/Esc. It does not claim OS
// keyboard replay or Slate focus-event delivery.
void ACourseworkHUD::TickTransitionAcceptance()
{
    const double Now=GetWorld()->GetRealTimeSeconds();if(!bLightingReady||Now<3||Now-TestCaseStart<.55)return;TestCaseStart=Now;
    auto* PC=GetOwningPlayerController();
    auto Record=[&](const TCHAR* Name,bool OK){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,OK?TEXT("true"):TEXT("false")));};
    auto Closed=[&](){return PanelMode==0&&!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored()&&!PC->bShowMouseCursor&&!UGameplayStatics::IsGamePaused(GetWorld());};
    auto Open=[&](int32 Panel){return PanelMode==Panel&&PC->IsMoveInputIgnored()&&PC->IsLookInputIgnored()&&PC->bShowMouseCursor;};
    static float PausedAlarm=0;
    static FVector OldLockPosition;
    auto* Player=Cast<ACharacter>(PC->GetPawn());
    switch(++TestCase)
    {
    case 0:
        SetPanel(0);ResetKitchenAlarm();
        Record(TEXT("coasting_probe_has_clear_floor"),Player->TeleportTo(FVector(-2100,9870,151.5),FRotator::ZeroRotator,false,false));
        OldLockPosition=Player->GetActorLocation();Player->GetCharacterMovement()->Velocity=FVector(180,0,0);
        // Reproduce the previous SetPanel behavior using actual engine calls.
        PC->SetIgnoreMoveInput(true);Player->DisableInput(PC);break;
    case 1:
        Record(TEXT("negative_control_old_input_lock_still_coasts"),FVector::Dist2D(OldLockPosition,Player->GetActorLocation())>.1f);
        PC->SetIgnoreMoveInput(false);Player->EnableInput(PC);
        Player->GetCharacterMovement()->Velocity=FVector(180,0,0);ToggleControls();
        Record(TEXT("opening_panel_clears_existing_walk_velocity"),Player->GetVelocity().IsNearlyZero(.001f));
        Record(TEXT("normal_tab_opens_lighting"),Open(6));OldLockPosition=Player->GetActorLocation();break;
    case 2:
        Record(TEXT("open_panel_does_not_coast_next_frames"),FVector::Dist2D(OldLockPosition,Player->GetActorLocation())<.1f);
        ToggleControls();Record(TEXT("normal_tab_closes_and_releases_input"),Closed());TriggerKitchenAlarm();break;
    case 3:
    {
        ToggleControls();Record(TEXT("smoke_phase_tab_opens_safety"),IsSafetyDrillRunning()&&Open(7));
        // Let Slate paint the new panel before capturing; keep it open while
        // the deferred screenshot is consumed by the renderer.
        FTimerHandle Timer;GetWorld()->GetTimerManager().SetTimer(Timer,[]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Transition_safety_tab.png"),true,false);},.25f,false);
        TestCaseStart=Now+1.f;break;
    }
    case 4:ToggleControls();Record(TEXT("safety_tab_closes"),Closed());break;
    case 5:
        if(!IsSafetyActive()){--TestCase;break;}
        ToggleControls();Record(TEXT("alarm_phase_tab_matches_banner"),Open(7));break;
    case 6:ToggleControls();TogglePause();PausedAlarm=AlarmElapsed;Record(TEXT("escape_pauses_drill"),Open(2)&&UGameplayStatics::IsGamePaused(GetWorld()));break;
    case 7:Record(TEXT("pause_freezes_alarm_clock"),FMath::IsNearlyEqual(PausedAlarm,AlarmElapsed,.001f));ToggleMap();Record(TEXT("map_from_pause_restores_world_tick"),Open(5)&&!UGameplayStatics::IsGamePaused(GetWorld()));break;
    case 8:ToggleMap();Record(TEXT("map_closes_releases_input"),Closed());break;
    case 9:
        for(int32 I=0;I<20;++I){SetPanel(I%2?4:9);SetPanel(0);}
        Record(TEXT("repeated_menu_changes_do_not_stack_input_locks"),Closed());
        Record(TEXT("drill_remains_active_after_menu_changes"),IsSafetyActive());ResetKitchenAlarm();break;
    case 10:ToggleControls();Record(TEXT("after_reset_tab_returns_to_lighting"),!IsSafetyDrillRunning()&&Open(6));break;
    case 11:ToggleControls();Record(TEXT("after_reset_input_released"),Closed());break;
    default:
        FFileHelper::SaveStringToFile(TEXT("{\"method\":\"HUD Tab/M/Esc handlers and actual pause/input states, no OS keyboard replay\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("TransitionAcceptance.json")));
        TestMode.Empty();QuitApplication();break;
    }
}
