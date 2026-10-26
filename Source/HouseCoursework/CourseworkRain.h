#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourseworkRain.generated.h"

// Local vertical precipitation. Each column ends at its first opaque surface;
// roof-covered rooms never receive a camera-attached screen-space rain layer.
UCLASS()
class HOUSECOURSEWORK_API ACourseworkRain : public AActor
{
    GENERATED_BODY()
public:
    ACourseworkRain();
    void Update(float Amount,float DeltaSeconds);
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    int32 GetVisibleDropCount()const{return VisibleDrops;}
    bool IsSheltered()const{return Sheltered;}
    float GetAudioGain()const{return AudioGain;}
    bool CheckRoofClipping()const;
    double GetAnimationSeconds()const{return Seconds;}
private:
    struct FColumn{FVector Position;float Floor=-500;};
    TArray<FColumn> Columns;
    TArray<uint8> PCM;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Streaks;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Material;
    UPROPERTY() TObjectPtr<class UAudioComponent> Audio;
    UPROPERTY() TObjectPtr<class USoundWaveProcedural> Wave;
    FVector Center=FVector::ZeroVector;
    double Seconds=0;float AudioGain=0,ShelterElapsed=1;
    int32 VisibleDrops=0;bool Sheltered=false,Ready=false;
    void Initialize();
    void BuildColumns(FVector Position);
};
