#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "StaticMeshComponentLODInfo.h"
#include "Engine/Level.h"
#include "Engine/PointLight.h"
#include "PrecomputedLightVolume.h"
#include "PrecomputedVolumetricLightmap.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void ACourseworkHUD::InitializeRoomLighting()
{
    // Benchmark-only A/B switch: retain the v0.9 geometry policy for comparison.
    const bool LegacyGeometry=FParse::Param(FCommandLine::Get(),TEXT("HouseLegacyMovableSurfaces"));
    // The old volume cache contains bedside illumination even after direct lights
    // are off. Room-local surfaces and dynamic objects use current lights and
    // skylight instead of the stale volume cache.
    for(auto* Level:GetWorld()->GetLevels())
    {
        if(Level->PrecomputedLightVolume)Level->PrecomputedLightVolume->RemoveFromScene(GetWorld()->Scene);
        if(Level->PrecomputedVolumetricLightmap)Level->PrecomputedVolumetricLightmap->RemoveFromScene(GetWorld()->Scene);
    }
    const FVector AmbientPositions[]={FVector(-2120,9870,240),FVector(-2260,9310,520)};
    for(int32 Room=0;Room<2;++Room)
    {
        FActorSpawnParameters Params;Params.Name=FName(*FString::Printf(TEXT("CourseworkNightAmbient_%d"),Room));
        auto* Actor=GetWorld()->SpawnActor<APointLight>(AmbientPositions[Room],FRotator::ZeroRotator,Params);
        auto* Ambient=Actor->PointLightComponent.Get();Ambient->SetMobility(EComponentMobility::Movable);
        Ambient->IntensityUnits=ELightUnits::Candelas;Ambient->SetIntensity(Room==0?.3f:.35f);Ambient->SetAttenuationRadius(650);Ambient->SetCastShadows(false);Ambient->SetLightColor(FLinearColor(.5f,.6f,.8f));
    }
    // Reuse the authored floor-lamp mesh and materials for a visible living fixture.
    for(TActorIterator<AActor> Source(GetWorld());Source;++Source) if(Source->GetName()==TEXT("SM_Lamp_A1_23"))
    {
        auto* Original=Cast<UStaticMeshComponent>(Source->GetRootComponent());if(!Original)break;
        FActorSpawnParameters Params;Params.Name=TEXT("CourseworkLivingLamp");
        auto* Lamp=GetWorld()->SpawnActor<AStaticMeshActor>(FVector(-2440,9770,60),FRotator::ZeroRotator,Params);
        auto* Mesh=Lamp->GetStaticMeshComponent();Mesh->SetMobility(LegacyGeometry?EComponentMobility::Movable:EComponentMobility::Stationary);Mesh->SetStaticMesh(Original->GetStaticMesh());
        for(int32 Slot=0;Slot<Original->GetNumMaterials();++Slot)Mesh->SetMaterial(Slot,Original->GetMaterial(Slot));
        Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));Mesh->IndirectLightingCacheQuality=ILCQ_Off;
        auto* Light=NewObject<UPointLightComponent>(Lamp,TEXT("LivingFloorLampLight"));Lamp->AddInstanceComponent(Light);Light->SetupAttachment(Mesh);Light->SetMobility(EComponentMobility::Movable);Light->SetRelativeLocation(FVector(0,0,122));Light->IntensityUnits=ELightUnits::Lumens;Light->SetIntensity(25);Light->SetAttenuationRadius(420);Light->SetLightColor(FLinearColor(1,.79f,.55f));Light->RegisterComponent();
        break;
    }
    // Room-local mesh pieces use dynamic illumination. Shared large exterior
    // geometry is left alone; no authored map or source lightmap is overwritten.
    const FBox Interior(FVector(-2480,9040,60),FVector(-1610,10750,600));
    int32 CachedSurfaces=0,MovingSurfaces=0;
    for(TActorIterator<AActor> Geometry(GetWorld());Geometry;++Geometry)
    {
        TArray<UStaticMeshComponent*> Surfaces;Geometry->GetComponents(Surfaces);
        for(auto* Surface:Surfaces)
        {
            // Ventilation replaced these hidden source windows with split
            // meshes. Reregistering them resurrected their closed collision.
            if(!Surface->IsVisible()||Surface->bHiddenInGame)continue;
            // Large ceiling slabs retain baked lamp halos unless explicitly
            // included. Keep the exterior/ground policy, but clear indoor slabs.
            const bool IndoorSlab=Surface->GetStaticMesh()&&Surface->GetStaticMesh()->GetName().StartsWith(TEXT("SM_Floor_"))
                &&Surface->Bounds.Origin.Z>300&&Surface->Bounds.Origin.Z<650;
            if(Surface->Bounds.GetBox().Intersect(Interior) && ((Surface->Bounds.BoxExtent.X<400 && Surface->Bounds.BoxExtent.Y<400)||IndoorSlab))
            {
                for(auto& LOD:Surface->LODData)LOD.MapBuildDataId.Invalidate();
                Surface->LightmapType=ELightmapType::Default;Surface->IndirectLightingCacheQuality=ILCQ_Off;
                // Stationary meshes receive live light while remaining eligible
                // for cached point/spot shadows. Preserve authored moving parts.
                if(LegacyGeometry){Surface->SetMobility(EComponentMobility::Movable);++MovingSurfaces;}
                else if(Surface->Mobility!=EComponentMobility::Movable){Surface->SetMobility(EComponentMobility::Stationary);++CachedSurfaces;}
                else ++MovingSurfaces;
                Surface->MarkRenderStateDirty();
            }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("ROOM_SHADOW_GEOMETRY cached=%d moving=%d"),CachedSurfaces,MovingSurfaces);
    // Explicit fixture membership excludes appliances and window/environment lights.
    const TArray<TArray<FString>> Members={
        {TEXT("CourseworkLivingLamp"),TEXT("PointLight5"),TEXT("SpotLight17"),TEXT("SpotLight24"),TEXT("SM_TableLamp_A_26")},
        {TEXT("PointLight20"),TEXT("PointLight18_3"),TEXT("BP_Lamp_C2_ON2"),TEXT("BP_Lamp_C2_ON_2"),TEXT("PointLight22"),TEXT("PointLight23"),TEXT("SM_Lamp_D_6")},
        {TEXT("PointLight9"),TEXT("PointLight4_0"),TEXT("PointLight6_1"),TEXT("SpotLight18"),TEXT("SpotLight19"),TEXT("SpotLight20"),TEXT("SpotLight25"),TEXT("SM_Lamp_A1_23"),TEXT("SM_Lamp_B1_2")},
        {TEXT("PointLight16"),TEXT("SpotLight28"),TEXT("PointLight27"),TEXT("BP_Lamp_E2_ON2_5"),TEXT("SM_Lamp_D2")},
        {TEXT("PointLight3"),TEXT("PointLight17"),TEXT("PointLight19"),TEXT("PointLight24"),TEXT("SM_TableLamp_A1_32")},
        {TEXT("PointLight14"),TEXT("BP_Mirror_A3_0"),TEXT("BP_Mirror_A4_1")},
        {TEXT("PointLight21"),TEXT("BP_Mirror_A1_3"),TEXT("SM_Lamp_E4")},
        {TEXT("PointLight10"),TEXT("PointLight11"),TEXT("PointLight13"),TEXT("BP_Lamp_E1_ON_2"),TEXT("BP_Lamp_B_2")},
        {TEXT("PointLight7"),TEXT("PointLight12"),TEXT("PointLight15"),TEXT("BP_Lamp_E2_ON_2"),TEXT("SM_Lamp_E2"),TEXT("SM_Lamp_E3"),TEXT("SM_Lamp_C1_2")}
    };
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        const FString Name=It->GetName(); const FVector P=It->GetActorLocation();
        int32 Room=INDEX_NONE;
        for(int32 I=0;I<CourseworkRooms::Count;++I)if(Members[I].Contains(Name)){Room=I;break;}
        if(Room!=INDEX_NONE)
        {
            // OFF variants in the authored map were decorative meshes. A room
            // fill cannot substitute for the visible fixture's own emitter.
            const bool TwinSconce=Name==TEXT("SM_Lamp_E2")||Name==TEXT("SM_Lamp_E3")||Name==TEXT("SM_Lamp_E4");
            const bool LandingLamp=Name==TEXT("SM_Lamp_C1_2");
            const bool KitchenFloor=Name==TEXT("SM_Lamp_A1_23"),KitchenPendant=Name==TEXT("SM_Lamp_B1_2"),MasterCeiling=Name==TEXT("SM_Lamp_D_6");
            if(TwinSconce||LandingLamp)
            {
                // The OFF-model shade is opaque and its tiny internal parts
                // cast oversized hard shadows from these close point sources.
                // Architecture still casts shadows; the emissive fixture does
                // not shadow itself (matching the authored ON-model intent).
                if(auto* Mesh=Cast<UStaticMeshComponent>(It->GetRootComponent()))Mesh->SetCastShadow(false);
                if(TwinSconce)if(auto* Mesh=Cast<UStaticMeshComponent>(It->GetRootComponent()))
                    Mesh->SetMaterial(2,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HouseFurniture/Materials/MI_Lamp_E1_Illum.MI_Lamp_E1_Illum")));
                // Emit just outside the opaque shades/bulbs, retaining shadows
                // from the architecture instead of trapping light in the mesh.
                for(int32 Bulb=0;Bulb<2;++Bulb)
                {
                    auto* Light=NewObject<UPointLightComponent>(*It,*FString::Printf(TEXT("CourseworkFixtureLight_%d"),Bulb));
                    It->AddInstanceComponent(Light);Light->SetupAttachment(It->GetRootComponent());Light->SetMobility(EComponentMobility::Movable);
                    Light->SetRelativeLocation(TwinSconce?FVector(Bulb==0?-14.5f:14.5f,13,11):FVector(0,0,Bulb==0?29.f:52.f));
                    Light->IntensityUnits=ELightUnits::Lumens;Light->SetIntensity(1.5f);
                    Light->SetAttenuationRadius(240);Light->SetSourceRadius(3.f);Light->SetLightColor(FLinearColor(1,.78f,.53f));Light->RegisterComponent();
                }
            }
            // The same positive-coverage audit found three other fixtures whose
            // glow was wired but whose illumination came only from distant fill.
            if(KitchenFloor||KitchenPendant||MasterCeiling)
            {
                for(int32 Bulb=0;Bulb<(KitchenPendant?2:1);++Bulb)
                {
                    auto* Light=NewObject<UPointLightComponent>(*It,*FString::Printf(TEXT("CourseworkFixtureLight_%d"),Bulb));
                    It->AddInstanceComponent(Light);Light->SetupAttachment(It->GetRootComponent());Light->SetMobility(EComponentMobility::Movable);
                    Light->SetRelativeLocation(KitchenFloor?FVector(0,0,122):(KitchenPendant?FVector(0,Bulb==0?-36.f:36.f,-63):FVector(0,0,-11)));
                    Light->IntensityUnits=ELightUnits::Lumens;Light->SetIntensity(MasterCeiling?12.f:6.f);Light->SetAttenuationRadius(MasterCeiling?380.f:280.f);
                    Light->SetSourceRadius(2);Light->SetLightColor(FLinearColor(1,.8f,.58f));Light->RegisterComponent();
                }
            }
            // These two authored actors were decorative meshes with no bulb or
            // local emitter. Room fill changes alone did not light their desks.
            if(Name==TEXT("SM_TableLamp_A_26")||Name==TEXT("SM_TableLamp_A1_32"))
            {
                const bool Study=Room==4;
                if(!Study)
                {
                    auto* Bulb=NewObject<UStaticMeshComponent>(*It,TEXT("CourseworkDeskBulb"));It->AddInstanceComponent(Bulb);Bulb->SetupAttachment(It->GetRootComponent());
                    Bulb->SetMobility(EComponentMobility::Movable);Bulb->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Coursework/Lighting/SM_DeskBulb.SM_DeskBulb")));
                    Bulb->SetRelativeLocation(FVector(0,-8,28));Bulb->SetRelativeScale3D(FVector(.035f));Bulb->SetCollisionEnabled(ECollisionEnabled::NoCollision);Bulb->SetCastShadow(false);
                    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/Lighting/M_DeskBulb.M_DeskBulb"));
                    if(Material){auto* MID=UMaterialInstanceDynamic::Create(Material,Bulb);Bulb->SetMaterial(0,MID);ControlledGlows.Add({MID,TEXT("CourseworkLightLevel"),1.f,Room,Bulb,0});}Bulb->RegisterComponent();
                }
                auto* Light=NewObject<USpotLightComponent>(*It,TEXT("CourseworkDeskLampLight"));
                It->AddInstanceComponent(Light);Light->SetupAttachment(It->GetRootComponent());Light->SetMobility(EComponentMobility::Movable);
                Light->SetRelativeLocation(Study?FVector(-33,0,43):FVector(0,-7,30));
                Light->SetRelativeRotation((Study?FVector(-.2,0,-1):FVector(0,-.6f,-1)).Rotation());
                // Match this authored night scene's exposure, rather than using
                // real-world bulb wattage/lumens in an already amplified scene.
                Light->IntensityUnits=ELightUnits::Lumens;Light->SetIntensity(Study?4.5f:3.f);
                Light->SetAttenuationRadius(180);Light->SetInnerConeAngle(35);Light->SetOuterConeAngle(65);
                Light->SetLightColor(FLinearColor(1,.78f,.5f));Light->SetSourceRadius(2);Light->RegisterComponent();
            }
            TArray<ULightComponent*> Lights;It->GetComponents(Lights);
            for(auto* Light:Lights)
            {
                ControlledLights.Add({Light,Light->Intensity,Room});
                Light->SetMobility(EComponentMobility::Movable);
                // Authored baked scenes use many overlapping fill lights. Keep
                // dynamic shadows on the key fixtures instead of rendering the
                // whole house repeatedly for every fill component.
                const bool KeyShadow=Name==TEXT("CourseworkLivingLamp") || Name==TEXT("SM_TableLamp_A_26") || Name==TEXT("SM_TableLamp_A1_32") ||
                    TwinSconce || LandingLamp || KitchenFloor || KitchenPendant || MasterCeiling ||
                    Name==TEXT("PointLight22") || Name==TEXT("PointLight23") ||
                    Name==TEXT("PointLight4_0") || Name==TEXT("SpotLight20") ||
                    Name==TEXT("PointLight27") || Name==TEXT("SpotLight28") ||
                    Name==TEXT("PointLight19") || Name==TEXT("PointLight17") ||
                    (Name.StartsWith(TEXT("BP_Mirror_")) && Light->GetName()==TEXT("LightComponent2")) ||
                    Name==TEXT("BP_Lamp_B_2") || Name==TEXT("PointLight10") ||
                    Name==TEXT("PointLight12") || Name==TEXT("PointLight7");
                Light->SetCastShadows(KeyShadow);
                if(auto* Local=Cast<ULocalLightComponent>(Light))
                {
                    Local->MaxDrawDistance=1100.f;Local->MaxDistanceFadeRange=200.f;
                    if(!KeyShadow)Local->SetAttenuationRadius(FMath::Min(Local->AttenuationRadius,350.f));
                }
            }
        }
        if(Room==INDEX_NONE)continue;
        TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
        for(auto* Mesh:Meshes)
        {
            if(Mesh->GetStaticMesh()&&(Mesh->GetStaticMesh()->GetName().Contains(TEXT("Lamp"))||Name.StartsWith(TEXT("BP_Mirror_"))))RegisterLightFixture(Mesh,Room);
        }
        for(auto* Mesh:Meshes) for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
        {
            auto* Material=Mesh->GetMaterial(Slot);if(!Material)continue;
            UMaterialInterface* Original=Material;
            while(auto* MID=Cast<UMaterialInstanceDynamic>(Original))Original=MID->Parent;
            // C1 also supplies metal parts on other lamps; the generated cloth
            // mask is calibrated only for the upright landing lamp.
            if(Original->GetName()==TEXT("MI_Lamp_C1")&&Name!=TEXT("SM_Lamp_C1_2"))continue;
            const FString Path=FString::Printf(TEXT("/Game/Coursework/Lighting/Controlled_%s.Controlled_%s"),*Original->GetName(),*Original->GetName());
            auto* Controlled=LoadObject<UMaterialInterface>(nullptr,*Path);
            if(!Controlled)continue;
            auto* Dynamic=UMaterialInstanceDynamic::Create(Controlled,Mesh);
            Dynamic->CopyMaterialUniformParameters(Material);
            Dynamic->SetScalarParameterValue(TEXT("CourseworkLightLevel"),1.f);
            if(Name==TEXT("SM_Lamp_C1_2"))
            {
                const FVector Origin=Mesh->GetComponentLocation();
                Dynamic->SetVectorParameterValue(TEXT("CourseworkFixtureOrigin"),FLinearColor(Origin.X,Origin.Y,Origin.Z));
                Dynamic->SetScalarParameterValue(TEXT("CourseworkFixtureScale"),Mesh->GetComponentScale().Z);
            }
            if(Name==TEXT("SM_TableLamp_A_26")||Name==TEXT("SM_TableLamp_A1_32"))
            {
                const bool Study=Room==4;const FVector Center=Mesh->GetComponentTransform().TransformPosition(Study?FVector(-23,0,49):FVector(0,-.5f,35));
                Dynamic->SetVectorParameterValue(TEXT("CourseworkBulbCenter"),FLinearColor(Center.X,Center.Y,Center.Z));
                // The living model has no bulb geometry: its separate small bulb
                // above emits, while the metal head/joint must remain opaque.
                Dynamic->SetScalarParameterValue(TEXT("CourseworkBulbRadius"),Study?4.3f:0.f);
            }
            Mesh->SetMaterial(Slot,Dynamic);
            ControlledGlows.Add({Dynamic,TEXT("CourseworkLightLevel"),1.f,Room,Mesh,Slot});
        }
    }
    UE_LOG(LogTemp,Display,TEXT("ROOM_LIGHTING lights=%d glow_parameters=%d"),ControlledLights.Num(),ControlledGlows.Num());
    bLightingReady=true;LoadRoomLighting();
    // Capture night ambient actors only after they and movable window surfaces exist.
    if(TestMode.IsEmpty()||TestMode==TEXT("clock"))InitializeTimeOfDay();
}
bool ACourseworkHUD::IsRoomLightOn(int32 Room)const{return SmartLightingScene?GetSmartRoomLevel(Room)>.001f:RoomLightOn[CourseworkRooms::Clamp(Room)];}
float ACourseworkHUD::GetRoomBrightness(int32 Room)const{return SmartLightingScene?GetSmartRoomLevel(Room):RoomBrightness[CourseworkRooms::Clamp(Room)];}
int32 ACourseworkHUD::GetRoomLightCount(int32 Room)const{int32 N=0;for(const auto& L:ControlledLights)if(L.Room==Room)++N;return N;}
void ACourseworkHUD::SetRoomLighting(int32 Room,bool Enabled,float Brightness)
{
    if(!FMath::IsFinite(Brightness))return;
    Room=CourseworkRooms::Clamp(Room);
    if(SmartLightingScene&&!bApplyingLightingSave)
    {
        SmartManualRooms.Add(Room);
        for(const auto& Pair:SmartFixtures)if(Pair.Value.Room==Room)SmartFixtureOverrides.Remove(Pair.Key);
    }
    if(bEnergyComparisonActive&&!bApplyingEnergyScene)EnergyScenario=3;
    // Zero turns the room off while retaining its last nonzero restore value.
    RoomLightOn[Room]=Enabled&&Brightness!=0;
    if(Brightness!=0)RoomBrightness[Room]=FMath::Clamp(Brightness,.01f,1.f);
    const float Factor=RoomLightOn[Room]?RoomBrightness[Room]:0.f;
    for(const auto& Light:ControlledLights)if(Light.Room==Room && Light.Component.IsValid())
    {Light.Component->SetIntensity(Light.Intensity*Factor);Light.Component->SetVisibility(RoomLightOn[Room]);}
    for(const auto& Glow:ControlledGlows)if(Glow.Room==Room && Glow.Material.IsValid()){if(Glow.Mesh.IsValid() && Glow.Mesh->GetMaterial(Glow.Slot)!=Glow.Material.Get())Glow.Mesh->SetMaterial(Glow.Slot,Glow.Material.Get());Glow.Material->SetScalarParameterValue(Glow.Parameter,Glow.Strength*Factor);}
    UE_LOG(LogTemp,Display,TEXT("LIGHTING_ACTION room=%d enabled=%d brightness=%.2f lights=%d"),Room,RoomLightOn[Room],Factor,GetRoomLightCount(Room));
    QueueLightingSave();
    if(SmartLightingScene)TickSmartLighting(0);
}
void ACourseworkHUD::ResetRoomLighting(int32 Room){SetRoomLighting(Room,true,1.f);}
void ACourseworkHUD::TickLightingAcceptance()
{
    const double Now=GetWorld()->GetRealTimeSeconds();if(Now-TestCaseStart<2)return;
    if(TestCase<0 && AcceptancePhase==0)
    {
        AcceptancePhase=1;GetOwningPlayerController()->SetIgnoreLookInput(true);GetOwningPlayerController()->SetIgnoreMoveInput(true);SetPanel(1);TestCaseStart=Now;
        if(auto* Character=Cast<ACharacter>(GetOwningPlayerController()->GetPawn())){Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->DisableMovement();Character->SetActorEnableCollision(false);}
        FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Control_navigation.png"),true,false);},.8f,false);return;
    }
    TestCaseStart=Now;++TestCase;
    if(TestCase==CourseworkRooms::Count*5){SetPanel(4);FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Control_settings.png"),true,false);},.8f,false);return;}
    if(TestCase>=CourseworkRooms::Count*5+1){FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("LightingAcceptance.json")));TestMode.Empty();QuitApplication();return;}
    const int32 Room=TestCase/5,Action=TestCase%5;
    if(Action==0)
    {
        auto* PC=GetOwningPlayerController();auto* Character=Cast<ACharacter>(PC->GetPawn());
        const auto& Descriptor=CourseworkRooms::Get(Room);
        // These viewpoints come from the independently completed walking route.
        // This visual test places the camera directly; it does not test traversal.
        Character->SetActorLocation(Descriptor.Position,false,nullptr,ETeleportType::TeleportPhysics);
        // The player-camera manager still caches the previous room on this frame.
        const FVector Eye=Character->GetPawnViewLocation();
        PC->SetControlRotation((Descriptor.Focus-Eye).Rotation());
        ResetRoomLighting(Room);SetPanel(6);Overlay->ShowLightingRoom(Room);
    }
    else
    {
        if(Action==1){struct FValue{float Value;};FValue Value{.35f};Overlay->ProcessEvent(Overlay->FindFunction(TEXT("LightBrightnessChanged")),&Value);SetPanel(0);}
        else if(Action==2 || Action==3)Overlay->ProcessEvent(Overlay->FindFunction(TEXT("ToggleLights")),nullptr);
        else Overlay->ProcessEvent(Overlay->FindFunction(TEXT("ResetLights")),nullptr);
    }
    bool Passed=GetRoomLightCount(Room)>0 && FVector::Dist(GetOwningPlayerController()->GetPawn()->GetActorLocation(),CourseworkRooms::Get(Room).Position)<75.f;int32 Glows=0;
    const float Factor=IsRoomLightOn(Room)?GetRoomBrightness(Room):0;
    for(const auto& L:ControlledLights)if(L.Room==Room)Passed &= L.Component.IsValid() && FMath::IsNearlyEqual(L.Component->Intensity,L.Intensity*Factor,.01f);
    for(const auto& G:ControlledGlows)if(G.Room==Room){++Glows;{float Actual=0;G.Material->GetScalarParameterValue(FMaterialParameterInfo(G.Parameter),Actual);Passed &=FMath::IsNearlyEqual(Actual,G.Strength*Factor,.01f);}}
    // The authored study desk-lamp material has no emissive input. Its actual
    // point lights still have to pass; other rooms must also test their emitters.
    Passed &=Room==4 || Glows>0;
    for(const auto& L:ControlledLights) if(L.Room!=Room)Passed &=L.Component.IsValid() && FMath::IsNearlyEqual(L.Component->Intensity,L.Intensity*(RoomLightOn[L.Room]?RoomBrightness[L.Room]:0.f),.01f);
    for(const auto& G:ControlledGlows)if(G.Room!=Room){float Actual=0;G.Material->GetScalarParameterValue(FMaterialParameterInfo(G.Parameter),Actual);Passed &=FMath::IsNearlyEqual(Actual,G.Strength*(RoomLightOn[G.Room]?RoomBrightness[G.Room]:0.f),.01f);}
    TestResults.Add(FString::Printf(TEXT("{\"room\":%d,\"action\":%d,\"passed\":%s,\"lights\":%d,\"glows\":%d,\"enabled\":%s,\"brightness\":%.2f}"),Room,Action,Passed?TEXT("true"):TEXT("false"),GetRoomLightCount(Room),Glows,IsRoomLightOn(Room)?TEXT("true"):TEXT("false"),GetRoomBrightness(Room)));
    // Capture after the lighting/rendering state settles, on a later frame.
    const FString Shot=FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Lighting_%d_%d.png"),Room,Action);
    FTimerHandle Handle;GetWorld()->GetTimerManager().SetTimer(Handle,[this,Shot,Room,Factor](){
        for(const auto& L:ControlledLights)if(L.Room==Room)UE_LOG(LogTemp,Display,TEXT("LIGHT_SETTLED %d %s %.3f EXPECTED %.3f"),Room,*L.Component->GetOwner()->GetName(),L.Component->Intensity,L.Intensity*Factor);
        for(const auto& G:ControlledGlows)if(G.Room==Room){float Actual=G.Material->K2_GetScalarParameterValue(G.Parameter);UE_LOG(LogTemp,Display,TEXT("GLOW_SETTLED %d %.3f EXPECTED %.3f"),Room,Actual,Factor);UE_LOG(LogTemp,Display,TEXT("GLOW_ASSIGNED %s %d"),*G.Mesh->GetOwner()->GetName(),G.Mesh->GetMaterial(G.Slot)==G.Material.Get());}
        FScreenshotRequest::RequestScreenshot(Shot,true,false);
    },.8f,false);
    if(Overlay)Overlay->RefreshPanel();
}
