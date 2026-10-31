#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourseworkVentilation.generated.h"
class UCanvas;
class APlayerController;
class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UBoxComponent;
class UAudioComponent;
class USoundWave;
class UMaterialInstanceDynamic;
class UMaterialInterface;

// Runtime teaching model: prescribed exterior wind + open-window/door connectivity.
// Air exchange levels and paths are illustrative, not a CFD/ACH calculation.
UCLASS()
class HOUSECOURSEWORK_API ACourseworkVentilation : public AActor
{
    GENERATED_BODY()
public:
    ACourseworkVentilation();
    virtual void BeginPlay()override;
    virtual void Tick(float DeltaSeconds)override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    int32 FindWindow(const UPrimitiveComponent* Component)const;
    bool ToggleWindow(int32 Index);
    void SetRoomWindows(int32 Room,bool Open);
    void CloseAll();
    int32 GetWindowCount(int32 Room=INDEX_NONE)const;
    int32 GetOpenCount(int32 Room=INDEX_NONE)const;
    bool IsOpen(int32 Index)const;
    int32 GetWindowRoom(int32 Index)const;
    FString GetWindowLabel(int32 Index)const;
    FVector GetWindowPoint(int32 Index)const;
    FVector GetWindowInward(int32 Index)const;
    float GetRoomStrength(int32 Room)const;
    bool HasCrossFlow(int32 Room)const;
    FString GetRoomStatus(int32 Room)const;
    void SetDisplay(bool Enabled);
    bool IsDisplayed()const{return bDisplay&&!bSuspended;}
    void Suspend(bool Value);
    bool IsReady()const{return Windows.Num()>0&&WindSound;}
    int32 GetPathCount()const{return Paths.Num();}
    int32 GetCrossPathCount()const;
    bool CheckPathsClear()const;
    bool CheckFlowDirections()const;
    float GetPressureResidual()const{return PressureResidual;}
    int32 GetOpenExteriorDoorCount()const;
    bool CheckAirflowDrawing()const;
    FVector GetFlowFocus()const;
    FString GetRenderDiagnostics()const;
    FVector GetResponseSample()const;
    bool CheckPolishState()const;
    FIntPoint GetTrailTransitionCounts()const{return {PathReuseCount,PathRetireCount};}
    void DrawAirflow(UCanvas* Canvas,APlayerController* Controller);
    bool CheckClosedPoses()const;
    bool CheckWindowGeometry(int32 Index,bool Open)const;
    int32 GetCurtainCount()const{return Curtains.Num();}
    bool CheckCurtains(bool Moving)const;
    bool CheckOriginalsRestored()const;
    void RestoreOriginals();
    int32 GetPlantCount()const{return Plants.Num();}
    float GetMaxLeafMotion()const;
    bool CheckPlantTransforms()const;
    bool CheckPlantMaterialsRestored()const;
private:
    struct FWindow
    {
        TWeakObjectPtr<USceneComponent> Hinge;
        TWeakObjectPtr<UStaticMeshComponent> Sash,FixedFrame;
        FTransform FixedTransform;
        TWeakObjectPtr<UBoxComponent> Proxy;
        TWeakObjectPtr<UAudioComponent> Audio;
        FVector Center,Inward,Travel,Axis,Extent;
        float Angle=0,Area=0;
        FString LocationLabel;
        FTransform Closed;
        int32 Room=0;
        float Target=0,Amount=0,Flow=0,Exchange=0,AirResponse=0;
    };
    struct FOriginal {TWeakObjectPtr<UStaticMeshComponent> Mesh;bool Visible,Hidden;ECollisionEnabled::Type Collision;};
    struct FDoor {TWeakObjectPtr<UStaticMeshComponent> Mesh;FTransform Closed;FVector Portal;int32 A,B;float Amount=0;};
    struct FFlowPath {TArray<FVector> Points,Offsets;int32 Room=0;bool Cross=false;float Length=0,Born=0,InletPressure=0,OutletPressure=0,Flux=0,Motion=0,Speed=24,Retired=-1;};
    struct FPlant {TWeakObjectPtr<UStaticMeshComponent> Mesh;TWeakObjectPtr<UMaterialInterface> Original;TWeakObjectPtr<UMaterialInstanceDynamic> Material;FTransform Transform;int32 Slot=0;float Target=0,Amount=0;FVector Direction=FVector::ZeroVector,TargetDirection=FVector::ZeroVector;};
    TArray<FPlant> Plants;
    TArray<FPlant> Curtains;
    TMap<UStaticMeshComponent*,ECollisionResponse> CurtainCollision;
    TMap<UStaticMeshComponent*,bool> CurtainDefaultCollision;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> CurtainAssets;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> CurtainOriginals;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> CurtainInstances;
    UPROPERTY() TObjectPtr<UMaterialInterface> LeafMaterial;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> LeafInstances;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> OriginalLeafMaterials;
    TArray<FWindow> Windows;
    TArray<FOriginal> Originals;
    TArray<FDoor> Doors;
    TArray<FFlowPath> Paths;
    TArray<FFlowPath> RetiringPaths;
    TArray<float> RoomTarget,RoomStrength;
    TArray<bool> CrossRooms;
    TMap<const UPrimitiveComponent*,int32> WindowLookup;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> PartAssets;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> OriginalAssets;
    UPROPERTY() TObjectPtr<USoundWave> WindSound;
    float RebuildElapsed=0,VisualWeight=0;
    float PressureResidual=0;
    int32 LastDrawSegments=0;
    int32 PathReuseCount=0,PathRetireCount=0;
    uint64 LastState=MAX_uint64;
    bool bDisplay=false,bSuspended=false,bRestored=false;
    void RebuildFlow();
    bool SegmentClear(const FVector& A,const FVector& B)const;
    TArray<FVector> FindAirPath(FVector Start,FVector End)const;
};
