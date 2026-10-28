#include "CourseworkHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Engine/StaticMesh.h"
#include "CourseworkDaylight.h"
#include "Engine/GameViewportClient.h"
#include "EngineGlobals.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "HAL/PlatformApplicationMisc.h"

static FVector AcceptanceAimPoint(UStaticMeshComponent* C,const FVector& Eye,float Height,float Side=0.f)
{
    const FBox Box=C->GetStaticMesh()->GetBoundingBox();
    const FVector E=Box.GetExtent();
    int32 Axis=E.X<E.Y?0:1; if(E.Z<E[Axis]) Axis=2;
    if(E[Axis]<6.f && !(Axis==2 && C->GetOwner()->GetClass()->GetName()==TEXT("BP_Table_A3_C")))
    {
        FVector Local=Box.GetCenter();
        if(Axis!=2) { Local.Z+=E.Z*Height; const int32 Span=Axis==0?1:0; Local[Span]+=E[Span]*Side; }
        else { const int32 Span=E.X>E.Y?0:1; Local[Span]+=E[Span]*Height; }
        const FVector LocalEye=C->GetComponentTransform().InverseTransformPosition(Eye);
        Local[Axis]+=FMath::Sign(LocalEye[Axis]-Local[Axis])*FMath::Max(E[Axis]-0.1f,0.f);
        return C->GetComponentTransform().TransformPosition(Local);
    }
    FVector H=Eye-C->Bounds.Origin; H.Z=0; H.Normalize();
    const FVector WorldE=C->Bounds.BoxExtent;
    const float Face=FMath::Min(WorldE.X/FMath::Max(FMath::Abs(H.X),0.001f),WorldE.Y/FMath::Max(FMath::Abs(H.Y),0.001f));
    return C->Bounds.Origin+H*FMath::Max(Face-0.3f,0.f)+FVector(0,0,WorldE.Z*Height);
}

void ACourseworkHUD::PressAcceptanceE()
{
    GetOwningPlayerController()->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.0));
    bAcceptanceKeyDown=true;
    AcceptanceKeyReleaseAt=GetWorld()->GetRealTimeSeconds()+0.15;
}

bool ACourseworkHUD::AimAcceptanceFurniture(UStaticMeshComponent* C)
{
    auto* PC=GetOwningPlayerController();
    auto* Character=Cast<ACharacter>(PC->GetPawn());
    const FVector Center=C->Bounds.Origin;
    // Placement checks collision; only local interaction cases reset their start.
    const float Radius[]={75,110,150,185};
    const int32 N=AcceptanceCandidate++;
    if(N>=32) return false;
    const float Angle=(N%8)*PI/4;
    const float Levels[]={0.f,0.5f,-0.5f,0.85f}; AcceptanceAimHeight=Levels[N/8];
    const FVector P(Center.X+FMath::Cos(Angle)*Radius[N/8],Center.Y+FMath::Sin(Angle)*Radius[N/8],AcceptanceFurnitureFloorZ);
    FVector PreviousEye; FRotator PreviousView; PC->GetPlayerViewPoint(PreviousEye,PreviousView);
    const FVector EyeOffset=PreviousEye-Character->GetActorLocation();
    if(!Character->TeleportTo(P,FRotator::ZeroRotator,false,false)) return false;
    Character->GetCharacterMovement()->StopMovementImmediately();
    const FVector Eye=Character->GetActorLocation()+EyeOffset;
    PC->SetControlRotation((AcceptanceAimPoint(C,Eye,AcceptanceAimHeight)-Eye).Rotation());
    return true;
}

void ACourseworkHUD::TickComprehensiveAcceptance()
{
    auto* PC=GetOwningPlayerController();
    auto* Character=Cast<ACharacter>(PC->GetPawn());
    const double Now=GetWorld()->GetRealTimeSeconds();
    // Opt-in audit conditions. Legacy furniture checks keep their original
    // environment; normal gameplay never enters this acceptance function.
    struct FRouteAudit
    {
        FString Scenario;bool Configured=false,Started=false;double At=0,WallFrame=0,GpuTotal=0;
        int32 ForegroundFrames=0;FIntPoint Size=FIntPoint::ZeroValue;TArray<FString> FrameRows;
    };static FRouteAudit Audit;
    FParse::Value(FCommandLine::Get(),TEXT("HouseRouteScenario="),Audit.Scenario);
    const bool Audited=TestMode==TEXT("route")&&!Audit.Scenario.IsEmpty();
    if(Audited)
    {
        if(!bLightingReady)return;
        if(!Audit.Configured)
        {
            if(Audit.Scenario!=TEXT("day")&&Audit.Scenario!=TEXT("rain")&&Audit.Scenario!=TEXT("night")&&Audit.Scenario!=TEXT("fire"))
            {UE_LOG(LogTemp,Error,TEXT("Unknown route audit scenario: %s"),*Audit.Scenario);QuitApplication();return;}
            // Legacy screenshot flags may have armed a smart scene already.
            // Establish the daily baseline first, otherwise SetAllRoomLighting
            // manually takes over every room and silently disables path fills.
            SetSmartLightingScene(0);
            InitializeTimeOfDay();SetTimeFlowing(false);SetDaylightHour(Audit.Scenario==TEXT("night")?0:12);
            Daylight->Snap(Audit.Scenario==TEXT("night")?0:12);Weather.Initialize(2471,false);
            Weather.Choose(Audit.Scenario==TEXT("rain")?3:0);Weather.Advance(10,0,false,false);
            SetAllRoomLighting(true);if(Audit.Scenario==TEXT("night"))SetSmartLightingScene(1);
            SetPanel(0);Audit.At=Now;Audit.Configured=true;Audit.FrameRows.Add(TEXT("frame_ms,gpu_ms,stop"));return;
        }
        if(Now-Audit.At<15)return;
        if(!Audit.Started)
        {
            Audit.Started=true;if(Audit.Scenario==TEXT("fire"))TriggerKitchenAlarm();
            UE_LOG(LogTemp,Display,TEXT("ROUTE_AUDIT_BEGIN scenario=%s warmup=15 screenshots=%d offscreen=%d"),*Audit.Scenario,!FParse::Param(FCommandLine::Get(),TEXT("HouseNoRouteScreenshots")),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
        }
    }
    if(bAcceptanceKeyDown && Now>=AcceptanceKeyReleaseAt) { PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.0)); bAcceptanceKeyDown=false; }
    if(TestCase<0)
    {
        TArray<FString> Inventory;
        FParse::Value(FCommandLine::Get(),TEXT("HouseFurnitureCycles="),AcceptanceRequiredCycles);
        AcceptanceRequiredCycles=FMath::Clamp(AcceptanceRequiredCycles,1,3);
        FString Filter; FParse::Value(FCommandLine::Get(),TEXT("HouseFurnitureFilter="),Filter);
        TArray<FString> Filters; Filter.ParseIntoArray(Filters,TEXT("|"),true);
        for(auto& Pair:ClosedTransforms)
        {
            auto* C=Cast<UStaticMeshComponent>(Pair.Key.Get());
            if(!C || !C->GetOwner()->FindFunction(TEXT("FurnitureAction"))) continue;
            if(ResolveFurnitureTarget(C)!=C) continue;
            if(!Filters.IsEmpty() && !Filters.ContainsByPredicate([C](const FString& F){return C->GetOwner()->GetName().Contains(F);})) continue;
            const FVector P=C->Bounds.Origin;
            if(P.X<-2600 || P.X>-1500 || P.Y<8800 || P.Y>10800) continue;
            AcceptanceFurniture.Add(C);
            const FVector E=C->Bounds.BoxExtent;
            Inventory.Add(FString::Printf(TEXT("{\"actor\":\"%s\",\"class\":\"%s\",\"component\":\"%s\",\"center\":[%.3f,%.3f,%.3f],\"extent\":[%.3f,%.3f,%.3f]}"),*C->GetOwner()->GetName(),*C->GetOwner()->GetClass()->GetName(),*C->GetName(),P.X,P.Y,P.Z,E.X,E.Y,E.Z));
        }
        AcceptanceFurniture.Sort([](const auto& A,const auto& B){return A->GetOwner()->GetName()+A->GetName()<B->GetOwner()->GetName()+B->GetName();});
        FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(Inventory,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("FurnitureInventory.json")));
        PC->SetIgnoreLookInput(true);
        if(TestMode==TEXT("furniture")) PC->SetIgnoreMoveInput(true);
        TestCase=0; TestCaseStart=Now; AcceptanceLastFrame=Now;
    }
    if(TestMode==TEXT("furniture"))
    {
        if(TestCase>=AcceptanceFurniture.Num())
        {
            FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("FurnitureAcceptance.json")));
            TestMode.Empty(); QuitApplication(); return;
        }
        auto* C=AcceptanceFurniture[TestCase].Get();
        auto Record=[&](bool Passed,const TCHAR* Reason)
        {
            const FVector B=ClosedTransforms[C].GetLocation(), A=C->GetRelativeLocation();
            const FRotator BR=ClosedTransforms[C].Rotator(),AR=C->GetRelativeRotation();
            TestResults.Add(FString::Printf(TEXT("{\"actor\":\"%s\",\"component\":\"%s\",\"passed\":%s,\"opened\":%s,\"cycles_completed\":%d,\"attached_decor\":%d,\"decor_moved\":%s,\"reason\":\"%s\",\"closed_before\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f],\"closed_after\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f]}"),*C->GetOwner()->GetName(),*C->GetName(),Passed?TEXT("true"):TEXT("false"),bAcceptanceOpened?TEXT("true"):TEXT("false"),AcceptanceCycles,AcceptanceDecorTransforms.Num(),bAcceptanceDecorMoved?TEXT("true"):TEXT("false"),Reason,B.X,B.Y,B.Z,BR.Pitch,BR.Yaw,BR.Roll,A.X,A.Y,A.Z,AR.Pitch,AR.Yaw,AR.Roll));
            UE_LOG(LogTemp,Display,TEXT("FURNITURE_ACCEPTANCE %s"),*TestResults.Last());
            FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("FurnitureAcceptance.json")));
            ++TestCase; AcceptanceCycles=0; AcceptanceCandidate=0; AcceptancePhase=0; bAcceptanceOpened=false; bAcceptanceAimReady=false; bAcceptanceDecorMoved=false; AcceptanceDecorTransforms.Empty(); TestCaseStart=Now;
        };
        if(AcceptancePhase==0 || AcceptancePhase==3)
        {
            if(AcceptancePhase==0 && AcceptanceCandidate==0) AcceptanceFurnitureFloorZ=C->Bounds.Origin.Z>300?413.5f:152.5f;
            if(AcceptancePhase==0 && AcceptanceCandidate==0 && AcceptanceCycles==0)
                for(USceneComponent* Child:C->GetAttachChildren()) if(Child->GetOwner()!=C->GetOwner()) AcceptanceDecorTransforms.Add(Child,Child->GetComponentTransform());
            if(AcceptanceCandidate>=32) { Record(false,AcceptancePhase==0?TEXT("no_closed_aim"):TEXT("no_open_aim")); return; }
            if(AimAcceptanceFurniture(C)) { ++AcceptancePhase; bAcceptanceAimReady=false; TestCaseStart=Now; }
        }
        else if(AcceptancePhase==1 || AcceptancePhase==4)
        {
            if(Now-TestCaseStart<0.25) return;
            if(!bAcceptanceAimReady)
            {
                FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
                PC->SetControlRotation((AcceptanceAimPoint(C,Eye,AcceptanceAimHeight)-Eye).Rotation());
                bAcceptanceAimReady=true; TestCaseStart=Now; return;
            }
            UpdateInteraction();
            if(TargetComponent.Get()!=C)
            {
                FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
                FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(AcceptanceAimDiagnostic),true,Character);
                GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*230,ECC_Visibility,Params);
                UE_LOG(LogTemp,Display,TEXT("ACCEPTANCE_AIM_MISS %s/%s CANDIDATE=%d HIT=%s/%s"),*C->GetOwner()->GetName(),*C->GetName(),AcceptanceCandidate,Hit.GetActor()?*Hit.GetActor()->GetName():TEXT("none"),Hit.GetComponent()?*Hit.GetComponent()->GetName():TEXT("none"));
                --AcceptancePhase; return;
            }
            PressAcceptanceE(); ++AcceptancePhase; TestCaseStart=Now;
        }
        else if(AcceptancePhase==2)
        {
            if(Now-TestCaseStart<1.5) return;
            bAcceptanceOpened=IsFurnitureOpen(C);
            if(!bAcceptanceOpened || !C->IsVisible()) { Record(false,TEXT("did_not_open_visibly")); return; }
            for(const auto& Pair:AcceptanceDecorTransforms) if(Pair.Key.IsValid() && FVector::Dist(Pair.Key->GetComponentLocation(),Pair.Value.GetLocation())>1.f) bAcceptanceDecorMoved=true;
            if(AcceptanceDecorTransforms.Num()>0 && !bAcceptanceDecorMoved) { Record(false,TEXT("decor_did_not_move")); return; }
            AcceptanceCandidate=0; AcceptancePhase=3;
        }
        else if(AcceptancePhase==5)
        {
            if(Now-TestCaseStart<1.5) return;
            bool Closed=C->IsVisible() && !IsFurnitureOpen(C);
            for(const auto& Pair:AcceptanceDecorTransforms) if(Pair.Key.IsValid() && (FVector::Dist(Pair.Key->GetComponentLocation(),Pair.Value.GetLocation())>0.5f || Pair.Key->GetComponentQuat().AngularDistance(Pair.Value.GetRotation())>FMath::DegreesToRadians(0.5f))) Closed=false;
            if(Closed) ++AcceptanceCycles;
            if(Closed && AcceptanceCycles<AcceptanceRequiredCycles)
            {
                AcceptanceCandidate=0; AcceptancePhase=0; bAcceptanceOpened=false; bAcceptanceAimReady=false; TestCaseStart=Now; return;
            }
            Record(Closed,TEXT("open_close_E_input"));
        }
        return;
    }

    struct FStop { const TCHAR* Name; FVector Point; const TCHAR* Door=nullptr; };
    static const FStop Stops[]={
        {TEXT("align_entry"),FVector(-2205,8700,70)},
        {TEXT("porch_steps"),FVector(-2205,8890,150)},
        {TEXT("entry_door"),FVector(-2205,8940,150)},
        {TEXT("inside_entry"),FVector(-2205,9170,150),TEXT("BP_Door_Outside_B_2")},
        {TEXT("kitchen_approach"),FVector(-1740,9170,150)},
        {TEXT("kitchen_aisle"),FVector(-1740,9500,150)},
        {TEXT("kitchen_end"),FVector(-1990,9500,150)},
        {TEXT("kitchen"),FVector(-1990,9450,150)},
        {TEXT("kitchen_exit"),FVector(-2040,9500,150)},
        {TEXT("kitchen_exit_hall"),FVector(-2040,9700,150)},
        {TEXT("hall"),FVector(-1990,9700,150)},
        {TEXT("living_approach"),FVector(-1990,9800,150)},
        {TEXT("living"),FVector(-2100,9870,150)},
        {TEXT("return_hall"),FVector(-1990,9800,150)},
        {TEXT("ground_bath_approach"),FVector(-1990,10228,150)},
        {TEXT("ground_bathroom"),FVector(-1770,10228,150),TEXT("BP_Door_Inside_A_2")},
        {TEXT("ground_bath_return"),FVector(-1990,10228,150)},
        {TEXT("stairs_approach"),FVector(-1990,10580,150)},
        {TEXT("stairs_base"),FVector(-2190,10610,150)},
        {TEXT("upper_landing"),FVector(-2190,10070,413)},
        {TEXT("upper_hall"),FVector(-2050,10070,413)},
        {TEXT("master_entry_approach"),FVector(-2114,9980,413)},
        {TEXT("master_entry"),FVector(-2114,9760,413),TEXT("BP_Door_Inside_A3_8")},
        {TEXT("master_front_hall"),FVector(-2114,9470,413)},
        {TEXT("master_bedroom"),FVector(-2280,9470,413)},
        {TEXT("upper_bath_approach"),FVector(-2395,9470,413)},
        {TEXT("upper_bathroom"),FVector(-2395,9620,413),TEXT("BP_Door_Inside_A4_11")},
        {TEXT("upper_bath_return"),FVector(-2395,9470,413)},
        {TEXT("master_return_hall"),FVector(-2114,9470,413)},
        {TEXT("child_approach_hall"),FVector(-2010,9470,413)},
        {TEXT("child_door_approach"),FVector(-2010,9116,413)},
        {TEXT("child_bedroom"),FVector(-1780,9116,413),TEXT("BP_Door_Inside_A5_14")},
        {TEXT("child_return"),FVector(-2010,9116,413)},
        {TEXT("dressing_approach"),FVector(-2010,9501,413)},
        {TEXT("dressing_room"),FVector(-1780,9501,413),TEXT("BP_Door_Inside_A6_17")},
        {TEXT("dressing_return"),FVector(-2010,9501,413)},
        {TEXT("master_exit_hall"),FVector(-2114,9760,413)},
        {TEXT("upper_hall_return"),FVector(-2114,10070,413)},
        {TEXT("study_door_approach"),FVector(-1892,10070,413)},
        {TEXT("study"),FVector(-1892,10300,413),TEXT("BP_Door_Inside_A2_5")},
        {TEXT("study_return"),FVector(-1892,10070,413)},
        {TEXT("upper_landing_return"),FVector(-2190,10070,413)},
        {TEXT("stairs_down"),FVector(-2190,10520,150)},
        {TEXT("ground_return_hall"),FVector(-1990,10520,150)},
        {TEXT("ground_entry_return"),FVector(-1990,9170,150)},
        {TEXT("entry_return"),FVector(-2205,9170,150)},
        {TEXT("outside_return"),FVector(-2205,8700,70)}
    };
    const double WallNow=FPlatformTime::Seconds();
    if(Now>AcceptanceLastFrame&&(!Audited||Audit.WallFrame>0))
    {
        const float FrameMs=Audited?float((WallNow-Audit.WallFrame)*1000.):float((Now-AcceptanceLastFrame)*1000.f);AcceptanceFrameTimes.Add(FrameMs);
        if(Audited)
        {
            const double GpuMs=FPlatformTime::ToMilliseconds(GGPUFrameTime);Audit.GpuTotal+=GpuMs;
            auto* Viewport=GetWorld()->GetGameViewport();auto Window=Viewport->GetWindow();
            Audit.Size=Viewport->Viewport->GetSizeXY();
            if(Window.IsValid()&&Window->IsActive()&&!Window->IsWindowMinimized()&&FPlatformApplicationMisc::IsThisApplicationForeground())++Audit.ForegroundFrames;
            Audit.FrameRows.Add(FString::Printf(TEXT("%.4f,%.4f,%s"),FrameMs,GpuMs,Stops[FMath::Clamp(TestCase,0,int32(UE_ARRAY_COUNT(Stops))-1)].Name));
        }
        if(FrameMs>100.f)UE_LOG(LogTemp,Display,TEXT("ROUTE_LONG_FRAME stop=%s frame_ms=%.3f"),Stops[FMath::Clamp(TestCase,0,int32(UE_ARRAY_COUNT(Stops))-1)].Name,FrameMs);
    }
    if(Audited)Audit.WallFrame=WallNow;
    AcceptanceLastFrame=Now;
    if(!AcceptancePendingShot.IsEmpty())
    {
        if(Now<AcceptancePauseUntil) return;
        if(!FParse::Param(FCommandLine::Get(),TEXT("HouseNoRouteScreenshots")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/AcceptancePendingShot,true,false);
        AcceptancePendingShot.Empty();
    }
    const FVector P=Character->GetActorLocation();
    FVector Direction=Stops[TestCase].Point-P; Direction.Z=0;
    // Open the entry through the same E input channel, then wait for its animation.
    if(Stops[TestCase].Door && AcceptancePhase<2)
    {
        UStaticMeshComponent* Door=nullptr;
        for(auto& Weak:AcceptanceFurniture) if(Weak->GetOwner()->GetName()==Stops[TestCase].Door && Weak->GetName()==TEXT("Door")) Door=Weak.Get();
        if(Door)
        {
            if(AcceptancePhase==0 && IsFurnitureOpen(Door)) AcceptancePhase=2;
            FVector DoorEye; FRotator DoorView; PC->GetPlayerViewPoint(DoorEye,DoorView);
            const float DoorHeights[]={0.f,0.5f,-0.5f,0.85f};
            const float AimHeight=DoorHeights[int32((Now-TestCaseStart)*2)%4];
            PC->SetControlRotation((AcceptanceAimPoint(Door,DoorEye,AimHeight,0.9f)-DoorEye).Rotation());
            if(AcceptancePhase==0) { UpdateInteraction(); if(TargetComponent.Get()==Door) { PressAcceptanceE(); AcceptancePhase=1; TestCaseStart=Now; } }
            else if(Now-TestCaseStart>1.5) AcceptancePhase=2;
            if(AcceptancePhase<2 && Now-TestCaseStart<6) return;
            AcceptancePhase=2;
        }
    }
    const bool Reached=Direction.Size()<18 && FMath::Abs(P.Z-Stops[TestCase].Point.Z)<30;
    if(Reached || Now-TestCaseStart>16)
    {
        FString Blocker;
        if(!Reached)
        {
            FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(FullRouteBlocker),false,Character);
            GetWorld()->SweepSingleByChannel(Hit,P,P+Direction.GetSafeNormal()*65,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Character->GetCapsuleComponent()->GetScaledCapsuleRadius(),Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Params);
            if(Hit.GetComponent()) Blocker=Hit.GetActor()->GetName()+TEXT("/")+Hit.GetComponent()->GetName();
        }
        TestResults.Add(FString::Printf(TEXT("{\"stop\":\"%s\",\"passed\":%s,\"position\":[%.3f,%.3f,%.3f],\"elapsed\":%.3f,\"blocker\":\"%s\"}"),Stops[TestCase].Name,Reached?TEXT("true"):TEXT("false"),P.X,P.Y,P.Z,Now-TestCaseStart,*Blocker));
        UE_LOG(LogTemp,Display,TEXT("CONTINUOUS_ROUTE %s"),*TestResults.Last());
        const FString StopName=Stops[TestCase].Name;
        const bool NightStairShot=FParse::Param(FCommandLine::Get(),TEXT("HouseSmartNightRoute"))&&(StopName==TEXT("stairs_base")||StopName==TEXT("upper_landing"));
        if(Reached && (StopName==TEXT("kitchen") || StopName==TEXT("living") || StopName.Contains(TEXT("bedroom")) || StopName.Contains(TEXT("bathroom")) || StopName==TEXT("study")||NightStairShot))
        {
            FVector Focus=P+PC->GetControlRotation().Vector()*150;
            if(StopName==TEXT("kitchen")) Focus=FVector(-1668,9400,165);
            if(StopName==TEXT("living")) Focus=FVector(-2335,9925,160);
            if(StopName==TEXT("master_bedroom")) Focus=FVector(-2340,9293,380);
            if(StopName==TEXT("child_bedroom")) Focus=FVector(-1804,9323,375);
            if(StopName==TEXT("ground_bathroom")) Focus=FVector(-1685,10326,140);
            if(StopName==TEXT("upper_bathroom")) Focus=FVector(-2413,9693,400);
            if(StopName==TEXT("study")) Focus=FVector(-1760,10644,430);
            if(NightStairShot)Focus=StopName==TEXT("stairs_base")?FVector(-2190,10200,310):FVector(-2190,10500,120);
            FVector RoomEye; FRotator RoomView; PC->GetPlayerViewPoint(RoomEye,RoomView);
            PC->SetControlRotation((Focus-RoomEye).Rotation());
            Character->GetCharacterMovement()->StopMovementImmediately();
            AcceptancePauseUntil=Now+0.5;
            AcceptancePendingShot=TEXT("Route_")+StopName+TEXT(".png");
        }
        if(!Reached || ++TestCase>=UE_ARRAY_COUNT(Stops))
        {
            float Total=0,Max=0; for(float V:AcceptanceFrameTimes){Total+=V;Max=FMath::Max(Max,V);}
            TArray<float> Sorted=AcceptanceFrameTimes;Sorted.Sort();
            const float P95=Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num()*.95f)-1,0,Sorted.Num()-1)],P99=Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num()*.99f)-1,0,Sorted.Num()-1)];
            int32 LongFrames=0;TArray<FString> FrameRows;FrameRows.Add(TEXT("frame_ms"));
            for(float V:AcceptanceFrameTimes){if(V>100.f)++LongFrames;FrameRows.Add(FString::Printf(TEXT("%.3f"),V));}
            FFileHelper::SaveStringToFile(FString::Join(FrameRows,TEXT("\n")),*(FPaths::ProjectSavedDir()/TEXT("RouteFrameTimes.csv")));
            FString AuditFields;
            if(Audited)
            {
                FFileHelper::SaveStringToFile(FString::Join(Audit.FrameRows,TEXT("\n")),*(FPaths::ProjectSavedDir()/TEXT("RouteAuditFrameTimes.csv")));
                const bool Offscreen=FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen"));
                const bool Screenshots=!FParse::Param(FCommandLine::Get(),TEXT("HouseNoRouteScreenshots"));
                const bool Eligible=!Offscreen&&!Screenshots&&Audit.ForegroundFrames==AcceptanceFrameTimes.Num()&&AcceptanceFrameTimes.Num()>500;
                AuditFields=FString::Printf(TEXT(",\"scenario\":\"%s\",\"warmup_seconds\":15,\"offscreen\":%s,\"foreground_frames\":%d,\"benchmark_eligible\":%s,\"resolution\":[%d,%d],\"gpu_mean_ms\":%.4f"),*Audit.Scenario,Offscreen?TEXT("true"):TEXT("false"),Audit.ForegroundFrames,Eligible?TEXT("true"):TEXT("false"),Audit.Size.X,Audit.Size.Y,Audit.GpuTotal/FMath::Max(1,AcceptanceFrameTimes.Num()));
                bool Conditions=FMath::IsNearlyEqual(GetDaylightHour(),Audit.Scenario==TEXT("night")?0.f:12.f,.01f)&&!Clock.bRunning;
                if(Audit.Scenario==TEXT("rain"))Conditions&=Weather.Rain>.99f;
                if(Audit.Scenario==TEXT("night"))Conditions&=SmartLightingScene==1&&SmartManualRooms.Num()==0&&NeedsNightLighting();
                if(Audit.Scenario==TEXT("fire"))Conditions&=IsSafetyDrillRunning()&&HasResidentGathered();
                AuditFields+=FString::Printf(TEXT(",\"scenario_valid\":%s,\"smart_scene\":%d,\"manual_rooms\":%d"),Conditions?TEXT("true"):TEXT("false"),SmartLightingScene,SmartManualRooms.Num());
            }
            const FString Report=FString::Printf(TEXT("{\"completed\":%s,\"mid_route_teleports\":0,\"screenshots\":%s,\"frames\":%d,\"average_fps\":%.3f,\"max_frame_ms\":%.3f,\"p95_ms\":%.3f,\"p99_ms\":%.3f,\"frames_over_100ms\":%d%s,\"stops\":[%s]}"),Reached?TEXT("true"):TEXT("false"),FParse::Param(FCommandLine::Get(),TEXT("HouseNoRouteScreenshots"))?TEXT("false"):TEXT("true"),AcceptanceFrameTimes.Num(),1000.f*AcceptanceFrameTimes.Num()/FMath::Max(Total,1.f),Max,P95,P99,LongFrames,*AuditFields,*FString::Join(TestResults,TEXT(",")));
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("ContinuousRouteAcceptance.json")));
            TestMode.Empty();
            if(!Reached)
            {
                Character->GetCharacterMovement()->StopMovementImmediately();
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Route_failure.png"),true,false);
                FTimerHandle Handle; GetWorld()->GetTimerManager().SetTimer(Handle,this,&ACourseworkHUD::QuitApplication,1.f,false);
            }
            else QuitApplication();
            return;
        }
        TestCaseStart=Now; AcceptancePhase=0;
        return;
    }
    Character->AddMovementInput(Direction.GetSafeNormal(),1.f);
    PC->SetControlRotation(Direction.Rotation());
}
