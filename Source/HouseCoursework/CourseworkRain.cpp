#include "CourseworkRain.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundWaveProcedural.h"

ACourseworkRain::ACourseworkRain()
{
    PrimaryActorTick.bCanEverTick=false;
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("RainRoot"));SetRootComponent(Root);
}
void ACourseworkRain::Initialize()
{
    Ready=true;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    // This material already has an instanced-mesh shader in the shipped solar
    // installation; borrowing an unlit non-instanced material falls back in cooked builds.
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if(!Cube||!Base){UE_LOG(LogTemp,Error,TEXT("RAIN missing cooked primitive/material"));return;}
    Streaks=NewObject<UInstancedStaticMeshComponent>(this);AddInstanceComponent(Streaks);Streaks->SetupAttachment(GetRootComponent());
    Streaks->SetStaticMesh(Cube);Streaks->SetCollisionEnabled(ECollisionEnabled::NoCollision);Streaks->SetCanEverAffectNavigation(false);Streaks->SetCastShadow(false);
    Material=UMaterialInstanceDynamic::Create(Base,this);Material->SetVectorParameterValue(TEXT("Color"),FLinearColor(.50f,.58f,.64f));Material->SetScalarParameterValue(TEXT("Roughness"),.22f);Streaks->SetMaterial(0,Material);Streaks->RegisterComponent();
    for(int32 I=0;I<24*24*5;++I)Streaks->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
    // Original filtered noise with sparse patter, no external sound asset.
    constexpr int32 Rate=22050,Count=Rate*4;PCM.SetNumUninitialized(Count*2);FRandomStream Noise(90631);float Smooth=0;
    for(int32 I=0;I<Count;++I)
    {
        const float N=Noise.FRandRange(-1.f,1.f);Smooth=FMath::Lerp(Smooth,N,.13f);
        const float T=float(I)/Rate,Envelope=.83f+.09f*FMath::Sin(T*2*PI/4)+.08f*FMath::Sin(T*2*PI);
        const int16 V=int16((N*.19f+Smooth*.65f)*Envelope*12000);
        PCM[2*I]=uint8(V&255);PCM[2*I+1]=uint8((V>>8)&255);
    }
    Wave=NewObject<USoundWaveProcedural>(this);Wave->SetSampleRate(Rate);Wave->NumChannels=1;Wave->Duration=INDEFINITELY_LOOPING_DURATION;Wave->SoundGroup=SOUNDGROUP_Effects;
    Audio=NewObject<UAudioComponent>(this);AddInstanceComponent(Audio);Audio->bAutoActivate=false;Audio->bAutoDestroy=false;Audio->SetSound(Wave);Audio->SetVolumeMultiplier(0);Audio->RegisterComponent();
}
void ACourseworkRain::BuildColumns(FVector Position)
{
    Center=FVector(FMath::GridSnap(Position.X,200.),FMath::GridSnap(Position.Y,200.),0);Columns.Empty();FRandomStream R(8129);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RainRoof),true);Q.AddIgnoredActor(this);if(auto* P=UGameplayStatics::GetPlayerPawn(this,0))Q.AddIgnoredActor(P);
    for(int32 Y=0;Y<24;++Y)for(int32 X=0;X<24;++X)
    {
        FColumn C;C.Position=Center+FVector((X-11.5)*100+R.FRandRange(-40,40),(Y-11.5)*100+R.FRandRange(-40,40),0);
        FHitResult H;if(GetWorld()->LineTraceSingleByChannel(H,C.Position+FVector(0,0,3000),C.Position-FVector(0,0,500),ECC_Visibility,Q))C.Floor=H.ImpactPoint.Z;
        Columns.Add(C);
    }
}
void ACourseworkRain::Update(float Amount,float Dt)
{
    Amount=FMath::Clamp(Amount,0.f,1.f);if(!Ready&&Amount>.001f)Initialize();if(!Streaks)return;
    auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);if(!Camera)return;const FVector View=Camera->GetCameraLocation();
    if(Columns.IsEmpty()||(Amount>.001f&&FVector::DistSquared2D(Center,View)>400*400))BuildColumns(View);
    const bool Paused=GetWorld()->IsPaused();if(!Paused)Seconds+=FMath::Max(0.f,Dt);
    ShelterElapsed+=Dt;
    if(ShelterElapsed>.25f)
    {
        ShelterElapsed=0;FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(RainListenerRoof),true);Q.AddIgnoredActor(this);if(auto* P=UGameplayStatics::GetPlayerPawn(this,0))Q.AddIgnoredActor(P);
        Sheltered=GetWorld()->LineTraceSingleByChannel(H,View,View+FVector(0,0,3000),ECC_Visibility,Q);
    }
    VisibleDrops=0;
    if(Amount>.003f)for(int32 I=0;I<Columns.Num();++I)for(int32 J=0;J<5;++J)
    {
        const int32 N=I*5+J;const auto& C=Columns[I];const float Length=10.f+float(N%13),Span=1800;
        const double Falling=FMath::Fmod(double(N)*137.27-Seconds*(900+N%250),double(Span));
        const float Z=C.Floor+Length+2+float(FMath::Fmod(Falling+Span,double(Span)));
        const bool Visible=Amount>.003f&&float((N*37)%101)/101.f<Amount;
        const float Width=Visible?.45f*FMath::Clamp(Amount*4,0.f,1.f):0;
        const FTransform T(FQuat::Identity,FVector(C.Position.X,C.Position.Y,Z),FVector(Width/100,Width/100,Visible?Length/100:0));
        Streaks->UpdateInstanceTransform(N,T,false,false,true);VisibleDrops+=Visible?1:0;
    }
    if(VisibleDrops>0)Streaks->MarkRenderStateDirty();Streaks->SetVisibility(VisibleDrops>0);
    // Roof attenuation changes smoothly; fire keeps its alarm intelligible.
    const float Target=Paused?0:Amount*(Sheltered?.10f:.40f);AudioGain=FMath::FInterpTo(AudioGain,Target,FMath::Max(Dt,Paused?.05f:0.f),2.5f);
    if(Audio&&Wave)
    {
        Audio->SetVolumeMultiplier(AudioGain);
        if(AudioGain>.001f){if(Wave->GetAvailableAudioByteCount()<22050)Wave->QueueAudio(PCM.GetData(),PCM.Num());if(!Audio->IsPlaying())Audio->Play();}
        else if(Audio->IsPlaying()){Audio->Stop();Wave->ResetAudio();}
    }
}
bool ACourseworkRain::CheckRoofClipping()const
{
    if(!Streaks||!Material||!Material->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes)||Columns.Num()!=24*24)return false;
    for(int32 I=0;I<Columns.Num();++I)for(int32 J=0;J<5;++J){FTransform T;if(!Streaks->GetInstanceTransform(I*5+J,T,true))return false;const float Half=T.GetScale3D().Z*50;if(T.GetLocation().Z-Half<=Columns[I].Floor)return false;}
    return true;
}
void ACourseworkRain::EndPlay(const EEndPlayReason::Type Reason)
{if(Audio)Audio->Stop();Super::EndPlay(Reason);}
