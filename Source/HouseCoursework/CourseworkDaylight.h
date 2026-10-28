#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"
#include "CourseworkDaylight.generated.h"

// Renders the shared 24-hour clock; household lighting is untouched.
UCLASS()
class HOUSECOURSEWORK_API ACourseworkDaylight : public AActor
{
    GENERATED_BODY()
public:
    ACourseworkDaylight();
    bool Apply(float Hour);
    bool Snap(float Hour);
    void Restore();
    bool IsActive()const{return bActive;}
    float GetHour()const{return Hour;}
    float GetElevation()const{return Elevation;}
    float GetRenderedElevation()const;
    bool CheckRestored()const;
    float GetSunIntensity()const;
    float GetSkyIntensity()const;
    float GetRenderedHour()const{return RenderedHour;}
    bool UsesRealtimeSky()const;
    FRotator GetSunRotation()const;
    void SetWeather(float Cover,float Direct,float Diffuse,float Light);
    bool IsSunShadowed()const;
    virtual void Tick(float DeltaSeconds)override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
private:
    struct FActorState {TWeakObjectPtr<AActor> Actor;bool Hidden;bool Tick;};
    struct FComponentState {TWeakObjectPtr<USceneComponent> Component;bool Visible;float AmbientIntensity=-1,FogDensity=-1,SecondFogDensity=-1;bool AtmosphereSun=false;float FogMaxOpacity=-1;};
    struct FSkyParameters {TWeakObjectPtr<class UMaterialInstanceDynamic> Material;TArray<TPair<FName,FLinearColor>> Colors;TArray<TPair<FName,float>> Brightness;};
    TArray<FSkyParameters> SkyParameters;
    struct FWeatherSky{TWeakObjectPtr<class UStaticMeshComponent> Mesh;int32 Slot=0;TWeakObjectPtr<class UMaterialInterface> Original;TWeakObjectPtr<class UMaterialInstanceDynamic> Material;TArray<TPair<FName,FLinearColor>> Colors;TArray<TPair<FName,float>> Brightness;FLinearColor Direction;};
    TArray<FWeatherSky> WeatherSkies;
    float WeatherCover=.1f,WeatherDirect=1,WeatherDiffuse=1,WeatherLight=1;
    float AuthoredBloom=0,AuthoredBloomThreshold=0,AuthoredLensFlare=0;
    struct FMeshState {TWeakObjectPtr<class UStaticMeshComponent> Component;TWeakObjectPtr<class UStaticMesh> Original;};
    TArray<FMeshState> Meshes;
    TArray<TStrongObjectPtr<UObject>> OriginalMeshReferences;
    UPROPERTY() TArray<TObjectPtr<class UStaticMesh>> ComparisonMeshes;
    TArray<FActorState> Actors;
    TArray<FComponentState> Components;
    UPROPERTY() TObjectPtr<class UDirectionalLightComponent> Sun;
    UPROPERTY() TObjectPtr<class USkyAtmosphereComponent> Atmosphere;
    UPROPERTY() TObjectPtr<class USkyLightComponent> Sky;
    UPROPERTY() TObjectPtr<class APostProcessVolume> Exposure;
    bool bActive=false;
    float Hour=12,Elevation=0;
    float RenderedHour=12,PreviewWeight=0;
    int32 OriginalSkyTimeSlice=1;
    bool bChangedSkyTimeSlice=false;
    FTimerHandle AtmosphereTimer;
    void UpdateLighting();
};
