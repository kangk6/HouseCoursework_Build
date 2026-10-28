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
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "CourseworkClock.h"

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
        Actors.Empty();Components.Empty();Meshes.Empty();OriginalMeshReferences.Empty();SkyParameters.Empty();WeatherSkies.Empty();
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
                auto* Ambient=Cast<ULightComponent>(C);
                auto* Fog=Cast<UExponentialHeightFogComponent>(C);auto* Directional=Cast<UDirectionalLightComponent>(C);
                Components.Add({C,C->IsVisible(),Ambient?Ambient->Intensity:-1.f,Fog?Fog->FogDensity:-1.f,Fog?Fog->SecondFogData.FogDensity:-1.f,Directional&&Directional->bAtmosphereSunLight,Fog?Fog->FogMaxOpacity:-1.f});
                if(Directional)Directional->SetAtmosphereSunLight(false);
                if(Class.Contains(TEXT("Sky")))if(auto* Mesh=Cast<UStaticMeshComponent>(C))for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)if(auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot)))
                {
                    FSkyParameters State;State.Material=MID;TArray<FMaterialParameterInfo> Infos;TArray<FGuid> IDs;MID->GetAllVectorParameterInfo(Infos,IDs);
                    for(const auto& Info:Infos){FLinearColor Color;if(Info.Name.ToString().Contains(TEXT("color"))&&MID->GetVectorParameterValue(Info,Color))State.Colors.Add({Info.Name,Color});}
                    Infos.Empty();IDs.Empty();MID->GetAllScalarParameterInfo(Infos,IDs);
                    for(const auto& Info:Infos){float ParameterValue=0;if(Info.Name.ToString().Contains(TEXT("brightness"))&&MID->GetScalarParameterValue(Info,ParameterValue))State.Brightness.Add({Info.Name,ParameterValue});}
                    SkyParameters.Add(MoveTemp(State));
                    FWeatherSky W;W.Mesh=Mesh;W.Slot=Slot;W.Original=MID;OriginalMeshReferences.Emplace(MID);W.Colors=SkyParameters.Last().Colors;W.Brightness=SkyParameters.Last().Brightness;
                    W.Direction=MID->K2_GetVectorParameterValue(TEXT("Light direction"));
                    auto* WeatherMID=UMaterialInstanceDynamic::Create(MID->Parent,Mesh);WeatherMID->CopyParameterOverrides(MID);Mesh->SetMaterial(Slot,WeatherMID);W.Material=WeatherMID;WeatherSkies.Add(MoveTemp(W));
                }
                if(!Ambient)C->SetVisibility(false,false);
            }
        }
        for(TActorIterator<AActor> It(GetWorld());It;++It)
        {TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* C:Parts)if(C->Mobility!=EComponentMobility::Static)if(UStaticMesh* Original=C->GetStaticMesh())
        {for(const auto& Comparison:ComparisonMeshes)if(Comparison->GetName()==Original->GetName()+TEXT("_Daylight"))
        {if(C->SetStaticMesh(Comparison)){Meshes.Add({C,Original});OriginalMeshReferences.Emplace(Original);}break;}}}
        // Keep the authored night metering envelope through the daytime blend.
        // Fixing day exposure at 1 while night adapts down to .15 made the same
        // electric lights much darker as soon as daytime weather lost direct sun.
        float ExposureMin=.15f,ExposureMax=5.f,ExposureBias=0,Priority=-FLT_MAX;
        EAutoExposureMethod ExposureMethod=AEM_Histogram;
        for(TActorIterator<APostProcessVolume> It(GetWorld());It;++It)if(It->bUnbound&&It->BlendWeight>0&&It->Priority>Priority)
        {Priority=It->Priority;ExposureMin=It->Settings.AutoExposureMinBrightness;ExposureMax=It->Settings.AutoExposureMaxBrightness;ExposureBias=It->Settings.AutoExposureBias;ExposureMethod=It->Settings.AutoExposureMethod;AuthoredBloom=It->Settings.BloomIntensity;AuthoredBloomThreshold=It->Settings.BloomThreshold;AuthoredLensFlare=It->Settings.LensFlareIntensity;}
        Exposure=GetWorld()->SpawnActor<APostProcessVolume>();Exposure->bUnbound=true;Exposure->Priority=100;Exposure->BlendWeight=0;
        auto& P=Exposure->Settings;P.bOverride_AutoExposureMinBrightness=true;P.bOverride_AutoExposureMaxBrightness=true;
        P.AutoExposureMinBrightness=ExposureMin;P.AutoExposureMaxBrightness=ExposureMax;
        P.bOverride_AutoExposureBias=true;P.AutoExposureBias=ExposureBias;
        P.bOverride_AutoExposureMethod=true;P.AutoExposureMethod=ExposureMethod;
        P.bOverride_BloomIntensity=true;P.bOverride_BloomThreshold=true;P.bOverride_LensFlareIntensity=true;
        // Let the movable sky replace the legacy static sky before enabling
        // atmosphere; no baked atmosphere dependency is introduced.
        // A 64px sky-only GPU capture is cheap enough to update every frame.
        // Keep irradiance and reflections on the same clock as the moving sun.
        if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice")))
        {OriginalSkyTimeSlice=CVar->GetInt();bChangedSkyTimeSlice=OriginalSkyTimeSlice!=0;if(bChangedSkyTimeSlice)CVar->SetWithCurrentPriority(0);}
        bActive=true;RenderedHour=float(FCourseworkClock::Wrap(Value));PreviewWeight=0;
        Sky->SetVisibility(true);Sun->SetVisibility(true);SetActorTickEnabled(true);
    }
    Hour=float(FCourseworkClock::Wrap(Value));
    Elevation=FMath::Max(0.f,65.f*FMath::Sin((Hour-6)/12.f*PI));
    if(PreviewWeight==0)UpdateLighting();
    return true;
}
bool ACourseworkDaylight::Snap(float Value)
{
    if(!Apply(Value))return false;
    RenderedHour=Hour;PreviewWeight=1;UpdateLighting();return true;
}
void ACourseworkDaylight::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!bActive)return;
    // Shortest circular distance: 23:59 -> 00:00 must not sweep through noon.
    float Delta=Hour-RenderedHour;
    if(Delta>12)Delta-=24;
    if(Delta< -12)Delta+=24;
    const float Step=Delta*(1-FMath::Exp(-DeltaSeconds/.12f));
    RenderedHour=float(FCourseworkClock::Wrap(RenderedHour+FMath::Clamp(Step,-10.f*DeltaSeconds,10.f*DeltaSeconds)));
    if(FMath::Abs(Delta)<.0001f)RenderedHour=Hour;
    PreviewWeight=FMath::Min(1.f,PreviewWeight+DeltaSeconds*4.f);
    UpdateLighting();
}
void ACourseworkDaylight::UpdateLighting()
{
    const float Phase=(RenderedHour-6)/12.f;
    const float SolarElevation=65.f*FMath::Sin(Phase*PI);
    const float DayWeight=FMath::SmoothStep(0.f,12.f,SolarElevation);
    const float NightWeight=FMath::SmoothStep(0.f,4.f,-SolarElevation);
    const float Cloud=FMath::Clamp((WeatherCover-.1f)/.82f,0.f,1.f);
    Atmosphere->SetSkyLuminanceFactor(FLinearColor(DayWeight,DayWeight,DayWeight));
    Atmosphere->SetAerialPespectiveViewDistanceScale(DayWeight);
    const float Yaw=FMath::Lerp(-90.f,90.f,Phase);
    Sun->SetWorldRotation(FRotator(-SolarElevation,Yaw,0));
    const float SunStrength=FMath::SmoothStep(0.f,8.f,SolarElevation);
    // Zero removes a directional light from the render scene and changes the
    // atmosphere's source selection. Keep a negligible 0.00001-unit source
    // registered through the horizon; acceptance checks its actual raw value.
    Sun->SetIntensity(FMath::Max(.00001f,PreviewWeight*SunStrength*(4+8*FMath::Max(0.f,FMath::Sin(Phase*PI)))*WeatherDirect*FMath::Lerp(1.f,.4f,Cloud)));
    Sun->SetLightSourceAngle(FMath::Lerp(.5357f,8.f,Cloud));
    Sun->SetLightColor(FMath::Lerp(FLinearColor(1,.53f,.24f),FLinearColor(1,.96f,.87f),FMath::Clamp(SolarElevation/40.f,0.f,1.f)));
    // Cloud cover reduces the direct beam, but the diffuse sky remains an
    // independent source. Do not attenuate it a second time with cloud amount.
    // Calibrate the captured dome separately from its visible emissive scale:
    // cloudy illumination is broad sky fill, not weakened wall occlusion.
    Sky->SetIntensity(FMath::Lerp(.08f,.9f*WeatherDiffuse*FMath::Lerp(1.f,3.f,Cloud),DayWeight)*PreviewWeight);
    Sky->SetLightColor(FMath::Lerp(FLinearColor(.35f,.5f,1),FLinearColor(1,.98f,.93f),DayWeight));
    // Night uses the authored night exposure, sky and fill lights. Previously
    // selecting 21:00 removed those fills and forced daylight exposure to zero,
    // making both household lights and the night guidance almost invisible.
    const float DayBlend=PreviewWeight*DayWeight;
    const bool OriginalNight=DayWeight<=KINDA_SMALL_NUMBER;
    for(const auto& S:Meshes)if(auto* C=S.Component.Get())if(auto* M=S.Original.Get())
    {
        UStaticMesh* Desired=M;
        for(const auto& Comparison:ComparisonMeshes)if(Comparison->GetName()==M->GetName()+TEXT("_Daylight")){Desired=Comparison;break;}
        if(C->GetStaticMesh()!=Desired)C->SetStaticMesh(Desired);
    }
    if(Exposure)Exposure->BlendWeight=DayBlend;
    for(const auto& S:SkyParameters)if(auto* M=S.Material.Get())
    {for(const auto& P:S.Colors)M->SetVectorParameterValue(P.Key,FLinearColor(P.Value.R*NightWeight,P.Value.G*NightWeight,P.Value.B*NightWeight,P.Value.A));for(const auto& P:S.Brightness)M->SetScalarParameterValue(P.Key,P.Value*NightWeight);}
    if(Exposure)
    {
        // The night scene's bloom (2.12, threshold .029) spreads ordinary cloudy
        // sky pixels into white halos. Fade only daytime cloud post-processing;
        // preserve the inherited metering and all authored night settings.
        Exposure->Settings.BloomIntensity=FMath::Lerp(AuthoredBloom,.8f,Cloud);
        Exposure->Settings.BloomThreshold=FMath::Lerp(AuthoredBloomThreshold,.2f,Cloud);
        Exposure->Settings.LensFlareIntensity=FMath::Lerp(AuthoredLensFlare,0.f,Cloud);
    }
    // Clear sky stays with the atmosphere. Fade the optional cloud dome over
    // a broad elevation range; multiplying its original night tint by a white
    // daytime tint near zero elevation caused an abrupt horizon brightening.
    const float SkyDay=Cloud*FMath::SmoothStep(0.f,30.f,SolarElevation);
    for(const auto& S:WeatherSkies)if(auto* M=S.Material.Get())
    {
        for(const auto& P:S.Colors)
        {
            FLinearColor Day=P.Value;
            // The legacy night dome is emissive, not an albedo. A unit overall
            // multiplier clipped cloudy horizons after the inherited eye adaptation.
            if(P.Key==TEXT("Overall Color"))Day=FLinearColor(.06f,.06f,.06f);
            if(P.Key==TEXT("Zenith Color"))Day=FMath::Lerp(FLinearColor(.025f,.12f,.28f),FLinearColor(.27f,.28f,.29f),Cloud);
            if(P.Key==TEXT("Horizon color"))Day=FMath::Lerp(FLinearColor(.38f,.60f,.82f),FLinearColor(.50f,.51f,.52f),Cloud);
            if(P.Key==TEXT("Cloud color"))Day=FMath::Lerp(FLinearColor(.86f,.9f,.96f),FLinearColor(.38f,.39f,.40f),Cloud);
            M->SetVectorParameterValue(P.Key,FLinearColor(P.Value.R*NightWeight+Day.R*SkyDay,P.Value.G*NightWeight+Day.G*SkyDay,P.Value.B*NightWeight+Day.B*SkyDay,P.Value.A));
        }
        for(const auto& P:S.Brightness)M->SetScalarParameterValue(P.Key,P.Value*NightWeight*(P.Key==TEXT("Stars brightness")?1-Cloud*.9f:1));
        M->SetScalarParameterValue(TEXT("Cloud opacity"),FMath::Lerp(.22f,4.5f,Cloud));
        M->SetScalarParameterValue(TEXT("Cloud speed"),.03f);
        M->SetScalarParameterValue(TEXT("Sun height"),FMath::Max(0.f,SolarElevation/90));
        const FVector D=Sun->GetForwardVector();M->SetVectorParameterValue(TEXT("Light direction"),FMath::Lerp(S.Direction,FLinearColor(D.X,D.Y,D.Z,1),DayWeight));
    }
    Sky->SetVisibility(true);Sun->SetVisibility(true);
    for(const auto& S:Actors)if(auto* A=S.Actor.Get())
        A->SetActorHiddenInGame(S.Hidden||A->FindComponentByClass<USkyLightComponent>()||(!WeatherSkies.Num()&&!OriginalNight&&A->GetClass()->GetName().Contains(TEXT("Sky"))));
    for(const auto& S:Components)if(auto* C=S.Component.Get())
    {
        if(Cast<USkyLightComponent>(C)){C->SetVisibility(false,false);continue;}
        // Density alone still produces saturated fog at far sky distances.
        // Fade the opacity limit too, so its first nonzero density cannot add
        // a blue veil in a single frame at the horizon.
        if(auto* Fog=Cast<UExponentialHeightFogComponent>(C)){Fog->SetFogDensity(S.FogDensity*NightWeight);Fog->SetSecondFogDensity(S.SecondFogDensity*NightWeight);Fog->SetFogMaxOpacity(S.FogMaxOpacity*NightWeight);}
        if(auto* L=Cast<ULightComponent>(C))
        {
            // The two existing low-level interior bounce proxies must not vanish
            // by day while remaining free ambient light at night. Scale their
            // daytime contribution by diffuse weather, without adding fixtures.
            // The captured daytime sky supplies part of the bounce already;
            // keep this legacy proxy below its authored night contribution.
            const float BounceWeight=1-DayBlend+DayBlend*.8f*WeatherDiffuse;
            if(S.AmbientIntensity>=0)L->SetIntensity(S.AmbientIntensity*(C->GetOwner()->GetName().StartsWith(TEXT("CourseworkNightAmbient_"))?BounceWeight:NightWeight));
            C->SetVisibility(S.Visible,false);
        }
        else C->SetVisibility(S.Visible&&(!C->GetOwner()->GetClass()->GetName().Contains(TEXT("Sky"))||OriginalNight||WeatherSkies.Num()>0),false);
    }
    // Re-entering daytime must follow the same ordering as first activation.
    // UE checks the render scene for static sky lights when adding atmosphere;
    // enabling it in the same frame as removing the old sky leaves a permanent
    // false "lighting needs rebuild" dependency on that static night sky.
    if(!Atmosphere->IsVisible()&&!GetWorld()->GetTimerManager().IsTimerActive(AtmosphereTimer))
        GetWorld()->GetTimerManager().SetTimer(AtmosphereTimer,[this](){if(bActive)Atmosphere->SetVisibility(true);},.1f,false);
}
void ACourseworkDaylight::Restore()
{
    if(!bActive)return;
    GetWorld()->GetTimerManager().ClearTimer(AtmosphereTimer);
    Sun->SetVisibility(false);Sun->SetIntensity(0);Atmosphere->SetVisibility(false);Sky->SetVisibility(false);
    if(Exposure){Exposure->Destroy();Exposure=nullptr;}
    for(const auto& S:WeatherSkies)if(S.Mesh.IsValid()&&S.Original.IsValid())S.Mesh->SetMaterial(S.Slot,S.Original.Get());WeatherSkies.Empty();
    for(const auto& S:SkyParameters)if(auto* M=S.Material.Get()){for(const auto& P:S.Colors)M->SetVectorParameterValue(P.Key,P.Value);for(const auto& P:S.Brightness)M->SetScalarParameterValue(P.Key,P.Value);}
    for(const auto& S:Meshes)if(auto* C=S.Component.Get())if(auto* M=S.Original.Get())C->SetStaticMesh(M);
    for(const auto& State:Actors)if(auto* A=State.Actor.Get()){A->SetActorHiddenInGame(State.Hidden);A->SetActorTickEnabled(State.Tick);}
    for(const auto& State:Components)if(auto* C=State.Component.Get()){if(State.AmbientIntensity>=0)if(auto* L=Cast<ULightComponent>(C))L->SetIntensity(State.AmbientIntensity);if(auto* D=Cast<UDirectionalLightComponent>(C))D->SetAtmosphereSunLight(State.AtmosphereSun);if(auto* Fog=Cast<UExponentialHeightFogComponent>(C)){Fog->SetFogDensity(State.FogDensity);Fog->SetSecondFogDensity(State.SecondFogDensity);Fog->SetFogMaxOpacity(State.FogMaxOpacity);}C->SetVisibility(State.Visible,false);}
    bActive=false;SetActorTickEnabled(false);OriginalMeshReferences.Empty();
    if(bChangedSkyTimeSlice)if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice")))if(CVar->GetInt()==0)CVar->SetWithCurrentPriority(OriginalSkyTimeSlice);
    bChangedSkyTimeSlice=false;
}
bool ACourseworkDaylight::CheckRestored()const
{
    if(bActive||IsActorTickEnabled()||Sun->IsVisible()||Sky->IsVisible()||Atmosphere->IsVisible()||Exposure)return false;
    for(const auto& S:SkyParameters)if(auto* M=S.Material.Get())
    {for(const auto& P:S.Colors){FLinearColor Color;if(!M->GetVectorParameterValue(FMaterialParameterInfo(P.Key),Color)||!Color.Equals(P.Value,.000001f))return false;}for(const auto& P:S.Brightness){float Value=0;if(!M->GetScalarParameterValue(FMaterialParameterInfo(P.Key),Value)||!FMath::IsNearlyEqual(Value,P.Value,.000001f))return false;}}
    if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice")))if(CVar->GetInt()!=OriginalSkyTimeSlice)return false;
    for(const auto& S:Meshes)if(auto* C=S.Component.Get())if(C->GetStaticMesh()!=S.Original.Get())return false;
    for(const auto& S:Actors)if(auto* A=S.Actor.Get())if(A->IsHidden()!=S.Hidden||A->IsActorTickEnabled()!=S.Tick)return false;
    for(const auto& S:Components)if(auto* C=S.Component.Get())
    {
        if(C->IsVisible()!=S.Visible)return false;
        if(auto* D=Cast<UDirectionalLightComponent>(C))if(D->bAtmosphereSunLight!=S.AtmosphereSun)return false;
        if(auto* Fog=Cast<UExponentialHeightFogComponent>(C))if(!FMath::IsNearlyEqual(Fog->FogDensity,S.FogDensity)||!FMath::IsNearlyEqual(Fog->SecondFogData.FogDensity,S.SecondFogDensity)||!FMath::IsNearlyEqual(Fog->FogMaxOpacity,S.FogMaxOpacity))return false;
        if(S.AmbientIntensity>=0)if(auto* L=Cast<ULightComponent>(C))if(!FMath::IsNearlyEqual(L->Intensity,S.AmbientIntensity))return false;
    }
    return true;
}
float ACourseworkDaylight::GetSunIntensity()const{return Sun->Intensity;}
FRotator ACourseworkDaylight::GetSunRotation()const{return Sun->GetComponentRotation();}
void ACourseworkDaylight::SetWeather(float Cover,float Direct,float Diffuse,float Light)
{WeatherCover=FMath::Clamp(Cover,.1f,.92f);WeatherDirect=FMath::Clamp(Direct,.01f,1.f);WeatherDiffuse=Diffuse;WeatherLight=Light;}
bool ACourseworkDaylight::IsSunShadowed()const{return Sun->CastShadows&&Sun->DynamicShadowDistanceMovableLight>=5000;}
void ACourseworkDaylight::EndPlay(const EEndPlayReason::Type Reason){Restore();Super::EndPlay(Reason);}

float ACourseworkDaylight::GetSkyIntensity()const{return Sky->Intensity;}
bool ACourseworkDaylight::UsesRealtimeSky()const{return Sky->IsRealTimeCaptureEnabled();}

float ACourseworkDaylight::GetRenderedElevation()const
{const float Height=float(-GetSunRotation().Vector().Z);return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Height,0.f,1.f)));}
