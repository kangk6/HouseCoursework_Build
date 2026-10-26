#pragma once
#include "CoreMinimal.h"
#include "CourseworkRooms.h"
#include "CourseworkClock.h"
#include "CourseworkSolarModel.h"
#include "CourseworkWeatherModel.h"
#include "GameFramework/HUD.h"
#include "CourseworkHUD.generated.h"
class UCourseworkOverlay;
class UStaticMeshComponent;
class ACharacter;

UCLASS()
class HOUSECOURSEWORK_API ACourseworkHUD : public AHUD
{
    GENERATED_BODY()
public:
    ACourseworkHUD();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void DrawHUD()override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetPanel(int32 NewPanel);
    int32 GetPanel() const { return PanelMode; }
    void GoToLocation(int32 Index);
    void ApplySensitivity(float Value);
    void ApplySpeed(float Value);
    float GetSensitivity() const { return Sensitivity; }
    float GetSpeedScale() const { return SpeedScale; }
    void QuitApplication();
    void SetRoomLighting(int32 Room,bool Enabled,float Brightness);
    void ResetRoomLighting(int32 Room);
    bool IsRoomLightOn(int32 Room) const;
    float GetRoomBrightness(int32 Room) const;
    float GetRoomRestoreBrightness(int32 Room)const{return RoomBrightness[CourseworkRooms::Clamp(Room)];}
    int32 GetRoomLightCount(int32 Room) const;
    void SetAllRoomLighting(bool Enabled);
    void ResetAllRoomLighting();
    FString GetLightingSaveStatus() const;
    // Temporary household scenes; automatic changes never replace saved settings.
    bool SetSmartLightingScene(int32 Scene);
    int32 GetSmartLightingScene()const{return SmartLightingScene;}
    FString GetSmartLightingStatus()const;
    FString GetSmartLightingDetail()const;
    int32 GetSmartManualCount()const{return SmartFixtureOverrides.Num()+SmartManualRooms.Num();}
    float GetSmartFixtureLevel(FName Actor)const;
    float GetSmartRoomLevel(int32 Room)const;
    bool SetSmartFixtureManual(FName Actor,float Level);

    // Educational lighting circuits: watts are configured per zone, not renderer lights.
    float GetRoomRatedWatts(int32 Room)const;
    float GetRoomWatts(int32 Room)const;
    float GetTotalWatts()const;
    float GetBaselineWatts()const;
    float GetEnergyHours()const{return EnergyHours;}
    void SetEnergyHours(float Hours);
    float GetEstimatedKWh(bool Baseline=false)const;
    bool ApplyEnergyScenario(bool Eco);
    bool RestoreEnergyComparison();
    bool IsEnergyComparisonActive()const{return bEnergyComparisonActive;}
    FString GetEnergyScenarioName()const;
    bool IsEnergyMapLayer()const{return bEnergyMapLayer&&!IsSafetyDrillRunning();}
    void ToggleEnergyMapLayer();
    void OpenEnergyMap();
    const FCourseworkSolarFlow& GetSolarFlow()const{return SolarFlow;}
    const FCourseworkSolarMeter& GetSolarMeter()const{return SolarMeter;}
    double GetSolarRatedWatts()const{return SolarPanels.Num()*360.;}
    int32 GetSolarPanelCount()const{return SolarPanels.Num();}
    bool IsSolarReady()const{return SolarPanels.Num()==6;}
    bool IsEnergySuspended()const;
    float GetApplianceWatts()const;
    float GetHouseholdWatts()const{return GetTotalWatts()+GetApplianceWatts();}
    float GetRoomDemandWatts(int32 Room)const;
    int32 GetApplianceCount()const{return Appliances.Num();}
    FString GetApplianceName(int32 Index)const;
    FString GetApplianceStatus(int32 Index)const;
    int32 GetApplianceRoom(int32 Index)const;
    float GetAppliancePower(int32 Index)const;
    bool IsApplianceOn(int32 Index)const;
    bool ToggleAppliance(int32 Index);
    int32 GetEnergyPage()const{return EnergyPage;}
    void SelectEnergyPage(int32 Page){EnergyPage=FMath::Clamp(Page,0,1);}
    void ChooseWeather(int32 Kind);
    void ToggleAutomaticWeather();
    bool IsAutomaticWeather()const{return Weather.Automatic;}
    int32 GetWeatherKind()const{return Weather.Kind;}
    FString GetWeatherName()const;
    float GetWeatherCloudCover()const{return Weather.Cover;}
    float GetWeatherTransition()const{return float(Weather.BlendAge/10);}
    float GetWeatherDaylight()const{return Weather.Daylight();}


    int32 GetComfortPage()const{return ComfortPage;}
    void SelectComfortPage(int32 Page){if(ComfortPage!=FMath::Clamp(Page,0,2)){ComfortPage=FMath::Clamp(Page,0,2);++TimeEditorRevision;}}
    int32 GetTimeEditorRevision()const{return TimeEditorRevision;}
    bool IsStartupCovered()const{return bStartupCover;}
    int32 GetRoomWindowCount(int32 Room)const;
    int32 GetRoomOpenWindowCount(int32 Room)const;
    void SetRoomWindows(int32 Room,bool Open);
    void CloseAllWindows();
    float GetRoomVentilation(int32 Room)const;
    FString GetVentilationStatus(int32 Room)const;
    bool HasRoomCrossFlow(int32 Room)const;
    bool IsAirflowDisplayed()const;
    void ToggleAirflowDisplay();
    void ObserveVentilation(int32 Room);

    bool SetDaylightHour(float Hour);
    void RestoreDaylight();
    bool IsDaylightActive()const;
    float GetDaylightHour()const;
    float GetDaylightElevation()const;
    bool NeedsNightLighting()const;
    void ObserveDaylight();
    bool SetTimeFlowing(bool Running);
    bool SetTimeDayMinutes(float Minutes);
    bool IsTimeFlowing()const{return Clock.bRunning;}
    float GetTimeDayMinutes()const{return Clock.DayMinutes;}
    int64 GetTimeDay()const{return Clock.Day;}
    bool IsTimeSuspended()const;
    FString GetTimeStatus()const;
    void InitializeTimeOfDay();

    // Runtime-only teaching simulation. Never persisted with household settings.
    void TriggerKitchenAlarm();
    void HandleKitchenAlarm();
    void ResetKitchenAlarm(bool ResumeEnvironment=true);
    int32 GetSafetyState() const { return SafetyState; }
    bool IsSafetyActive() const { return SafetyState>=2; }
    bool IsSafetyDrillRunning() const { return SafetyState!=0; }
    bool IsSafetyReady() const;
    FString GetSafetyStatus() const;
    float GetSafetyProgress() const;
    FVector GetAlarmLocation() const { return AlarmLocation; }
    float GetSmokeReading() const { return SmokeReading; }
    float GetAlarmElapsed() const { return AlarmElapsed; }
    bool HasPlayerGathered() const { return bPlayerGathered; }
    bool HasResidentGathered() const { return ResidentEvacuationState==3; }
    FString GetResidentEvacuationStatus() const;
    FString GetLastEvacuationResult() const { return LastEvacuationResult; }
    FVector GetAssemblyPoint() const { return AssemblyPoint; }
    FVector GetResidentPosition() const;

    void ToggleMap();
    int32 GetPlayerFloor() const;
    int32 GetMapFloor() const { return SelectedMapFloor; }
    void SelectMapFloor(int32 Floor);
private:
    UPROPERTY() TObjectPtr<UCourseworkOverlay> Overlay;
    TMap<TWeakObjectPtr<USceneComponent>, FTransform> ClosedTransforms;
    TMap<TWeakObjectPtr<USceneComponent>, uint32> FurnitureActionRevisions;
    TWeakObjectPtr<AActor> TargetActor;
    TWeakObjectPtr<UStaticMeshComponent> TargetComponent;
    FString TargetLabel;
    int32 TargetLightRoom=INDEX_NONE,HeldLightRoom=INDEX_NONE,WheelBindingIndex=INDEX_NONE;
    struct FLightFixture {TWeakObjectPtr<UStaticMeshComponent> Mesh;TWeakObjectPtr<class UBoxComponent> Proxy;int32 Room;};
    TArray<FLightFixture> LightFixtures;
    TMap<TWeakObjectPtr<class UPrimitiveComponent>,int32> LightFixtureRooms;
    TWeakObjectPtr<class UPrimitiveComponent> TargetLightFixture,HeldLightFixture;
    FVector TargetLightAimPoint=FVector::ZeroVector,HeldLightAimPoint=FVector::ZeroVector;
    bool bLightKeyHeld=false,bLightDimming=false;
    double LightKeyPressedAt=0;
    void RegisterLightFixture(UStaticMeshComponent* Mesh,int32 Room);
    bool SetLightTarget(const FHitResult& Hit);
    void BeginInteract();
    void EndInteract();
    void TickLightInteraction();
    void CancelLightInteraction();
    bool IsHeldLightValid() const;
    void AdjustHeldLight(float Delta);
    void RefreshLightInteraction();
    void TickFixtureAcceptance();
    void TickEnergyAcceptance();
    void TickSolarAcceptance();
    void TickHouseholdAcceptance();
    void TickInteractionAcceptance();
    void TickLinkageAcceptance();
    void TickRainAcceptance();
    void InitializeHousehold();
    void TickHousehold(float DeltaSeconds);
    void RestoreHousehold();
    bool SetApplianceTarget(const FHitResult& Hit);
    struct FAppliance {FString Name;int32 Room=0;bool On=false,Fridge=false;float Watts=0;TWeakObjectPtr<UStaticMeshComponent> Mesh;TWeakObjectPtr<class UMaterialInstanceDynamic> Screen;TWeakObjectPtr<class UMaterialInterface> Original;int32 Slot=INDEX_NONE;TWeakObjectPtr<UStaticMeshComponent> Overlay;};
    TArray<FAppliance> Appliances;
    TMap<TWeakObjectPtr<class UPrimitiveComponent>,int32> ApplianceTargets;
    UPROPERTY() TObjectPtr<class UTexture2D> DesktopTexture;
    int32 TargetAppliance=INDEX_NONE,EnergyPage=0;
    double ApplianceSeconds=0;
    FCourseworkWeather Weather;
    UPROPERTY() TObjectPtr<class ACourseworkRain> RainEffect;
    void InitializeSolar();
    void TickSolar(float DeltaSeconds);
    FCourseworkSolarFlow SolarFlow;
    FCourseworkSolarMeter SolarMeter;
    struct FSolarPanel{FVector Center,Normal;FQuat Rotation;};
    TArray<FSolarPanel> SolarPanels;
    UPROPERTY() TObjectPtr<AActor> SolarInstallation;
    void TickDaylightAcceptance();
    void TickEnvironmentRegression();
    void TickDaylightSweep();
    void TickTimeOfDay(float DeltaSeconds);
    void TickClockAcceptance();
    void TickDialAcceptance();
    void TickStartupAcceptance();
    void TickDayCycleAcceptance();
    void TickDrillRestoreAcceptance();
    FCourseworkClock Clock;
    int32 TimeEditorRevision=0;
    bool bStartupCover=false;
    double StartupRevealAt=0;
    UPROPERTY() TObjectPtr<class ACourseworkDaylight> Daylight;
    UPROPERTY() TObjectPtr<class ACourseworkVentilation> Ventilation;
    int32 ComfortPage=0,TargetWindow=INDEX_NONE;
    bool SetWindowTarget(const FHitResult& Hit);
    void TickVentilationAcceptance();
    void TickVentilationPerformance();
    float EnergyHours=4.f;
    bool bEnergyComparisonActive=false,bApplyingEnergyScene=false,bEnergyMapLayer=false;
    int32 EnergyScenario=0;
    TArray<bool> EnergySavedOn;
    TArray<float> EnergySavedBrightness;
    int32 PanelMode = 0;
    int32 SelectedMapFloor = 0;
    mutable int32 PlayerFloor = 0;
    float Sensitivity = 1.f;
    float SpeedScale = 1.f;
    float OriginalMouseSensitivity = 0.07f;
    float OriginalWalkSpeed = 300.f;
    bool bMenuInputBlocked = false;
    double NextInteractionTime = 0;
    double ToastUntil = 0;
    float HintElapsed = 0;
    FString TestMode;
    int32 TestCase = -1;
    int32 TestWaypoint = 0;
    double TestStartTime = 0;
    double TestCaseStart = 0;
    float TestMaxZ = 0;
    TArray<FString> TestResults;
    TWeakObjectPtr<AActor> TestDoorActor;
    TWeakObjectPtr<UStaticMeshComponent> TestDoorComponent;
    FVector TestDoorCenter = FVector::ZeroVector;
    FVector TestDoorNormal = FVector::ZeroVector;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> AcceptanceFurniture;
    TArray<float> AcceptanceFrameTimes;
    int32 AcceptanceCandidate = 0;
    int32 AcceptancePhase = 0;
    bool bAcceptanceOpened = false;
    bool bAcceptanceKeyDown = false;
    bool bAcceptanceAimReady = false;
    double AcceptanceKeyReleaseAt = 0;
    double AcceptanceLastFrame = 0;
    double AcceptancePauseUntil = 0;
    float AcceptanceFurnitureFloorZ = 152.5f;
    float AcceptanceAimHeight = 0;
    int32 AcceptanceCycles = 0;
    int32 AcceptanceRequiredCycles = 1;
    TMap<TWeakObjectPtr<USceneComponent>,FTransform> AcceptanceDecorTransforms;
    bool bAcceptanceDecorMoved = false;
    FString AcceptancePendingShot;
    bool AimAcceptanceFurniture(UStaticMeshComponent* Component);
    UStaticMeshComponent* ResolveFurnitureTarget(UStaticMeshComponent* Component) const;
    bool IsFurnitureOpen(UStaticMeshComponent* Component) const;
    void PressAcceptanceE();
    void TickComprehensiveAcceptance();
    void InitializeRoomLighting();
    void TickLightingAcceptance();
    void TickControlAcceptance();
    void TickTransitionAcceptance();
    void LoadRoomLighting();
    void SaveRoomLighting();
    void QueueLightingSave();
    void TickLightingStateAcceptance();
    void InitializeSafety();
    void TickSafety(float DeltaSeconds);
    void InitializeEvacuation();
    void BeginResidentEvacuation();
    void TickEvacuation(float DeltaSeconds);
    void RestoreResident();
    void TickEvacuationAcceptance();
    UPROPERTY() TObjectPtr<class UParticleSystem> SafetySmokeTemplate;
    UPROPERTY() TObjectPtr<class UParticleSystem> SafetyFireTemplate;
    UPROPERTY() TObjectPtr<class UParticleSystemComponent> SafetyFire;
    UPROPERTY() TObjectPtr<class UPointLightComponent> SafetyFireLight;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> SafetySmokeMaterial;
    UPROPERTY() TObjectPtr<class UBlendSpace1D> ResidentLocomotion;
    UPROPERTY() TObjectPtr<class UParticleSystemComponent> SafetySmoke;
    UPROPERTY() TObjectPtr<class USoundWaveProcedural> SafetySirenWave;
    UPROPERTY() TObjectPtr<class UAudioComponent> SafetySiren;
    int32 SafetyState=0;
    int32 SafetySavedSmartScene=0;
    TMap<FName,float> SafetySavedFixtureOverrides;
    TMap<FName,float> SafetySavedFixtureRestore;
    TSet<int32> SafetySavedManualRooms;
    UPROPERTY() TObjectPtr<class UAnimSequence> ResidentRunAnimation;
    UPROPERTY() TObjectPtr<class UAnimSequence> ResidentIdleAnimation;
    UPROPERTY() TObjectPtr<ACharacter> EvacuatingResident;
    TWeakObjectPtr<AActor> OriginalResident;
    TWeakObjectPtr<UStaticMeshComponent> SafetyEntryDoor;
    UPROPERTY() TObjectPtr<AActor> AssemblyMarker;
    UPROPERTY() TObjectPtr<AActor> SmokeSensor;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> SensorSignal;
    FTransform OriginalResidentTransform;
    FVector AssemblyPoint=FVector(-2205,8550,70);
    TArray<FVector> ResidentRoute;
    TArray<FString> EvacuationSamples;
    TArray<FVector> EvacuationPlayerRoute;
    int32 ResidentRouteIndex=0,ResidentEvacuationState=0,EvacuationPlayerIndex=0;
    float SmokeReading=0,AlarmElapsed=0,ResidentWaitTime=0,ResidentStuckTime=0,SampleElapsed=0;
    float PlayerArrivalTime=-1,ResidentArrivalTime=-1;
    FVector ResidentLastPosition=FVector::ZeroVector;
    bool bPlayerGathered=false,bResidentRunning=false,bOriginalResidentTick=true;
    bool bEntryInitiallyOpen=false,bEntryOpenedByResident=false;
    uint32 EntrySafetyRevision=0;
    float EntryWaitTime=0;
    FString LastEvacuationResult;
    float SafetySmokeLevel=0;
    FVector AlarmLocation=FVector(-1668,9400,155);
    TArray<uint8> SafetySirenPCM;
    FString LightingSavePath;
    FTimerHandle LightingSaveTimer;
    bool bLightingReady=false,bLightingPersistenceEnabled=false,bLightingDirty=false,bLightingSaveFailed=false,bApplyingLightingSave=false;
    struct FControlledLight { TWeakObjectPtr<class ULightComponent> Component; float Intensity; int32 Room; };
    struct FControlledGlow { TWeakObjectPtr<class UMaterialInstanceDynamic> Material; FName Parameter; float Strength; int32 Room; TWeakObjectPtr<UStaticMeshComponent> Mesh; int32 Slot; };
    TArray<FControlledLight> ControlledLights;
    TArray<FControlledGlow> ControlledGlows;
    TArray<bool> RoomLightOn = []{TArray<bool> V;V.Init(true,CourseworkRooms::Count);return V;}();
    TArray<float> RoomBrightness = []{TArray<float> V;V.Init(1.f,CourseworkRooms::Count);return V;}();
    struct FSmartFixture {int32 Room=0;float Level=0,Target=0,Restore=1;};
    TMap<FName,FSmartFixture> SmartFixtures;
    TMap<FName,float> SmartFixtureOverrides;
    TSet<int32> SmartManualRooms;
    TMap<FName,double> SmartTrailUntil;
    TMap<TWeakObjectPtr<class ULightComponent>,FLinearColor> SmartOriginalColors;
    TArray<TWeakObjectPtr<class UPointLightComponent>> SmartStairFills;
    TArray<float> SmartStairLevels;
    struct FSmartPathFill {TWeakObjectPtr<class UPointLightComponent> Light;int32 Room=8;float Level=0;double Until=0;};
    TArray<FSmartPathFill> SmartPathFills;
    TMap<int32,double> SmartRoomUntil;
    int32 SmartLightingScene=0,SmartPlayerZone=INDEX_NONE;
    bool bSmartReadingOccupied=false;
    double SmartReadingUntil=0;
    void TickSmartLighting(float DeltaSeconds);
    void ApplySmartLighting();
    int32 SmartZoneAt(FVector Position)const;
    float GetSmartTargetLevel(FName Actor)const;
    void TickSmartLightingAcceptance();

    void ToggleControls();
    void TogglePause();
    void Interact();
    void UpdateInteraction();
    void ShowToast(const FString& Text);
    bool InvokeFurniture(AActor* Actor, UStaticMeshComponent* Component);
    void TickAcceptanceTest();
};
