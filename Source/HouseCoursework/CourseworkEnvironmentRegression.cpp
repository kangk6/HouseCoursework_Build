#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "TimerManager.h"

// Reproduces the combinations in the 2026-10-18 demo. Screenshots complement
// state checks: a nonzero light intensity alone is not proof of visible light.
void ACourseworkHUD::TickEnvironmentRegression()
{
    if(!bLightingReady)return;
    struct FRun{int32 Step=0;double At=0;TWeakObjectPtr<UStaticMeshComponent> Door;TWeakObjectPtr<AActor> Blocker;};
    static FRun R;
    auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    const double Now=GetWorld()->GetRealTimeSeconds();
    auto Check=[this](const FString& Name,bool Pass){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Pass?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("ENVIRONMENT_CHECK %s"),*TestResults.Last());};
    auto Place=[PC,Player](FVector Position,FVector Focus){Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation((Focus-Player->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);};
    auto Later=[this](TFunction<void()> Action){FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,FTimerDelegate::CreateLambda(MoveTemp(Action)),.3f,false);};
    auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name,true,false);};
    auto PathLit=[&](int32 Room){for(const auto& F:SmartPathFills)if(F.Room==Room&&F.Light.IsValid()&&F.Light->IsVisible()&&!F.Light->GetOwner()->IsHidden()&&F.Light->Intensity>1)return true;return false;};
    auto AimDoor=[&](float X,float Z){if(auto* C=R.Door.Get()){const FBox B=C->GetStaticMesh()->GetBoundingBox();FVector Local=B.GetCenter();Local.X+=B.GetExtent().X*X;Local.Z+=B.GetExtent().Z*Z;const FVector Aim=C->GetComponentTransform().TransformPosition(Local);Place(FVector(Aim.X,Aim.Y-150,150),Aim);}};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=Now;SetPanel(0);SetSmartLightingScene(0);ResetKitchenAlarm();RestoreEnergyComparison();RestoreDaylight();
        // This profile is isolated from the normal user's lighting save.
        SetAllRoomLighting(true);
        Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();Player->SetActorEnableCollision(false);
        Place(FVector(-2100,9870,151.5),FVector(-2350,9870,90));
        for(const auto& Pair:ClosedTransforms)if(auto* C=Cast<UStaticMeshComponent>(Pair.Key.Get()))if(C->GetOwner()->GetClass()->GetName().Contains(TEXT("Door_Outside_B"))&&C->GetName()==TEXT("Door")){R.Door=C;break;}
        return;
    }
    if(Now-R.At<4)return;R.At=Now;
    const int32 Step=R.Step++;
    if(Step>=18&&Step<=26)
    {
        UpdateInteraction();Check(FString::Printf(TEXT("entrance_glass_grid_%d"),Step-18),R.Door.IsValid()&&TargetComponent==R.Door.Get());
        if(Step<26)AimDoor(((Step-17)%3-1)*.5f,((Step-17)/3-1)*.5f);
        else {Shot(TEXT("Environment_entrance_glass.png"));Later([this](){BeginInteract();EndInteract();});}
        return;
    }
    switch(Step)
    {
    case 0:Check(TEXT("startup_is_night"),NeedsNightLighting());Shot(TEXT("Environment_default_night.png"));Later([this](){SetDaylightHour(21);});break;
    case 1:Check(TEXT("manual_21_is_night"),NeedsNightLighting()&&GetDaylightHour()==21);Shot(TEXT("Environment_manual_night.png"));Later([this](){SetDaylightHour(12);});break;
    case 2:Check(TEXT("noon_is_day"),!NeedsNightLighting());TriggerKitchenAlarm();Check(TEXT("noon_fire_keeps_clock"),IsSafetyDrillRunning()&&IsDaylightActive()&&GetDaylightHour()==12);break;
    case 3:
        Check(TEXT("noon_fire_keeps_rendered_sun"),Daylight&&Daylight->GetSunIntensity()>10);
        Check(TEXT("noon_fire_has_no_unbuilt_lighting"),GetWorld()->NumLightingUnbuiltObjects==0);
        if(GetWorld()->NumLightingUnbuiltObjects)PC->ConsoleCommand(TEXT("DumpUnbuiltLightInteractions"),true);
        Shot(TEXT("Environment_noon_fire.png"));
        if(FParse::Param(FCommandLine::Get(),TEXT("HouseEnvProbe"))){TestMode.Empty();Later([this](){QuitApplication();});return;}
        Later([this,Place,Check](){ResetKitchenAlarm();Check(TEXT("reset_keeps_noon"),GetDaylightHour()==12);SetDaylightHour(18+10.f/60);SetSmartLightingScene(1);Place(FVector(-2260,9455,410),FVector(-2114,9470,335));});break;
    case 4:Check(TEXT("1810_after_sunset_enables_guidance"),NeedsNightLighting()&&SmartLightingScene==1&&PathLit(1));Shot(TEXT("Environment_1810_bedroom.png"));Later([Place](){Place(FVector(-2114,9777,415),FVector(-2114,9970,335));});break;
    case 5:Check(TEXT("1810_bedroom_exit_lit"),PathLit(1));Shot(TEXT("Environment_1810_bedroom_door.png"));Later([Place](){Place(FVector(-1793,9117,413),FVector(-2010,9130,335));});break;
    case 6:Check(TEXT("1810_child_exit_lit"),PathLit(3));Shot(TEXT("Environment_1810_child_exit.png"));Later([Place](){Place(FVector(-1990,10020,150),FVector(-1990,10300,80));});break;
    case 7:Check(TEXT("1810_hall_lit"),PathLit(8));Shot(TEXT("Environment_1810_hall.png"));Later([this](){SetDaylightHour(12);});break;
    case 8:Check(TEXT("noon_guidance_sleeps"),!NeedsNightLighting()&&!PathLit(8));SetDaylightHour(21);break;
    case 9:Check(TEXT("manual_21_guidance_resumes"),NeedsNightLighting()&&PathLit(8));Shot(TEXT("Environment_21_hall.png"));Check(TEXT("environment_stays_active_until_restored"),IsDaylightActive());Later([this](){SetSmartLightingScene(0);RestoreDaylight();SetPanel(9);});break;
    case 10:Check(TEXT("environment_restores_exactly"),Daylight->CheckRestored());Check(TEXT("night_dial_pointer"),Overlay->ExerciseTimeSelection(21)&&GetDaylightHour()==21);SetPanel(6);break;
    case 11:Check(TEXT("switching_tab_does_not_change_clock"),GetDaylightHour()==21);SetPanel(9);break;
    case 12:Check(TEXT("dial_1810_pointer"),Overlay->ExerciseTimeSelection(18+10.f/60)&&FMath::IsNearlyEqual(GetDaylightHour(),18+10.f/60,.001f));SetPanel(6);break;
    case 13:Check(TEXT("night_scene_pointer_after_slider"),Overlay->ExerciseControlPointer({1040,380})&&SmartLightingScene==1&&NeedsNightLighting());SetPanel(0);SetDaylightHour(21);TriggerKitchenAlarm();Check(TEXT("night_fire_keeps_clock_and_exits_smart"),GetDaylightHour()==21&&SmartLightingScene==0&&IsSafetyDrillRunning());break;
    case 14:Check(TEXT("night_fire_has_no_unbuilt_lighting"),GetWorld()->NumLightingUnbuiltObjects==0);if(GetWorld()->NumLightingUnbuiltObjects)PC->ConsoleCommand(TEXT("DumpUnbuiltLightInteractions"),true);ResetKitchenAlarm();RestoreDaylight();Check(TEXT("night_fire_reset_keeps_night_selection"),GetDaylightHour()==21);break;
    case 15:Check(TEXT("entrance_leaf_found"),R.Door.IsValid());AimDoor(0,0);break;
    case 16:UpdateInteraction();Check(TEXT("entrance_glass_center_selectable"),TargetComponent==R.Door.Get());
        if(R.Door.IsValid())
        {const FVector Center=R.Door->Bounds.Origin;auto* A=GetWorld()->SpawnActor<AActor>();auto* B=NewObject<UBoxComponent>(A);A->SetRootComponent(B);A->AddInstanceComponent(B);B->SetBoxExtent(FVector(60,10,60));B->SetCollisionEnabled(ECollisionEnabled::QueryOnly);B->SetCollisionResponseToAllChannels(ECR_Ignore);B->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);B->RegisterComponent();A->SetActorLocation(Center+FVector(0,-70,0));R.Blocker=A;}break;
    case 17:UpdateInteraction();Check(TEXT("entrance_cannot_select_through_blocker"),!TargetActor.IsValid());if(R.Blocker.IsValid())R.Blocker->Destroy();AimDoor(-.5f,-.5f);break;
    case 27:Check(TEXT("entrance_E_opens_from_glass"),R.Door.IsValid()&&IsFurnitureOpen(R.Door.Get()));AimDoor(0,0);break;
    case 28:UpdateInteraction();Check(TEXT("open_leaf_glass_still_selectable"),TargetComponent==R.Door.Get());BeginInteract();EndInteract();break;
    case 29:Check(TEXT("entrance_E_closes_from_glass"),R.Door.IsValid()&&!IsFurnitureOpen(R.Door.Get()));break;
    default:FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("EnvironmentRegression.json")));TestMode.Empty();QuitApplication();break;
    }
}
