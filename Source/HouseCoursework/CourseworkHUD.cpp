#include "CourseworkHUD.h"
#include "CourseworkVisibility.h"
#include "CourseworkOverlay.h"
#include "CourseworkDaylight.h"
#include "CourseworkVentilation.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "PhysicsEngine/BodySetup.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Particles/ParticleSystem.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"

ACourseworkHUD::ACourseworkHUD()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    InputPriority = 20;
    static ConstructorHelpers::FObjectFinder<UParticleSystem> Smoke(TEXT("/Game/Coursework/Safety/P_KitchenSmoke.P_KitchenSmoke"));
    SafetySmokeTemplate=Smoke.Object;
    static ConstructorHelpers::FObjectFinder<UParticleSystem> Fire(TEXT("/Game/Coursework/Safety/P_KitchenFire.P_KitchenFire"));SafetyFireTemplate=Fire.Object;
    static ConstructorHelpers::FObjectFinder<UBlendSpace1D> Locomotion(TEXT("/Game/Coursework/ResidentBrisk/BS_ResidentEvacuation.BS_ResidentEvacuation"));ResidentLocomotion=Locomotion.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Run(TEXT("/Game/Coursework/ResidentBrisk/harper-brisk-linear_Anim.harper-brisk-linear_Anim"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Coursework/ResidentAnimated/harper-idle-linear_Anim.harper-idle-linear_Anim"));
    ResidentRunAnimation=Run.Object;ResidentIdleAnimation=Idle.Object;
}

void ACourseworkHUD::BeginPlay()
{
    Super::BeginPlay();
    // Epic's 200% TSR history is a large bandwidth cost on integrated GPUs.
    // Keep the scene/output resolution and the other AA features unchanged.
    // Only cap inherited scalability defaults; explicit user overrides win.
    if(!FParse::Param(FCommandLine::Get(),TEXT("HouseLegacyTemporalHistory")))
        if(auto* History=IConsoleManager::Get().FindConsoleVariable(TEXT("r.TSR.History.ScreenPercentage")))
            if(History->GetFloat()>150.f&&(uint32(History->GetFlags())&ECVF_SetByMask)<=ECVF_SetByScalability)
            {
                OriginalTsrHistoryPercentage=History->GetFloat();OriginalTsrHistoryPriority=uint32(History->GetFlags())&ECVF_SetByMask;
                History->SetWithCurrentPriority(150.f);
                UE_LOG(LogTemp,Display,TEXT("RENDER_HISTORY capped original=%.1f current=%.1f"),OriginalTsrHistoryPercentage,History->GetFloat());
            }
    auto* PC = GetOwningPlayerController();
    if (!PC) return;
    FString StartupTest;FParse::Value(FCommandLine::Get(),TEXT("HouseTest="),StartupTest);
    bStartupCover=StartupTest.IsEmpty()||StartupTest==TEXT("startup");
    if(bStartupCover)PC->PlayerCameraManager->SetManualCameraFade(1,FLinearColor::Black,false);
    EnableInput(PC);
    auto& Tab = InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ACourseworkHUD::ToggleControls);
    Tab.bExecuteWhenPaused = true;
    auto& Escape = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ACourseworkHUD::TogglePause);
    Escape.bExecuteWhenPaused = true;
    auto& Map = InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ACourseworkHUD::ToggleMap);
    Map.bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::G,IE_Pressed,this,&ACourseworkHUD::ToggleEnergyMapLayer);
    auto& Action = InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ACourseworkHUD::BeginInteract);
    Action.bConsumeInput = true;
    Action.bExecuteWhenPaused = true;
    auto& Release=InputComponent->BindKey(EKeys::E,IE_Released,this,&ACourseworkHUD::EndInteract);Release.bExecuteWhenPaused=true;
    auto& Wheel=InputComponent->BindAxisKey(EKeys::MouseWheelAxis,this,&ACourseworkHUD::AdjustHeldLight);Wheel.bConsumeInput=false;
    WheelBindingIndex=InputComponent->AxisKeyBindings.Num()-1;
    if (PC->PlayerInput) OriginalMouseSensitivity = PC->PlayerInput->GetMouseSensitivityX();
    if (auto* Character = Cast<ACharacter>(PC->GetPawn())) OriginalWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if(It->ActorHasTag(TEXT("CourseworkResidentBody")))
        {
            auto* Capsule=NewObject<UCapsuleComponent>(*It,TEXT("ResidentBodyCollision"));
            It->AddInstanceComponent(Capsule);
            Capsule->SetupAttachment(It->GetRootComponent());
            Capsule->InitCapsuleSize(20.f,88.f);
            Capsule->SetRelativeLocation(FVector(0,0,88));
            Capsule->SetCollisionProfileName(TEXT("BlockAllDynamic"));
            Capsule->SetCanEverAffectNavigation(false);
            Capsule->RegisterComponent();
        }
        if (!It->FindFunction(TEXT("FurnitureAction"))) continue;
        TArray<UStaticMeshComponent*> Components;
        It->GetComponents(Components);
        // The legacy interior-door timeline writes an absolute local 0..90 yaw.
        // Keep an instance's authored closed pose on a parent hinge, so its
        // existing timeline can operate from local zero without moving that pose.
        if(It->GetClass()->GetName().Contains(TEXT("BP_Door_Inside_")))
        {
            for(auto* Door:Components)
            {
                if(Door->GetName()!=TEXT("Door") || !Door->GetAttachParent() || Door->GetRelativeRotation().IsNearlyZero(.1f)) continue;
                const FTransform ClosedPose=Door->GetRelativeTransform();
                auto* Hinge=NewObject<USceneComponent>(*It,MakeUniqueObjectName(*It,USceneComponent::StaticClass(),TEXT("CourseworkDoorHinge")));
                Hinge->SetMobility(EComponentMobility::Movable);
                It->AddInstanceComponent(Hinge);
                Hinge->SetupAttachment(Door->GetAttachParent(),Door->GetAttachSocketName());
                Hinge->SetRelativeTransform(ClosedPose);
                Hinge->RegisterComponent();
                Door->AttachToComponent(Hinge,FAttachmentTransformRules::KeepWorldTransform);
                UE_LOG(LogTemp,Display,TEXT("COURSEWORK_DOOR_HINGE_NORMALIZED %s CLOSED_ROT=%s"),*It->GetName(),*ClosedPose.Rotator().ToString());
            }
        }
        for (auto* Component : Components)
            if (Component->Mobility == EComponentMobility::Movable)
                ClosedTransforms.Add(Component, Component->GetRelativeTransform());
    }
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        if(!It->GetClass()->GetName().Contains(TEXT("Oven"))) continue;
        UStaticMeshComponent* Door=nullptr;
        TArray<UStaticMeshComponent*> Parts; It->GetComponents(Parts);
        for(auto* C:Parts) if(C->GetName()==TEXT("StaticMeshComponent1")) Door=C;
        if(!Door) continue;
        for(TActorIterator<AActor> Decor(GetWorld());Decor;++Decor)
        {
            if(Decor->FindFunction(TEXT("FurnitureAction"))) continue;
            auto* Cloth=Cast<UStaticMeshComponent>(Decor->GetRootComponent());
            if(!Cloth || !Cloth->GetStaticMesh() || !Cloth->GetStaticMesh()->GetName().Contains(TEXT("Towel")) || FVector::Dist(Cloth->Bounds.Origin,Door->Bounds.Origin)>35.f) continue;
            Cloth->SetMobility(EComponentMobility::Movable);
            Cloth->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
            Cloth->AttachToComponent(Door,FAttachmentTransformRules::KeepWorldTransform);
            UE_LOG(LogTemp,Display,TEXT("OVEN_TOWEL_ATTACHED %s TO %s/%s"),*Decor->GetName(),*It->GetName(),*Door->GetName());
        }
    }
    FTimerHandle LightingReady;GetWorld()->GetTimerManager().SetTimer(LightingReady,this,&ACourseworkHUD::InitializeRoomLighting,.25f,false);
    Ventilation=GetWorld()->SpawnActor<ACourseworkVentilation>();
    InitializeSafety();
    Overlay = CreateWidget<UCourseworkOverlay>(PC, UCourseworkOverlay::StaticClass());
    Overlay->SetOwnerHUD(this);
    Overlay->AddToViewport(20);
    FParse::Value(FCommandLine::Get(), TEXT("HouseTest="), TestMode);
    TestStartTime = GetWorld()->GetRealTimeSeconds();
    if(TestMode.StartsWith(TEXT("perf_")))
    {
        if(auto* Character=Cast<ACharacter>(PC->GetPawn()))
        {
            FVector Position(-2100,9870,151.5); FRotator View(-18,167,0);
            if(TestMode==TEXT("perf_kitchen")) { Position=FVector(-2050,9370,160); View=FRotator(-8,0,0); }
            if(TestMode==TEXT("perf_bedroom")) { Position=FVector(-2150,9350,430); View=FRotator(-8,0,0); }
            Character->GetCharacterMovement()->StopMovementImmediately();
            Character->GetCharacterMovement()->DisableMovement();
            Character->SetActorEnableCollision(false);
            Character->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
            PC->SetControlRotation(View);
            PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
            Character->DisableInput(PC); DisableInput(PC);
            UE_LOG(LogTemp,Display,TEXT("RESIDENT_PERF_FIXED %s %s"),*TestMode,*Position.ToString());
        }
    }
    if (TestMode.IsEmpty() && !FParse::Param(FCommandLine::Get(), TEXT("HouseSkipHelp"))) SetPanel(3);
    UE_LOG(LogTemp, Display, TEXT("COURSEWORK_UI_READY"));
}

void ACourseworkHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    RestoreHousehold();
    SetSmartLightingScene(0);
    RestoreDaylight();
    if(Ventilation)Ventilation->RestoreOriginals();
    RestoreEnergyComparison();
    ResetKitchenAlarm(false);
    SaveRoomLighting();
    if (Overlay) Overlay->RemoveFromParent();
    if (bMenuInputBlocked)
    {
        if (auto* PC = GetOwningPlayerController())
        {
            PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false);
            if (PC->GetPawn()) PC->GetPawn()->EnableInput(PC);
        }
    }
    if(OriginalTsrHistoryPercentage>0)
        if(auto* History=IConsoleManager::Get().FindConsoleVariable(TEXT("r.TSR.History.ScreenPercentage")))
            if(FMath::IsNearlyEqual(History->GetFloat(),150.f)&&(uint32(History->GetFlags())&ECVF_SetByMask)==OriginalTsrHistoryPriority)
            {
                History->SetWithCurrentPriority(OriginalTsrHistoryPercentage);
                UE_LOG(LogTemp,Display,TEXT("RENDER_HISTORY restored value=%.1f"),History->GetFloat());
            }
    Super::EndPlay(Reason);
}

void ACourseworkHUD::SetPanel(int32 NewPanel)
{
    auto* PC = GetOwningPlayerController();
    if (!PC) return;
    NewPanel = FMath::Clamp(NewPanel, 0, 9);
    if(NewPanel==1)NewPanel=6; // Legacy entry now opens the main lighting page.
    const bool bOpen = NewPanel != 0;
    if(bOpen)CancelLightInteraction();
    if (bOpen != bMenuInputBlocked)
    {
        PC->SetIgnoreMoveInput(bOpen); PC->SetIgnoreLookInput(bOpen);
        if (PC->GetPawn())
        {
            if (bOpen)
            {
                PC->GetPawn()->DisableInput(PC);
                PC->GetPawn()->ConsumeMovementInputVector();
                if(auto* Character=Cast<ACharacter>(PC->GetPawn()))Character->GetCharacterMovement()->StopMovementImmediately();
            }
            else PC->GetPawn()->EnableInput(PC);
        }
        bMenuInputBlocked = bOpen;
    }
    PC->bShowMouseCursor = bOpen;
    if (bOpen)
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        if (Overlay) Mode.SetWidgetToFocus(Overlay->TakeWidget());
        PC->SetInputMode(Mode);
    }
    else PC->SetInputMode(FInputModeGameOnly());
    PC->SetPause(NewPanel == 2);
    PanelMode = NewPanel;
    ++TimeEditorRevision;
    if (Overlay) Overlay->RefreshPanel();
    UE_LOG(LogTemp, Display, TEXT("COURSEWORK_PANEL %d MOVE_BLOCKED=%d CURSOR=%d"), PanelMode, bMenuInputBlocked, PC->bShowMouseCursor);
}

void ACourseworkHUD::ToggleControls() { SetPanel(PanelMode == 0 ? (IsSafetyDrillRunning()?7:6) : 0); }
void ACourseworkHUD::TogglePause() { SetPanel(PanelMode == 0 ? 2 : 0); }
void ACourseworkHUD::ToggleMap()
{
    if (PanelMode == 5) SetPanel(0);
    else { SelectedMapFloor = GetPlayerFloor(); SetPanel(5); }
}
int32 ACourseworkHUD::GetPlayerFloor() const
{
    if (auto* PC = GetOwningPlayerController()) if (auto* Pawn = PC->GetPawn())
    {
        const float Z = Pawn->GetActorLocation().Z;
        if (Z > 350) PlayerFloor = 1;
        else if (Z < 285) PlayerFloor = 0;
    }
    return PlayerFloor;
}
void ACourseworkHUD::SelectMapFloor(int32 Floor)
{
    SelectedMapFloor = FMath::Clamp(Floor,0,1);
    if (Overlay) Overlay->RefreshPanel();
}

void ACourseworkHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bStartupCover&&StartupRevealAt>0&&GetWorld()->GetRealTimeSeconds()>=StartupRevealAt)
    {
        bStartupCover=false;
        GetOwningPlayerController()->PlayerCameraManager->StartCameraFade(1,0,.4f,FLinearColor::Black,false,false);
        UE_LOG(LogTemp,Display,TEXT("STARTUP_NOON_REVEALED hour=%.4f"),GetDaylightHour());
    }
    HintElapsed += DeltaSeconds;
    TickTimeOfDay(DeltaSeconds);
    TickSafety(DeltaSeconds);
    TickHousehold(DeltaSeconds);
    TickSmartLighting(DeltaSeconds);
    TickSolar(DeltaSeconds);
    if(Ventilation)Ventilation->Suspend(IsSafetyDrillRunning());
    TickLightInteraction();
    if (PanelMode == 0 && HintElapsed >= 0.075f) { HintElapsed = 0; UpdateInteraction(); }
    if (Overlay && GetWorld()->GetRealTimeSeconds() > ToastUntil) Overlay->SetToast(TEXT(""));
    if(TestMode==TEXT("startup"))TickStartupAcceptance();
    else if (!TestMode.IsEmpty()) TickAcceptanceTest();
}

void ACourseworkHUD::UpdateInteraction()
{
    TargetActor.Reset(); TargetComponent.Reset(); TargetLabel.Empty();TargetLightRoom=INDEX_NONE;TargetLightFixture.Reset();TargetWindow=INDEX_NONE;TargetAppliance=INDEX_NONE;
    if(bLightKeyHeld){RefreshLightInteraction();return;}
    auto* PC = GetOwningPlayerController();
    if (!PC || !PC->GetPawn()) return;
    FVector Start; FRotator Rotation;
    PC->GetPlayerViewPoint(Start, Rotation);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CourseworkInteraction), !FParse::Param(FCommandLine::Get(),TEXT("HouseSimpleTrace")), PC->GetPawn());
    const bool HitPrecise=GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Rotation.Vector()*350.f,ECC_Visibility,Params);
    auto SetHit=[&](const FHitResult& Selected)->bool
    {
        if(!Selected.GetActor())return false;
        if(!LightFixtureRooms.Contains(Selected.GetComponent())&&CourseworkVisibility::IsOpaqueViewBlocked(GetWorld(),PC->GetPawn(),Selected.GetActor(),Start,Selected.ImpactPoint))return false;
        if(SetWindowTarget(Selected))return true;
        if(SetApplianceTarget(Selected))return true;
        if(SetLightTarget(Selected))return true;
        if(Selected.Distance>230.f)return false;
        auto* Actor = Selected.GetActor();
        auto* Mesh = Cast<UStaticMeshComponent>(Selected.GetComponent());
        Mesh=ResolveFurnitureTarget(Mesh);
        if(Mesh && ClosedTransforms.Contains(Mesh)) Actor=Mesh->GetOwner();
        // Map fixed oven parts and the entrance's fixed pane to their single
        // controllable leaf, while respecting closer world occlusion.
        if(Actor && (Actor->GetClass()->GetName().Contains(TEXT("Oven"))||Actor->GetClass()->GetName().Contains(TEXT("Door_Outside_B"))) && (!Mesh || !ClosedTransforms.Contains(Mesh)))
        {
            TArray<UStaticMeshComponent*> Parts; Actor->GetComponents(Parts);
            for(auto* Part:Parts) if(ClosedTransforms.Contains(Part) && (Part->GetName()==TEXT("StaticMeshComponent1")||Part->GetName()==TEXT("Door"))) { Mesh=Part; break; }
        }
        if (Actor && Actor->FindFunction(TEXT("FurnitureAction")) && Mesh && ClosedTransforms.Contains(Mesh))
        {
            TargetActor = Actor; TargetComponent = Mesh;
            FString Class = Actor->GetClass()->GetName();
            TargetLabel = Class.Contains(TEXT("Door_Outside_B")) ? TEXT("入户推拉门") :
                Class.Contains(TEXT("Door_Outside")) ? TEXT("后门") :
                Class.Contains(TEXT("Door")) ? TEXT("房门") :
                Class.Contains(TEXT("Fridge")) ? TEXT("冰箱") :
                Class.Contains(TEXT("BedTable")) ? TEXT("床头柜抽屉") :
                Class.Contains(TEXT("ChildBed")) ? TEXT("床下抽屉") :
                Class.Contains(TEXT("Microwave")) ? TEXT("微波炉") :
                Class.Contains(TEXT("Oven")) ? TEXT("烤箱") :
                Class.Contains(TEXT("Toilet")) ? TEXT("马桶盖") :
                Class.Contains(TEXT("ClothBasket")) ? TEXT("洗衣篮") :
                Class.Contains(TEXT("Teapod")) ? TEXT("茶壶盖") :
                Class.Contains(TEXT("BookShelf")) ? TEXT("书柜门") :
                Class.Contains(TEXT("Shelf_TV")) ? TEXT("电视柜") :
                Class.Contains(TEXT("WorkingTable")) ? TEXT("书桌抽屉") :
                Class.Contains(TEXT("Table_A3")) ? TEXT("桌下抽屉") :
                Class.Contains(TEXT("BathShelf")) ? TEXT("浴室柜") :
                Class.Contains(TEXT("Wardrobe")) ? TEXT("衣柜抽屉") : TEXT("橱柜 / 抽屉");
            const bool bOpen = IsFurnitureOpen(Mesh);
            if (Overlay) Overlay->SetInteraction(FString::Printf(TEXT("%s  ·  按 E %s"), *TargetLabel, bOpen ? TEXT("关闭") : TEXT("打开")));
            return true;
        }
        return false;
    };
    // The entrance glass has no triangles in the visibility query. Intersect
    // the actual moving leaf's local bounds instead of requiring aim at its
    // narrow frame. A closer opaque object still blocks the interaction.
    FHitResult DoorHit;float DoorDistance=230.f;
    for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->GetClass()->GetName().Contains(TEXT("Door_Outside_B")))
    {
        TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);
        for(auto* Part:Parts)if(Part->GetStaticMesh())
        {
            const auto& Transform=Part->GetComponentTransform();
            FVector Point,Normal;float Time=0;
            if(!FMath::LineExtentBoxIntersection(Part->GetStaticMesh()->GetBoundingBox(),Transform.InverseTransformPosition(Start),Transform.InverseTransformPosition(Start+Rotation.Vector()*230.f),FVector::ZeroVector,Point,Normal,Time))continue;
            const float Distance=230.f*Time;
            if(Distance>DoorDistance||(HitPrecise&&Hit.GetActor()!=*It&&Hit.Distance+.5f<Distance))continue;
            DoorDistance=Distance;DoorHit=FHitResult(*It,Part,Transform.TransformPosition(Point),Transform.TransformVectorNoScale(Normal));DoorHit.Distance=Distance;
        }
    }
    if(DoorHit.GetActor()&&SetHit(DoorHit))return;
    if(HitPrecise && SetHit(Hit))return;
    // Small lamps should not require pixel-perfect aim. The first blocking hit
    // still wins, so the forgiving query cannot select through walls/cabinets.
    FHitResult NearbyLight;
    if(GetWorld()->SweepSingleByChannel(NearbyLight,Start,Start+Rotation.Vector()*350.f,FQuat::Identity,
        ECC_Visibility,FCollisionShape::MakeSphere(8.f),Params)&&SetLightTarget(NearbyLight))return;
    // The oven's thin/glass door uses its authored interaction collision from
    // either face. Never choose it through a closer unrelated blocking surface.
    FHitResult Simple; Params.bTraceComplex=false;
    if(GetWorld()->LineTraceSingleByChannel(Simple,Start,Start+Rotation.Vector()*230.f,ECC_Visibility,Params)
        && Simple.GetActor() && (Simple.GetActor()->GetClass()->GetName().Contains(TEXT("Oven"))||Simple.GetActor()->GetClass()->GetName().Contains(TEXT("Door_Outside_B")))
        && (!HitPrecise || Hit.GetActor()==Simple.GetActor() || Simple.Distance+0.5f<Hit.Distance)
        && SetHit(Simple)) return;
    if (Overlay) Overlay->SetInteraction(TEXT(""));
}

UStaticMeshComponent* ACourseworkHUD::ResolveFurnitureTarget(UStaticMeshComponent* Mesh) const
{
    if(!Mesh) return nullptr;
    while(auto* Parent=Cast<UStaticMeshComponent>(Mesh->GetAttachParent()))
    {
        if(!ClosedTransforms.Contains(Parent) || (Parent->GetOwner()!=Mesh->GetOwner() && Mesh->GetOwner()->FindFunction(TEXT("FurnitureAction")))) break;
        Mesh=Parent;
    }
    return Mesh;
}

bool ACourseworkHUD::IsFurnitureOpen(UStaticMeshComponent* Component) const
{
    if(!Component) return false;
    const auto* Closed=ClosedTransforms.Find(Component);
    if(!Closed) return false;
    const FTransform Current=Component->GetRelativeTransform();
    return !Current.GetLocation().Equals(Closed->GetLocation(),0.5f)
        || Current.GetRotation().AngularDistance(Closed->GetRotation())>FMath::DegreesToRadians(0.5f)
        || !Current.GetScale3D().Equals(Closed->GetScale3D(),0.01f);
}

bool ACourseworkHUD::InvokeFurniture(AActor* Actor, UStaticMeshComponent* Component)
{
    if (!Actor || !Component) return false;
    UFunction* Function = Actor->FindFunction(TEXT("FurnitureAction"));
    if (!Function) return false;
    FStructOnScope Parameters(Function);
    bool bAssigned = false;
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm) || It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
        if (auto* ObjectProperty = CastField<FObjectProperty>(*It))
        {
            if (!Component->IsA(ObjectProperty->PropertyClass)) return false;
            ObjectProperty->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Component);
            bAssigned = true;
        }
        else return false;
    }
    if (!bAssigned) return false;
    const bool Closing=IsFurnitureOpen(Component);
    const uint32 Revision=++FurnitureActionRevisions.FindOrAdd(Component);
    Actor->ProcessEvent(Function, Parameters.GetStructMemory());
    if(Closing && Actor->GetClass()->GetName().Contains(TEXT("Shelf_TV")))
    {
        const FTransform Closed=ClosedTransforms[Component];
        TWeakObjectPtr<ACourseworkHUD> Self(this);
        TWeakObjectPtr<UStaticMeshComponent> Mesh(Component);
        FTimerHandle Restore;
        GetWorld()->GetTimerManager().SetTimer(Restore,[Self,Mesh,Closed,Revision]()
        {
            if(!Self.IsValid() || !Mesh.IsValid() || Self->FurnitureActionRevisions.FindRef(Mesh.Get())!=Revision) return;
            const FTransform Current=Mesh->GetRelativeTransform();
            // Variant drawers use one Blueprint close coordinate. Restore their
            // authored pose only after that animation ends near the closed pose.
            if(FVector::Dist(Current.GetLocation(),Closed.GetLocation())<2.f && Current.GetRotation().AngularDistance(Closed.GetRotation())<FMath::DegreesToRadians(3.f))
                Mesh->SetRelativeTransform(Closed,false,nullptr,ETeleportType::TeleportPhysics);
        },1.3f,false);
    }
    return true;
}

void ACourseworkHUD::Interact()
{
    if (PanelMode != 0 || GetWorld()->GetRealTimeSeconds() < NextInteractionTime) return;
    UpdateInteraction();
    if(TargetAppliance!=INDEX_NONE){if(IsSafetyDrillRunning()){ShowToast(TEXT("演练期间请优先疏散"));return;}ToggleAppliance(TargetAppliance);NextInteractionTime=GetWorld()->GetRealTimeSeconds()+.25;UpdateInteraction();return;}
    if(TargetWindow!=INDEX_NONE&&Ventilation)
    {
        if(IsSafetyDrillRunning()){ShowToast(TEXT("演练期间请优先疏散"));return;}
        if(Ventilation->ToggleWindow(TargetWindow)){NextInteractionTime=GetWorld()->GetRealTimeSeconds()+.3;UpdateInteraction();}
        return;
    }
    if (InvokeFurniture(TargetActor.Get(), TargetComponent.Get()))
    {
        NextInteractionTime = GetWorld()->GetRealTimeSeconds() + 0.65;
        ShowToast(TargetLabel + TEXT("：操作已执行"));
        UE_LOG(LogTemp, Display, TEXT("COURSEWORK_INTERACT %s %s"), *TargetActor->GetName(), *TargetComponent->GetName());
    }
}

void ACourseworkHUD::ShowToast(const FString& Text)
{
    if (Overlay) Overlay->SetToast(Text);
    ToastUntil = GetWorld()->GetRealTimeSeconds() + 2.5;
}

void ACourseworkHUD::GoToLocation(int32 Index)
{
    const FVector Locations[] = {
        FVector(-2205, 8000, 120), FVector(-1990, 9800, 160), FVector(-1990, 9450, 160),
        FVector(-2190, 10070, 420), FVector(-2205, 8890, 160)
    };
    const float Yaws[] = { 90.f, 180.f, 0.f, 0.f, 90.f };
    const float Pitches[] = { 10.f, 0.f, 0.f, 0.f, 0.f };
    const TCHAR* Names[] = { TEXT("房屋外观"), TEXT("客厅"), TEXT("厨房"), TEXT("二楼"), TEXT("入口") };
    auto* PC = GetOwningPlayerController();
    if (!PC || !PC->GetPawn() || Index < 0 || Index >= UE_ARRAY_COUNT(Locations)) return;
    if(IsSafetyDrillRunning()){SetPanel(0);ShowToast(TEXT("疏散演练期间请步行前往室外集合点"));return;}
    auto* Character = Cast<ACharacter>(PC->GetPawn());
    if (Character) Character->GetCharacterMovement()->StopMovementImmediately();
    if (!PC->GetPawn()->TeleportTo(Locations[Index], FRotator(0, Yaws[Index], 0), false, false))
    {
        ShowToast(TEXT("该位置暂不可用，请继续步行参观。"));
        UE_LOG(LogTemp, Warning, TEXT("COURSEWORK_NAV_BLOCKED %d"), Index);
        return;
    }
    PC->SetControlRotation(FRotator(Pitches[Index], Yaws[Index], 0));
    SetPanel(0);
    ShowToast(FString::Printf(TEXT("已到达%s"), Names[Index]));
    UE_LOG(LogTemp, Display, TEXT("COURSEWORK_NAV %d %s"), Index, *PC->GetPawn()->GetActorLocation().ToString());
}

void ACourseworkHUD::ApplySensitivity(float Value)
{
    Sensitivity = FMath::Clamp(Value, 0.25f, 2.f);
    if (auto* PC = GetOwningPlayerController()) if (PC->PlayerInput) PC->PlayerInput->SetMouseSensitivity(OriginalMouseSensitivity * Sensitivity);
    ShowToast(FString::Printf(TEXT("鼠标灵敏度：%.2f 倍"), Sensitivity));
}
void ACourseworkHUD::ApplySpeed(float Value)
{
    SpeedScale = FMath::Clamp(Value, 0.5f, 2.f);
    if (auto* PC = GetOwningPlayerController()) if (auto* Character = Cast<ACharacter>(PC->GetPawn())) Character->GetCharacterMovement()->MaxWalkSpeed = OriginalWalkSpeed * SpeedScale;
    ShowToast(FString::Printf(TEXT("移动速度：%.2f 倍"), SpeedScale));
}
void ACourseworkHUD::QuitApplication()
{
    SetPanel(0);
    if (auto* PC = GetOwningPlayerController()) PC->ConsoleCommand(TEXT("quit"));
}

void ACourseworkHUD::TickAcceptanceTest()
{
    if(TestMode==TEXT("environment_regression")){TickEnvironmentRegression();return;}
    if(TestMode==TEXT("smart_lighting")){TickSmartLightingAcceptance();return;}
    if(TestMode==TEXT("night_guidance")){TickNightGuidanceAcceptance();return;}
    if(TestMode==TEXT("ui_transitions")){TickTransitionAcceptance();return;}
    auto* PC = GetOwningPlayerController();
    auto* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
    if (!Character || GetWorld()->GetRealTimeSeconds() - TestStartTime < 3) return;
    if(TestMode.StartsWith(TEXT("lighting_state_"))) { TickLightingStateAcceptance(); return; }
    if(TestMode==TEXT("ventilation_perf")){TickVentilationPerformance();return;}
    if(TestMode==TEXT("ventilation")||TestMode==TEXT("ventilation_view")||TestMode==TEXT("ventilation_polish")){TickVentilationAcceptance();return;}
    if(TestMode==TEXT("clock")){TickClockAcceptance();return;}
    if(TestMode==TEXT("day_cycle")){TickDayCycleAcceptance();return;}
    if(TestMode==TEXT("drill_restore")){TickDrillRestoreAcceptance();return;}
    if(TestMode==TEXT("dial")){TickDialAcceptance();return;}
    if(TestMode==TEXT("daylight_sweep")){TickDaylightSweep();return;}
    if(TestMode==TEXT("daylight")){TickDaylightAcceptance();return;}
    if(TestMode==TEXT("energy")){TickEnergyAcceptance();return;}
    if(TestMode==TEXT("solar")){TickSolarAcceptance();return;}
    if(TestMode==TEXT("household_weather")){TickHouseholdAcceptance();return;}
    if(TestMode==TEXT("rain")||TestMode==TEXT("rain_brightness")){TickRainAcceptance();return;}
    if(TestMode==TEXT("lamp_glare")||TestMode==TEXT("rain_polish")||TestMode==TEXT("rain_profile")||TestMode==TEXT("weather_exterior")){TickRainPolishAcceptance();return;}
    if(TestMode==TEXT("linkage")){TickLinkageAcceptance();return;}
    if(TestMode==TEXT("interaction")){TickInteractionAcceptance();return;}
    if(TestMode==TEXT("fixture")) {TickFixtureAcceptance();return;}
    if(TestMode.StartsWith(TEXT("evacuation"))||TestMode==TEXT("safety")||TestMode==TEXT("safety_perf")) { TickEvacuationAcceptance(); return; }
    if(TestMode==TEXT("lighting")) { TickLightingAcceptance(); return; }
    if(TestMode==TEXT("control")) { TickControlAcceptance(); return; }
    if(TestMode==TEXT("typography")) { TickControlAcceptance(); return; }
    if(TestMode==TEXT("route") || TestMode==TEXT("furniture"))
    {
        if(TestMode==TEXT("route")&&FParse::Param(FCommandLine::Get(),TEXT("HouseSmartNightRoute"))&&!SmartLightingScene)
        {if(!bLightingReady)return;SetSmartLightingScene(1);}
        TickComprehensiveAcceptance();return;
    }
    if(TestMode.StartsWith(TEXT("perf_")))
    {
        if(TestCase<0)
        {
            TestCase=0;
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(TestMode+TEXT(".png")),true,false);
        }
        return;
    }
    if(TestMode==TEXT("idle"))
    {
        const double Now=GetWorld()->GetRealTimeSeconds();
        if(Now-TestCaseStart<0.5) return;
        TestCaseStart=Now; ++TestCase;
        USkeletalMeshComponent* Resident=nullptr;
        for(TActorIterator<AActor> It(GetWorld());It;++It)
            if(It->ActorHasTag(TEXT("CourseworkResidentIdle"))) Resident=It->FindComponentByClass<USkeletalMeshComponent>();
        if(!Resident) { UE_LOG(LogTemp,Error,TEXT("IDLE_RESIDENT_MISSING")); QuitApplication(); return; }
        if(TestCase==0)
        {
            Character->TeleportTo(FVector(-2100,9870,151.5),FRotator::ZeroRotator,false,false);
            PC->SetControlRotation(FRotator(-18,167,0));
            PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true);
        }
        const auto* Instance=Resident->GetSingleNodeInstance();
        TArray<FString> Bones;
        for(int32 I=0;I<Resident->GetNumBones();++I)
        {
            const FName Name=Resident->GetBoneName(I);
            const FString N=Name.ToString();
            if(!N.Contains(TEXT("head")) && !N.Contains(TEXT("spine")) && !N.Contains(TEXT("foot")) && !N.Contains(TEXT("hand"))) continue;
            const FTransform T=Resident->GetBoneTransform(I);
            const FVector P=T.GetLocation(); const FRotator R=T.Rotator();
            Bones.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"position\":[%.5f,%.5f,%.5f],\"rotation\":[%.5f,%.5f,%.5f]}"),*N,P.X,P.Y,P.Z,R.Pitch,R.Yaw,R.Roll));
        }
        TestResults.Add(FString::Printf(TEXT("{\"sample\":%d,\"animation_time\":%.5f,\"playing\":%s,\"looping\":%s,\"bones\":[%s]}"),TestCase,Instance?Instance->GetCurrentTime():-1.f,Resident->IsPlaying()?TEXT("true"):TEXT("false"),Instance && Instance->IsLooping()?TEXT("true"):TEXT("false"),*FString::Join(Bones,TEXT(","))));
        if(TestCase==2 || TestCase==8 || TestCase==14)
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("Idle_%d.png"),TestCase),true,false);
        if(TestCase>=26)
        {
            FFileHelper::SaveStringToFile(TEXT("[\n")+FString::Join(TestResults,TEXT(",\n"))+TEXT("\n]"),*(FPaths::ProjectSavedDir()/TEXT("ResidentIdleAcceptance.json")));
            TestMode.Empty(); QuitApplication();
        }
        return;
    }
    if(TestMode==TEXT("door"))
    {
        const double Now=GetWorld()->GetRealTimeSeconds();
        if(TestCase==11)
        {
            const FVector Goal=TestDoorCenter+TestDoorNormal*150;
            FVector Direction=Goal-Character->GetActorLocation(); Direction.Z=0;
            const bool Reached=Direction.Size()<22;
            if(Reached || Now-TestCaseStart>12)
            {
                const FVector P=Character->GetActorLocation();
                TestResults.Add(FString::Printf(TEXT("{\"stage\":\"doorway_walk\",\"passed\":%s,\"pawn_location\":[%.3f,%.3f,%.3f]}"),Reached?TEXT("true"):TEXT("false"),P.X,P.Y,P.Z));
                UE_LOG(LogTemp,Display,TEXT("COURSEWORK_DOOR_WALK %s"),*TestResults.Last());
                FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("DoorDiagnosis.json")));
                TestMode.Empty(); QuitApplication();
            }
            else if(Now-TestCaseStart>1) Character->AddMovementInput(Direction.GetSafeNormal(),1.f);
            return;
        }
        if(Now-TestCaseStart<1.5) return;
        TestCaseStart=Now; ++TestCase;
        FString ActorName=TEXT("BP_Door_Inside_A3_8");
        FParse::Value(FCommandLine::Get(),TEXT("DoorActor="),ActorName);
        auto State=[&](const TCHAR* Stage)
        {
            auto* A=TestDoorActor.Get(); auto* C=TestDoorComponent.Get();
            if(!A || !C) { TestResults.Add(TEXT("{\"valid\":false}")); return; }
            const FVector L=C->GetRelativeLocation(),W=C->GetComponentLocation(),S=C->GetRelativeScale3D();
            const FRotator R=C->GetRelativeRotation(),WR=C->GetComponentRotation();
            TestResults.Add(FString::Printf(TEXT("{\"stage\":\"%s\",\"actor\":\"%s\",\"visible\":%s,\"actor_hidden\":%s,\"destroying\":%s,\"registered\":%s,\"collision\":%d,\"visibility_response\":%d,\"relative_location\":[%.4f,%.4f,%.4f],\"relative_rotation\":[%.4f,%.4f,%.4f],\"relative_scale\":[%.4f,%.4f,%.4f],\"world_location\":[%.4f,%.4f,%.4f],\"world_rotation\":[%.4f,%.4f,%.4f]}"),
                Stage,*A->GetName(),C->IsVisible()?TEXT("true"):TEXT("false"),A->IsHidden()?TEXT("true"):TEXT("false"),A->IsActorBeingDestroyed()?TEXT("true"):TEXT("false"),C->IsRegistered()?TEXT("true"):TEXT("false"),int32(C->GetCollisionEnabled()),int32(C->GetCollisionResponseToChannel(ECC_Visibility)),L.X,L.Y,L.Z,R.Pitch,R.Yaw,R.Roll,S.X,S.Y,S.Z,W.X,W.Y,W.Z,WR.Pitch,WR.Yaw,WR.Roll));
            UE_LOG(LogTemp,Display,TEXT("COURSEWORK_DOOR_STATE %s"),*TestResults.Last());
        };
        auto Shot=[&](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false); };
        switch(TestCase)
        {
        case 0:
            for(TActorIterator<AActor> It(GetWorld());It;++It) if(It->GetName()==ActorName)
            {
                TestDoorActor=*It;
                TArray<UStaticMeshComponent*> Components; It->GetComponents(Components);
                for(auto* C:Components) if(C->GetName()==TEXT("Door")) TestDoorComponent=C;
                break;
            }
            if(!TestDoorComponent.IsValid()) { UE_LOG(LogTemp,Error,TEXT("COURSEWORK_DOOR_NOT_FOUND %s"),*ActorName); TestMode.Empty(); QuitApplication(); return; }
            {
                const FVector Center=TestDoorComponent->Bounds.Origin;
                const bool ThinY=TestDoorComponent->Bounds.BoxExtent.Y<TestDoorComponent->Bounds.BoxExtent.X;
                TestDoorCenter=FVector(Center.X,Center.Y,413);
                TestDoorNormal=ThinY?FVector(0,1,0):FVector(1,0,0);
                const FVector Stand=Center-(ThinY?FVector(0,150,0):FVector(150,0,0));
                Character->TeleportTo(FVector(Stand.X,Stand.Y,413),FRotator::ZeroRotator,false,false);
                PC->SetControlRotation(FRotator(0,ThinY?90:0,0));
                State(TEXT("initial"));
            }
            break;
        case 1: Shot(TEXT("Door_before.png")); break;
        case 2: Interact(); break;
        case 3:
            State(TEXT("opened"));
            {
                FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
                PC->SetControlRotation((TestDoorComponent->Bounds.Origin-Eye).Rotation());
            }
            break;
        case 4: Shot(TEXT("Door_opened.png")); break;
        case 5: Interact(); break;
        case 6:
            State(TEXT("closed")); Shot(TEXT("Door_closed.png"));
            {
                FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
                PC->SetControlRotation((TestDoorComponent->Bounds.Origin-Eye).Rotation());
            }
            break;
        case 7: Interact(); break;
        case 8:
            State(TEXT("opened_again"));
            {
                FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
                PC->SetControlRotation((TestDoorComponent->Bounds.Origin-Eye).Rotation());
            }
            break;
        case 9: Interact(); break;
        case 10:
            State(TEXT("closed_again"));
            {
                FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
                PC->SetControlRotation((TestDoorComponent->Bounds.Origin-Eye).Rotation());
            }
            break;
        case 11: Interact(); break;
        default:
            FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("DoorDiagnosis.json")));
            TestMode.Empty(); QuitApplication(); break;
        }
        return;
    }
    if(TestMode==TEXT("resident"))
    {
        const double Now=GetWorld()->GetRealTimeSeconds();
        if(TestCase==6) Character->AddMovementInput(FVector(-1,0,0),1.f);
        if(TestCase==8) Character->AddMovementInput(FVector(0,1,0),1.f);
        if(Now-TestCaseStart<3.0) return;
        TestCaseStart=Now; ++TestCase;
        const FVector Views[]={FVector(-2100,9940,155),FVector(-2100,9870,155),FVector(-2430,9900,155)};
        if(TestCase<6)
        {
            if(TestCase%2==0)
            {
                const FVector View=Views[TestCase/2];
                const bool Moved=Character->TeleportTo(FVector(View.X,View.Y,151.5),FRotator::ZeroRotator,false,false);
                PC->SetControlRotation((FVector(-2335,9925,155)-FVector(View.X,View.Y,220)).Rotation());
                UE_LOG(LogTemp,Display,TEXT("RESIDENT_VIEW %d TELEPORT=%d POS=%s"),TestCase/2,Moved,*Character->GetActorLocation().ToString());
            }
            else FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("Resident_view_%d.png"),TestCase/2),true,false);
        }
        else if(TestCase==6)
        {
            FHitResult FloorHit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(ResidentGround),true);
            for(TActorIterator<AActor> It(GetWorld());It;++It)
                if(It->GetName().Contains(TEXT("Coursework_Resident")) || It->ActorHasTag(TEXT("CourseworkResidentBody"))) Params.AddIgnoredActor(*It);
            const bool Hit=GetWorld()->LineTraceSingleByChannel(FloorHit,FVector(-2335,9925,90),FVector(-2335,9925,-20),ECC_Visibility,Params);
            TestResults.Add(FString::Printf(TEXT("{\"case\":\"ground\",\"hit\":%s,\"z\":%.4f,\"foot_z\":60.96}"),Hit?TEXT("true"):TEXT("false"),FloorHit.ImpactPoint.Z));
            Character->TeleportTo(FVector(-2200,9930,151.5),FRotator::ZeroRotator,false,false);
            Character->GetCharacterMovement()->StopMovementImmediately();
        }
        else if(TestCase==7)
        {
            const FVector P=Character->GetActorLocation();
            const bool Blocked=P.X>-2315 && P.X<-2270;
            TestResults.Add(FString::Printf(TEXT("{\"case\":\"body_blocks_player\",\"passed\":%s,\"x\":%.3f}"),Blocked?TEXT("true"):TEXT("false"),P.X));
        }
        else if(TestCase==8)
        {
            const bool Moved=Character->TeleportTo(FVector(-2100,9870,151.5),FRotator::ZeroRotator,false,false);
            UE_LOG(LogTemp,Display,TEXT("RESIDENT_SIDE_PASSAGE_START TELEPORT=%d POS=%s"),Moved,*Character->GetActorLocation().ToString());
            Character->GetCharacterMovement()->StopMovementImmediately();
        }
        else if(TestCase==9)
        {
            const FVector P=Character->GetActorLocation();
            TestResults.Add(FString::Printf(TEXT("{\"case\":\"side_passage\",\"passed\":%s,\"x\":%.3f,\"y\":%.3f}"),P.Y>9930 && P.X>-2150?TEXT("true"):TEXT("false"),P.X,P.Y));
            FFileHelper::SaveStringToFile(TEXT("[\n")+FString::Join(TestResults,TEXT(",\n"))+TEXT("\n]"),*(FPaths::ProjectSavedDir()/TEXT("ResidentAcceptance.json")));
            TestMode.Empty(); QuitApplication();
        }
        return;
    }
    if(TestMode==TEXT("visual"))
    {
        const double Now=GetWorld()->GetRealTimeSeconds();
        if(Now-TestCaseStart<2.0) return;
        TestCaseStart=Now; ++TestCase;
        auto Shot=[&](const TCHAR* Name)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString(Name),true,false);
            UE_LOG(LogTemp,Display,TEXT("COURSEWORK_VISUAL_CAPTURE %s"),Name);
        };
        switch(TestCase)
        {
        case 0: GoToLocation(4); break;
        case 1: Shot(TEXT("HUD_entrance.png")); break;
        case 2: GoToLocation(1); break;
        case 3: Shot(TEXT("HUD_living.png")); break;
        case 4:
            Character->TeleportTo(FVector(-2050,9116,413),FRotator::ZeroRotator,false,false);
            PC->SetControlRotation(FRotator::ZeroRotator); break;
        case 5: Shot(TEXT("HUD_bedroom_door.png")); break;
        case 6:
            Character->TeleportTo(FVector(-1750,9116,413),FRotator(0,180,0),false,false);
            PC->SetControlRotation(FRotator(0,180,0)); break;
        case 7: Shot(TEXT("HUD_bright_room.png")); break;
        case 8: GoToLocation(3); break;
        case 9: Shot(TEXT("HUD_hallway.png")); break;
        case 10: ToggleMap(); break;
        case 11: Shot(TEXT("HUD_map_upper.png")); break;
        case 12: SelectMapFloor(0); break;
        case 13: Shot(TEXT("HUD_map_ground.png")); break;
        default: TestMode.Empty(); QuitApplication(); break;
        }
        return;
    }
    if (TestMode == TEXT("map"))
    {
        const double Now=GetWorld()->GetRealTimeSeconds();
        if(Now-TestCaseStart<1) return;
        TestCaseStart=Now; ++TestCase;
        bool bPassed=false; FString Name;
        switch(TestCase)
        {
        case 0: Name=TEXT("open_map_ground"); ToggleMap(); bPassed=PanelMode==5 && GetMapFloor()==0 && GetPlayerFloor()==0 && PC->IsMoveInputIgnored() && PC->IsLookInputIgnored(); break;
        case 1: Name=TEXT("browse_other_floor"); SelectMapFloor(1); bPassed=GetMapFloor()==1 && GetPlayerFloor()==0; break;
        case 2: Name=TEXT("close_map_release_input"); ToggleMap(); bPassed=PanelMode==0 && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor; break;
        case 3: Name=TEXT("upstairs_auto_floor"); GoToLocation(3); ToggleMap(); bPassed=GetMapFloor()==1 && GetPlayerFloor()==1; break;
        case 4: Name=TEXT("stairs_keep_upper"); Character->SetActorLocation(FVector(-2190,10300,320),false); bPassed=GetPlayerFloor()==1; break;
        case 5: Name=TEXT("stairs_switch_down"); Character->SetActorLocation(FVector(-2190,10300,280),false); bPassed=GetPlayerFloor()==0; break;
        case 6: Name=TEXT("stairs_keep_lower"); Character->SetActorLocation(FVector(-2190,10300,320),false); bPassed=GetPlayerFloor()==0; break;
        case 7: Name=TEXT("stairs_switch_up"); Character->SetActorLocation(FVector(-2190,10300,360),false); bPassed=GetPlayerFloor()==1; break;
        case 8: Name=TEXT("return_entrance_ground"); GoToLocation(4); bPassed=GetPlayerFloor()==0 && PanelMode==0 && !PC->IsMoveInputIgnored(); break;
        }
        TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,bPassed?TEXT("true"):TEXT("false")));
        UE_LOG(LogTemp,Display,TEXT("COURSEWORK_MAP_TEST %s"),*TestResults.Last());
        if(TestCase>=8)
        {
            FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("CourseworkMapAcceptance.json")));
            TestMode.Empty(); QuitApplication();
        }
        return;
    }
    if (TestMode == TEXT("ui"))
    {
        const double Now = GetWorld()->GetRealTimeSeconds();
        if (Now - TestCaseStart < 1.0) return;
        TestCaseStart = Now;
        ++TestCase;
        bool bPassed = false;
        FString Name;
        if (TestCase < 5)
        {
            Name = FString::Printf(TEXT("navigation_%d"), TestCase);
            SetPanel(1);
            GoToLocation(TestCase);
            const FVector Expected[] = { FVector(-2205,8000,120), FVector(-1990,9800,160), FVector(-1990,9450,160), FVector(-2190,10070,420), FVector(-2205,8890,160) };
            bPassed = PanelMode == 0 && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor && FVector::Dist(Character->GetActorLocation(),Expected[TestCase]) < 50;
        }
        else if (TestCase == 5)
        {
            Name = TEXT("controls_toggle");
            ToggleControls();
            const bool bOpened = PanelMode == 6 && PC->IsMoveInputIgnored() && PC->IsLookInputIgnored() && PC->bShowMouseCursor;
            ToggleControls();
            bPassed = bOpened && PanelMode == 0 && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored();
        }
        else if (TestCase == 6)
        {
            Name = TEXT("pause_resume");
            TogglePause();
            const bool bPaused = PanelMode == 2 && PC->IsPaused();
            TogglePause();
            bPassed = bPaused && PanelMode == 0 && !PC->IsPaused() && !PC->IsMoveInputIgnored();
        }
        else if (TestCase == 7)
        {
            Name = TEXT("settings_apply");
            SetPanel(4); ApplySensitivity(1.5f); ApplySpeed(1.5f);
            bPassed = PC->PlayerInput && FMath::IsNearlyEqual(PC->PlayerInput->GetMouseSensitivityX(), OriginalMouseSensitivity*1.5f) && FMath::IsNearlyEqual(Character->GetCharacterMovement()->MaxWalkSpeed, OriginalWalkSpeed*1.5f);
            ApplySensitivity(1); ApplySpeed(1); SetPanel(0);
        }
        else if (TestCase == 8)
        {
            Name = TEXT("entry_e_interaction");
            GoToLocation(4);
            FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
            for(auto& Pair:ClosedTransforms)
                if(Pair.Key.IsValid() && Pair.Key->GetOwner()->GetName()==TEXT("BP_Door_Outside_B_2") && Pair.Key->GetName()==TEXT("Door"))
                {
                    const auto* Door=Cast<UStaticMeshComponent>(Pair.Key.Get());
                    const FVector Aim=Door->Bounds.Origin+FVector(Door->Bounds.BoxExtent.X*0.9f,-Door->Bounds.BoxExtent.Y,0);
                    PC->SetControlRotation((Aim-Eye).Rotation());
                }
            return;
        }
        else if (TestCase == 9)
        {
            UpdateInteraction();
            PC->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.0));
            return;
        }
        else if (TestCase == 10)
        {
            PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.0));
            Name = TEXT("entry_e_interaction");
            // Opening moves the panel away from the crosshair, so the current
            // interaction target may correctly become empty during the animation.
            for (TActorIterator<AActor> It(GetWorld()); It; ++It)
                if (It->GetClass()->GetName().Contains(TEXT("BP_Door_Outside_B")))
                {
                    TArray<UStaticMeshComponent*> Components; It->GetComponents(Components);
                    for (auto* C : Components)
                        if (C->GetName() == TEXT("Door") && ClosedTransforms.Contains(C))
                            bPassed |= FVector::Dist(C->GetRelativeLocation(),ClosedTransforms[C].GetLocation()) > 50;
                }
        }
        const FVector P = Character->GetActorLocation();
        TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s,\"x\":%.3f,\"y\":%.3f,\"z\":%.3f}"),*Name,bPassed?TEXT("true"):TEXT("false"),P.X,P.Y,P.Z));
        UE_LOG(LogTemp, Display, TEXT("COURSEWORK_UI_TEST %s"), *TestResults.Last());
        if (TestCase >= 10)
        {
            FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("CourseworkUIAcceptance.json")));
            TestMode.Empty(); QuitApplication();
        }
        return;
    }
    if (TestMode == TEXT("probe"))
    {
        AActor* Stair = nullptr;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
            if (It->GetClass()->GetName().Contains(TEXT("BP_Stairs_D_1-2"))) { Stair=*It; break; }
        if (!Stair) { TestMode.Empty(); QuitApplication(); return; }
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CourseworkProbe), true);
        for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (*It!=Stair) Params.AddIgnoredActor(*It);
        TArray<FString> Samples;
        for (int32 X=-2290; X<=-2090; X+=40) for (int32 Y=10040; Y<=10560; Y+=40)
        {
            FHitResult Hit;
            if (GetWorld()->LineTraceSingleByChannel(Hit,FVector(X,Y,450),FVector(X,Y,40),ECC_Visibility,Params))
                Samples.Add(FString::Printf(TEXT("{\"x\":%d,\"y\":%d,\"z\":%.2f,\"nz\":%.3f}"),X,Y,Hit.ImpactPoint.Z,Hit.ImpactNormal.Z));
        }
        FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(Samples,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("CourseworkProbe.json")));
        TArray<UStaticMeshComponent*> Components; Stair->GetComponents(Components);
        for (auto* C:Components) if (C->GetStaticMesh() && C->GetStaticMesh()->GetBodySetup())
        {
            auto* Body=C->GetStaticMesh()->GetBodySetup();
            UE_LOG(LogTemp,Display,TEXT("COURSEWORK_COLLISION %s FLAG=%d BOXES=%d CONVEX=%d"),*C->GetName(),int32(Body->CollisionTraceFlag),Body->AggGeom.BoxElems.Num(),Body->AggGeom.ConvexElems.Num());
        }
        TestMode.Empty(); QuitApplication(); return;
    }
    static const FVector Starts[] = { FVector(-2205,8940,160), FVector(-2205,9190,160), FVector(-2190,10610,160), FVector(-2190,10070,420) };
    static const FVector Targets[] = { FVector(-2205,9190,160), FVector(-2205,8940,160), FVector(-2190,10070,420), FVector(-2190,10520,160) };
    const double Now = GetWorld()->GetRealTimeSeconds();
    if (TestCase < 0)
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
            if (It->GetClass()->GetName().Contains(TEXT("BP_Door_Outside_B")))
            {
                TArray<UStaticMeshComponent*> Components; It->GetComponents(Components);
                for (auto* C : Components) if (C->GetName() == TEXT("Door")) InvokeFurniture(*It, C);
            }
        TestCase = 0; TestCaseStart = Now;
        Character->SetActorLocation(Starts[0], false);
        Character->GetCharacterMovement()->StopMovementImmediately();
        TestMaxZ = Starts[0].Z;
    }
    const FVector Position = Character->GetActorLocation();
    TestMaxZ = FMath::Max(TestMaxZ, Position.Z);
    FVector Direction = Targets[TestCase] - Position; Direction.Z = 0;
    const float Distance = Direction.Size();
    const bool bReached = Distance < 22.f && (TestCase != 2 || Position.Z > 390.f) && (TestCase != 3 || Position.Z < 180.f);
    if (bReached || Now - TestCaseStart > 14)
    {
        FString Blocker;
        if (!bReached)
        {
            FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(CourseworkStairTest), false, Character);
            GetWorld()->SweepSingleByChannel(Hit, Position, Position + Direction.GetSafeNormal()*70.f, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Character->GetCapsuleComponent()->GetScaledCapsuleRadius(), Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Params);
            if (Hit.GetComponent()) Blocker = Hit.GetActor()->GetName()+TEXT("/")+Hit.GetComponent()->GetName();
            UE_LOG(LogTemp,Display,TEXT("COURSEWORK_BLOCKER POINT=%s NORMAL=%s TIME=%.3f"),*Hit.ImpactPoint.ToString(),*Hit.ImpactNormal.ToString(),Hit.Time);
        }
        TestResults.Add(FString::Printf(TEXT("{\"case\":%d,\"passed\":%s,\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"max_z\":%.3f,\"blocker\":\"%s\"}"), TestCase, bReached?TEXT("true"):TEXT("false"),Position.X,Position.Y,Position.Z,TestMaxZ,*Blocker));
        UE_LOG(LogTemp, Display, TEXT("COURSEWORK_WALK_TEST %s"), *TestResults.Last());
        ++TestCase;
        if (TestCase >= UE_ARRAY_COUNT(Starts))
        {
            FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"), *(FPaths::ProjectSavedDir()/TEXT("CourseworkAcceptance.json")));
            TestMode.Empty(); QuitApplication(); return;
        }
        Character->SetActorLocation(Starts[TestCase],false);
        Character->GetCharacterMovement()->StopMovementImmediately();
        TestCaseStart=Now; TestMaxZ=Starts[TestCase].Z;
        return;
    }
    if (Now-TestCaseStart>1)
    {
        Character->AddMovementInput(Direction.GetSafeNormal(),1.f);
        PC->SetControlRotation(Direction.Rotation());
    }
}
