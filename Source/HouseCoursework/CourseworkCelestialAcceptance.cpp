#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void ACourseworkHUD::TickCelestialAcceptance()
{
 if(!bLightingReady)return;
 struct FRun{int32 Stage=0;double At=0;FVector Fixed,Before;double Hour=0;};static FRun R;
 auto* PC=GetOwningPlayerController();auto* P=Cast<ACharacter>(PC->GetPawn());const double Now=GetWorld()->GetRealTimeSeconds();
 auto Check=[&](const TCHAR* Name,bool Good){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),Name,Good?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("CELESTIAL_CHECK %s"),*TestResults.Last());};
 auto Aim=[&](bool Moon){FRotator View=(Moon?Daylight->GetMoonDirection():-Daylight->GetSunRotation().Vector()).Rotation();View.Yaw+=5;View.Pitch-=3;PC->SetControlRotation(View);PC->PlayerCameraManager->UpdateCamera(0);};
 auto Hour=[&](float H){SetDaylightHour(H);Daylight->Snap(H);};
 auto WeatherNow=[&](int32 Kind){Weather.Choose(Kind);Weather.Advance(11,0,false,false);};
 auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.3f,false);};
 if(TestCase<0)
 {
  TestCase=0;R=FRun();R.At=Now;SetPanel(0);InitializeTimeOfDay();SetTimeFlowing(false);Weather.Initialize(1,false);
  P->GetCharacterMovement()->DisableMovement();P->SetActorEnableCollision(false);P->SetActorLocation({-1400,11000,1000});
  Hour(12);Aim(false);R.Fixed=Daylight->GetSunRotation().Vector();return;
 }
 if(Now-R.At<4)return;R.At=Now;
 switch(R.Stage++)
 {
 case 0:
  Check(TEXT("cooked_celestial_material_loaded"),Daylight->HasCelestialSky());
  Check(TEXT("old_disks_disabled_and_directions_match"),Daylight->CheckCelestialParameters());
  Check(TEXT("noon_has_sun_without_moon_or_stars"),Daylight->GetCelestialSunAmount()>.99f&&Daylight->GetCelestialMoonAmount()==0&&Daylight->GetCelestialStarAmount()==0);
  Check(TEXT("fixed_time_keeps_body_position"),R.Fixed.Equals(Daylight->GetSunRotation().Vector(),.00001f));
  Check(TEXT("sun_uses_existing_solar_direction"),FMath::IsNearlyEqual(Daylight->GetRenderedElevation(),65.f,.01f));Shot(TEXT("Celestial_noon.png"));break;
 case 1:Hour(17.5f);Aim(false);break;
 case 2:Check(TEXT("sunset_tracks_horizon"),Daylight->GetRenderedElevation()>0&&Daylight->GetRenderedElevation()<10);Shot(TEXT("Celestial_sunset.png"));break;
 case 3:Hour(0);Aim(true);break;
 case 4:
  Check(TEXT("midnight_has_moon_and_stars_without_sun"),Daylight->GetCelestialMoonAmount()>.99f&&Daylight->GetCelestialStarAmount()>.99f&&Daylight->GetCelestialSunAmount()==0);
  Check(TEXT("moon_direction_matches_visible_body"),Daylight->CheckCelestialParameters());Check(TEXT("moon_light_is_low_and_nonzero"),Daylight->GetMoonIntensity()>.01f&&Daylight->GetMoonIntensity()<=.12f);
  Check(TEXT("night_solar_stays_zero"),SolarFlow.Generation==0);Shot(TEXT("Celestial_midnight.png"));break;
 case 5:WeatherNow(1);break;
 case 6:Check(TEXT("cloudy_sky_keeps_some_celestial_visibility"),Daylight->GetCelestialMoonAmount()>0);Shot(TEXT("Celestial_cloudy.png"));break;
 case 7:WeatherNow(3);break;
 case 8:
  Check(TEXT("rain_hides_moon_and_stars"),Daylight->GetCelestialMoonAmount()==0&&Daylight->GetCelestialStarAmount()==0);
  Check(TEXT("rain_removes_directional_moonlight"),Daylight->GetMoonIntensity()==0);Shot(TEXT("Celestial_rain_night.png"));break;
 case 9:Hour(12);Aim(false);break;
 case 10:Check(TEXT("rain_hides_noon_sun"),Daylight->GetCelestialSunAmount()==0);Shot(TEXT("Celestial_rain_noon.png"));break;
 case 11:WeatherNow(0);Hour(6);PC->SetControlRotation({6,90,0});break;
 case 12:Check(TEXT("dawn_has_no_star_field"),Daylight->GetCelestialStarAmount()==0);Shot(TEXT("Celestial_dawn.png"));break;
 case 13:Hour(23.99f);R.Before=Daylight->GetMoonDirection();SetDaylightHour(.01f);break;
 case 14:
  Check(TEXT("midnight_wrap_keeps_body_continuity"),FVector::DotProduct(R.Before,Daylight->GetMoonDirection())>.999f);
  R.Before=Daylight->GetMoonDirection();SetTimeFlowing(true);break;
 case 15:
  Check(TEXT("flowing_clock_moves_bodies"),!R.Before.Equals(Daylight->GetMoonDirection(),.00001f));
  R.Before=Daylight->GetMoonDirection();R.Hour=GetDaylightHour();SetPanel(2);break;
 case 16:Check(TEXT("pause_keeps_time_and_body_position"),GetDaylightHour()==R.Hour&&R.Before.Equals(Daylight->GetMoonDirection(),.00001f));SetPanel(0);SetTimeFlowing(false);Hour(21);TriggerKitchenAlarm();R.Hour=GetDaylightHour();break;
 case 17:Check(TEXT("fire_keeps_selected_time"),IsSafetyDrillRunning()&&GetDaylightHour()==R.Hour);ResetKitchenAlarm();Check(TEXT("after_fire_celestial_state_remains_valid"),Daylight->CheckCelestialParameters());RestoreDaylight();Check(TEXT("restore_removes_moon_and_restores_authored_sky"),Daylight->CheckRestored());
  InitializeTimeOfDay();SetTimeFlowing(false);WeatherNow(0);Hour(21);P->SetActorEnableCollision(true);
  Check(TEXT("courtyard_standing_position_reachable"),P->TeleportTo({-2800,10400,150},FRotator::ZeroRotator,false,false));P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Aim(true);break;
 case 18:Shot(TEXT("Celestial_yard_moon.png"));break;
 case 19:Hour(17.7f);Aim(false);break;
 case 20:Shot(TEXT("Celestial_yard_sunset.png"));break;
 case 21:Hour(19);ObserveVentilation(1);PC->SetControlRotation(PC->GetControlRotation()+FRotator(12,0,0));break;
 case 22:Check(TEXT("bedroom_window_uses_normal_eye_height"),P->GetActorLocation().Z>380&&P->GetActorLocation().Z<440);Shot(TEXT("Celestial_bedroom_window.png"));break;
 case 23:Hour(21);ObserveVentilation(4);PC->SetControlRotation(PC->GetControlRotation()+FRotator(12,0,0));break;
 case 24:Check(TEXT("study_window_uses_normal_eye_height"),P->GetActorLocation().Z>380&&P->GetActorLocation().Z<440);Shot(TEXT("Celestial_study_window.png"));break;
 case 25:Hour(12);WeatherNow(0);P->TeleportTo({-2800,10400,150},FRotator::ZeroRotator,false,false);PC->SetControlRotation({-3,-95,0});break;
 case 26:Shot(TEXT("Celestial_yard_noon.png"));break;
 case 27:WeatherNow(3);break;
 case 28:Shot(TEXT("Celestial_yard_rain.png"));break;
 case 29:WeatherNow(0);Hour(0);ObserveVentilation(1);PC->SetControlRotation(PC->GetControlRotation()+FRotator(12,0,0));break;
 case 30:Shot(TEXT("Celestial_bedroom_midnight.png"));break;
 case 31:
  FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Diagnostic sky and normal-height courtyard/window placements, native time/weather state changes; not a continuous player walkthrough\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("CelestialAcceptance.json")));TestMode.Empty();QuitApplication();return;
 }
}
