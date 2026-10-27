#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourseworkRain.generated.h"

// Local wind-aligned precipitation. Each trajectory ends at its first surface;
// roof-covered rooms never receive a camera-attached screen-space rain layer.
UCLASS()
class HOUSECOURSEWORK_API ACourseworkRain : public AActor
{
    GENERATED_BODY()
public:
    ACourseworkRain();
    void Update(float Amount,float DeltaSeconds,float DaylightFactor=1.f);
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    int32 GetVisibleDropCount()const{return VisibleDrops;}
    bool IsSheltered()const{return Sheltered;}
    float GetAudioGain()const{return AudioGain;}
    bool CheckRoofClipping()const;
    double GetAnimationSeconds()const{return Seconds;}
    float GetWetness()const{return Wetness;}
    int32 GetWetSurfaceCount()const{return WetSurfaces.Num();}
    int32 GetSplashCount()const{return VisibleSplashes;}
    bool CheckWetSurfaces()const;
    bool CheckSplashes()const;
    bool IsPolished()const{return !Legacy;}
    double GetEffectMilliseconds()const{return EffectMilliseconds;}
private:
    struct FColumn{FVector Position,Impact,Normal=FVector::UpVector;float Floor=-500;bool Hard=false;};
    struct FSurfaceSplash{FVector Position,Normal=FVector::UpVector;float Age=1,Life=.26f;bool Active=false;};
    struct FWetParameter{FName Name;float Original;bool Roughness;};
    struct FWetSurface{TWeakObjectPtr<class UStaticMeshComponent> Mesh;int32 Slot=0,MaterialIndex=0;TArray<FWetParameter> Parameters;bool Road=false;};
    TArray<FColumn> Columns;
    TArray<FSurfaceSplash> SplashPool;
    TArray<FWetSurface> WetSurfaces;
    UPROPERTY() TArray<TObjectPtr<class UMaterialInterface>> OriginalSurfaces;
    UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> WetMaterials;
    TArray<uint8> PCM;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Streaks;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Material;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> SplashMesh;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> SplashMaterial;
    UPROPERTY() TObjectPtr<class UAudioComponent> Audio;
    UPROPERTY() TObjectPtr<class USoundWaveProcedural> Wave;
    FVector Center=FVector::ZeroVector;
    double Seconds=0,EffectMilliseconds=0;float AudioGain=0,ShelterElapsed=1,Wetness=0,SurfaceElapsed=0;
    int32 VisibleDrops=0,VisibleSplashes=0;bool Sheltered=false,Ready=false,Legacy=false;
    FRandomStream VisualRandom{73162};
    void Initialize();
    void BuildColumns(FVector Position);
    static bool IsHardMaterial(const class UMaterialInterface* Material);
    void UpdateSplashes(float Amount,float Dt,FVector View);
    void RegisterWetSurfaces();
    void UpdateWetSurfaces(float Amount,float Dt);
    void RestoreDrySurfaces();
};
