#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"
#include "CourseworkDaylight.generated.h"

// Owns only the temporary environmental comparison; household lighting is untouched.
UCLASS()
class HOUSECOURSEWORK_API ACourseworkDaylight : public AActor
{
    GENERATED_BODY()
public:
    ACourseworkDaylight();
    bool Apply(float Hour);
    void Restore();
    bool IsActive()const{return bActive;}
    float GetHour()const{return Hour;}
    float GetElevation()const{return Elevation;}
    bool CheckRestored()const;
    float GetSunIntensity()const;
    float GetSkyIntensity()const;
    float GetRenderedHour()const{return RenderedHour;}
    bool UsesRealtimeSky()const;
    FRotator GetSunRotation()const;
    bool IsSunShadowed()const;
    virtual void Tick(float DeltaSeconds)override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
private:
    struct FActorState {TWeakObjectPtr<AActor> Actor;bool Hidden;bool Tick;};
    struct FComponentState {TWeakObjectPtr<USceneComponent> Component;bool Visible;float AmbientIntensity=-1;};
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
