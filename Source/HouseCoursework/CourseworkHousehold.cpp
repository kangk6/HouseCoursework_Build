#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"

void ACourseworkHUD::InitializeHousehold()
{
    if(Appliances.Num())return;
    int32 Seed=int32(FPlatformTime::Cycles());FParse::Value(FCommandLine::Get(),TEXT("HouseWeatherSeed="),Seed);
    // Legacy checks isolate their original clear-sky / lighting-only conditions.
    const bool Normal=TestMode.IsEmpty()||TestMode==TEXT("household_weather");
    Weather.Initialize(Seed,Normal);
    if(TestMode==TEXT("household_weather")&&!IsDaylightActive())InitializeTimeOfDay();
    const struct FSpec{const TCHAR* Actor;const TCHAR* Name;int32 Room;float Watts;bool Fridge;} Specs[]={
        {TEXT("SM_Monitor_A1_22"),TEXT("书房电脑"),4,180,false},
        {TEXT("SM_TV_A3"),TEXT("客厅电视"),0,90,false},
        {TEXT("SM_TV_A2_65"),TEXT("主卧电视"),1,90,false},
        {TEXT("BP_Fridge_A2"),TEXT("厨房冰箱"),2,120,true}};
    DesktopTexture=UTexture2D::CreateTransient(512,288,PF_B8G8R8A8);
    if(DesktopTexture)
    {
        TArray<FColor> Pixels;Pixels.SetNum(512*288);
        for(int32 Y=0;Y<288;++Y)for(int32 X=0;X<512;++X)
        {
            FColor C(17+X/20,38+Y/12,65+X/8);
            if(Y>264)C=FColor(18,23,32);
            for(int32 I=0;I<4;++I)if(X>18&&X<42&&Y>20+I*52&&Y<44+I*52)C=FColor(145+I*20,185,210);
            if(X>122&&X<470&&Y>38&&Y<245)C=Y<62?FColor(224,231,237):FColor(35,45,62);
            if(X>140&&X<420&&Y>81&&Y<221&&Y%20<3)C=X<(Y%53)*4+170?FColor(106,187,159):FColor(65,97,123);
            Pixels[(287-Y)*512+(511-X)]=C;
        }
        void* Data=DesktopTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(Data,Pixels.GetData(),Pixels.Num()*sizeof(FColor));DesktopTexture->GetPlatformData()->Mips[0].BulkData.Unlock();DesktopTexture->UpdateResource();
    }
    for(const auto& Spec:Specs)for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->GetFName()==FName(Spec.Actor))
    {
        auto* Mesh=It->FindComponentByClass<UStaticMeshComponent>();if(!Mesh)continue;
        FAppliance Device;Device.Name=Spec.Name;Device.Room=Spec.Room;Device.Watts=Spec.Watts;Device.Fridge=Spec.Fridge;Device.On=Normal&&Spec.Fridge;Device.Mesh=Mesh;
        if(!Spec.Fridge)
        {
            UStaticMeshComponent* ScreenMesh=Mesh;int32 Slot=1;
            if(Spec.Room==4)
            {
                // A thin front overlay avoids recoloring the monitor's one-slot body.
                auto* Screen=NewObject<UStaticMeshComponent>(*It);It->AddInstanceComponent(Screen);Screen->SetupAttachment(Mesh);
                Screen->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);Screen->SetCanEverAffectNavigation(false);Screen->SetCastShadow(false);Screen->RegisterComponent();
                FVector N(0,-1,0);const FVector Aim=Mesh->Bounds.Origin+FVector(0,0,10.5);FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ComputerScreen),true);
                FVector Center=Aim+N*3;
                if(Mesh->LineTraceComponent(Hit,Aim+N*100,Aim-N*100,Q)){N=Hit.ImpactNormal.GetSafeNormal();Center=Hit.ImpactPoint+N*.2;}
                Screen->SetWorldLocation(Center);Screen->SetWorldRotation(FRotationMatrix::MakeFromXZ(FVector::XAxisVector,N).Rotator());Screen->SetWorldScale3D(FVector(.62,.35,1));
                Screen->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HouseFurniture/Materials/MI_TVScreen_A2.MI_TVScreen_A2")));
                ScreenMesh=Screen;Slot=0;Device.Overlay=Screen;
            }
            if(auto* Original=ScreenMesh->GetMaterial(Slot))
            {
                Device.Original=Original;Device.Slot=Slot;
                auto* MID=ScreenMesh->CreateDynamicMaterialInstance(Slot);Device.Screen=MID;
                if(MID&&Spec.Room==4&&DesktopTexture)
                {
                    TArray<FMaterialParameterInfo> Infos;TArray<FGuid> IDs;MID->GetAllTextureParameterInfo(Infos,IDs);
                    for(const auto& Info:Infos){const FString Name=Info.Name.ToString().ToLower();if(!Name.Contains(TEXT("normal"))&&!Name.Contains(TEXT("rough")))MID->SetTextureParameterValue(Info.Name,DesktopTexture);}
                }
            }
            // Only screens/bodies intercept E; furniture drawers remain separate actors.
            auto* Proxy=NewObject<UBoxComponent>(*It);It->AddInstanceComponent(Proxy);Proxy->SetupAttachment(Mesh);Proxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Proxy->SetCollisionResponseToAllChannels(ECR_Ignore);Proxy->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Proxy->SetGenerateOverlapEvents(false);Proxy->SetCanEverAffectNavigation(false);Proxy->RegisterComponent();Proxy->SetWorldLocation(Mesh->Bounds.Origin);Proxy->SetWorldRotation(FRotator::ZeroRotator);Proxy->SetWorldScale3D(FVector(1));Proxy->SetBoxExtent(Mesh->Bounds.BoxExtent+FVector(.5));
            ApplianceTargets.Add(Mesh,Appliances.Num());ApplianceTargets.Add(Proxy,Appliances.Num());
        }
        Appliances.Add(Device);
        UE_LOG(LogTemp,Display,TEXT("APPLIANCE_READY name=%s actor=%s rated_w=%.0f"),*Device.Name,*It->GetName(),Device.Watts);
    }
    TickHousehold(0);
    UE_LOG(LogTemp,Display,TEXT("WEATHER_READY seed=%d automatic=%d"),Weather.Seed,Weather.Automatic);
}

FString ACourseworkHUD::GetApplianceName(int32 I)const{return Appliances.IsValidIndex(I)?Appliances[I].Name:TEXT("");}
bool ACourseworkHUD::IsApplianceOn(int32 I)const{return Appliances.IsValidIndex(I)&&Appliances[I].On;}
int32 ACourseworkHUD::GetApplianceRoom(int32 I)const{return Appliances.IsValidIndex(I)?Appliances[I].Room:0;}
float ACourseworkHUD::GetAppliancePower(int32 I)const
{
    if(!Appliances.IsValidIndex(I)||!Appliances[I].On)return 0;
    const auto& D=Appliances[I];return D.Fridge?(FMath::Fmod(ApplianceSeconds,1800.)<600?D.Watts:2.f):D.Watts;
}
FString ACourseworkHUD::GetApplianceStatus(int32 I)const
{
    if(!IsApplianceOn(I))return Appliances.IsValidIndex(I)&&Appliances[I].Fridge?TEXT("已断电"):TEXT("已关闭");
    return Appliances[I].Fridge?(GetAppliancePower(I)>2?TEXT("制冷中"):TEXT("待机")):TEXT("运行中");
}
float ACourseworkHUD::GetApplianceWatts()const{float W=0;for(int32 I=0;I<Appliances.Num();++I)W+=GetAppliancePower(I);return W;}
float ACourseworkHUD::GetRoomDemandWatts(int32 Room)const{float W=GetRoomWatts(Room);for(int32 I=0;I<Appliances.Num();++I)if(Appliances[I].Room==Room)W+=GetAppliancePower(I);return W;}
bool ACourseworkHUD::ToggleAppliance(int32 I)
{
    if(!Appliances.IsValidIndex(I)||IsSafetyDrillRunning())return false;
    Appliances[I].On=!Appliances[I].On;TickHousehold(0);
    UE_LOG(LogTemp,Display,TEXT("APPLIANCE_ACTION name=%s on=%d watts=%.1f"),*Appliances[I].Name,Appliances[I].On,GetAppliancePower(I));return true;
}
bool ACourseworkHUD::SetApplianceTarget(const FHitResult& Hit)
{
    const auto* I=ApplianceTargets.Find(Hit.GetComponent());if(!I||Hit.Distance>250)return false;
    TargetAppliance=*I;TargetLabel=GetApplianceName(*I);
    if(Overlay)Overlay->SetInteraction(TargetLabel+TEXT("  ·  按 E ")+(IsApplianceOn(*I)?TEXT("关闭"):TEXT("开启")));
    return true;
}
FString ACourseworkHUD::GetWeatherName()const{const TCHAR* Names[]={TEXT("晴"),TEXT("多云"),TEXT("阴天")};return Names[FMath::Clamp(Weather.Kind,0,2)];}
void ACourseworkHUD::ChooseWeather(int32 Kind){if(IsSafetyDrillRunning())return;Weather.Choose(Kind);UE_LOG(LogTemp,Display,TEXT("WEATHER_MANUAL kind=%d seed=%d"),Weather.Kind,Weather.Seed);}
void ACourseworkHUD::ToggleAutomaticWeather(){if(IsSafetyDrillRunning())return;Weather.SetAutomatic(!Weather.Automatic);UE_LOG(LogTemp,Display,TEXT("WEATHER_AUTO enabled=%d seed=%d"),Weather.Automatic,Weather.Seed);}
void ACourseworkHUD::TickHousehold(float Dt)
{
    const bool Suspended=IsTimeSuspended()||bStartupCover;
    const double SimSeconds=double(Dt)*1440./Clock.DayMinutes;const int32 Before=Weather.Kind;
    Weather.Advance(Dt,SimSeconds,Clock.bRunning,Suspended);
    if(Before!=Weather.Kind)UE_LOG(LogTemp,Display,TEXT("WEATHER_AUTOMATIC from=%d to=%d seed=%d hold=%.0f"),Before,Weather.Kind,Weather.Seed,Weather.Hold);
    if(Daylight)Daylight->SetWeather(Weather.Cover,Weather.Direct(),Weather.Diffuse(),Weather.Daylight());
    if(!IsEnergySuspended()&&Dt>0)ApplianceSeconds+=SimSeconds;
    for(auto& D:Appliances)if(auto* M=D.Screen.Get()){M->SetScalarParameterValue(TEXT("EmmisivePower"),D.On?.8f:0.f);M->SetScalarParameterValue(TEXT("Brightness"),D.On?1.f:0.f);}
}
void ACourseworkHUD::RestoreHousehold()
{
    for(auto& D:Appliances){if(auto* S=D.Overlay.Get())S->DestroyComponent();else if(auto* M=D.Mesh.Get())if(D.Original.IsValid()&&D.Slot>=0)M->SetMaterial(D.Slot,D.Original.Get());}
}
