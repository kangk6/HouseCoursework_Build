#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
bool ACourseworkHUD::SetDaylightHour(float Hour)
{
    if(IsSafetyDrillRunning()){ShowToast(TEXT("疏散演练期间暂停采光对比"));return false;}
    if(!FMath::IsFinite(Hour))return false;
    if(!Daylight)Daylight=GetWorld()->SpawnActor<ACourseworkDaylight>();
    const bool Applied=Daylight&&Daylight->Apply(Hour);
    if(Applied)UE_LOG(LogTemp,Display,TEXT("ENVIRONMENT_TIME target=%.3f elevation=%.3f smart_scene=%d"),GetDaylightHour(),GetDaylightElevation(),SmartLightingScene);
    return Applied;
}
void ACourseworkHUD::RestoreDaylight(){if(Daylight)Daylight->Restore();}
bool ACourseworkHUD::IsDaylightActive()const{return Daylight&&Daylight->IsActive();}
float ACourseworkHUD::GetDaylightHour()const{return Daylight?Daylight->GetHour():12.f;}
float ACourseworkHUD::GetDaylightElevation()const{return Daylight?Daylight->GetElevation():65.f;}
bool ACourseworkHUD::NeedsNightLighting()const{return !IsDaylightActive()||GetDaylightElevation()<6.f;}
void ACourseworkHUD::ObserveDaylight()
{
    if(IsSafetyDrillRunning()){ShowToast(TEXT("演练期间请步行疏散"));return;}
    if(auto* PC=GetOwningPlayerController())if(auto* C=Cast<ACharacter>(PC->GetPawn()))
    {
        C->GetCharacterMovement()->StopMovementImmediately();
        if(C->TeleportTo(FVector(-2100,9870,151.5),FRotator(0,180,0),false,false))
        {PC->SetControlRotation(FRotator(-25,180,0));SetPanel(0);}
        else ShowToast(TEXT("观察位置被占用，请靠近客厅窗边查看"));
    }
}
