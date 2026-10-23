#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include <limits>
#include "UObject/UObjectGlobals.h"

void ACourseworkHUD::TickDaylightAcceptance()
{
    if(!bLightingReady)return;
    struct FRun{int32 Stage=0;double At=0;TArray<bool> On;TArray<float> Levels;FRotator Morning,Noon;};
    static FRun R;const double Now=GetWorld()->GetRealTimeSeconds();
    auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
    auto Record=[&](const TCHAR* Name,bool Passed){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Passed?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("DAYLIGHT_CHECK %s"),*TestResults.Last());};
    auto Click=[&](float X,float Y){return Overlay->ExerciseControlPointer({X,Y});};
    auto LightsSame=[&](){return R.On==RoomLightOn&&R.Levels==RoomBrightness;};
    auto Shot=[&](const TCHAR* Name,int32 Reopen=-1){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.5f,false);if(Reopen>=0){FTimerHandle ReopenTimer;GetWorld()->GetTimerManager().SetTimer(ReopenTimer,[this,Reopen](){SetPanel(Reopen);},1.f,false);}};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=Now;ResetKitchenAlarm();RestoreEnergyComparison();RestoreDaylight();
        for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,I%3!=0,.2f+.06f*I);
        R.On=RoomLightOn;R.Levels=RoomBrightness;SaveRoomLighting();
        ObserveDaylight();Player->GetCharacterMovement()->DisableMovement();PC->PlayerCameraManager->UpdateCamera(0);
        Record(TEXT("observation_point_reachable"),PanelMode==0&&FVector::Dist(Player->GetActorLocation(),FVector(-2100,9870,151.5))<45);
        Shot(TEXT("Daylight_original.png"),6);return;
    }
    if(Now-R.At<1.8)return;R.At=Now;
    switch(R.Stage++)
    {
    case 0:Record(TEXT("comfort_header_pointer"),Click(690,43)&&PanelMode==9);Record(TEXT("opening_page_leaves_original_night"),!IsDaylightActive());Shot(TEXT("Daylight_panel_original.png"));break;
    case 1:Record(TEXT("morning_preset_pointer"),Click(360,580)&&IsDaylightActive()&&GetDaylightHour()==8);Record(TEXT("morning_elevation"),FMath::IsNearlyEqual(GetDaylightElevation(),32.5f,.01f));Record(TEXT("sun_casts_dynamic_shadows"),Daylight&&Daylight->IsSunShadowed());Record(TEXT("daylight_preserves_all_household_lights"),LightsSame());R.Morning=Daylight->GetSunRotation();SetPanel(0);break;
    case 2:Record(TEXT("no_unbuilt_environment_lighting"),GetWorld()->NumLightingUnbuiltObjects==0);Shot(TEXT("Daylight_morning.png"),9);break;
    case 3:Record(TEXT("noon_preset_pointer"),Click(500,580)&&GetDaylightHour()==12);Record(TEXT("noon_elevation"),FMath::IsNearlyEqual(GetDaylightElevation(),65.f,.01f));SetPanel(0);break;
    case 4:R.Noon=Daylight->GetSunRotation();Record(TEXT("sun_direction_changes"),!R.Noon.Equals(R.Morning,1));Shot(TEXT("Daylight_noon.png"),9);break;
    case 5:Record(TEXT("evening_preset_pointer"),Click(630,580)&&GetDaylightHour()==17);Record(TEXT("evening_angle_lower_than_noon"),GetDaylightElevation()<25);SetPanel(0);break;
    case 6:Shot(TEXT("Daylight_evening.png"),9);break;
    case 7:Record(TEXT("night_preset_pointer"),Click(770,580)&&GetDaylightHour()==21);SetPanel(0);break;
    case 8:Record(TEXT("night_has_no_sunlight"),Daylight->GetSunIntensity()==0);Shot(TEXT("Daylight_night.png"),9);break;
    case 9:Record(TEXT("continuous_slider_pointer"),Click(570,470)&&FMath::IsNearlyEqual(GetDaylightHour(),12.f,.001f));Record(TEXT("slider_leaves_room_states"),LightsSame());Shot(TEXT("Daylight_panel_active.png"));break;
    case 10:CollectGarbage(RF_NoFlags);Record(TEXT("preview_survives_garbage_collection"),IsDaylightActive());Record(TEXT("slider_minimum"),Click(300,470)&&GetDaylightHour()==0);Record(TEXT("slider_maximum"),Click(840,470)&&FMath::IsNearlyEqual(GetDaylightHour(),1439.f/60,.001f));Record(TEXT("nan_rejected"),!SetDaylightHour(std::numeric_limits<float>::quiet_NaN())&&FMath::IsNearlyEqual(GetDaylightHour(),1439.f/60,.001f));break;
    case 11:Record(TEXT("restore_pointer"),Click(420,710)&&IsDaylightActive()&&GetDaylightHour()==12);Record(TEXT("noon_reset_preserves_fixed_mode"),!IsTimeFlowing());RestoreDaylight();Record(TEXT("environment_snapshot_exactly_restored"),Daylight->CheckRestored());Record(TEXT("restore_preserves_lights"),LightsSame());SetPanel(0);break;
    case 12:Shot(TEXT("Daylight_restored.png"),9);break;
    case 13:Click(360,580);Record(TEXT("observation_pointer"),Click(1200,710)&&PanelMode==0&&FVector::Dist(Player->GetActorLocation(),FVector(-2100,9870,151.5))<45);break;
    case 14:SetPanel(9);TriggerKitchenAlarm();Record(TEXT("fire_preserves_selected_environment"),IsSafetyDrillRunning()&&IsDaylightActive()&&GetDaylightHour()==8);Record(TEXT("fire_rejects_daylight_change"),!SetDaylightHour(12)&&GetDaylightHour()==8);Record(TEXT("fire_rejects_preset_pointer"),Click(500,580)&&GetDaylightHour()==8);Record(TEXT("fire_rejects_restore_pointer"),Click(420,710)&&GetDaylightHour()==8&&IsDaylightActive());break;
    case 15:ResetKitchenAlarm();Record(TEXT("after_fire_can_compare_again"),SetDaylightHour(12)&&IsDaylightActive());Record(TEXT("repeated_preview_preserves_lights"),LightsSame());Record(TEXT("energy_can_coexist"),ApplyEnergyScenario(true)&&IsDaylightActive());RestoreEnergyComparison();Record(TEXT("energy_restore_preserves_daylight_and_lights"),IsDaylightActive()&&LightsSame());RestoreDaylight();Record(TEXT("second_environment_snapshot_restored"),Daylight->CheckRestored());break;
    default:
        Record(TEXT("open_panel_blocks_movement"),PanelMode==9&&PC->IsMoveInputIgnored());SetPanel(0);Record(TEXT("closing_releases_input"),!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored()&&!PC->bShowMouseCursor);
        FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("DaylightAcceptance.json")));TestMode.Empty();QuitApplication();return;
    }
}
