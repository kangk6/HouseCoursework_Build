#include "CourseworkDaylight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/LightComponent.h"
#include "HAL/IConsoleManager.h"

ACourseworkDaylight::ACourseworkDaylight()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    // Explicit cooked references; original mesh assets remain unchanged.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh0(TEXT("/Game/Coursework/Daylight/SM_Window_Floor1_A3_400_Daylight.SM_Window_Floor1_A3_400_Daylight"));
    if(Mesh0.Object)ComparisonMeshes.Add(Mesh0.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh1(TEXT("/Game/Coursework/Daylight/SM_Curtains_A1_Daylight.SM_Curtains_A1_Daylight"));
    if(Mesh1.Object)ComparisonMeshes.Add(Mesh1.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh2(TEXT("/Game/Coursework/Daylight/SM_Curtains_A2_Daylight.SM_Curtains_A2_Daylight"));
    if(Mesh2.Object)ComparisonMeshes.Add(Mesh2.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh3(TEXT("/Game/Coursework/Daylight/SM_Curtains_A3_Daylight.SM_Curtains_A3_Daylight"));
    if(Mesh3.Object)ComparisonMeshes.Add(Mesh3.Object);
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("EnvironmentRoot"));SetRootComponent(Root);
    Sun=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("ComparisonSun"));Sun->SetupAttachment(Root);
    Sun->SetMobility(EComponentMobility::Movable);Sun->SetVisibility(false);
    Sun->DynamicShadowDistanceMovableLight=6000;Sun->DynamicShadowCascades=4;Sun->ShadowBias=.35f;
    Sun->bAtmosphereSunLight=true;Sun->AtmosphereSunLightIndex=0;
    Atmosphere=CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("ComparisonAtmosphere"));Atmosphere->SetupAttachment(Root);Atmosphere->SetVisibility(false);
    Sky=CreateDefaultSubobject<USkyLightComponent>(TEXT("ComparisonSky"));Sky->SetupAttachment(Root);
    Sky->SetMobility(EComponentMobility::Movable);Sky->SourceType=SLS_CapturedScene;Sky->bRealTimeCapture=true;Sky->bLowerHemisphereIsBlack=true;Sky->CubemapResolution=64;Sky->SetVisibility(false);
}
bool ACourseworkDaylight::Apply(float Value)
{
    if(!FMath::IsFinite(Value))return false;
    if(!bActive)
    {
        Actors.Empty();Components.Empty();Meshes.Empty();OriginalMeshReferences.Empty();
        for(TActorIterator<AActor> It(GetWorld());It;++It)
        {
            if(*It==this)continue;
            const FString Class=It->GetClass()->GetName();
            const bool Environment=Class.Contains(TEXT("Sky"))||Class==TEXT("DirectionalLight")||Class.Contains(TEXT("HeightFog"))||Class.Contains(TEXT("SkyAtmosphere"))||It->GetName().StartsWith(TEXT("CourseworkNightAmbient_"));
            if(!Environment)continue;
            Actors.Add({*It,It->IsHidden(),It->IsActorTickEnabled()});It->SetActorTickEnabled(false);
            const bool NightAmbient=It->GetName().StartsWith(TEXT("CourseworkNightAmbient_"));
            if(!NightAmbient)It->SetActorHiddenInGame(true);
            TArray<USceneComponent*> Parts;It->GetComponents(Parts);
            for(auto* C:Parts)
            {
                auto* Ambient=NightAmbient?Cast<ULightComponent>(C):nullptr;
                Components.Add({C,C->IsVisible(),Ambient?Ambient->Intensity:-1.f});
                if(!Ambient)C->SetVisibility(false,false);
            }
        }
        for(TActorIterator<AActor> It(GetWorld());It;++It)
        {TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* C:Parts)if(C->Mobility!=EComponentMobility::Static)if(UStaticMesh* Original=C->GetStaticMesh())
        {for(const auto& Comparison:ComparisonMeshes)if(Comparison->GetName()==Original->GetName()+TEXT("_Daylight"))
        {if(C->SetStaticMesh(Comparison)){Meshes.Add({C,Original});OriginalMeshReferences.Emplace(Original);}break;}}}
        Exposure=GetWorld()->SpawnActor<APostProcessVolume>();Exposure->bUnbound=true;Exposure->Priority=100;Exposure->BlendWeight=0;
        auto& P=Exposure->Settings;P.bOverride_AutoExposureMinBrightness=true;P.bOverride_AutoExposureMaxBrightness=true;
        P.AutoExposureMinBrightness=1;P.AutoExposureMaxBrightness=1;
        P.bOverride_AutoExposureBias=true;P.AutoExposureBias=0;
        // Let the movable sky replace the legacy static sky before enabling
        // atmosphere; no baked atmosphere dependency is introduced.
        // A 64px sky-only GPU capture is cheap enough to update every frame.
        // Keep irradiance and reflections on the same clock as the moving sun.
        if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice")))
        {OriginalSkyTimeSlice=CVar->GetInt();bChangedSkyTimeSlice=OriginalSkyTimeSlice!=0;if(bChangedSkyTimeSlice)CVar->SetWithCurrentPriority(0);}
        bActive=true;RenderedHour=FMath::Clamp(Value,6.f,22.f);PreviewWeight=0;
        Sky->SetVisibility(true);Sun->SetVisibility(true);SetActorTickEnabled(true);
        GetWorld()->GetTimerManager().SetTimer(AtmosphereTimer,[this](){if(bActive)Atmosphere->SetVisibility(true);},.1f,false);
    }
    Hour=FMath::Clamp(Value,6.f,22.f);
    Elevation=FMath::Max(0.f,65.f*FMath::Sin(FMath::Clamp((Hour-6)/12.f,0.f,1.f)*PI));
    if(PreviewWeight==0)UpdateLighting();
    return true;
}
void ACourseworkDaylight::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!bActive)return;
    const float Delta=Hour-RenderedHour;
    const float Step=Delta*(1-FMath::Exp(-DeltaSeconds/.12f));
    RenderedHour+=FMath::Clamp(Step,-10.f*DeltaSeconds,10.f*DeltaSeconds);
    if(FMath::Abs(Hour-RenderedHour)<.0001f)RenderedHour=Hour;
    PreviewWeight=FMath::Min(1.f,PreviewWeight+DeltaSeconds*4.f);
    UpdateLighting();
}
void ACourseworkDaylight::UpdateLighting()
{
    const float Phase=(RenderedHour-6)/12.f;
    const float SolarElevation=65.f*FMath::Sin(Phase*PI);
    const float DayWeight=FMath::SmoothStep(-6.f,12.f,SolarElevation);
    const float Yaw=FMath::Lerp(-90.f,90.f,Phase);
    Sun->SetWorldRotation(FRotator(-SolarElevation,Yaw,0));
    const float SunStrength=FMath::SmoothStep(0.f,8.f,SolarElevation);
    Sun->SetIntensity(PreviewWeight*SunStrength*(4+8*FMath::Max(0.f,FMath::Sin(Phase*PI))));
    Sun->SetLightColor(FMath::Lerp(FLinearColor(1,.53f,.24f),FLinearColor(1,.96f,.87f),FMath::Clamp(SolarElevation/40.f,0.f,1.f)));
    Sky->SetIntensity(FMath::Lerp(.08f,.9f,DayWeight)*PreviewWeight);
    Sky->SetLightColor(FMath::Lerp(FLinearColor(.35f,.5f,1),FLinearColor(1,.98f,.93f),DayWeight));
    if(Exposure)Exposure->BlendWeight=PreviewWeight;
    for(const auto& S:Components)if(S.AmbientIntensity>=0)if(auto* L=Cast<ULightComponent>(S.Component.Get()))L->SetIntensity(S.AmbientIntensity*(1-PreviewWeight));
}
void ACourseworkDaylight::Restore()
{
    if(!bActive)return;
    GetWorld()->GetTimerManager().ClearTimer(AtmosphereTimer);
    Sun->SetVisibility(false);Sun->SetIntensity(0);Atmosphere->SetVisibility(false);Sky->SetVisibility(false);
    if(Exposure){Exposure->Destroy();Exposure=nullptr;}
    for(const auto& S:Meshes)if(auto* C=S.Component.Get())if(auto* M=S.Original.Get())C->SetStaticMesh(M);
    for(const auto& State:Actors)if(auto* A=State.Actor.Get()){A->SetActorHiddenInGame(State.Hidden);A->SetActorTickEnabled(State.Tick);}
    for(const auto& State:Components)if(auto* C=State.Component.Get()){if(State.AmbientIntensity>=0)if(auto* L=Cast<ULightComponent>(C))L->SetIntensity(State.AmbientIntensity);C->SetVisibility(State.Visible,false);}
    bActive=false;SetActorTickEnabled(false);OriginalMeshReferences.Empty();
    if(bChangedSkyTimeSlice)if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice")))if(CVar->GetInt()==0)CVar->SetWithCurrentPriority(OriginalSkyTimeSlice);
    bChangedSkyTimeSlice=false;
}
bool ACourseworkDaylight::CheckRestored()const
{
    if(bActive||IsActorTickEnabled()||Sun->IsVisible()||Sky->IsVisible()||Atmosphere->IsVisible()||Exposure)return false;
    if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice")))if(CVar->GetInt()!=OriginalSkyTimeSlice)return false;
    for(const auto& S:Meshes)if(auto* C=S.Component.Get())if(C->GetStaticMesh()!=S.Original.Get())return false;
    for(const auto& S:Actors)if(auto* A=S.Actor.Get())if(A->IsHidden()!=S.Hidden||A->IsActorTickEnabled()!=S.Tick)return false;
    for(const auto& S:Components)if(auto* C=S.Component.Get())
    {
        if(C->IsVisible()!=S.Visible)return false;
        if(S.AmbientIntensity>=0)if(auto* L=Cast<ULightComponent>(C))if(!FMath::IsNearlyEqual(L->Intensity,S.AmbientIntensity))return false;
    }
    return true;
}
float ACourseworkDaylight::GetSunIntensity()const{return Sun->Intensity;}
FRotator ACourseworkDaylight::GetSunRotation()const{return Sun->GetComponentRotation();}
bool ACourseworkDaylight::IsSunShadowed()const{return Sun->CastShadows&&Sun->DynamicShadowDistanceMovableLight>=5000;}
void ACourseworkDaylight::EndPlay(const EEndPlayReason::Type Reason){Restore();Super::EndPlay(Reason);}

float ACourseworkDaylight::GetSkyIntensity()const{return Sky->Intensity;}
bool ACourseworkDaylight::UsesRealtimeSky()const{return Sky->IsRealTimeCaptureEnabled();}
