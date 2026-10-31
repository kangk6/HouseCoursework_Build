#include "CourseworkVentilation.h"
#include "CourseworkRooms.h"
#include "CourseworkAirNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "VentilationWindows.inl"

namespace
{
// The small side room is a separate airflow node; it is not merged through its closed door.
constexpr int32 SideRoom=CourseworkRooms::Count,FlowRoomCount=CourseworkRooms::Count+1;
struct FWindowInstance {const TCHAR* Name;int32 Room;};
const FWindowInstance Instances[]={
 {TEXT("SM_Window_Floor1_A3_400"),0},{TEXT("SM_Window_Floor1_A3_401"),0},
 {TEXT("SM_Window_Floor1_A3_402"),2},{TEXT("SM_Window_Floor1_A5_8"),5},
 {TEXT("SM_Window_Floor2_A4_39"),8},{TEXT("SM_Window_Floor2_A3_402"),1},
 {TEXT("SM_Window_Floor2_A4_402"),3},{TEXT("SM_Window_Floor2_A4_1"),4},
 {TEXT("SM_Window_Floor2_С1_12"),4},{TEXT("SM_Corner_2Floor_E1_14"),SideRoom},
 {TEXT("SM_Corner_1Floor_A3_Sym_29"),7},{TEXT("SM_Corner_1Floor_A6_8"),2}};
struct FDoorInstance {const TCHAR* Name;int32 A,B;};
const FDoorInstance DoorInstances[]={
 {TEXT("BP_Door_Inside_A_2"),7,5},{TEXT("BP_Door_Inside_A2_5"),8,4},
 {TEXT("BP_Door_Inside_A3_8"),8,1},{TEXT("BP_Door_Inside_A4_11"),1,6},
 {TEXT("BP_Door_Inside_A5_14"),1,3},
 {TEXT("BP_Door_Inside_A7"),4,SideRoom},
 {TEXT("BP_Door_Outside_B_2"),0,-1},{TEXT("BP_Door_Outside_A3_2"),7,-1}};
}
ACourseworkVentilation::ACourseworkVentilation()
{
 PrimaryActorTick.bCanEverTick=true;
 auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("VentilationRoot"));SetRootComponent(Root);
 TSet<FString> Names;for(const auto& S:WindowParts){Names.Add(S.Part);Names.Add(FString(S.Source)+TEXT("_Frame"));}
 for(const auto& Name:Names){const FString Path=TEXT("/Game/Coursework/Ventilation/")+Name+TEXT(".")+Name;ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(*Path);if(Asset.Object)PartAssets.Add(Asset.Object);}
 static ConstructorHelpers::FObjectFinder<USoundWave> Sound(TEXT("/Game/Coursework/Ventilation/wind.wind"));WindSound=Sound.Object;
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Leaves(TEXT("/Game/Coursework/Ventilation/MI_LeavesWind.MI_LeavesWind"));LeafMaterial=Leaves.Object;
 for(const TCHAR* Name:{TEXT("MI_Curtains_A1"),TEXT("MI_Curtains_A2"),TEXT("MI_Curtains_A3_Illum1"),TEXT("MI_Fabric_C1"),TEXT("MI_Fabric_C7")})
 {const FString N=FString(Name)+TEXT("_Wind");const FString Path=TEXT("/Game/Coursework/Ventilation/Curtains/")+N+TEXT(".")+N;ConstructorHelpers::FObjectFinder<UMaterialInterface> Asset(*Path);if(Asset.Object)CurtainAssets.Add(Asset.Object);}
 RoomStrength.Init(0,FlowRoomCount);RoomTarget=RoomStrength;CrossRooms.Init(false,FlowRoomCount);
}
void ACourseworkVentilation::BeginPlay()
{
 Super::BeginPlay();
 auto FindAsset=[&](const FString& Name)->UStaticMesh*{for(const auto& M:PartAssets)if(M&&M->GetName()==Name)return M;return nullptr;};
 // Match authored instances, not every decorative house in the surrounding street.
 TArray<AActor*> Actors;for(TActorIterator<AActor> It(GetWorld());It;++It)if(*It!=this)Actors.Add(*It);
 for(auto* Actor:Actors)
 {
  int32 Room=INDEX_NONE;for(const auto& I:Instances)if(Actor->GetName()==I.Name){Room=I.Room;break;}
  TArray<UStaticMeshComponent*> Components;Actor->GetComponents(Components);
  if(Room!=INDEX_NONE)for(auto* Original:Components)
  {
   UStaticMesh* Source=Original->GetStaticMesh();if(!Source)continue;auto* Frame=FindAsset(Source->GetName()+TEXT("_Frame"));if(!Frame)continue;
   const FTransform Transform=Original->GetComponentTransform();
   Originals.Add({Original,Original->IsVisible(),bool(Original->bHiddenInGame),Original->GetCollisionEnabled()});OriginalAssets.Add(Source);
   Original->SetVisibility(false);Original->SetHiddenInGame(true);Original->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   auto* Fixed=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Fixed);Fixed->SetupAttachment(GetRootComponent());Fixed->SetMobility(EComponentMobility::Movable);Fixed->SetStaticMesh(Frame);
   for(int32 I=0;I<Original->GetNumMaterials();++I)Fixed->SetMaterial(I,Original->GetMaterial(I));
   Fixed->SetCollisionProfileName(TEXT("BlockAllDynamic"));Fixed->SetCanEverAffectNavigation(false);Fixed->RegisterComponent();Fixed->SetWorldTransform(Transform);
   for(const auto& S:WindowParts)if(Source->GetName()==S.Source)
   {
    auto* Part=FindAsset(S.Part);if(!Part)continue;
    FWindow W;W.FixedFrame=Fixed;W.FixedTransform=Transform;W.Room=Room;const auto& R=CourseworkRooms::Get(Room);W.LocationLabel=FString::Printf(TEXT("%s · %s窗户"),R.Floor,R.Name);if(Room==SideRoom)W.LocationLabel=TEXT("二楼 · 侧室窗户");W.Center=Transform.TransformPosition(S.Center);W.Travel=S.Travel;W.Axis=S.Axis;W.Angle=S.Angle;W.Extent=S.Extent;W.Area=4*S.Extent.X*S.Extent.Z/10000.f; if(!S.Travel.IsNearlyZero()){const float Size=S.Travel.Z!=0?2*S.Extent.Z:2*S.Extent.X;W.Area*=FMath::Min(1.f,float(S.Travel.Size())/Size);}W.Inward=-Transform.TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
    auto* Hinge=NewObject<USceneComponent>(this);AddInstanceComponent(Hinge);Hinge->SetupAttachment(GetRootComponent());Hinge->SetMobility(EComponentMobility::Movable);Hinge->RegisterComponent();Hinge->SetWorldTransform(Transform);Hinge->AddLocalOffset(S.Pivot);W.Hinge=Hinge;W.Closed=Hinge->GetRelativeTransform();
    if(S.Angle>0){W.Center=Transform.TransformPosition(S.Center-FVector(0,0,S.Extent.Z-3));W.Area*=.23f/FMath::Sin(FMath::DegreesToRadians(S.Angle));}
    auto* Sash=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Sash);Sash->SetupAttachment(Hinge);Sash->SetMobility(EComponentMobility::Movable);Sash->SetStaticMesh(Part);Sash->SetRelativeLocation(-S.Pivot);
    for(int32 I=0;I<Original->GetNumMaterials();++I)Sash->SetMaterial(I,Original->GetMaterial(I));
    Sash->SetCollisionProfileName(TEXT("BlockAllDynamic"));Sash->SetCanEverAffectNavigation(false);Sash->RegisterComponent();W.Sash=Sash;
    // Full opening query remains accessible above a desk and after a leaf slides.
    auto* Proxy=NewObject<UBoxComponent>(this);AddInstanceComponent(Proxy);Proxy->SetupAttachment(GetRootComponent());Proxy->SetBoxExtent(S.ProxyExtent+FVector(1,2,1));
    Proxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Proxy->SetCollisionResponseToAllChannels(ECR_Ignore);Proxy->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Proxy->SetGenerateOverlapEvents(false);Proxy->SetCanEverAffectNavigation(false);Proxy->RegisterComponent();
    FTransform PT=Transform;PT.SetLocation(Transform.TransformPosition(S.ProxyCenter));Proxy->SetWorldTransform(PT);W.Proxy=Proxy;
    auto* Audio=NewObject<UAudioComponent>(this);AddInstanceComponent(Audio);Audio->SetupAttachment(Proxy);Audio->bAutoActivate=false;Audio->SetSound(WindSound);Audio->bOverrideAttenuation=true;
    Audio->AttenuationOverrides.bAttenuate=true;Audio->AttenuationOverrides.bSpatialize=true;Audio->AttenuationOverrides.AttenuationShapeExtents=FVector(45);Audio->AttenuationOverrides.FalloffDistance=350;Audio->RegisterComponent();W.Audio=Audio;
    Audio->AttenuationOverrides.bEnableOcclusion=true;Audio->AttenuationOverrides.OcclusionTraceChannel=ECC_Visibility;
    Audio->AttenuationOverrides.OcclusionLowPassFilterFrequency=700;Audio->AttenuationOverrides.OcclusionVolumeAttenuation=.3f;Audio->AttenuationOverrides.OcclusionInterpolationTime=.35f;
    const int32 Index=Windows.Add(W);WindowLookup.Add(Sash,Index);WindowLookup.Add(Proxy,Index);
   }
  }
  for(const auto& D:DoorInstances)if(Actor->GetName()==D.Name)for(auto* C:Components)if(C->GetName()==TEXT("Door"))Doors.Add({C,C->GetRelativeTransform(),C->Bounds.Origin,D.A,D.B,0});
 }
 for(auto* Actor:Actors){TArray<UStaticMeshComponent*> Parts;Actor->GetComponents(Parts);for(auto* M:Parts){if(!LeafMaterial||!M->GetStaticMesh()||!M->GetStaticMesh()->GetName().Contains(TEXT("HousePlant")))continue;
 const auto Center=M->Bounds.Origin;if(Center.X< -2600||Center.X> -1500||Center.Y<8900||Center.Y>10800)continue;
 for(int32 Slot=0;Slot<M->GetNumMaterials();++Slot){auto* Original=M->GetMaterial(Slot);if(!Original||!Original->GetName().Contains(TEXT("Plant_A_Leaves")))continue;
 auto* MID=UMaterialInstanceDynamic::Create(LeafMaterial,this);MID->SetScalarParameterValue(TEXT("CourseworkWindAmount"),0);LeafInstances.Add(MID);OriginalLeafMaterials.Add(Original);Plants.Add({M,Original,MID,M->GetComponentTransform(),Slot,0,0});M->SetMaterial(Slot,MID);
 }}}
 // Cloth is softly displaced in the shader; rail/rings and component transforms stay fixed.
 for(auto* Actor:Actors){TArray<UStaticMeshComponent*> Parts;Actor->GetComponents(Parts);for(auto* M:Parts){if(!M->GetStaticMesh()||!M->GetStaticMesh()->GetName().Contains(TEXT("Curtains")))continue;
 const auto B=M->Bounds;if(B.Origin.X< -2600||B.Origin.X> -1500||B.Origin.Y<8900||B.Origin.Y>10800)continue;
 // Soft fabric should not prevent aiming at an adjacent window through sheer cloth.
 CurtainDefaultCollision.Add(M,M->bUseDefaultCollision);M->bUseDefaultCollision=false;CurtainCollision.Add(M,M->GetCollisionResponseToChannel(ECC_Visibility));M->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
 for(int32 Slot=0;Slot<M->GetNumMaterials();++Slot){auto* Original=M->GetMaterial(Slot);if(!Original)continue;UMaterialInterface* Wind=nullptr;for(const auto& Asset:CurtainAssets)if(Asset&&Asset->GetName()==Original->GetName()+TEXT("_Wind")){Wind=Asset;break;}if(!Wind)continue;
 auto* MID=UMaterialInstanceDynamic::Create(Wind,this);MID->SetScalarParameterValue(TEXT("CurtainWindAmount"),0);MID->SetScalarParameterValue(TEXT("CurtainTop"),B.Origin.Z+B.BoxExtent.Z-8);MID->SetScalarParameterValue(TEXT("CurtainHeight"),FMath::Max(1.f,2*B.BoxExtent.Z-8));
 CurtainInstances.Add(MID);CurtainOriginals.Add(Original);Curtains.Add({M,Original,MID,M->GetComponentTransform(),Slot,0,0});M->SetMaterial(Slot,MID);}
 }}
 UE_LOG(LogTemp,Display,TEXT("VENTILATION_READY modules=%d sashes=%d doors=%d assets=%d"),Originals.Num(),Windows.Num(),Doors.Num(),PartAssets.Num());
 RebuildFlow();
}
int32 ACourseworkVentilation::FindWindow(const UPrimitiveComponent* C)const{const auto* I=WindowLookup.Find(C);return I?*I:INDEX_NONE;}
bool ACourseworkVentilation::ToggleWindow(int32 I){if(!Windows.IsValidIndex(I)||bSuspended||bRestored)return false;Windows[I].Target=1-Windows[I].Target;UE_LOG(LogTemp,Display,TEXT("WINDOW_ACTION index=%d room=%d open=%d center=%s"),I,Windows[I].Room,IsOpen(I),*Windows[I].Center.ToCompactString());return true;}
void ACourseworkVentilation::SetRoomWindows(int32 Room,bool Open){if(bSuspended||bRestored)return;for(auto& W:Windows)if(W.Room==Room)W.Target=Open?1.f:0.f;UE_LOG(LogTemp,Display,TEXT("WINDOW_ROOM_ACTION room=%d open=%d"),Room,Open);}
void ACourseworkVentilation::CloseAll(){for(auto& W:Windows)W.Target=0;}
int32 ACourseworkVentilation::GetWindowCount(int32 Room)const{int32 N=0;for(const auto& W:Windows)if(Room==INDEX_NONE||W.Room==Room)++N;return N;}
int32 ACourseworkVentilation::GetOpenCount(int32 Room)const{int32 N=0;for(const auto& W:Windows)if((Room==INDEX_NONE||W.Room==Room)&&W.Target>.5f)++N;return N;}
bool ACourseworkVentilation::IsOpen(int32 I)const{return Windows.IsValidIndex(I)&&Windows[I].Target>.5f;}
FString ACourseworkVentilation::GetWindowLabel(int32 I)const{return Windows.IsValidIndex(I)?Windows[I].LocationLabel:FString();}
int32 ACourseworkVentilation::GetWindowRoom(int32 I)const{return Windows.IsValidIndex(I)?Windows[I].Room:INDEX_NONE;}
FVector ACourseworkVentilation::GetWindowPoint(int32 I)const{return Windows.IsValidIndex(I)?Windows[I].Center:FVector::ZeroVector;}
FVector ACourseworkVentilation::GetWindowInward(int32 I)const{return Windows.IsValidIndex(I)?Windows[I].Inward:FVector::ZeroVector;}
float ACourseworkVentilation::GetRoomStrength(int32 Room)const{return RoomStrength.IsValidIndex(Room)?RoomStrength[Room]:0;}
bool ACourseworkVentilation::HasCrossFlow(int32 Room)const{return CrossRooms.IsValidIndex(Room)&&CrossRooms[Room]&&!bSuspended;}
FString ACourseworkVentilation::GetRoomStatus(int32 Room)const
{
 if(bSuspended)return TEXT("演练期间暂停通风展示");
 if(HasCrossFlow(Room))return TEXT("对流通风 · 通路已连通");
 if(GetOpenCount(Room)>0)return TEXT("单侧换气 · 窗边为主");
 if(GetRoomStrength(Room)>.03f)return TEXT("间接换气 · 经相邻空间");
 return GetWindowCount(Room)>0?TEXT("窗户关闭 · 自然换气较弱"):TEXT("无直接外窗 · 依靠相邻空间");
}
void ACourseworkVentilation::SetDisplay(bool V){bDisplay=V;}
void ACourseworkVentilation::Suspend(bool V){if(bSuspended==V)return;bSuspended=V;LastState=MAX_uint64;}
int32 ACourseworkVentilation::GetCrossPathCount()const{int32 N=0;for(const auto& P:Paths)if(P.Cross)++N;return N;}
int32 ACourseworkVentilation::GetOpenExteriorDoorCount()const{int32 N=0;for(const auto& D:Doors)if(D.B<0&&D.Amount>.95f)++N;return N;}
void ACourseworkVentilation::Tick(float Dt)
{
 Super::Tick(Dt);if(bRestored)return;
 const float Time=GetWorld()->GetTimeSeconds();
 auto Breeze=[&](FVector Position){const float Phase=Position.X*.017f+Position.Y*.013f;return .82f+.1f*FMath::Sin(Time*.53f+Phase)+.08f*FMath::Sin(Time*1.11f+Phase*.61f);};
 for(auto& W:Windows)
 {
  W.Amount=FMath::FInterpConstantTo(W.Amount,W.Target,Dt,1.35f);
  if(auto* H=W.Hinge.Get()){FTransform T=W.Closed;const float A=FMath::SmoothStep(0.f,1.f,W.Amount);T.SetLocation(W.Closed.GetLocation()+W.Closed.TransformVector(W.Travel*A));if(W.Angle!=0)T.SetRotation(W.Closed.GetRotation()*FQuat(W.Axis,FMath::DegreesToRadians(W.Angle*A)));H->SetRelativeTransform(T);}
  if(auto* A=W.Audio.Get())
  {
   W.AirResponse=FMath::FInterpTo(W.AirResponse,bSuspended?0.f:CourseworkAirNetwork::Response(W.Exchange),Dt,2.f);
   const float Gain=W.AirResponse*.26f*Breeze(W.Center);
   A->SetVolumeMultiplier(Gain);A->SetPitchMultiplier(1.f+.025f*FMath::Sin(Time*.21f+W.Center.Y*.019f));
   if(Gain>.002f&&!A->IsPlaying()){const float Duration=WindSound?WindSound->Duration:0;const float Start=Duration>0?FMath::Fmod(FMath::Abs(W.Center.X*.031f+W.Center.Y*.017f),Duration):0;A->Play(Start);}
   else if(Gain<=.002f&&A->IsPlaying())A->Stop();
  }
 }
 for(auto& P:Plants){P.Amount=FMath::FInterpTo(P.Amount,bSuspended?0.f:P.Target,Dt,3.f);if(auto* M=P.Material.Get())M->SetScalarParameterValue(TEXT("CourseworkWindAmount"),P.Amount);}
 for(auto& C:Curtains){const FVector Position=C.Mesh.IsValid()?C.Mesh->Bounds.Origin:C.Transform.GetLocation();const float Envelope=Breeze(Position+FVector(C.Slot*31,0,0));C.Amount=FMath::FInterpTo(C.Amount,bSuspended?0.f:C.Target*Envelope,Dt,1.7f);if(C.Target==0&&C.Amount<.01f)C.Amount=0;C.Direction=FMath::VInterpTo(C.Direction,C.TargetDirection,Dt,2.5f);if(auto* M=C.Material.Get()){M->SetScalarParameterValue(TEXT("CurtainWindAmount"),C.Amount);M->SetScalarParameterValue(TEXT("CurtainDX"),C.Direction.X);M->SetScalarParameterValue(TEXT("CurtainDY"),C.Direction.Y);}}
 uint64 State=bSuspended?1:0;
 auto OpeningState=[](float Amount){return Amount<=0.f?0:Amount>=1.f?22:1+FMath::FloorToInt(Amount*20);};
 for(const auto& W:Windows)State=State*23+OpeningState(W.Amount);
 for(auto& D:Doors)if(auto* M=D.Mesh.Get())
 {const auto T=M->GetRelativeTransform();D.Amount=FMath::Clamp(FMath::Max(float(FMath::RadiansToDegrees(T.GetRotation().AngularDistance(D.Closed.GetRotation())))/90.f,float(FVector::Dist(T.GetLocation(),D.Closed.GetLocation()))/80.f),0.f,1.f);State=State*23+OpeningState(D.Amount);}
 RebuildElapsed+=Dt;if(State!=LastState&&RebuildElapsed>.2f){LastState=State;RebuildElapsed=0;RebuildFlow();}
 for(int32 I=0;I<RoomStrength.Num();++I)RoomStrength[I]=FMath::FInterpTo(RoomStrength[I],RoomTarget[I],Dt,3.f);
 VisualWeight=FMath::FInterpTo(VisualWeight,IsDisplayed()?1.f:0.f,Dt,5.f);
 auto Advance=[&](FFlowPath& P){const float Target=P.Cross?FMath::Lerp(36.f,100.f,CourseworkAirNetwork::Response(P.Flux)):24.f;P.Speed=FMath::FInterpTo(P.Speed,Target,Dt,2.5f);P.Motion=FMath::Fmod(P.Motion+Dt*P.Speed,P.Length+80.f);};
 for(auto& P:Paths)Advance(P);for(auto& P:RetiringPaths)Advance(P);
 RetiringPaths.RemoveAll([&](const FFlowPath& P){return Time-P.Retired>.45f;});
}
bool ACourseworkVentilation::SegmentClear(const FVector& A,const FVector& B)const
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(VentilationPath),true);for(const auto& W:Windows)if(W.Proxy.IsValid())Q.AddIgnoredComponent(W.Proxy.Get());
 for(const auto& O:Originals)if(O.Mesh.IsValid())Q.AddIgnoredComponent(O.Mesh.Get());
 for(TActorIterator<APawn> It(GetWorld());It;++It)Q.AddIgnoredActor(*It);
 FHitResult Hit;
 return !GetWorld()->LineTraceSingleByChannel(Hit,A,B,ECC_Visibility,Q)&&!GetWorld()->LineTraceSingleByChannel(Hit,B,A,ECC_Visibility,Q);
}
TArray<FVector> ACourseworkVentilation::FindAirPath(FVector Start,FVector End)const
{
 if(SegmentClear(Start,End))return {Start,End};
 // Small lazy A* grid, computed only when opening connectivity changes. Every
 // edge is ray-tested in both directions against actual scene collision.
 constexpr int32 NX=37,NY=73;constexpr float Step=24.f,MinX=-2482,MinY=9020;
 const float Z=Start.Z;
 auto At=[&](int32 I){return FVector(MinX+(I%NX)*Step,MinY+(I/NX)*Step,Z);};
 auto Index=[&](FVector P){return FMath::Clamp(FMath::RoundToInt((P.Y-MinY)/Step),0,NY-1)*NX+FMath::Clamp(FMath::RoundToInt((P.X-MinX)/Step),0,NX-1);};
 int32 S=Index(Start),E=Index(End);if(!SegmentClear(Start,At(S))||!SegmentClear(At(E),End))return {};
 TArray<float> Cost;Cost.Init(MAX_flt,NX*NY);TArray<int32> Parent;Parent.Init(-1,NX*NY);TArray<uint8> Done;Done.Init(0,NX*NY);TArray<int32> Open={S};Cost[S]=0;int32 Visits=0;
 while(Open.Num()&&++Visits<2200)
 {
  int32 Best=0;float Score=MAX_flt;for(int32 J=0;J<Open.Num();++J){const float F=Cost[Open[J]]+FVector::Dist(At(Open[J]),At(E));if(F<Score){Score=F;Best=J;}}
  const int32 I=Open[Best];Open.RemoveAtSwap(Best);if(Done[I])continue;Done[I]=1;if(I==E)break;
  for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX){if(!DX&&!DY)continue;const int32 X=I%NX+DX,Y=I/NX+DY;if(X<0||X>=NX||Y<0||Y>=NY)continue;const int32 N=Y*NX+X;if(Done[N])continue;
   const float C=Cost[I]+Step*(DX&&DY?1.414214f:1.f);if(C>=Cost[N]||!SegmentClear(At(I),At(N)))continue;Cost[N]=C;Parent[N]=I;Open.Add(N);
  }
 }
 if(!Done[E])return {};TArray<FVector> Reverse={End};for(int32 I=E;I>=0;I=Parent[I]){Reverse.Add(At(I));if(I==S)break;}Reverse.Add(Start);
 TArray<FVector> Result;for(int32 I=Reverse.Num()-1;I>=0;){Result.Add(Reverse[I]);if(I==0)break;int32 Next=I-1;for(int32 J=0;J<I-1;++J)if(SegmentClear(Reverse[I],Reverse[J])){Next=J;break;}I=Next;}
 return Result;
}
void ACourseworkVentilation::RebuildFlow()
{

 TArray<FFlowPath> Previous=MoveTemp(Paths);Paths.Reset();RoomTarget.Init(0,FlowRoomCount);CrossRooms.Init(false,FlowRoomCount);for(auto& W:Windows){W.Flow=0;W.Exchange=0;}for(auto& P:Plants)P.Target=0;PressureResidual=0;for(auto& C:Curtains)C.Target=0;if(bSuspended){RetiringPaths.Empty();return;}
 // A steady relative-pressure network, not a velocity-field/CFD solver. Exterior
 // east->west wind: windward +1, leeward -.5, side faces -.25. Interior pressure
 // is solved from conservation at each room, including both real exterior doors.
 struct FLink{int32 A,B;FVector Point;float G=3,Flow=0;bool Door=false;};
 struct FOpening{int32 Room;FVector Center,Inward;float G,Pressure,Flow=0;int32 Window=INDEX_NONE;};
 // The ground-floor passage runs beside the stair enclosure. The former
 // placeholder at x=-2110 lay inside that enclosure rather than in the opening.
 TArray<FLink> Links={{0,2,FVector(-2000,9660,245)},{0,7,FVector(-1990,10025,245)}};
 TArray<FOpening> Openings;
 auto Cp=[](FVector N){const float D=FVector::DotProduct(N,FVector(-1,0,0));return D>.5f?1.f:D<-.5f?-.5f:-.25f;};
 for(int32 I=0;I<Windows.Num();++I){const auto& W=Windows[I];const float Open=CourseworkAirNetwork::Aperture(FMath::SmoothStep(0.f,1.f,W.Amount),W.Angle);if(Open>.001f){const float Area=FMath::Clamp(W.Area,.03f,4.f)*Open;Openings.Add({W.Room,W.Center,W.Inward,Area,Cp(W.Inward),0,I});RoomTarget[W.Room]=FMath::Max(RoomTarget[W.Room],.26f*Open);}}
 for(const auto& D:Doors)if(D.Amount>.01f)
 {
  const float Open=CourseworkAirNetwork::Aperture(D.Amount,90.f);
  if(D.B>=0){FVector P=D.Portal;P.Z=P.Z<300?245:465;Links.Add({D.A,D.B,P,3*Open,0,true});}
  else {const FVector N=D.Portal.Y<9800?FVector(0,1,0):FVector(0,-1,0);Openings.Add({D.A,D.Portal,N,1.4f*Open,Cp(N)});RoomTarget[D.A]=FMath::Max(RoomTarget[D.A],.26f*Open);}
 }
 TArray<CourseworkAirNetwork::FEdge> Edges;
 for(const auto& O:Openings)Edges.Add({O.Room,INDEX_NONE,O.G,O.Pressure});
 for(const auto& L:Links)Edges.Add({L.A,L.B,L.G});
 TArray<float> Pressure;PressureResidual=CourseworkAirNetwork::Solve(FlowRoomCount,Edges,Pressure);
 for(int32 I=0;I<Openings.Num();++I){auto& O=Openings[I];O.Flow=-Edges[I].Flow;if(Windows.IsValidIndex(O.Window)){auto& W=Windows[O.Window];W.Flow=O.Flow;
  // Unresolved single-sided turbulence has zero NET flux; keep this weak local
  // exchange separate from conservation, instead of inventing a through-flow.
  W.Exchange=FMath::Abs(O.Flow)+.06f*O.G;}}
 for(const auto& O:Openings){FCollisionQueryParams Q(SCENE_QUERY_STAT(AirOpeningAudit),true);for(const auto& W:Windows)if(W.Proxy.IsValid())Q.AddIgnoredComponent(W.Proxy.Get());FHitResult Hit;GetWorld()->LineTraceSingleByChannel(Hit,O.Center-O.Inward*14,O.Center+O.Inward*16,ECC_Visibility,Q);UE_LOG(LogTemp,Display,TEXT("AIR_OPENING room=%d center=%s inward=%s flow=%.4f obstruction=%s/%s"),O.Room,*O.Center.ToCompactString(),*O.Inward.ToCompactString(),O.Flow,Hit.GetActor()?*Hit.GetActor()->GetName():TEXT("none"),Hit.GetComponent()?*Hit.GetComponent()->GetName():TEXT("none"));}
 for(int32 I=0;I<Links.Num();++I)Links[I].Flow=Edges[Openings.Num()+I].Flow;
 auto AddPath=[&](TArray<FVector> Points,int32 Room,bool Cross,float Inlet,float Outlet,float Flux=0.f)->bool
 {
  if(Points.Num()<2)return false;
  // Round navigation corners only when every sampled arc is collision clear.
  TArray<FVector> Smooth={Points[0]};
  for(int32 K=1;K<Points.Num()-1;++K){const FVector C=Points[K];const float Radius=FMath::Min(35.f,FMath::Min(FVector::Dist(C,Points[K-1]),FVector::Dist(C,Points[K+1]))*.24f);
   const FVector A=C+(Points[K-1]-C).GetSafeNormal()*Radius,B=C+(Points[K+1]-C).GetSafeNormal()*Radius;TArray<FVector> Arc={A};
   for(int32 N=1;N<=6;++N){const float T=N/6.f;Arc.Add(FMath::Lerp(FMath::Lerp(A,C,T),FMath::Lerp(C,B,T),T));}
   bool Clear=SegmentClear(Smooth.Last(),Arc[0]);for(int32 N=1;N<Arc.Num();++N)Clear&=SegmentClear(Arc[N-1],Arc[N]);
   if(Clear)Smooth.Append(Arc);else Smooth.Add(C);
  }Smooth.Add(Points.Last());
  FFlowPath P;P.Points=MoveTemp(Smooth);P.Room=Room;P.Cross=Cross;P.InletPressure=Inlet;P.OutletPressure=Outlet;P.Born=GetWorld()->GetTimeSeconds();P.Flux=Flux;
  for(int32 I=1;I<P.Points.Num();++I){if(!SegmentClear(P.Points[I-1],P.Points[I]))return false;P.Length+=FVector::Dist(P.Points[I-1],P.Points[I]);}if(P.Length<15)return false;
  const int32 Count=Cross?FMath::Clamp(FMath::Max(FMath::CeilToInt(Flux*10.f),FMath::CeilToInt(P.Length/250.f)),2,7):2;
  for(int32 N=0;N<Count;++N){FVector Offset(0,0,(N%3-1)*4.f);for(int32 K=1;K<P.Points.Num();++K)if(!SegmentClear(P.Points[K-1]+Offset,P.Points[K]+Offset)){Offset=FVector::ZeroVector;break;}P.Offsets.Add(Offset);}
  for(int32 I=0;I<Previous.Num();++I)if(Previous[I].Cross==P.Cross&&Previous[I].Room==P.Room&&Previous[I].Points[0].Equals(P.Points[0],2)&&Previous[I].Points.Last().Equals(P.Points.Last(),2))
  {P.Born=Previous[I].Born;P.Motion=Previous[I].Motion;P.Speed=Previous[I].Speed;Previous.RemoveAt(I);++PathReuseCount;break;}
  Paths.Add(MoveTemp(P));return true;
 };
 // Trace only along descending room pressure. A doorway cannot simultaneously
 // feed both directions, and an outlet never emits an invented inward stream.
 TArray<float> OpeningRemaining,LinkRemaining;for(const auto& O:Openings)OpeningRemaining.Add(FMath::Abs(O.Flow));for(const auto& L:Links)LinkRemaining.Add(FMath::Abs(L.Flow));
 for(int32 I=0;I<Openings.Num();++I)
 {
  const auto& In=Openings[I];if(In.Flow<.008f)continue;
  for(int32 J=0;J<Openings.Num();++J)
  {
   if(OpeningRemaining[I]<.008f)break;const auto& Out=Openings[J];if(Out.Flow>-.008f||OpeningRemaining[J]<.008f||Out.Pressure>=In.Pressure-.01f)continue;
   TArray<int32> Parent,ParentLink;Parent.Init(-1,Pressure.Num());ParentLink=Parent;TArray<int32> Queue={In.Room};Parent[In.Room]=In.Room;
   for(int32 QI=0;QI<Queue.Num();++QI)for(int32 K=0;K<Links.Num();++K){if(LinkRemaining[K]<.008f)continue;const auto& L=Links[K];const int32 R=Queue[QI],N=L.A==R&&L.Flow>.008f?L.B:L.B==R&&L.Flow<-.008f?L.A:-1;if(N>=0&&Parent[N]<0){Parent[N]=R;ParentLink[N]=K;Queue.Add(N);}}
   if(Parent[Out.Room]<0)continue;
   float Flux=FMath::Min(OpeningRemaining[I],OpeningRemaining[J]);TArray<FVector> Portals;TArray<int32> Rooms={Out.Room};for(int32 R=Out.Room;R!=In.Room;R=Parent[R]){const auto& Link=Links[ParentLink[R]];if(Link.Door)Portals.Insert(Link.Point,0);Rooms.Add(Parent[R]);Flux=FMath::Min(Flux,LinkRemaining[ParentLink[R]]);}
   const float Z=In.Center.Z<300?245:465;
   auto Interior=[&](const FOpening& O){const FVector Start=O.Center+O.Inward*16,Side=FVector::CrossProduct(FVector::UpVector,O.Inward);
    // A window can open beside a plant/bookcase. Choose a clear entry before
    // lifting the path to room height; every segment still respects collision.
    for(float Height:{Z,Z+40,Z-30})for(float Distance:{26.f,50.f,80.f})for(float Offset:{0.f,40.f,-40.f,75.f,-75.f}){FVector P=O.Center+O.Inward*Distance+Side*Offset;P.Z=Height;if(SegmentClear(Start,P)&&SegmentClear(P,P+O.Inward*24))return P;}
    FVector P=O.Center+O.Inward*26;P.Z=Z;return P;};
   FVector A=Interior(In),B=Interior(Out);
   TArray<FVector> Stops={In.Center-In.Inward*14,In.Center+In.Inward*16,A};Stops.Append(Portals);Stops.Append({B,Out.Center+Out.Inward*16,Out.Center-Out.Inward*14});
   TArray<FVector> Points;bool Valid=true;
   for(int32 K=1;K<Stops.Num();++K){auto Leg=FindAirPath(Stops[K-1],Stops[K]);if(Leg.Num()<2){UE_LOG(LogTemp,Display,TEXT("AIR_ROUTE_BLOCKED leg=%d a=%s b=%s"),K,*Stops[K-1].ToCompactString(),*Stops[K].ToCompactString());Valid=false;break;}if(Points.Num())Leg.RemoveAt(0);Points.Append(Leg);}
   if(Valid&&AddPath(MoveTemp(Points),In.Room,true,In.Pressure,Out.Pressure,Flux))
   {for(int32 R:Rooms){RoomTarget[R]=FMath::Min(.95f,RoomTarget[R]+CourseworkAirNetwork::Response(Flux)*.7f);CrossRooms[R]=true;}
    OpeningRemaining[I]-=Flux;OpeningRemaining[J]-=Flux;for(int32 R=Out.Room;R!=In.Room;R=Parent[R])LinkRemaining[ParentLink[R]]-=Flux;
    UE_LOG(LogTemp,Display,TEXT("AIRFLOW_ROUTE inlet=%s outlet=%s pressure=%.3f->%.3f relative_flux=%.4f rooms=%d"),*In.Center.ToCompactString(),*Out.Center.ToCompactString(),In.Pressure,Out.Pressure,Flux,Rooms.Num());}
  }
 }
 // Single-sided exchange has zero net through-flow: separate short incoming and
 // outgoing traces at the aperture, never a fabricated circular indoor loop.
 for(const auto& O:Openings)if(!CrossRooms[O.Room]&&FMath::Abs(O.Flow)<.008f)
 {
  const FVector Low=O.Center-FVector(0,0,5),High=O.Center+FVector(0,0,5);
  AddPath({Low-O.Inward*12,Low+O.Inward*18,Low+O.Inward*65},O.Room,false,O.Pressure,O.Pressure);
  AddPath({High+O.Inward*65,High+O.Inward*18,High-O.Inward*12},O.Room,false,O.Pressure,O.Pressure);
 }
 for(const auto& L:Links){if(RoomTarget[L.A]>.15f)RoomTarget[L.B]=FMath::Max(RoomTarget[L.B],RoomTarget[L.A]*.25f);if(RoomTarget[L.B]>.15f)RoomTarget[L.A]=FMath::Max(RoomTarget[L.A],RoomTarget[L.B]*.25f);}
 for(auto& P:Plants)if(auto* M=P.Mesh.Get())for(const auto& W:Windows){if(W.Amount<.15f||FMath::Abs(W.Center.Z-M->Bounds.Origin.Z)>160)continue;const FVector Leaf=M->Bounds.Origin+FVector(0,0,M->Bounds.BoxExtent.Z*.45f);const float Distance=FVector::Dist(Leaf,W.Center);if(Distance>360||FVector::DotProduct(Leaf-W.Center,W.Inward)<0)continue;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(LeafWindOcclusion),true);Q.AddIgnoredComponent(M);for(const auto& O:Windows)if(O.Proxy.IsValid())Q.AddIgnoredComponent(O.Proxy.Get());FHitResult Hit;
 if(!GetWorld()->LineTraceSingleByChannel(Hit,W.Center+W.Inward*20,Leaf,ECC_Visibility,Q))P.Target=FMath::Max(P.Target,CourseworkAirNetwork::Response(W.Exchange)*FMath::Clamp(1-Distance/400.f,0.f,1.f));}
 for(auto& C:Curtains)if(auto* Mesh=C.Mesh.Get())for(const auto& W:Windows){const auto P=Mesh->Bounds.Origin;const auto D=P-W.Center;const float Along=FVector::DotProduct(D,W.Inward);const float Lateral=(D-W.Inward*Along).Size();if(W.Amount<.15f||Along< -12||Along>90||Lateral>190)continue;
 const float Strength=CourseworkAirNetwork::Response(W.Exchange)*FMath::Clamp(1-Lateral/240.f,0.f,1.f);const bool Sheer=C.Original.IsValid()&&C.Original->GetBlendMode()==BLEND_Translucent;const float Target=Strength*(Sheer?3.f:1.6f);if(Target>C.Target){C.Target=Target;C.TargetDirection=(W.Flow<-.008f?-1.f:1.f)*W.Inward;}
 }
 // Retain only a short visual tail. Rendering checks the moving door/window
 // collision again, so the retired trail cannot continue through a closed leaf.
 const float Now=GetWorld()->GetTimeSeconds();for(auto& P:Previous){P.Retired=Now;RetiringPaths.Add(MoveTemp(P));++PathRetireCount;}
 if(RetiringPaths.Num()>24)RetiringPaths.RemoveAt(0,RetiringPaths.Num()-24);
 if(IsDisplayed()){for(const auto& P:Paths)if(P.Cross){FString Line;for(const auto& V:P.Points)Line+=V.ToCompactString()+TEXT(";");UE_LOG(LogTemp,Verbose,TEXT("VENTILATION_PATH %s"),*Line);}}
 UE_LOG(LogTemp,Display,TEXT("VENTILATION_FLOW open=%d exterior_doors=%d paths=%d cross=%d balance_error=%.6f"),GetOpenCount(),GetOpenExteriorDoorCount(),Paths.Num(),GetCrossPathCount(),PressureResidual);
}
bool ACourseworkVentilation::CheckFlowDirections()const{for(const auto& P:Paths)if(P.Cross&&P.InletPressure<=P.OutletPressure)return false;return PressureResidual<.001f;}
bool ACourseworkVentilation::CheckPathsClear()const{for(const auto& P:Paths)for(int32 I=1;I<P.Points.Num();++I)if(!SegmentClear(P.Points[I-1],P.Points[I]))return false;return true;}
bool ACourseworkVentilation::CheckClosedPoses()const{for(const auto& W:Windows)if(W.Amount>.001f||W.Target>.001f||!W.Hinge.IsValid()||!W.Hinge->GetRelativeTransform().Equals(W.Closed,.001f))return false;return true;}
void ACourseworkVentilation::RestoreOriginals()
{
 for(auto& P:Plants){if(auto* M=P.Mesh.Get())M->SetMaterial(P.Slot,P.Original.Get());P.Target=0;P.Amount=0;}
 for(auto& C:Curtains)if(auto* M=C.Mesh.Get()){M->SetMaterial(C.Slot,C.Original.Get());if(const auto* Response=CurtainCollision.Find(M))M->SetCollisionResponseToChannel(ECC_Visibility,*Response);if(const auto* Default=CurtainDefaultCollision.Find(M))M->bUseDefaultCollision=*Default;}
 if(bRestored)return;bRestored=true;SetActorTickEnabled(false);
 for(auto& W:Windows){if(W.Audio.IsValid())W.Audio->Stop();if(W.Sash.IsValid())W.Sash->SetVisibility(false);if(W.Proxy.IsValid())W.Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
 for(const auto& O:Originals)if(auto* M=O.Mesh.Get()){M->SetVisibility(O.Visible);M->SetHiddenInGame(O.Hidden);M->SetCollisionEnabled(O.Collision);}
 TArray<UStaticMeshComponent*> Meshes;GetComponents(Meshes);for(auto* M:Meshes)M->SetVisibility(false);OriginalAssets.Empty();
}
bool ACourseworkVentilation::CheckOriginalsRestored()const{for(const auto& O:Originals)if(auto* M=O.Mesh.Get())if(M->IsVisible()!=O.Visible||M->bHiddenInGame!=O.Hidden||M->GetCollisionEnabled()!=O.Collision)return false;return bRestored;}
void ACourseworkVentilation::EndPlay(const EEndPlayReason::Type Reason){RestoreOriginals();Super::EndPlay(Reason);}

bool ACourseworkVentilation::CheckWindowGeometry(int32 I,bool Open)const
{
 if(!Windows.IsValidIndex(I))return false;const auto& W=Windows[I];if(!W.Sash.IsValid()||!W.Hinge.IsValid()||!W.Sash->GetStaticMesh()||!W.Sash->IsVisible()||W.Sash->bHiddenInGame)return false;
 const float Angle=FMath::RadiansToDegrees(W.Hinge->GetRelativeTransform().GetRotation().AngularDistance(W.Closed.GetRotation()));
 FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(WindowApertureCheck),true);
 const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,W.Center+W.Inward*18,W.Center-W.Inward*18,ECC_Pawn,Q);
 UE_LOG(LogTemp,Display,TEXT("VENTILATION_GEOMETRY i=%d target=%.2f amount=%.2f angle=%.3f blocked=%d hit=%s/%s"),I,W.Target,W.Amount,Angle,Blocked,Hit.GetActor()?*Hit.GetActor()->GetName():TEXT("none"),Hit.GetComponent()?*Hit.GetComponent()->GetName():TEXT("none"));
 const auto T=W.Hinge->GetRelativeTransform();const float Distance=FVector::Distance(T.GetLocation(),W.Closed.GetLocation());
 const bool Pose=W.Angle!=0?(Open?FMath::Abs(Angle-FMath::Abs(W.Angle))<.05f:Angle<.01f):(Angle<.01f&&FMath::Abs(Distance-(Open?W.Closed.TransformVector(W.Travel).Size():0.f))<.05f);
 return Pose&&Blocked!=Open&&W.FixedFrame.IsValid()&&W.FixedFrame->GetComponentTransform().Equals(W.FixedTransform,.001f);
}

float ACourseworkVentilation::GetMaxLeafMotion()const{float Max=0;for(const auto& P:Plants)Max=FMath::Max(Max,P.Amount);return Max;}
bool ACourseworkVentilation::CheckPlantTransforms()const{for(const auto& P:Plants)if(!P.Mesh.IsValid()||!P.Mesh->GetComponentTransform().Equals(P.Transform,.001f))return false;return true;}
bool ACourseworkVentilation::CheckPlantMaterialsRestored()const{for(const auto& P:Plants)if(!P.Mesh.IsValid()||P.Mesh->GetMaterial(P.Slot)!=P.Original.Get())return false;return true;}

bool ACourseworkVentilation::CheckAirflowDrawing()const
{
 return LastDrawSegments>0&&VisualWeight>.8f;
}

FVector ACourseworkVentilation::GetFlowFocus()const
{
 float Longest=0;FVector Focus=FVector::ZeroVector;for(const auto& P:Paths)if(P.Cross)for(int32 I=1;I<P.Points.Num();++I){const float L=FVector::Dist(P.Points[I-1],P.Points[I]);if(L>Longest){Longest=L;Focus=(P.Points[I-1]+P.Points[I])*.5f;}}return Focus;
}
FString ACourseworkVentilation::GetRenderDiagnostics()const
{
 FString Result=FString::Printf(TEXT("backend=world-projected translucent trails with view occlusion; relative-pressure network; exterior_wind=(-1,0,0); exterior_doors=%d; residual=%.6f; display=%d segments=%d focus=%s"),GetOpenExteriorDoorCount(),PressureResidual,IsDisplayed(),LastDrawSegments,*GetFlowFocus().ToString());
 Result+=FString::Printf(TEXT("\ntrail_reused=%d trail_retired=%d retiring_now=%d"),PathReuseCount,PathRetireCount,RetiringPaths.Num());
 for(const auto& W:Windows)Result+=FString::Printf(TEXT("\nwindow room=%d pose=%.3f net_flux=%.5f exchange=%.5f response=%.4f"),W.Room,W.Amount,W.Flow,W.Exchange,W.AirResponse);
 for(const auto& P:Paths){Result+=FString::Printf(TEXT("\nlength=%.2f cross=%d room=%d pressure=%.3f->%.3f flux=%.5f"),P.Length,P.Cross,P.Room,P.InletPressure,P.OutletPressure,P.Flux);for(const auto& V:P.Points)Result+=TEXT(" ")+V.ToCompactString();}return Result;
}
FVector ACourseworkVentilation::GetResponseSample()const
{
 FVector Result=FVector::ZeroVector;
 for(const auto& W:Windows){Result.X=FMath::Max(Result.X,double(W.Exchange));if(auto* A=W.Audio.Get())Result.Y=FMath::Max(Result.Y,double(A->VolumeMultiplier));}
 for(const auto& C:Curtains)Result.Z=FMath::Max(Result.Z,double(C.Amount));
 return Result;
}

void ACourseworkVentilation::DrawAirflow(UCanvas* Canvas,APlayerController* PC)
{
 LastDrawSegments=0;if(!Canvas||!PC||VisualWeight<.005f)return;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);FCollisionQueryParams Q(SCENE_QUERY_STAT(AirflowViewOcclusion),true);
 for(const auto& W:Windows)if(W.Proxy.IsValid())Q.AddIgnoredComponent(W.Proxy.Get());for(TActorIterator<APawn> It(GetWorld());It;++It)Q.AddIgnoredActor(*It);
 for(const auto& O:Originals)if(O.Mesh.IsValid())Q.AddIgnoredComponent(O.Mesh.Get());
 auto Along=[](const FFlowPath& P,float D){for(int32 I=1;I<P.Points.Num();++I){const float L=FVector::Dist(P.Points[I-1],P.Points[I]);if(D<=L)return FMath::Lerp(P.Points[I-1],P.Points[I],L>0?D/L:0.f);D-=L;}return P.Points.Last();};
 TArray<FCanvasUVTri> Triangles;
 const float Time=GetWorld()->GetTimeSeconds();
 auto DrawPath=[&](const FFlowPath& P,int32 Index){const bool Retired=P.Retired>=0;const float Strength=Retired?.45f:GetRoomStrength(P.Room),Fade=FMath::SmoothStep(0.f,.4f,Time-P.Born)*(Retired?1-FMath::SmoothStep(0.f,.45f,Time-P.Retired):1);if(Strength<.01f)return;
 const float Tail=P.Cross?74.f:45.f;const int32 Count=P.Offsets.Num();for(int32 N=0;N<Count;++N){const float Head=FMath::Fmod(P.Motion+P.Length*(N+.23f*Index)/Count,P.Length+80.f);
 for(int32 K=0;K<12;++K){const float T=float(K)/12,D=Head-(1-T)*Tail,D2=D+Tail/12;if(D<0||D2>P.Length)continue;const FVector A=Along(P,D)+P.Offsets[N],B=Along(P,D2)+P.Offsets[N],Mid=(A+B)*.5f;FVector2D ScreenA,ScreenB;
 if(Retired&&!SegmentClear(A,B))continue;
 const float Distance=FVector::Dist(Eye,Mid);if(Distance<90.f)continue;
 if(!PC->ProjectWorldLocationToScreen(A,ScreenA)||!PC->ProjectWorldLocationToScreen(B,ScreenB))continue;
 FHitResult Hit;if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,Mid,ECC_Visibility,Q)&&FVector::Dist(Hit.ImpactPoint,Mid)>2.f)continue;
 const FVector2D Delta=(ScreenB-ScreenA).GetSafeNormal(),Right(-Delta.Y,Delta.X);
 const float Width=FMath::Clamp(360.f/Distance,.8f,1.8f)*Canvas->SizeY/900.f;
 const float Alpha=VisualWeight*Fade*.64f*(.12f+.88f*T*T)*FMath::Clamp(Strength*2,0.f,1.f)*FMath::SmoothStep(90.f,140.f,Distance);
 const FLinearColor Color(.65f,.82f,.84f,Alpha);
 FCanvasUVTri A0;A0.V0_Pos=ScreenA-Right*Width;A0.V1_Pos=ScreenA+Right*Width;A0.V2_Pos=ScreenB+Right*Width;A0.V0_Color=A0.V1_Color=A0.V2_Color=Color;A0.V0_UV=A0.V1_UV=A0.V2_UV=FVector2D::ZeroVector;
 FCanvasUVTri B0=A0;B0.V1_Pos=ScreenB+Right*Width;B0.V2_Pos=ScreenB-Right*Width;Triangles.Add(A0);Triangles.Add(B0);++LastDrawSegments;
 }}};
 for(int32 I=0;I<Paths.Num();++I)DrawPath(Paths[I],I);
 for(int32 I=0;I<RetiringPaths.Num();++I)DrawPath(RetiringPaths[I],I);
 if(Triangles.Num()){FCanvasTriangleItem Item(Triangles,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);}
}
bool ACourseworkVentilation::CheckPolishState()const
{
 for(const auto& P:Paths)if(!FMath::IsFinite(P.Motion)||!FMath::IsFinite(P.Speed)||P.Motion<0||P.Motion>P.Length+80.f||P.Speed<23||P.Speed>101)return false;
 for(const auto& C:Curtains)if(!FMath::IsFinite(C.Amount)||C.Amount<0||C.Amount>3.01f||C.Direction.Size()>1.01f)return false;
 for(const auto& W:Windows)if(auto* A=W.Audio.Get())if(A->VolumeMultiplier<0||A->VolumeMultiplier>.261f||A->PitchMultiplier<.97f||A->PitchMultiplier>1.03f||!A->AttenuationOverrides.bEnableOcclusion)return false;
 return RetiringPaths.Num()<=24;
}

bool ACourseworkVentilation::CheckCurtains(bool Moving)const
{
 if(Curtains.Num()==0)return false;float Maximum=0;
 for(const auto& C:Curtains){if(!C.Mesh.IsValid()||!C.Material.IsValid()||C.Mesh->GetMaterial(C.Slot)!=C.Material.Get()||!C.Mesh->GetComponentTransform().Equals(C.Transform,.001f))return false;Maximum=FMath::Max(Maximum,C.Amount);}
 return Moving?Maximum>.1f:Maximum<.08f;
}
