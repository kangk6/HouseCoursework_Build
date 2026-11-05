#include "CourseworkHUD.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ACourseworkHUD::InitializeEvacuation()
{
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        if(It->ActorHasTag(TEXT("CourseworkResidentIdle")))
        {
            OriginalResident=*It;OriginalResidentTransform=It->GetActorTransform();
            if(auto* Mesh=It->FindComponentByClass<USkeletalMeshComponent>())bOriginalResidentTick=Mesh->IsComponentTickEnabled();
        }
        if(It->GetName()==TEXT("BP_Door_Outside_B_2"))
        {
            TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);
            for(auto* Part:Parts)if(Part->GetName()==TEXT("Door"))SafetyEntryDoor=Part;
        }
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AssemblyGround),false);
    if(auto* PC=GetOwningPlayerController())Params.AddIgnoredActor(PC->GetPawn());
    FHitResult Hit;
    if(GetWorld()->LineTraceSingleByChannel(Hit,AssemblyPoint+FVector(0,0,400),AssemblyPoint-FVector(0,0,600),ECC_Visibility,Params))AssemblyPoint.Z=Hit.ImpactPoint.Z+88;
    auto AddMesh=[&](AActor* Actor,const TCHAR* MeshPath,FVector Position,FVector Scale,UMaterialInterface* Material)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(Actor);Actor->AddInstanceComponent(Mesh);
        if(!Actor->GetRootComponent())Actor->SetRootComponent(Mesh);else Mesh->SetupAttachment(Actor->GetRootComponent());
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,MeshPath));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);Mesh->RegisterComponent();Mesh->SetWorldLocation(Position);Mesh->SetWorldScale3D(Scale);
        if(Material)Mesh->SetMaterial(0,Material);return Mesh;
    };
    AssemblyMarker=GetWorld()->SpawnActor<AActor>();
    auto* Ring=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/Safety/M_AssemblyMarker.M_AssemblyMarker"));
    AddMesh(AssemblyMarker,TEXT("/Engine/BasicShapes/Plane.Plane"),AssemblyPoint-FVector(0,0,86),FVector(3.6f),Ring);
    auto* Label=NewObject<UTextRenderComponent>(AssemblyMarker);AssemblyMarker->AddInstanceComponent(Label);
    Label->SetupAttachment(AssemblyMarker->GetRootComponent());Label->SetText(FText::FromString(TEXT("ASSEMBLY")));
    Label->SetHorizontalAlignment(EHTA_Center);Label->SetWorldSize(24);Label->SetTextRenderColor(FColor(125,225,186));
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);Label->RegisterComponent();Label->SetWorldLocation(AssemblyPoint+FVector(0,0,36));Label->SetWorldRotation(FRotator(0,90,0));Label->SetWorldScale3D(FVector(1));
    AssemblyMarker->SetActorHiddenInGame(true);
    SmokeSensor=GetWorld()->SpawnActor<AActor>();
    FVector Sensor(-1990,9450,318);
    if(GetWorld()->LineTraceSingleByChannel(Hit,FVector(-1990,9450,260),FVector(-1990,9450,600),ECC_Visibility,Params))Sensor.Z=Hit.ImpactPoint.Z-3;
    auto* White=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    AddMesh(SmokeSensor,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),Sensor,FVector(.16f,.16f,.035f),White);
    auto* Signal=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/Safety/M_SensorSignal.M_SensorSignal"));
    if(Signal)SensorSignal=UMaterialInstanceDynamic::Create(Signal,this);
    AddMesh(SmokeSensor,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),Sensor-FVector(0,0,3),FVector(.04f,.04f,.012f),SensorSignal);
}

FVector ACourseworkHUD::GetResidentPosition()const
{
    if(EvacuatingResident)return EvacuatingResident->GetActorLocation();
    return OriginalResident.IsValid()?OriginalResident->GetActorLocation()+FVector(0,0,88):FVector::ZeroVector;
}
FString ACourseworkHUD::GetResidentEvacuationStatus()const
{
    switch(ResidentEvacuationState)
    {
    case 1:return TEXT("已响应报警 · 准备撤离");
    case 2:return TEXT("正在撤往室外集合点");
    case 3:return FString::Printf(TEXT("已到集合点 · %.1f 秒"),ResidentArrivalTime);
    case 4:return TEXT("通道受阻 · 请让开出口");
    case 5:return TEXT("角色启动失败 · 请中止复位");
    default:return TEXT("客厅待机");
    }
}
void ACourseworkHUD::BeginResidentEvacuation()
{
    ResidentEvacuationState=1;ResidentWaitTime=.8f;ResidentRouteIndex=0;ResidentStuckTime=0;
    bResidentTurning=false;bResidentSettling=false;ResidentTurnElapsed=0;ResidentSettleElapsed=0;
    ResidentRoute={FVector(-2335,9951,150),FVector(-2210,9951,150),FVector(-2100,9951,150),FVector(-2100,9870,150),FVector(-1990,9800,150),
        FVector(-1990,9170,150),FVector(-2205,9170,150),FVector(-2205,9000,150),FVector(-2205,8890,150),
        FVector(-2205,8700,AssemblyPoint.Z),AssemblyPoint+FVector(-135,0,0)};
    bEntryInitiallyOpen=SafetyEntryDoor.IsValid()&&IsFurnitureOpen(SafetyEntryDoor.Get());bEntryOpenedByResident=false;EntryWaitTime=-10;
    UE_LOG(LogTemp,Display,TEXT("EVAC_RESIDENT responding"));
}

void ACourseworkHUD::RestoreResident()
{
    if(EvacuatingResident){EvacuatingResident->SetActorEnableCollision(false);EvacuatingResident->Destroy();EvacuatingResident=nullptr;}
    if(OriginalResident.IsValid())
    {
        OriginalResident->SetActorHiddenInGame(false);OriginalResident->SetActorEnableCollision(true);
        if(auto* Mesh=OriginalResident->FindComponentByClass<USkeletalMeshComponent>())Mesh->SetComponentTickEnabled(bOriginalResidentTick);
    }
    if(bEntryOpenedByResident&&!bEntryInitiallyOpen&&SafetyEntryDoor.IsValid()&&IsFurnitureOpen(SafetyEntryDoor.Get())&&FurnitureActionRevisions.FindRef(SafetyEntryDoor.Get())==EntrySafetyRevision)
        InvokeFurniture(SafetyEntryDoor->GetOwner(),SafetyEntryDoor.Get());
    ResidentEvacuationState=0;ResidentRouteIndex=0;bResidentRunning=false;bEntryOpenedByResident=false;
    bResidentTurning=false;bResidentSettling=false;ResidentTurnElapsed=0;ResidentSettleElapsed=0;
}

void ACourseworkHUD::TickEvacuation(float DeltaSeconds)
{
    auto* PC=GetOwningPlayerController();auto* Player=PC?PC->GetPawn():nullptr;
    if(Player)
    {
        const FVector P=Player->GetActorLocation();const bool AtGoal=FVector::Dist2D(P,AssemblyPoint)<150&&FMath::Abs(P.Z-AssemblyPoint.Z)<65;
        if(AtGoal&&!bPlayerGathered){PlayerArrivalTime=AlarmElapsed;ShowToast(TEXT("你已到达集合点 · 请确认住户也已撤离"));}
        if(!AtGoal)PlayerArrivalTime=-1;
        bPlayerGathered=AtGoal;
    }
    if(ResidentEvacuationState==1)
    {
        ResidentWaitTime-=DeltaSeconds;if(ResidentWaitTime>0)return;
        auto* Source=OriginalResident.IsValid()?OriginalResident->FindComponentByClass<USkeletalMeshComponent>():nullptr;
        if(!Source||!ResidentRunAnimation){ResidentEvacuationState=5;return;}
        OriginalResident->SetActorEnableCollision(false);OriginalResident->SetActorHiddenInGame(true);Source->SetComponentTickEnabled(false);
        FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FRotator InitialFacing=Source->GetComponentTransform().TransformVectorNoScale(FVector(0,1,0)).GetSafeNormal2D().Rotation();
        EvacuatingResident=GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(),OriginalResidentTransform.GetLocation()+FVector(0,0,90),FRotator(0,InitialFacing.Yaw,0),Spawn);
        if(!EvacuatingResident){OriginalResident->SetActorHiddenInGame(false);OriginalResident->SetActorEnableCollision(true);Source->SetComponentTickEnabled(bOriginalResidentTick);ResidentEvacuationState=5;return;}
        EvacuatingResident->Tags.Add(TEXT("CourseworkEvacuatingResident"));
        auto* Capsule=EvacuatingResident->GetCapsuleComponent();Capsule->SetCapsuleSize(20,88);
        auto* Mesh=EvacuatingResident->GetMesh();Mesh->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
        for(int32 I=0;I<Source->GetNumMaterials();++I)Mesh->SetMaterial(I,Source->GetMaterial(I));
        // CharacterMovement keeps the capsule about 2.15 cm above the floor.
        Mesh->SetRelativeLocation(FVector(0,0,-90.15f));Mesh->SetRelativeRotation(FRotator(0,-90,0));Mesh->SetWorldScale3D(Source->GetComponentScale());
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Mesh->PlayAnimation(ResidentLocomotion,true);
        auto* Movement=EvacuatingResident->GetCharacterMovement();Movement->bRunPhysicsWithNoController=true;
        Movement->MaxWalkSpeed=140;Movement->MaxAcceleration=360;Movement->BrakingDecelerationWalking=550;Movement->MaxStepHeight=24;
        Movement->bOrientRotationToMovement=false;Movement->RotationRate=FRotator(0,180,0);Movement->SetMovementMode(MOVE_Walking);
        ResidentTurnStartYaw=InitialFacing.Yaw;
        ResidentTurnDelta=FMath::FindDeltaAngleDegrees(InitialFacing.Yaw,(ResidentRoute[0]-EvacuatingResident->GetActorLocation()).GetSafeNormal2D().Rotation().Yaw);
        bResidentTurning=FMath::Abs(ResidentTurnDelta)>5;ResidentTurnElapsed=0;
        if(bResidentTurning)
        {
            // The fixed living-room route starts with a quarter turn. Its two
            // foot plants are synchronized with actor yaw instead of a snap.
            UAnimSequence* Turn=ResidentTurnDelta>0?ResidentTurnPositive:ResidentTurnNegative;
            Mesh->PlayAnimation(Turn?Turn:ResidentIdleAnimation.Get(),false);
            if(auto* Animation=Mesh->GetSingleNodeInstance())Animation->SetPlaying(false);
        }
        ResidentLastPosition=EvacuatingResident->GetActorLocation();ResidentEvacuationState=2;
        UE_LOG(LogTemp,Display,TEXT("EVAC_RESIDENT spawned %s"),*ResidentLastPosition.ToString());
    }
    if(!EvacuatingResident || (ResidentEvacuationState!=2&&ResidentEvacuationState!=4))return;
    auto* Movement=EvacuatingResident->GetCharacterMovement();const FVector Position=EvacuatingResident->GetActorLocation();
    auto UpdateLocomotion=[&]()
    {
        const float Speed=Movement->Velocity.Size2D();bResidentRunning=Speed>15;
        if(auto* Animation=EvacuatingResident->GetMesh()->GetSingleNodeInstance())
        {Animation->SetBlendSpacePosition(FVector(FMath::Clamp(Speed*170.f/140.f,0.f,170.f),0,0));Animation->SetPlayRate(Speed<15?1.f:140.f/135.62175f);}
    };
    if(bResidentTurning)
    {
        ResidentTurnElapsed=FMath::Min(1.f,ResidentTurnElapsed+DeltaSeconds);
        const float T=ResidentTurnElapsed;const float Curve=T*T*(3-2*T);
        EvacuatingResident->SetActorRotation(FRotator(0,ResidentTurnStartYaw+ResidentTurnDelta*Curve,0));
        auto* Mesh=EvacuatingResident->GetMesh();
        if(auto* Animation=Mesh->GetSingleNodeInstance()){Animation->SetPosition(T,false);Mesh->TickAnimation(0,false);Mesh->RefreshBoneTransforms();}
        ResidentLastPosition=Position;bResidentRunning=false;
        if(T>=1){bResidentTurning=false;Mesh->PlayAnimation(ResidentLocomotion,true);UE_LOG(LogTemp,Display,TEXT("EVAC_TURN complete delta=%.2f"),ResidentTurnDelta);}
        return;
    }
    if(bResidentSettling)
    {
        // Leave velocity to CharacterMovement braking, with the idle blend
        // following measured speed, before confirming the assembly arrival.
        UpdateLocomotion();ResidentSettleElapsed=Movement->Velocity.Size2D()<5?ResidentSettleElapsed+DeltaSeconds:0;
        if(FVector::Dist2D(Position,ResidentRoute.Last())>70){bResidentSettling=false;ResidentSettleElapsed=0;}
        else if(ResidentSettleElapsed>=.25f)
        {
            ResidentEvacuationState=3;ResidentArrivalTime=AlarmElapsed;Movement->StopMovementImmediately();UpdateLocomotion();bResidentSettling=false;ResidentRouteIndex=ResidentRoute.Num();
            UE_LOG(LogTemp,Display,TEXT("EVAC_RESIDENT gathered %.2f"),ResidentArrivalTime);
        }
        ResidentLastPosition=Position;return;
    }
    if(ResidentRouteIndex<ResidentRoute.Num())
    {
        FVector Direction=ResidentRoute[ResidentRouteIndex]-Position;Direction.Z=0;
        const float ArrivalRadius=ResidentRouteIndex<4?8.f:22.f;
        const FVector Previous=ResidentRouteIndex>0?ResidentRoute[ResidentRouteIndex-1]:OriginalResidentTransform.GetLocation();
        const FVector Segment=(ResidentRoute[ResidentRouteIndex]-Previous).GetSafeNormal2D();
        const float PassRadius=FMath::Max(28.f,Movement->Velocity.Size2D()*FMath::Min(DeltaSeconds,.35f)+ArrivalRadius);
        const bool Passed=ResidentRouteIndex<ResidentRoute.Num()-1&&Direction.Size()<PassRadius&&FVector::DotProduct(Position-ResidentRoute[ResidentRouteIndex],Segment)>=0;
        if((Direction.Size()<ArrivalRadius||Passed)&&FMath::Abs(Position.Z-ResidentRoute[ResidentRouteIndex].Z)<50)
        {
            UE_LOG(LogTemp,Display,TEXT("EVAC_WAYPOINT %d %s"),ResidentRouteIndex,*Position.ToString());
            if(ResidentRouteIndex==ResidentRoute.Num()-1)
            {
                bResidentSettling=true;ResidentSettleElapsed=0;UpdateLocomotion();return;
            }
            ++ResidentRouteIndex;
            Direction=ResidentRoute[ResidentRouteIndex]-Position;Direction.Z=0;
        }
        bool WaitingForDoor=false;
        if(SafetyEntryDoor.IsValid()&&ResidentRouteIndex>=3&&FVector::Dist2D(Position,SafetyEntryDoor->Bounds.Origin)<340)
        {
            const float Now=GetWorld()->GetTimeSeconds();
            if(!IsFurnitureOpen(SafetyEntryDoor.Get())&&Now-EntryWaitTime>1.6f)
            {
                if(InvokeFurniture(SafetyEntryDoor->GetOwner(),SafetyEntryDoor.Get())){bEntryOpenedByResident=true;EntrySafetyRevision=FurnitureActionRevisions.FindRef(SafetyEntryDoor.Get());EntryWaitTime=Now;}
            }
            WaitingForDoor=FVector::Dist2D(Position,SafetyEntryDoor->Bounds.Origin)<180&&(!IsFurnitureOpen(SafetyEntryDoor.Get())||Now-EntryWaitTime<1.4f);
        }
        if(!WaitingForDoor)
        {
            // Intermediate collinear markers must not produce a stop/start gait.
            const bool Final=ResidentRouteIndex==ResidentRoute.Num()-1;
            float Pace=Final?FMath::Clamp(float(Direction.Size())*2.5f,25.f,140.f):140.f;
            if(!Final)
            {
                const FVector Next=(ResidentRoute[ResidentRouteIndex+1]-ResidentRoute[ResidentRouteIndex]).GetSafeNormal2D();
                if(FVector::DotProduct(Direction.GetSafeNormal2D(),Next)<.7f)Pace=FMath::Lerp(85.f,140.f,FMath::Clamp(float(Direction.Size())/90.f,0.f,1.f));
            }
            Movement->MaxWalkSpeed=Pace;
            FVector Steer=Direction.GetSafeNormal();
            if(Player&&FVector::Dist2D(Position,Player->GetActorLocation())<110&&FVector::DotProduct((Player->GetActorLocation()-Position).GetSafeNormal2D(),Steer)>.3f)
            {
                FCollisionQueryParams Query(SCENE_QUERY_STAT(ResidentAvoidPlayer),false,EvacuatingResident);
                for(float Side:{1.f,-1.f})
                {
                    const FVector Detour=(Steer*.25f+FVector(-Steer.Y,Steer.X,0)*Side).GetSafeNormal();FHitResult Hit;
                    if(!GetWorld()->SweepSingleByChannel(Hit,Position,Position+Detour*70,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(20,84),Query)){Steer=Detour;break;}
                }
            }
            const FRotator Facing=FMath::RInterpConstantTo(EvacuatingResident->GetActorRotation(),Steer.Rotation(),DeltaSeconds,180.f);
            EvacuatingResident->SetActorRotation(FRotator(0,Facing.Yaw,0));
            const float Alignment=FVector::DotProduct(EvacuatingResident->GetActorForwardVector(),Steer);
            // Finish turning before taking a full stride in the new direction.
            EvacuatingResident->AddMovementInput(Steer,FMath::Clamp((Alignment-.15f)/.70f,0.f,1.f),true);
            if(FVector::Dist2D(Position,ResidentLastPosition)<.3f)ResidentStuckTime+=DeltaSeconds;else ResidentStuckTime=0;
            const int32 Next=ResidentStuckTime>2?4:2;
            if(Next==4&&ResidentEvacuationState!=4)
            {
                FHitResult Block;FCollisionQueryParams Trace(SCENE_QUERY_STAT(ResidentBlocked),false,EvacuatingResident);
                GetWorld()->SweepSingleByChannel(Block,Position,Position+Steer*60,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(20,84),Trace);
                UE_LOG(LogTemp,Warning,TEXT("EVAC_BLOCKED waypoint=%d position=%s actor=%s component=%s"),ResidentRouteIndex,*Position.ToString(),Block.GetActor()?*Block.GetActor()->GetName():TEXT("none"),Block.GetComponent()?*Block.GetComponent()->GetName():TEXT("none"));
            }
            ResidentEvacuationState=Next;
        }
        else {Movement->StopMovementImmediately();ResidentStuckTime=0;}
        UpdateLocomotion();
        ResidentLastPosition=Position;
    }
}
