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
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"

ACourseworkRain::ACourseworkRain()
{
    PrimaryActorTick.bCanEverTick=false;
    Legacy=FParse::Param(FCommandLine::Get(),TEXT("HouseRainLegacy"));
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("RainRoot"));SetRootComponent(Root);
}
void ACourseworkRain::Initialize()
{
    Ready=true;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,Legacy?TEXT("/Engine/BasicShapes/Cube.Cube"):TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,Legacy?TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"):TEXT("/Game/Coursework/RainPolish/M_RainStreak.M_RainStreak"));
    if(!Cube||!Base){UE_LOG(LogTemp,Error,TEXT("RAIN_POLISH_REQUIRED_ASSET missing rain material; import rain polish resource pack"));return;}
    Streaks=NewObject<UInstancedStaticMeshComponent>(this);AddInstanceComponent(Streaks);Streaks->SetupAttachment(GetRootComponent());
    Streaks->SetStaticMesh(Cube);Streaks->SetCollisionEnabled(ECollisionEnabled::NoCollision);Streaks->SetCanEverAffectNavigation(false);Streaks->SetCastShadow(false);
    Material=UMaterialInstanceDynamic::Create(Base,this);Material->SetVectorParameterValue(TEXT("Color"),FLinearColor(.50f,.58f,.64f));Material->SetScalarParameterValue(TEXT("Roughness"),.22f);Streaks->SetMaterial(0,Material);Streaks->RegisterComponent();
    for(int32 I=0;I<24*24*5;++I)Streaks->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
    if(!Legacy)
    {
        auto* SplashBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/RainPolish/M_RainSplash.M_RainSplash"));
        if(!SplashBase){UE_LOG(LogTemp,Error,TEXT("RAIN_POLISH_REQUIRED_ASSET missing splash material"));return;}
        SplashMesh=NewObject<UInstancedStaticMeshComponent>(this);AddInstanceComponent(SplashMesh);SplashMesh->SetupAttachment(GetRootComponent());SplashMesh->SetStaticMesh(Cube);
        SplashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);SplashMesh->SetCanEverAffectNavigation(false);SplashMesh->SetCastShadow(false);
        SplashMaterial=UMaterialInstanceDynamic::Create(SplashBase,this);SplashMesh->SetMaterial(0,SplashMaterial);SplashMesh->RegisterComponent();SplashPool.SetNum(48);
        for(int32 I=0;I<48*9;++I)SplashMesh->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
    }
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
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RainRoof),true);Q.bReturnFaceIndex=true;Q.AddIgnoredActor(this);if(auto* P=UGameplayStatics::GetPlayerPawn(this,0))Q.AddIgnoredActor(P);
    for(int32 Y=0;Y<24;++Y)for(int32 X=0;X<24;++X)
    {
        FColumn C;C.Position=Center+FVector((X-11.5)*100+R.FRandRange(-40,40),(Y-11.5)*100+R.FRandRange(-40,40),0);
        const float Slant=Legacy?0:.06f;
        FHitResult H;if(GetWorld()->LineTraceSingleByChannel(H,C.Position+FVector(Slant*3000,0,3000),C.Position-FVector(Slant*500,0,500),ECC_Visibility,Q))
        {
            C.Floor=H.ImpactPoint.Z;C.Impact=H.ImpactPoint;C.Normal=H.ImpactNormal;
            int32 Section=0;if(auto* M=Cast<UStaticMeshComponent>(H.GetComponent()))C.Hard=C.Normal.Z>.45f&&IsHardMaterial(M->GetMaterialFromCollisionFaceIndex(H.FaceIndex,Section));
        }
        Columns.Add(C);
    }
    for(auto& S:SplashPool)S.Active=false;
}
void ACourseworkRain::Update(float Amount,float Dt,float DaylightFactor)
{
    const double Began=FPlatformTime::Seconds();
    Amount=FMath::Clamp(Amount,0.f,1.f);if(!Ready&&Amount>.001f)Initialize();if(!Streaks)return;
    auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);if(!Camera)return;const FVector View=Camera->GetCameraLocation();
    if(Columns.IsEmpty()||(Amount>.001f&&FVector::DistSquared2D(Center,View)>400*400))BuildColumns(View);
    const bool Paused=GetWorld()->IsPaused();if(!Paused)Seconds+=FMath::Max(0.f,Dt);
    if(!Legacy)
    {
        const FLinearColor Tint=FMath::Lerp(FLinearColor(.025f,.035f,.05f),FLinearColor(.32f,.37f,.42f),FMath::Clamp(DaylightFactor,0.f,1.f));
        Material->SetVectorParameterValue(TEXT("RainTint"),Tint);if(SplashMaterial)SplashMaterial->SetVectorParameterValue(TEXT("RainTint"),Tint*1.4f);
        UpdateWetSurfaces(Amount,Paused?0:Dt);
    }
    ShelterElapsed+=Dt;
    if(ShelterElapsed>.25f)
    {
        ShelterElapsed=0;FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(RainListenerRoof),true);Q.AddIgnoredActor(this);if(auto* P=UGameplayStatics::GetPlayerPawn(this,0))Q.AddIgnoredActor(P);
        Sheltered=GetWorld()->LineTraceSingleByChannel(H,View,View+FVector(0,0,3000),ECC_Visibility,Q);
    }
    VisibleDrops=0;
    if(Amount>.003f)for(int32 I=0;I<Columns.Num();++I)for(int32 J=0;J<5;++J)
    {
        const int32 N=I*5+J;const auto& C=Columns[I];const float Length=Legacy?10.f+float(N%13):12.f+float(N%19),Span=1800;
        const double Falling=FMath::Fmod(double(N)*137.27-Seconds*(900+N%250),double(Span));
        const float Z=C.Floor+Length+2+float(FMath::Fmod(Falling+Span,double(Span)));
        const FVector Position=C.Position+FVector(Legacy?0:.06f*Z,0,Z);const float Distance=FVector::Distance(Position,View);
        const float Fade=Legacy?1:FMath::SmoothStep(45.f,160.f,Distance)*(1-FMath::SmoothStep(1300.f,1850.f,Distance));
        const bool Visible=Amount>.003f&&float((N*37)%101)/101.f<Amount&&Fade>.001f;
        const float Width=Visible?(Legacy?.45f:FMath::Lerp(.55f,1.25f,FMath::Clamp(Distance/1600,0.f,1.f)))*FMath::Clamp(Amount*4,0.f,1.f)*Fade:0;
        const FVector Axis=FVector(.06f,0,1).GetSafeNormal(),Right=FVector::CrossProduct(Axis,View-Position).GetSafeNormal(SMALL_NUMBER,FVector::XAxisVector);
        const FQuat Rotation=Legacy?FQuat::Identity:FRotationMatrix::MakeFromXY(Right,Axis).ToQuat();
        const FTransform T(Rotation,Position,Legacy?FVector(Width/100,Width/100,Visible?Length/100:0):FVector(Width/100,Visible?Length/100:0,1));
        Streaks->UpdateInstanceTransform(N,T,false,false,true);VisibleDrops+=Visible?1:0;
    }
    if(VisibleDrops>0)Streaks->MarkRenderStateDirty();Streaks->SetVisibility(VisibleDrops>0);
    if(!Legacy)UpdateSplashes(Amount,Paused?0:Dt,View);
    // Roof attenuation changes smoothly; fire keeps its alarm intelligible.
    const float Target=Paused?0:Amount*(Sheltered?.10f:.40f);AudioGain=FMath::FInterpTo(AudioGain,Target,FMath::Max(Dt,Paused?.05f:0.f),2.5f);
    if(Audio&&Wave)
    {
        Audio->SetVolumeMultiplier(AudioGain);
        if(AudioGain>.001f){if(Wave->GetAvailableAudioByteCount()<22050)Wave->QueueAudio(PCM.GetData(),PCM.Num());if(!Audio->IsPlaying())Audio->Play();}
        else if(Audio->IsPlaying()){Audio->Stop();Wave->ResetAudio();}
    }
    EffectMilliseconds=(FPlatformTime::Seconds()-Began)*1000;
}
bool ACourseworkRain::CheckRoofClipping()const
{
    if(!Streaks||!Material||!Material->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes)||Columns.Num()!=24*24)return false;
    for(int32 I=0;I<Columns.Num();++I)for(int32 J=0;J<5;++J){FTransform T;if(!Streaks->GetInstanceTransform(I*5+J,T,true))return false;const float Half=(Legacy?T.GetScale3D().Z:T.GetScale3D().Y)*50;if(T.GetLocation().Z-Half<=Columns[I].Floor)return false;}
    return true;
}
void ACourseworkRain::EndPlay(const EEndPlayReason::Type Reason)
{RestoreDrySurfaces();if(Audio)Audio->Stop();Super::EndPlay(Reason);}
