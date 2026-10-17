#include "CourseworkHUD.h"
#include "CourseworkVentilation.h"
#include "CourseworkOverlay.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

int32 ACourseworkHUD::GetRoomWindowCount(int32 Room)const{return Ventilation?Ventilation->GetWindowCount(Room):0;}
int32 ACourseworkHUD::GetRoomOpenWindowCount(int32 Room)const{return Ventilation?Ventilation->GetOpenCount(Room):0;}
void ACourseworkHUD::SetRoomWindows(int32 Room,bool Open){if(IsSafetyDrillRunning()){ShowToast(TEXT("演练期间请优先疏散"));return;}if(Ventilation)Ventilation->SetRoomWindows(Room,Open);}
void ACourseworkHUD::CloseAllWindows(){if(IsSafetyDrillRunning())return;if(Ventilation)Ventilation->CloseAll();}
float ACourseworkHUD::GetRoomVentilation(int32 Room)const{return Ventilation?Ventilation->GetRoomStrength(Room):0;}
FString ACourseworkHUD::GetVentilationStatus(int32 Room)const{return Ventilation?Ventilation->GetRoomStatus(Room):TEXT("通风初始化中");}
bool ACourseworkHUD::HasRoomCrossFlow(int32 Room)const{return Ventilation&&Ventilation->HasCrossFlow(Room);}
bool ACourseworkHUD::IsAirflowDisplayed()const{return Ventilation&&Ventilation->IsDisplayed();}
void ACourseworkHUD::ToggleAirflowDisplay(){if(IsSafetyDrillRunning())return;if(Ventilation)Ventilation->SetDisplay(!Ventilation->IsDisplayed());}
bool ACourseworkHUD::SetWindowTarget(const FHitResult& Hit)
{
 if(!Ventilation||Hit.Distance>350)return false;const int32 Index=Ventilation->FindWindow(Hit.GetComponent());if(Index==INDEX_NONE)return false;
 TargetWindow=Index;const auto& R=CourseworkRooms::Get(Ventilation->GetWindowRoom(Index));
 TargetLabel=Ventilation->GetWindowLabel(Index);
 if(Overlay)Overlay->SetInteraction(TargetLabel+TEXT("  ·  按 E ")+(Ventilation->IsOpen(Index)?TEXT("关闭"):TEXT("打开")));
 return true;
}
void ACourseworkHUD::ObserveVentilation(int32 Room)
{
 if(IsSafetyDrillRunning()){ShowToast(TEXT("演练期间请步行疏散"));return;}
 if(!Ventilation)return;
 for(int32 I=0;I<Ventilation->GetWindowCount();++I)if(Ventilation->GetWindowRoom(I)==Room)
 {
  const FVector Point=Ventilation->GetWindowPoint(I),Inward=Ventilation->GetWindowInward(I);
  FVector Position=Point+Inward*150;Position.Z=Point.Z<300?151.5:413.f;
  if(auto* PC=GetOwningPlayerController())if(auto* C=Cast<ACharacter>(PC->GetPawn()))
  {
   C->GetCharacterMovement()->StopMovementImmediately();
   if(C->TeleportTo(Position,(-Inward).Rotation(),false,false)){PC->SetControlRotation((Point-(C->GetActorLocation()+FVector(0,0,64))).Rotation());SetPanel(0);return;}
  }
 }
 ShowToast(TEXT("窗边位置被占用，请步行靠近窗户观察"));
}

void ACourseworkHUD::DrawHUD()
{
 Super::DrawHUD();if(Ventilation&&(PanelMode==0||PanelMode==9))Ventilation->DrawAirflow(Canvas,GetOwningPlayerController());
}
