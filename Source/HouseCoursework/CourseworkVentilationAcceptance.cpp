#include "CourseworkHUD.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CourseworkVentilation.h"
#include "CourseworkAirNetwork.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "AudioMixerBlueprintLibrary.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/UObjectGlobals.h"

void ACourseworkHUD::TickVentilationAcceptance()
{
 if(!bLightingReady||!Ventilation)return;
 struct FRun{int32 Phase=0,Window=0,Step=0;double At=0;FVector LocalResponse=FVector::ZeroVector;TArray<bool> On;TArray<float> Levels;TWeakObjectPtr<UStaticMeshComponent> Door;};
 static FRun R;const double Now=GetWorld()->GetRealTimeSeconds();auto* PC=GetOwningPlayerController();auto* Player=Cast<ACharacter>(PC->GetPawn());
 auto Record=[&](FString Name,bool Pass){TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Pass?TEXT("true"):TEXT("false")));UE_LOG(LogTemp,Display,TEXT("VENTILATION_CHECK %s"),*TestResults.Last());};
 auto Key=[&](bool Down){PC->InputKey(FInputKeyParams(EKeys::E,Down?IE_Pressed:IE_Released,Down?1.:0.));};
 auto Click=[&](float X,float Y){return Overlay->ExerciseControlPointer({X,Y});};
 auto Shot=[&](const TCHAR* Name,float Delay=.25f){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},Delay,false);};
 auto SameLights=[&](){return R.On==RoomLightOn&&R.Levels==RoomBrightness;};
 auto LivingSide=[&](bool Windward){Ventilation->CloseAll();for(int32 I=0;I<Ventilation->GetWindowCount();++I)if(Ventilation->GetWindowRoom(I)==0&&((Ventilation->GetWindowInward(I).X<-.5f)==Windward))Ventilation->ToggleWindow(I);};
 auto Aim=[&](int32 Index)
 {
  SetPanel(0);const auto Center=Ventilation->GetWindowPoint(Index),Inward=Ventilation->GetWindowInward(Index);const auto Side=FVector::CrossProduct(FVector::UpVector,Inward);
  const float Base=Center.Z<300?151.5f:413.f;FCollisionQueryParams Q(SCENE_QUERY_STAT(WindowTestAim),true,Player);
  for(float Height:{0.f,30.f,50.f,-15.f})for(float Distance:{85.f,115.f,145.f,180.f,220.f})for(float Offset:{0.f,35.f,-35.f,65.f,-65.f,95.f,-95.f})
  {
   FVector P=Center+Inward*Distance+Side*Offset;P.Z=Base;FHitResult Floor;
   if(!GetWorld()->LineTraceSingleByChannel(Floor,P+FVector(0,0,30),P-FVector(0,0,150),ECC_Pawn,Q)||Floor.ImpactNormal.Z<.8f)continue;
   P.Z=Floor.ImpactPoint.Z+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1;
   if(FMath::Abs(P.Z-Base)>40||!Player->TeleportTo(P,(-Inward).Rotation(),false,false))continue;
   PC->SetControlRotation((Center+FVector(0,0,Height)-Player->GetPawnViewLocation()).Rotation());PC->PlayerCameraManager->UpdateCamera(0);UpdateInteraction();
   if(TargetWindow==Index)return true;
   FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*350,ECC_Visibility,Q);UE_LOG(LogTemp,Verbose,TEXT("WINDOW_AIM_BLOCKED index=%d height=%.0f eye=%s component=%s/%s target=%d"),Index,Height,*Eye.ToCompactString(),Hit.GetActor()?*Hit.GetActor()->GetName():TEXT("none"),Hit.GetComponent()?*Hit.GetComponent()->GetName():TEXT("none"),TargetWindow);
  }
  return false;
 };
 if(TestMode==TEXT("ventilation_polish"))
 {
  struct FReview{int32 Stage=0,Frame=0;double At=0,LastShot=0;bool Valid=true;TWeakObjectPtr<ACameraActor> Camera;TWeakObjectPtr<UStaticMeshComponent> Door;};static FReview V;
  const FString AudioPath=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("VentilationAudio"));
  auto StartAudio=[&](){UAudioMixerBlueprintLibrary::StartRecordingOutput(this,6.f);};
  auto StopAudio=[&](const TCHAR* Name){UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,Name,AudioPath);};
  if(TestCase<0)
  {
   TestCase=0;V=FReview();V.At=Now;SetPanel(0);SetTimeFlowing(false);SetDaylightHour(12);Daylight->Snap(12);Ventilation->CloseAll();Ventilation->SetDisplay(true);
   Player->GetCharacterMovement()->DisableMovement();
   int32 Window=INDEX_NONE;for(int32 I=0;I<Ventilation->GetWindowCount();++I)if(Ventilation->GetWindowRoom(I)==4&&Ventilation->GetWindowInward(I).X<-.5f){Window=I;break;}
   Record(TEXT("study_windward_window_available"),Window!=INDEX_NONE);
   if(Window==INDEX_NONE){TestMode.Empty();QuitApplication();return;}
   const FVector Center=Ventilation->GetWindowPoint(Window),Inward=Ventilation->GetWindowInward(Window),Side=FVector::CrossProduct(FVector::UpVector,Inward);
   auto* Camera=GetWorld()->SpawnActor<ACameraActor>();V.Camera=Camera;Camera->SetActorLocation(Center+Inward*170+Side*55+FVector(0,0,15));Camera->SetActorRotation((Center+Inward*30-Camera->GetActorLocation()).Rotation());Camera->GetCameraComponent()->SetFieldOfView(78);
   PC->SetViewTarget(Camera);PC->SetAudioListenerOverride(Camera->GetRootComponent(),FVector::ZeroVector,FRotator::ZeroRotator);Ventilation->ToggleWindow(Window);
   for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->GetName()==TEXT("BP_Door_Inside_A2_5")){TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* M:Parts)if(M->GetName()==TEXT("Door"))V.Door=M;}
   return;
  }
  V.Valid&=Ventilation->CheckPolishState();
  if((V.Stage==1||V.Stage==3||V.Stage==4)&&Now-V.LastShot>.25){V.LastShot=Now;FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Polish_%d_%03d.png"),V.Stage,V.Frame++),true,false);}
  const double Duration=V.Stage==1||V.Stage==3?5.:V.Stage==5?3.:2.;if(Now-V.At<Duration)return;V.At=Now;
  switch(V.Stage++)
  {
   case 0:Record(TEXT("single_window_has_no_false_crossflow"),!Ventilation->HasCrossFlow(4));StartAudio();break;
   case 1:StopAudio(TEXT("single"));SetRoomWindows(8,true);Record(TEXT("study_door_available"),V.Door.IsValid());if(V.Door.IsValid())InvokeFurniture(V.Door->GetOwner(),V.Door.Get());break;
   case 2:Record(TEXT("open_hall_door_forms_study_crossflow"),Ventilation->HasCrossFlow(4)&&Ventilation->CheckPathsClear());StartAudio();break;
   case 3:StopAudio(TEXT("cross"));Ventilation->CloseAll();if(V.Door.IsValid())InvokeFurniture(V.Door->GetOwner(),V.Door.Get());break;
   case 4:Record(TEXT("closing_windows_removes_active_routes"),Ventilation->GetPathCount()==0);StartAudio();break;
   case 5:StopAudio(TEXT("closed"));break;
   default:
    Record(TEXT("transition_values_finite_and_bounded"),V.Valid);Record(TEXT("closed_fabric_settles_and_audio_gain_silences"),Ventilation->CheckCurtains(false)&&Ventilation->GetResponseSample().Y<.002);
    const FIntPoint Transitions=Ventilation->GetTrailTransitionCounts();Record(TEXT("routes_reuse_motion_and_retire_old_tails"),Transitions.X>0&&Transitions.Y>0);
    FFileHelper::SaveStringToFile(Ventilation->GetRenderDiagnostics(),*(FPaths::ProjectSavedDir()/TEXT("VentilationPolishRendering.txt")));
    FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Fixed study camera and audio listener; single window, hall crossflow and closed master-mix recordings; synthetic private profile\",\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("VentilationPolish.json")));PC->ClearAudioListenerOverride();TestMode.Empty();QuitApplication();return;
  }
  return;
 }
 if(TestMode==TEXT("ventilation_view"))
 {
  if(Now-R.At<2)return;R.At=Now;
  switch(R.Step++){
   case 0:TestCase=0;Player->GetCharacterMovement()->DisableMovement();SetRoomWindows(0,true);SetRoomWindows(2,true);Ventilation->SetDisplay(true);Player->SetActorLocation(FVector(-2050,9660,151.5));break;
   case 1:{PC->PlayerCameraManager->UpdateCamera(0);FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);PC->SetControlRotation((Ventilation->GetFlowFocus()-Eye).Rotation());Shot(TEXT("Ventilation_material_view.png"));FFileHelper::SaveStringToFile(Ventilation->GetRenderDiagnostics(),*(FPaths::ProjectSavedDir()/TEXT("VentilationRendering.txt")));break;}
   case 2:Ventilation->SetDisplay(false);Shot(TEXT("Ventilation_hidden_view.png"));break;
   default:TestMode.Empty();QuitApplication();break;
  }return;
 }
 if(TestCase<0)
 {
  TestCase=0;R=FRun();R.At=Now;ResetKitchenAlarm();RestoreEnergyComparison();RestoreDaylight();Ventilation->CloseAll();Ventilation->SetDisplay(false);
  R.On=RoomLightOn;R.Levels=RoomBrightness;Player->GetCharacterMovement()->DisableMovement();
  Record(TEXT("assets_and_runtime_ready"),Ventilation->IsReady());Record(TEXT("all_twelve_modules_sixteen_moving_leaves"),Ventilation->GetWindowCount()==16);
  {
   using namespace CourseworkAirNetwork;
   TArray<float> P;TArray<CourseworkAirNetwork::FEdge> Sym={{0,INDEX_NONE,1,1},{0,INDEX_NONE,1,-.5f}};
   const float Balance=Solve(1,Sym,P),Full=FMath::Abs(Sym[0].Flow);
   Record(TEXT("orifice_network_matches_symmetric_analytic_solution"),Balance<1.e-4f&&FMath::Abs(P[0]-.25f)<1.e-4f&&FMath::Abs(Full-FMath::Sqrt(.75f))<1.e-4f);
   TArray<CourseworkAirNetwork::FEdge> Narrow={{0,INDEX_NONE,.1f,1},{0,INDEX_NONE,1,-.5f}};
   Solve(1,Narrow,P);Record(TEXT("small_aperture_limits_exchange_without_reversing_flow"),Narrow[0].Flow<0&&Narrow[1].Flow>0&&FMath::Abs(Narrow[0].Flow)<Full*.2f);
   TArray<CourseworkAirNetwork::FEdge> Reverse={{0,INDEX_NONE,1,-.5f},{0,INDEX_NONE,1,1}};
   Solve(1,Reverse,P);Record(TEXT("reversed_boundary_pressure_reverses_flow"),Reverse[0].Flow>0&&Reverse[1].Flow<0&&FMath::Abs(Reverse[0].Flow-Full)<1.e-4f);
   TArray<CourseworkAirNetwork::FEdge> Chain={{0,INDEX_NONE,1,1},{1,INDEX_NONE,1,-.5f},{0,1,.2f}};
   const float ChainBalance=Solve(2,Chain,P),Restricted=FMath::Abs(Chain[0].Flow);Chain[2].Conductance=3;
   const float WideBalance=Solve(2,Chain,P);
   Record(TEXT("partial_door_conductance_continuously_limits_multizone_flow"),ChainBalance<1.e-4f&&WideBalance<1.e-4f&&Restricted<FMath::Abs(Chain[0].Flow)*.4f);
   TArray<CourseworkAirNetwork::FEdge> Isolated={{0,INDEX_NONE,1,1},{1,INDEX_NONE,1,-.5f}};
   Solve(2,Isolated,P);Record(TEXT("closed_door_has_no_invented_net_throughflow"),FMath::Abs(Isolated[0].Flow)<1.e-4f&&FMath::Abs(Isolated[1].Flow)<1.e-4f);
   Record(TEXT("effective_area_follows_drawn_slide_and_hinge_pose"),Aperture(0,40)==0&&Aperture(0,0)==0&&FMath::Abs(Aperture(.25f,0)-.25f)<1.e-6f&&Aperture(.25f,40)<Aperture(.5f,40)&&Aperture(.5f,40)<Aperture(1,40));
   TArray<CourseworkAirNetwork::FEdge> DeadEnd={{0,INDEX_NONE,.8f,-.5f},{0,1,3},{0,2,3}};
   Record(TEXT("single_opening_with_dead_end_rooms_conserves_mass"),Solve(4,DeadEnd,P)<1.e-5f&&FMath::Abs(DeadEnd[0].Flow)<1.e-5f&&FMath::Abs(DeadEnd[1].Flow)<1.e-5f);
   DeadEnd.Add({0,INDEX_NONE,.8f,1});DeadEnd.Add({1,INDEX_NONE,.4f,-.5f});
   Record(TEXT("asymmetric_openings_and_unanchored_room_converge"),Solve(4,DeadEnd,P)<1.e-5f&&FMath::Abs(DeadEnd[2].Flow)<1.e-5f&&P[3]==0);
   DeadEnd.Add({4,5,.01f});Record(TEXT("sealed_connected_component_does_not_break_anchored_solver"),Solve(6,DeadEnd,P)<1.e-5f&&P[4]==0&&P[5]==0);
  }
  Record(TEXT("room_inventory_includes_new_corner_windows"),GetRoomWindowCount(0)==4&&GetRoomWindowCount(2)==2&&GetRoomWindowCount(5)==1&&GetRoomWindowCount(1)==1&&GetRoomWindowCount(3)==1&&GetRoomWindowCount(4)==2&&GetRoomWindowCount(8)==2);
  Record(TEXT("corner_windows_in_dining_and_side_room"),GetRoomWindowCount(6)==0&&GetRoomWindowCount(7)==2&&GetRoomWindowCount(9)==1);
  Record(TEXT("ribbons_hidden_by_default"),!IsAirflowDisplayed());Record(TEXT("leaf_materials_available"),Ventilation->GetPlantCount()>=3);Record(TEXT("curtains_ready_and_pinned_when_closed"),Ventilation->GetCurtainCount()>0&&Ventilation->CheckCurtains(false));Record(TEXT("closed_windows_no_leaf_motion"),Ventilation->GetMaxLeafMotion()<.001f);return;
 }
 if(Now-R.At<(R.Phase==0?.9:1.6))return;R.At=Now;
 if(R.Phase==0)
 {
  int32 Limit=0;FParse::Value(FCommandLine::Get(),TEXT("HouseWindowLimit="),Limit);if(Limit>0&&R.Window>=Limit){FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("WindowProbe.json")));TestMode.Empty();QuitApplication();return;}
  if(R.Window>=Ventilation->GetWindowCount()){R.Phase=1;R.Step=0;SetPanel(9);return;}
  const FString Prefix=FString::Printf(TEXT("window_%02d_"),R.Window);
  switch(R.Step++)
  {
   case 0:Record(Prefix+TEXT("closed_collision_and_mesh"),Ventilation->CheckWindowGeometry(R.Window,false));Record(Prefix+TEXT("aim_closed_aperture"),Aim(R.Window));Shot(*FString::Printf(TEXT("Ventilation_window_%02d_closed.png"),R.Window));if(R.Window==0)Shot(TEXT("Ventilation_window_closed.png"));break;
   case 1:Key(true);break;
   case 2:Key(false);Record(Prefix+TEXT("e_opens_without_disappearing"),Ventilation->IsOpen(R.Window)&&Ventilation->GetOpenCount()==1&&Ventilation->CheckWindowGeometry(R.Window,true));if(R.Window==0){CollectGarbage(RF_NoFlags);Record(TEXT("open_geometry_survives_gc"),Ventilation->CheckWindowGeometry(R.Window,true));Shot(TEXT("Ventilation_window_open.png"));}break;
   case 3:Record(Prefix+TEXT("open_aperture_can_be_targeted"),Aim(R.Window));Shot(*FString::Printf(TEXT("Ventilation_window_%02d_open.png"),R.Window));if(R.Window==15){Shot(TEXT("Ventilation_curtain_A.png"));Shot(TEXT("Ventilation_curtain_B.png"),.65f);}break;
   case 4:Key(true);break;
   default:Key(false);Record(Prefix+TEXT("e_closes_and_restores_collision"),!Ventilation->IsOpen(R.Window)&&Ventilation->CheckWindowGeometry(R.Window,false));++R.Window;R.Step=0;break;
  }
  return;
 }
 switch(R.Step++)
 {
 case 0:Record(TEXT("all_window_closed_poses_exact"),Ventilation->CheckClosedPoses());Record(TEXT("comfort_header_pointer"),Click(690,43)&&PanelMode==9);Record(TEXT("ventilation_subpage_pointer"),Click(150,270)&&GetComfortPage()==1);Overlay->ShowLightingRoom(0);Shot(TEXT("Ventilation_panel_closed.png"));break;
 case 1:Record(TEXT("room_open_pointer"),Click(425,580)&&GetRoomOpenWindowCount(0)==4);Record(TEXT("show_airflow_pointer"),Click(1200,580)&&IsAirflowDisplayed());LivingSide(false);break;
 case 2:Record(TEXT("single_side_exchange_without_false_crossflow"),GetRoomVentilation(0)>.15f&&!HasRoomCrossFlow(0));Record(TEXT("single_side_paths_respect_solids"),Ventilation->GetPathCount()>0&&Ventilation->CheckPathsClear());R.LocalResponse=Ventilation->GetResponseSample();Shot(TEXT("Ventilation_panel_local.png"));break;
 case 3:SetRoomWindows(0,true);SetRoomWindows(2,true);break;
 case 4:{Record(TEXT("opposed_windows_form_crossflow"),HasRoomCrossFlow(0)&&HasRoomCrossFlow(2)&&Ventilation->GetCrossPathCount()>0);Record(TEXT("pressure_balanced_and_high_to_low"),Ventilation->CheckFlowDirections());Record(TEXT("crossflow_stronger_than_local_exchange"),GetRoomVentilation(0)>.6f);Record(TEXT("crossflow_routes_do_not_cross_solids"),Ventilation->CheckPathsClear());Record(TEXT("open_windows_move_nearby_leaves"),Ventilation->GetMaxLeafMotion()>.01f);Record(TEXT("plant_pots_and_stems_do_not_translate"),Ventilation->CheckPlantTransforms());const FVector Cross=Ventilation->GetResponseSample();Record(TEXT("computed_exchange_drives_stronger_wind_audio_and_curtains"),Cross.X>R.LocalResponse.X*2&&Cross.Y>R.LocalResponse.Y*1.3&&Cross.Z>R.LocalResponse.Z*1.3);UE_LOG(LogTemp,Display,TEXT("VENTILATION_RESPONSE local=%s cross=%s"),*R.LocalResponse.ToString(),*Cross.ToString());Shot(TEXT("Ventilation_panel_cross.png"));break;}
 case 5:SetPanel(0);ObserveVentilation(0);Player->TeleportTo(FVector(-2050,9660,151.5),FRotator::ZeroRotator,false,false);PC->PlayerCameraManager->UpdateCamera(0);{FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);PC->SetControlRotation((Ventilation->GetFlowFocus()-Eye).Rotation());UE_LOG(LogTemp,Display,TEXT("VENTILATION_CAMERA eye=%s focus=%s"),*Eye.ToString(),*Ventilation->GetFlowFocus().ToString());}FFileHelper::SaveStringToFile(Ventilation->GetRenderDiagnostics(),*(FPaths::ProjectSavedDir()/TEXT("VentilationRendering.txt")));Shot(TEXT("Ventilation_cross_scene.png"));break;
 case 6:Record(TEXT("world_projected_trails_drawn"),Ventilation->CheckAirflowDrawing());LivingSide(false);break;
 case 7:Record(TEXT("curtain_hems_move_with_open_windows"),Ventilation->CheckCurtains(true));Record(TEXT("closing_outlet_removes_crossflow"),!HasRoomCrossFlow(0)&&!HasRoomCrossFlow(2)&&GetRoomVentilation(0)<.4f);CloseAllWindows();break;
 case 8:Record(TEXT("all_closed_removes_paths"),Ventilation->GetPathCount()==0&&GetRoomVentilation(0)<.08f);for(int32 I=0;I<Ventilation->GetWindowCount();++I)if(Ventilation->GetWindowRoom(I)==4&&Ventilation->GetWindowInward(I).X<-.5f)Ventilation->ToggleWindow(I);SetRoomWindows(8,true);for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->GetName()==TEXT("BP_Door_Inside_A2_5")){TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* M:Parts)if(M->GetName()==TEXT("Door"))R.Door=M;}break;
 case 9:Record(TEXT("closed_room_door_blocks_opposed_window_route"),!HasRoomCrossFlow(4)&&Ventilation->GetCrossPathCount()==0);if(R.Door.IsValid())InvokeFurniture(R.Door->GetOwner(),R.Door.Get());break;
 case 10:Record(TEXT("opening_room_door_connects_crossflow"),R.Door.IsValid()&&HasRoomCrossFlow(4)&&HasRoomCrossFlow(8));Record(TEXT("open_door_route_respects_solids"),Ventilation->CheckPathsClear());SetPanel(9);Overlay->ShowLightingRoom(4);Shot(TEXT("Ventilation_upper_cross.png"));break;
 case 11:if(R.Door.IsValid())InvokeFurniture(R.Door->GetOwner(),R.Door.Get());break;
 case 12:Record(TEXT("closing_room_door_interrupts_crossflow"),!HasRoomCrossFlow(4)&&GetRoomVentilation(4)<.4f);CloseAllWindows();break;
 case 13:Record(TEXT("daylight_and_ventilation_coexist"),SetDaylightHour(12)&&IsDaylightActive());SetRoomWindows(0,true);break;
 case 14:{bool Geometry=true;for(int32 I=0;I<Ventilation->GetWindowCount();++I)if(Ventilation->GetWindowRoom(I)==0)Geometry&=Ventilation->CheckWindowGeometry(I,true);Record(TEXT("daylight_keeps_operable_window_geometry"),Ventilation->GetOpenCount(0)==4&&Geometry);RestoreDaylight();Record(TEXT("daylight_restore_keeps_window_state"),Ventilation->GetOpenCount(0)==4&&Daylight->CheckRestored());Record(TEXT("energy_comparison_coexists"),ApplyEnergyScenario(true));RestoreEnergyComparison();Record(TEXT("lighting_states_preserved"),SameLights());TriggerKitchenAlarm();break;}
 case 15:Record(TEXT("fire_hides_airflow"),IsSafetyDrillRunning()&&!IsAirflowDisplayed());Record(TEXT("fire_pauses_airflow_paths"),Ventilation->GetPathCount()==0);SetRoomWindows(2,true);Record(TEXT("fire_blocks_new_window_actions"),GetRoomOpenWindowCount(2)==0);Record(TEXT("fire_preserves_physical_window_state"),GetRoomOpenWindowCount(0)==4);ResetKitchenAlarm();break;
 case 16:Record(TEXT("after_fire_window_actions_work"),!IsSafetyDrillRunning());SetRoomWindows(2,true);break;
 case 17:Record(TEXT("after_fire_flow_resumes"),HasRoomCrossFlow(0)&&HasRoomCrossFlow(2));SetPanel(9);Click(150,217);Record(TEXT("return_to_daylight_subpage"),GetComfortPage()==0);Click(150,270);Record(TEXT("return_to_ventilation_subpage"),GetComfortPage()==1);Click(690,43);Record(TEXT("menu_still_blocks_movement"),PC->IsMoveInputIgnored()&&PC->IsLookInputIgnored());SetPanel(0);Record(TEXT("closing_menu_releases_input"),!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored());CloseAllWindows();Ventilation->SetDisplay(false);break;
 case 18:
  LivingSide(false);for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->GetName()==TEXT("BP_Door_Outside_B_2")){TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* M:Parts)if(M->GetName()==TEXT("Door"))R.Door=M;}
  Record(TEXT("exterior_doors_initially_closed"),Ventilation->GetOpenExteriorDoorCount()==0);if(R.Door.IsValid())InvokeFurniture(R.Door->GetOwner(),R.Door.Get());break;
 case 19:
  Record(TEXT("front_door_is_external_air_opening"),Ventilation->GetOpenExteriorDoorCount()==1);Record(TEXT("side_door_feeds_leeward_windows"),Ventilation->GetCrossPathCount()>0&&HasRoomCrossFlow(0)&&Ventilation->CheckFlowDirections()&&Ventilation->CheckPathsClear());
  Ventilation->SetDisplay(true);Player->SetActorLocation(FVector(-2140,9400,151.5));PC->SetControlRotation(FRotator(15,95,0));Shot(TEXT("Ventilation_frontdoor_flow.png"));if(R.Door.IsValid())InvokeFurniture(R.Door->GetOwner(),R.Door.Get());break;
 case 20:Record(TEXT("closing_frontdoor_removes_its_pressure_route"),Ventilation->GetOpenExteriorDoorCount()==0&&Ventilation->GetCrossPathCount()==0);LivingSide(true);for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->GetName()==TEXT("BP_Door_Outside_A3_2")){TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* M:Parts)if(M->GetName()==TEXT("Door"))R.Door=M;}if(R.Door.IsValid())InvokeFurniture(R.Door->GetOwner(),R.Door.Get());break;
 case 21:Record(TEXT("backdoor_is_external_air_opening"),Ventilation->GetOpenExteriorDoorCount()==1);Record(TEXT("windward_windows_feed_side_backdoor"),Ventilation->GetCrossPathCount()>0&&Ventilation->CheckFlowDirections()&&Ventilation->CheckPathsClear());FFileHelper::SaveStringToFile(Ventilation->GetRenderDiagnostics(),*(FPaths::ProjectSavedDir()/TEXT("VentilationDoorRendering.txt")));if(R.Door.IsValid())InvokeFurniture(R.Door->GetOwner(),R.Door.Get());CloseAllWindows();Ventilation->SetDisplay(false);break;
 case 22:Record(TEXT("both_exterior_doors_closed_final"),Ventilation->GetOpenExteriorDoorCount()==0);SetRoomWindows(4,true);SetRoomWindows(9,true);break;
 case 23:Record(TEXT("study_two_facades_crossflow_without_hall_door"),HasRoomCrossFlow(4)&&Ventilation->CheckFlowDirections()&&Ventilation->CheckPathsClear());Record(TEXT("side_room_closed_door_keeps_local_exchange"),GetRoomVentilation(9)>.1f&&!HasRoomCrossFlow(9));Shot(TEXT("Ventilation_study_two_facades.png"));CloseAllWindows();break;
 case 24:Record(TEXT("all_windows_physically_closed_after_secondary_rooms"),Ventilation->CheckClosedPoses());break;
 default:
  Record(TEXT("final_closed_pose_restored"),Ventilation->CheckClosedPoses());Record(TEXT("normal_lighting_unchanged"),SameLights());Record(TEXT("closed_curtains_settle"),Ventilation->CheckCurtains(false));Record(TEXT("closed_windows_leaf_motion_settles"),Ventilation->GetMaxLeafMotion()<.02f);Ventilation->RestoreOriginals();Record(TEXT("original_leaf_materials_restored"),Ventilation->CheckPlantMaterialsRestored());Record(TEXT("original_architecture_exactly_restored"),Ventilation->CheckOriginalsRestored());
  FFileHelper::SaveStringToFile(TEXT("{\"checks\":[")+FString::Join(TestResults,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("VentilationAcceptance.json")));TestMode.Empty();QuitApplication();return;
 }
}

void ACourseworkHUD::TickVentilationPerformance()
{
 if(!bLightingReady||!Ventilation)return;
 struct FProfile {int32 Stage=0;double Start=0,Last=0;TArray<double> Frames;TArray<FString> Results;};static FProfile R;
 const double Now=GetWorld()->GetRealTimeSeconds();
 if(TestCase<0){TestCase=0;R=FProfile();R.Start=R.Last=Now;Ventilation->CloseAll();Ventilation->SetDisplay(false);auto* PC=GetOwningPlayerController();if(auto* Player=Cast<ACharacter>(PC->GetPawn())){Player->GetCharacterMovement()->DisableMovement();Player->SetActorLocation(FVector(-2050,9660,151.5));PC->SetControlRotation(FRotator(5,150,0));}SetPanel(0);return;}
 const double Dt=Now-R.Last;R.Last=Now;if(Now-R.Start>2)R.Frames.Add(Dt);
 if(Now-R.Start<12)return;
 double Total=0,Max=0;for(double D:R.Frames){Total+=D;Max=FMath::Max(Max,D);}R.Frames.Sort();const double FPS=R.Frames.Num()/FMath::Max(.001,Total),P95=R.Frames[FMath::Clamp(FMath::FloorToInt(R.Frames.Num()*.95),0,R.Frames.Num()-1)];
 R.Results.Add(FString::Printf(TEXT("{\"state\":\"%s\",\"frames\":%d,\"average_fps\":%.3f,\"max_frame_ms\":%.3f,\"p95_ms\":%.3f,\"passed\":%s}"),R.Stage?TEXT("airflow_and_leaf_motion"):TEXT("windows_closed"),R.Frames.Num(),FPS,Max*1000,P95*1000,FPS>=25?TEXT("true"):TEXT("false")));
 if(R.Stage++==0){SetRoomWindows(0,true);SetRoomWindows(2,true);Ventilation->SetDisplay(true);R.Start=R.Last=Now;R.Frames.Empty();return;}
 FFileHelper::SaveStringToFile(TEXT("{\"method\":\"Fixed camera; 2s warmup plus 10s per state; no screenshot readbacks during measurement\",\"runs\":[")+FString::Join(R.Results,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("VentilationPerformance.json")));TestMode.Empty();QuitApplication();
}
