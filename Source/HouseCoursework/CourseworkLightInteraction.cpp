#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

namespace
{
bool LampViewBlocked(UWorld* World,AActor* Pawn,AActor* Lamp,const FVector& Eye,const FVector& Point)
{
    // Authored thin architectural surfaces have one-sided collision. Check
    // both directions so the back of an opaque wall also occludes a lamp.
    FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LampOpaqueOcclusion),true,Pawn);Query.AddIgnoredActor(Lamp);Query.bReturnFaceIndex=true;
    for(bool Reverse:{false,true})
    {
        TArray<FHitResult> Surfaces;
        World->LineTraceMultiByObjectType(Surfaces,Reverse?Point:Eye,Reverse?Eye:Point,Objects,Query);
        for(const auto& Surface:Surfaces)
        {
            auto* Mesh=Cast<UStaticMeshComponent>(Surface.GetComponent());
            if(!Mesh||!Mesh->IsVisible()||Mesh->bHiddenInGame||FVector::Dist(Surface.ImpactPoint,Eye)<1.f||FVector::Dist(Surface.ImpactPoint,Point)<1.f)continue;
            int32 Section=0;const auto* Material=Mesh->GetMaterialFromCollisionFaceIndex(Surface.FaceIndex,Section);
            if(Material&&(Material->GetBlendMode()==BLEND_Translucent||Material->GetBlendMode()==BLEND_Additive||Material->GetBlendMode()==BLEND_Modulate))continue;
            return true;
        }
    }
    return false;
}
}

void ACourseworkHUD::RegisterLightFixture(UStaticMeshComponent* Mesh,int32 Room)
{
    if(!Mesh||Mesh->GetOwner()->IsHidden()||Mesh->Bounds.BoxExtent.IsNearlyZero())return;
    // Fixture-sized query proxy includes the shade/base and never blocks walking.
    auto* Proxy=NewObject<UBoxComponent>(Mesh->GetOwner());Mesh->GetOwner()->AddInstanceComponent(Proxy);
    Proxy->SetupAttachment(Mesh);Proxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Proxy->SetCollisionResponseToAllChannels(ECR_Ignore);Proxy->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Proxy->SetGenerateOverlapEvents(false);Proxy->SetCanEverAffectNavigation(false);Proxy->RegisterComponent();
    Proxy->SetWorldLocation(Mesh->Bounds.Origin);Proxy->SetWorldRotation(FRotator::ZeroRotator);Proxy->SetWorldScale3D(FVector(1));
    Proxy->SetBoxExtent(Mesh->Bounds.BoxExtent+FVector(1));
    LightFixtures.Add({Mesh,Proxy,Room});LightFixtureRooms.Add(Mesh,Room);LightFixtureRooms.Add(Proxy,Room);
}
bool ACourseworkHUD::SetLightTarget(const FHitResult& Hit)
{
    const auto* Room=LightFixtureRooms.Find(Hit.GetComponent());if(!Room||!bLightingReady)return false;
    TargetLightFixture.Reset();
    for(const auto& Fixture:LightFixtures)if(Fixture.Mesh.Get()==Hit.GetComponent()||Fixture.Proxy.Get()==Hit.GetComponent())
    {TargetLightFixture=Fixture.Proxy.Get();break;}
    if(!TargetLightFixture.IsValid())return false;
    // Keep the visible point that was actually selected, not an occluded center.
    const FVector Point=Hit.ImpactPoint+(TargetLightFixture->Bounds.Origin-Hit.ImpactPoint).GetSafeNormal()*2.f;
    FVector Eye;FRotator View;GetOwningPlayerController()->GetPlayerViewPoint(Eye,View);
    FHitResult Visible;FCollisionQueryParams Query(SCENE_QUERY_STAT(SelectedLampOcclusion),true,GetOwningPlayerController()->GetPawn());
    if(!GetWorld()->LineTraceSingleByChannel(Visible,Eye,Point,ECC_Visibility,Query)||Visible.GetActor()!=Hit.GetActor()||LampViewBlocked(GetWorld(),GetOwningPlayerController()->GetPawn(),Hit.GetActor(),Eye,Point)){TargetLightFixture.Reset();return false;}
    TargetLightAimPoint=TargetLightFixture->GetComponentTransform().InverseTransformPosition(Point);
    TargetLightRoom=*Room;TargetActor=Hit.GetActor();TargetLabel=FString::Printf(TEXT("%s照明"),CourseworkRooms::Get(*Room).Name);
    RefreshLightInteraction();return true;
}
void ACourseworkHUD::RefreshLightInteraction()
{
    const int32 Room=bLightKeyHeld?HeldLightRoom:TargetLightRoom;if(Room==INDEX_NONE||!Overlay)return;
    const auto& Descriptor=CourseworkRooms::Get(Room);
    const FString Name=FString::Printf(TEXT("%s · %s照明"),Descriptor.Floor,Descriptor.Name);
    auto* Fixture=bLightKeyHeld?HeldLightFixture.Get():TargetLightFixture.Get();
    const float Actual=SmartLightingScene&&Fixture?GetSmartFixtureLevel(Fixture->GetOwner()->GetFName()):(IsRoomLightOn(Room)?GetRoomBrightness(Room):0);
    const FString DisplayName=SmartLightingScene?FString::Printf(TEXT("%s · %s灯具"),Descriptor.Floor,Descriptor.Name):Name;
    Overlay->SetLightInteraction(DisplayName,FString::Printf(TEXT("%s  ·  %d%%"),Actual>.001f?TEXT("开启"):TEXT("关闭"),FMath::RoundToInt(Actual*100)),Actual,bLightDimming);
}
void ACourseworkHUD::BeginInteract()
{
    if(PanelMode!=0||bLightKeyHeld)return;
    UpdateInteraction();
    if(TargetLightRoom==INDEX_NONE){Interact();return;}
    HeldLightRoom=TargetLightRoom;
    HeldLightFixture=TargetLightFixture;HeldLightAimPoint=TargetLightAimPoint;
    if(!HeldLightFixture.IsValid())return;
    UE_LOG(LogTemp,Display,TEXT("LAMP_INTERACTION actor=%s room=%d on=%d brightness=%.2f"),*HeldLightFixture->GetOwner()->GetName(),HeldLightRoom,IsRoomLightOn(HeldLightRoom),GetRoomBrightness(HeldLightRoom));
    bLightKeyHeld=true;bLightDimming=false;LightKeyPressedAt=GetWorld()->GetRealTimeSeconds();
    if(InputComponent&&InputComponent->AxisKeyBindings.IsValidIndex(WheelBindingIndex))InputComponent->AxisKeyBindings[WheelBindingIndex].bConsumeInput=true;
    RefreshLightInteraction();
}
bool ACourseworkHUD::IsHeldLightValid()const
{
    auto* PC=GetOwningPlayerController();auto* Fixture=HeldLightFixture.Get();
    if(!PC||!Fixture||PanelMode!=0)return false;
    FVector Eye;FRotator Rotation;PC->GetPlayerViewPoint(Eye,Rotation);
    const FVector Center=Fixture->Bounds.Origin;const FVector Closest=Fixture->Bounds.GetBox().GetClosestPointTo(Eye);
    if(FVector::Dist(Eye,Closest)>390||FVector::DotProduct(Rotation.Vector(),(Center-Eye).GetSafeNormal())<.25f)return false;
    FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(HeldLampOcclusion),true,PC->GetPawn());
    const FVector Point=Fixture->GetComponentTransform().TransformPosition(HeldLightAimPoint);
    return GetWorld()->LineTraceSingleByChannel(Hit,Eye,Point,ECC_Visibility,Query)&&Hit.GetActor()==Fixture->GetOwner()
        &&!LampViewBlocked(GetWorld(),PC->GetPawn(),Fixture->GetOwner(),Eye,Point);
}
void ACourseworkHUD::TickLightInteraction()
{
    if(!bLightKeyHeld)return;
    const auto* Viewport=GetWorld()->GetGameViewport();
    if(!IsHeldLightValid()||(TestMode.IsEmpty()&&Viewport&&Viewport->Viewport&&!Viewport->Viewport->HasFocus())){CancelLightInteraction();return;}
    if(GetWorld()->GetRealTimeSeconds()-LightKeyPressedAt>=.3)bLightDimming=true;
    RefreshLightInteraction();
}
void ACourseworkHUD::EndInteract()
{
    if(!bLightKeyHeld)return;
    if(IsHeldLightValid()&&!bLightDimming&&GetWorld()->GetRealTimeSeconds()-LightKeyPressedAt<.3)
    {
        if(SmartLightingScene&&HeldLightFixture.IsValid())
        {
            const FName Name=HeldLightFixture->GetOwner()->GetFName();
            const auto* State=SmartFixtures.Find(Name);
            SetSmartFixtureManual(Name,GetSmartTargetLevel(Name)>.001f?0.f:(State?State->Restore:1.f));
        }
        else SetRoomLighting(HeldLightRoom,!IsRoomLightOn(HeldLightRoom),GetRoomBrightness(HeldLightRoom));
    }
    CancelLightInteraction();UpdateInteraction();
}
void ACourseworkHUD::CancelLightInteraction()
{
    bLightKeyHeld=false;bLightDimming=false;HeldLightRoom=INDEX_NONE;HeldLightFixture.Reset();TargetLightRoom=INDEX_NONE;
    if(InputComponent&&InputComponent->AxisKeyBindings.IsValidIndex(WheelBindingIndex))InputComponent->AxisKeyBindings[WheelBindingIndex].bConsumeInput=false;
    if(Overlay)Overlay->SetInteraction(TEXT(""));
}
void ACourseworkHUD::AdjustHeldLight(float Delta)
{
    if(FMath::IsNearlyZero(Delta)||!bLightKeyHeld||!IsHeldLightValid()||GetWorld()->GetRealTimeSeconds()-LightKeyPressedAt<.3)return;
    bLightDimming=true;
    auto* PC=GetOwningPlayerController();const bool Fine=PC->IsInputKeyDown(EKeys::LeftShift)||PC->IsInputKeyDown(EKeys::RightShift);
    const int32 Current=FMath::RoundToInt((SmartLightingScene&&HeldLightFixture.IsValid()?GetSmartFixtureLevel(HeldLightFixture->GetOwner()->GetFName()):(IsRoomLightOn(HeldLightRoom)?GetRoomBrightness(HeldLightRoom):0))*100);
    const int32 Next=FMath::Clamp(Current+FMath::RoundToInt(Delta*(Fine?1.f:5.f)),0,100);
    if(SmartLightingScene&&HeldLightFixture.IsValid())SetSmartFixtureManual(HeldLightFixture->GetOwner()->GetFName(),Next/100.f);
    else SetRoomLighting(HeldLightRoom,Next>0,Next/100.f);
    RefreshLightInteraction();
}
