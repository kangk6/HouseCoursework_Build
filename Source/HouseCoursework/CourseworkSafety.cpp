#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "CourseworkVentilation.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void ACourseworkHUD::InitializeSafety()
{
    // Locate the actual oven rather than emitting smoke at a camera position.
    for(TActorIterator<AActor> It(GetWorld());It;++It)
        if(It->GetClass()->GetName().Contains(TEXT("Oven")) && It->GetActorLocation().Z<300)
        {
            FVector Origin,Extent;It->GetActorBounds(false,Origin,Extent);
            AlarmLocation=FVector(Origin.X,Origin.Y,Origin.Z+Extent.Z+4);
            break;
        }
    if(SafetySmokeTemplate)
    {
        // This separately cooked indoor preset leaves StarterContent unchanged.
        // Clear the cookware and the hood face rather than emitting inside them.
        SafetySmoke=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),SafetySmokeTemplate,AlarmLocation+FVector(-35,0,25),FRotator::ZeroRotator,FVector(.75f),false,EPSCPoolMethod::None,false);
        if(SafetySmoke)
        {
            if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/Safety/M_KitchenSmoke.M_KitchenSmoke"))){SafetySmokeMaterial=UMaterialInstanceDynamic::Create(Material,this);SafetySmoke->SetMaterial(0,SafetySmokeMaterial);}
            SafetySmoke->SetComponentTickEnabled(false);
        }
    }
    if(SafetyFireTemplate)
    {
        SafetyFire=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),SafetyFireTemplate,AlarmLocation+FVector(-35,0,3),FRotator::ZeroRotator,FVector(.3f),false,EPSCPoolMethod::None,false);
        if(SafetyFire)SafetyFire->SetComponentTickEnabled(false);
    }
    SafetyFireLight=NewObject<UPointLightComponent>(this);SafetyFireLight->SetMobility(EComponentMobility::Movable);
    SafetyFireLight->SetLightColor(FLinearColor(1.f,.23f,.035f));SafetyFireLight->SetAttenuationRadius(270);SafetyFireLight->SetCastShadows(true);
    SafetyFireLight->SetIntensity(0);SafetyFireLight->RegisterComponent();SafetyFireLight->SetWorldLocation(AlarmLocation+FVector(-35,0,35));
    auto* HeatingRig=GetWorld()->SpawnActor<AActor>();
    SafetyHeatLight=NewObject<UPointLightComponent>(HeatingRig);SafetyHeatLight->SetMobility(EComponentMobility::Movable);
    SafetyHeatLight->SetLightColor(FLinearColor(1.f,.065f,.008f));SafetyHeatLight->SetAttenuationRadius(65);SafetyHeatLight->SetCastShadows(false);SafetyHeatLight->SetIntensity(0);SafetyHeatLight->RegisterComponent();SafetyHeatLight->SetWorldLocation(AlarmLocation+FVector(-49,0,15));
    auto* Pilot=NewObject<UStaticMeshComponent>(HeatingRig);HeatingRig->AddInstanceComponent(Pilot);Pilot->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    SafetyHeatSignal=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/Safety/M_SensorSignal.M_SensorSignal")),this);Pilot->SetMaterial(0,SafetyHeatSignal);
    Pilot->SetCollisionEnabled(ECollisionEnabled::NoCollision);Pilot->SetCanEverAffectNavigation(false);Pilot->SetCastShadow(false);Pilot->RegisterComponent();FVector PilotPosition=AlarmLocation+FVector(-47,25,-8);FHitResult Front;FCollisionQueryParams Surface(SCENE_QUERY_STAT(HeatingPilotSurface),true);Surface.AddIgnoredActor(GetOwningPlayerController()->GetPawn());
    if(GetWorld()->LineTraceSingleByChannel(Front,{AlarmLocation.X-180,AlarmLocation.Y+25,AlarmLocation.Z-8},{AlarmLocation.X+80,AlarmLocation.Y+25,AlarmLocation.Z-8},ECC_Visibility,Surface)&&Front.ImpactNormal.X<-.7f){PilotPosition=Front.ImpactPoint+Front.ImpactNormal*1.2f;SafetyHeatLight->SetWorldLocation(PilotPosition+Front.ImpactNormal*7+FVector(0,0,6));}
    UE_LOG(LogTemp,Display,TEXT("SAFETY_HEAT_PILOT position=%s surface=%s"),*PilotPosition.ToString(),*GetNameSafe(Front.GetActor()));Pilot->SetWorldLocation(PilotPosition);Pilot->SetWorldScale3D({.008f,.04f,.015f});SafetyHeatSignal->SetVectorParameterValue(TEXT("SignalColor"),FLinearColor::Black);
    // Original, synthesized two-tone chime. Mono 16-bit / 22050 Hz, 2 s loop.
    constexpr int32 Rate=22050,SampleCount=Rate*2;
    SafetySirenPCM.SetNumUninitialized(SampleCount*2);
    for(int32 I=0;I<SampleCount;++I)
    {
        const float T=float(I)/Rate,Beat=FMath::Fmod(T,.5f);
        const float Envelope=FMath::Clamp(Beat/.025f,0.f,1.f)*FMath::Clamp((.42f-Beat)/.06f,0.f,1.f);
        const float Frequency=I<Rate?660.f:880.f;
        const int16 Sample=int16(8000.f*Envelope*FMath::Sin(2.f*PI*Frequency*T));
        SafetySirenPCM[2*I]=uint8(Sample&255);SafetySirenPCM[2*I+1]=uint8((Sample>>8)&255);
    }
    SafetySirenWave=NewObject<USoundWaveProcedural>(this);
    SafetySirenWave->SetSampleRate(Rate);SafetySirenWave->NumChannels=1;
    SafetySirenWave->Duration=INDEFINITELY_LOOPING_DURATION;SafetySirenWave->SoundGroup=SOUNDGROUP_Effects;
    SafetySiren=NewObject<UAudioComponent>(this);
    SafetySiren->bAutoActivate=false;SafetySiren->bAutoDestroy=false;SafetySiren->bIsUISound=false;
    SafetySiren->SetSound(SafetySirenWave);SafetySiren->SetVolumeMultiplier(.22f);
    SafetySiren->RegisterComponent();
    InitializeEvacuation();InitializeSafetyIsolator();
    UE_LOG(LogTemp,Display,TEXT("SAFETY_READY smoke=%d audio=%d location=%s"),SafetySmoke!=nullptr,SafetySiren!=nullptr,*AlarmLocation.ToString());
}

bool ACourseworkHUD::IsSafetyReady()const
{return SafetyIsolator&&SafetySmoke&&SafetyFire&&SafetySiren&&OriginalResident.IsValid()&&ResidentLocomotion&&ResidentRunAnimation&&ResidentIdleAnimation&&SensorSignal&&AssemblyMarker;}
float ACourseworkHUD::GetSafetyProgress()const
{return SafetyState==1?FMath::Clamp(SmokeReading/30.f,0.f,1.f):(int32(bPlayerGathered)+int32(HasResidentGathered()))*.5f;}
FString ACourseworkHUD::GetSafetyStatus()const
{
    if(SafetyState==0)return TEXT("尚未启动演练");
    if(SafetyState==3)return KitchenIncident.Ignited?TEXT("人员已集合，火情未解除"):TEXT("两人已集合");
    if(KitchenIncident.Cleared())return SafetyState>=2?TEXT("预警解除，继续撤离"):TEXT("预警已解除");
    if(SafetyState>=2)return TEXT("正在疏散");
    return KitchenIncident.Isolated?TEXT("设备已隔离，正在冷却"):TEXT("厨房过热预警");
}
void ACourseworkHUD::TriggerKitchenAlarm()
{
    if(IsSafetyDrillRunning()||!IsSafetyReady())return;
    SafetySavedSmartScene=SmartLightingScene;
    SafetySavedFixtureOverrides=SmartFixtureOverrides;
    SafetySavedFixtureRestore.Empty();
    for(const auto& Override:SmartFixtureOverrides)if(const auto* Fixture=SmartFixtures.Find(Override.Key))SafetySavedFixtureRestore.Add(Override.Key,Fixture->Restore);
    SafetySavedManualRooms=SmartManualRooms;
    SetSmartLightingScene(0);
    // A drill changes the hazard state, not the user's selected time of day.
    RestoreEnergyComparison();bEnergyMapLayer=false;
    KitchenIncident=FCourseworkKitchenIncident();SafetyAlarmAt=-1;
    SafetyState=1;UpdateSafetyIsolator();SmokeReading=0;AlarmElapsed=0;SafetySmokeLevel=0;
    bPlayerGathered=false;PlayerArrivalTime=-1;ResidentArrivalTime=-1;LastEvacuationResult.Empty();
    SafetySmoke->SetRelativeScale3D(FVector(.35f));SafetySmoke->SetComponentTickEnabled(true);SafetySmoke->ActivateSystem(true);
    SafetyFire->DeactivateImmediate();SafetyFire->SetComponentTickEnabled(false);
    if(Overlay)Overlay->RefreshPanel();
    UE_LOG(LogTemp,Display,TEXT("SAFETY_STATE smoke_building"));
}
void ACourseworkHUD::HandleKitchenAlarm()
{
    // Ending an exercise never claims that the simulated fire was extinguished.
    if(!CanFinishSafety())return;
    ResetKitchenAlarm();
}
void ACourseworkHUD::ResetKitchenAlarm(bool ResumeEnvironment)
{
    const bool WasRunning=IsSafetyDrillRunning();
    if(IsSafetyDrillRunning())
    {
        const FString People=SafetyState==3?FString::Printf(TEXT("两人已集合，%.1f秒"),FMath::Max(PlayerArrivalTime,ResidentArrivalTime)):KitchenIncident.Cleared()&&SafetyState==1?TEXT("早期预警解除"):TEXT("演练已中止，疏散未完成");
        const FString Isolation=KitchenIncident.Isolated?(SafetyAlarmAt<0||KitchenIncident.IsolationAt<SafetyAlarmAt?TEXT("报警前断开加热"):FString::Printf(TEXT("报警后%.1f秒断开加热"),KitchenIncident.IsolationAt-SafetyAlarmAt)):TEXT("加热回路未断开");
        const FString Hazard=KitchenIncident.Ignited?TEXT("火源未控制"):KitchenIncident.Cleared()?TEXT("未发生起火"):TEXT("仍有过热或余烟");
        LastEvacuationResult=People+TEXT("\n")+Isolation+TEXT("\n")+Hazard;
    }
    RestoreResident();SafetyState=0;SmokeReading=0;AlarmElapsed=0;SafetySmokeLevel=0;
    bPlayerGathered=false;PlayerArrivalTime=-1;ResidentArrivalTime=-1;
    KitchenIncident=FCourseworkKitchenIncident();SafetyAlarmAt=-1;UpdateSafetyIsolator();
    if(SafetySmoke)SafetySmoke->DeactivateImmediate();
    if(SafetyFire)SafetyFire->DeactivateImmediate();
    if(SafetyFireLight)SafetyFireLight->SetIntensity(0);
    if(SafetyHeatLight)SafetyHeatLight->SetIntensity(0);
    if(SafetyHeatSignal)SafetyHeatSignal->SetVectorParameterValue(TEXT("SignalColor"),FLinearColor::Black);
    if(SafetySiren)SafetySiren->Stop();
    if(SafetySirenWave)SafetySirenWave->ResetAudio();
    if(IsValid(AssemblyMarker))AssemblyMarker->SetActorHiddenInGame(true);
    if(WasRunning&&ResumeEnvironment)
    {
        if(Ventilation)Ventilation->Suspend(false);
        if(SafetySavedSmartScene&&SetSmartLightingScene(SafetySavedSmartScene))
        {
            SmartFixtureOverrides=SafetySavedFixtureOverrides;
            SmartManualRooms=SafetySavedManualRooms;
            for(const auto& Override:SmartFixtureOverrides)if(auto* Fixture=SmartFixtures.Find(Override.Key))
            {Fixture->Level=Fixture->Target=Override.Value;Fixture->Restore=SafetySavedFixtureRestore.FindRef(Override.Key);}
            // Presence and route timers are evaluated afresh at the current
            // position. Preserve user choices, not expired sensor outputs.
            TickSmartLighting(0);
        }
        UE_LOG(LogTemp,Display,TEXT("SAFETY_ENVIRONMENT_RESUMED scene=%d overrides=%d rooms=%d hour=%.5f flowing=%d"),SmartLightingScene,SmartFixtureOverrides.Num(),SmartManualRooms.Num(),Clock.Hour,Clock.bRunning);
    }
    SafetySavedSmartScene=0;SafetySavedFixtureOverrides.Empty();SafetySavedFixtureRestore.Empty();SafetySavedManualRooms.Empty();
    UE_LOG(LogTemp,Display,TEXT("SAFETY_STATE drill_reset"));
}
void ACourseworkHUD::TickSafety(float DeltaSeconds)
{
    if(UGameplayStatics::IsGamePaused(GetWorld()))return;
    if(SensorSignal)
    {
        const float Pulse=.2f+.8f*FMath::Max(0.f,FMath::Sin(GetWorld()->GetTimeSeconds()*7.f));
        SensorSignal->SetVectorParameterValue(TEXT("SignalColor"),IsSafetyActive()?FLinearColor(.8f*Pulse,.015f,.005f):SafetyState==1&&!KitchenIncident.Cleared()?FLinearColor(.4f,.15f,.01f):FLinearColor(.02f,.07f,.025f));
    }
    if(!IsSafetyDrillRunning())return;
    KitchenIncident.Advance(DeltaSeconds);SmokeReading=KitchenIncident.Sensor;SafetySmokeLevel=KitchenIncident.Smoke/100.f;
    if(SafetySmoke)SafetySmoke->SetRelativeScale3D(FVector(.38f+.72f*SafetySmokeLevel));
    if(SafetySmokeMaterial)SafetySmokeMaterial->SetScalarParameterValue(TEXT("SmokeOpacity"),.52f*FMath::Sqrt(SafetySmokeLevel));
    if(SafetyHeatLight)SafetyHeatLight->SetIntensity(KitchenIncident.Isolated?0.f:45.f*FMath::Clamp(KitchenIncident.Heat,0.f,1.f));
    if(SafetyHeatSignal)SafetyHeatSignal->SetVectorParameterValue(TEXT("SignalColor"),KitchenIncident.Isolated?FLinearColor::Black:FLinearColor(.9f,.08f,.008f));
    const float FireStrength=KitchenIncident.Fire;
    if(KitchenIncident.Ignited&&SafetyFire&&!SafetyFire->IsActive()){SafetyFire->SetComponentTickEnabled(true);SafetyFire->ActivateSystem(true);}
    if(SafetyFire)SafetyFire->SetRelativeScale3D(FVector(.24f+.36f*FireStrength));
    if(SafetyFireLight){const float T=GetWorld()->GetTimeSeconds();SafetyFireLight->SetIntensity((FireStrength>0?250.f+550.f*FireStrength:0.f)*(.8f+.13f*FMath::Sin(T*11.7f)+.07f*FMath::Sin(T*23.1f)));}
    if(SafetyState==1&&SmokeReading>=30.f)
    {
        SafetyState=2;SafetyAlarmAt=KitchenIncident.Elapsed;AlarmElapsed=0;BeginResidentEvacuation();
        AssemblyMarker->SetActorHiddenInGame(false);
        SafetySirenWave->ResetAudio();SafetySirenWave->QueueAudio(SafetySirenPCM.GetData(),SafetySirenPCM.Num());SafetySiren->SetVolumeMultiplier(.22f);SafetySiren->Play();
        ShowToast(TEXT("烟感已报警，请和住户前往室外集合点"));
        UE_LOG(LogTemp,Display,TEXT("SAFETY_STATE detector_alarm reading=%.2f"),SmokeReading);
    }
    if(!IsSafetyActive())return;
    AlarmElapsed+=DeltaSeconds;TickEvacuation(DeltaSeconds);
    if(SafetySirenWave->GetAvailableAudioByteCount()<22050)SafetySirenWave->QueueAudio(SafetySirenPCM.GetData(),SafetySirenPCM.Num());
    SafetySiren->SetVolumeMultiplier(bPlayerGathered?.08f:.22f);
    const int32 NextState=bPlayerGathered&&HasResidentGathered()?3:2;
    if(NextState==3&&SafetyState!=3){ShowToast(TEXT("两人均已到达集合点，可结束疏散演练"));UE_LOG(LogTemp,Display,TEXT("EVAC_ALL_GATHERED %.2f"),AlarmElapsed);}
    SafetyState=NextState;
}
