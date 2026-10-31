#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "CourseworkNightGuidance.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
const FName StudyDesk(TEXT("SM_TableLamp_A1_32"));
const FName LowerHall(TEXT("SM_Lamp_E2"));
const FName UpperHall(TEXT("SM_Lamp_E3"));
const FName Landing(TEXT("SM_Lamp_C1_2"));
const FName UpperWall(TEXT("BP_Lamp_E2_ON_2"));
const FName UpperBath(TEXT("SM_Lamp_E4"));
// Physical-fixture teaching ratings sum to the unchanged nine circuit ratings.
// Authored PointLight/SpotLight fill actors do not represent extra appliances.
struct FFixtureRating{const TCHAR* Actor;int32 Room;float Watts;};
const FFixtureRating Ratings[]={
 {TEXT("CourseworkLivingLamp"),0,32},{TEXT("SM_TableLamp_A_26"),0,16},
 {TEXT("BP_Lamp_C2_ON2"),1,10},{TEXT("BP_Lamp_C2_ON_2"),1,10},{TEXT("SM_Lamp_D_6"),1,12},
 {TEXT("SM_Lamp_A1_23"),2,12},{TEXT("SM_Lamp_B1_2"),2,24},
 {TEXT("BP_Lamp_E2_ON2_5"),3,8},{TEXT("SM_Lamp_D2"),3,16},
 {TEXT("SM_TableLamp_A1_32"),4,12},
 {TEXT("BP_Mirror_A3_0"),5,6},{TEXT("BP_Mirror_A4_1"),5,6},
 {TEXT("BP_Mirror_A1_3"),6,6},{TEXT("SM_Lamp_E4"),6,6},
 {TEXT("BP_Lamp_B_2"),7,12},{TEXT("BP_Lamp_E1_ON_2"),7,12},
 {TEXT("SM_Lamp_E2"),8,4},{TEXT("SM_Lamp_E3"),8,4},{TEXT("SM_Lamp_C1_2"),8,4},{TEXT("BP_Lamp_E2_ON_2"),8,4}};
}

int32 ACourseworkHUD::SmartZoneAt(FVector P)const
{
    if(P.X< -2500||P.X> -1600||P.Y<9030||P.Y>10760||P.Z<90||P.Z>620)return INDEX_NONE;
    // The real flight runs north from the landing to the ground-floor base.
    // Detect its full sloped footprint, not just a flat upstairs hallway box.
    if(P.X> -2260&&P.X< -2100&&P.Y>10040&&P.Y<10665)return 8;
    if(P.Z>310)
    {
        if(P.Y>10220)return 4;
        if(P.X< -2220&&P.Y>9540&&P.Y<9800)return 6;
        if(P.Y<9380&&P.X> -2070)return 3;
        if(P.Y<9820&&P.X<= -2070)return 1;
        return 8;
    }
    if(P.Z>180&&P.Y>9750&&P.Y<10200&&P.X< -1900&&P.X> -2250)return 8;
    if(P.X> -2110&&P.X< -1920&&P.Y>9560)return 8;
    if(P.Y<9670)return 2;
    if(P.Y>10015)return P.X> -1920?5:7;
    return 0;
}

float ACourseworkHUD::GetSmartFixtureLevel(FName Actor)const
{const auto* State=SmartFixtures.Find(Actor);return State?State->Level:0;}
float ACourseworkHUD::GetSmartTargetLevel(FName Actor)const
{const auto* State=SmartFixtures.Find(Actor);return State?State->Target:0;}
float ACourseworkHUD::GetSmartRoomLevel(int32 Room)const
{
    float FixtureWatts=0;for(const auto& Rating:Ratings)if(Rating.Room==Room)FixtureWatts+=Rating.Watts*GetSmartFixtureLevel(Rating.Actor);
    return FixtureWatts/GetRoomRatedWatts(Room);
}

bool ACourseworkHUD::SetSmartLightingScene(int32 Scene)
{
    Scene=FMath::Clamp(Scene,0,2);
    if(Scene&&(!bLightingReady||IsSafetyDrillRunning())){ShowToast(TEXT("演练期间暂停生活照明"));return false;}
    if(Scene==SmartLightingScene)return true;
    if(SmartLightingScene)
    {
        SmartLightingScene=0;
        for(const auto& Pair:SmartOriginalColors)if(Pair.Key.IsValid())Pair.Key->SetLightColor(Pair.Value);
        SmartFixtures.Empty();SmartFixtureOverrides.Empty();SmartManualRooms.Empty();SmartTrailUntil.Empty();SmartOriginalColors.Empty();SmartRoomUntil.Empty();
        for(auto& Fill:SmartPathFills){Fill.Level=0;Fill.Until=0;if(Fill.Light.IsValid()){Fill.Light->SetIntensity(0);Fill.Light->SetVisibility(false);}}
        for(auto& Light:SmartStairFills)if(Light.IsValid()){Light->SetIntensity(0);Light->SetVisibility(false);}
        for(auto& Level:SmartStairLevels)Level=0;
        bSmartReadingOccupied=false;SmartReadingUntil=0;
        SmartNightHeading=FVector::ZeroVector;SmartNightPreferredFixture=NAME_None;bSmartNightWalking=false;
        // Restore the saved/manual baseline, not a snapshot of automatic outputs.
        bApplyingLightingSave=true;
        for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,RoomLightOn[I],RoomBrightness[I]);
        bApplyingLightingSave=false;
    }
    if(!Scene){if(Overlay)Overlay->InvalidateLayoutAndVolatility();return true;}
    RestoreEnergyComparison();SaveRoomLighting();
    if(bLightingSaveFailed){ShowToast(TEXT("原配置保存失败，未启动生活照明"));return false;}
    if(SmartStairFills.IsEmpty())
    {
        // Local indirect-light stand-ins along the real sloping flight. These
        // are render fills tied to guide lights, not extra rated appliances.
        for(int32 I=0;I<4;++I)
        {
            // AHUD is hidden by engine design: lights owned by it can have
            // valid intensities yet never illuminate the scene. Use visible
            // world actors for these render components.
            auto* Holder=GetWorld()->SpawnActor<AActor>();Holder->SetActorHiddenInGame(false);
            auto* Light=NewObject<UPointLightComponent>(Holder,*FString::Printf(TEXT("NightStairBounce_%d"),I));
            Holder->SetRootComponent(Light);Holder->AddInstanceComponent(Light);
            Light->SetMobility(EComponentMobility::Movable);Light->IntensityUnits=ELightUnits::Lumens;
            Light->SetAttenuationRadius(185);Light->SetSourceRadius(9);Light->SetLightColor(FLinearColor(1,.65f,.36f));
            Light->SetIntensity(0);Light->SetVisibility(false);Light->SetCastShadows(true);Light->RegisterComponent();
            const float Y=10540-I*140.f;
            FHitResult Floor;FCollisionQueryParams Query(SCENE_QUERY_STAT(NightStairFloor),true,GetOwningPlayerController()->GetPawn());
            const float ApproximateFloor=98.f+(10540-Y)*.505f;
            // The lower flight has an upstairs slab overhead. Start below that
            // slab rather than mistaking its top for the ground-floor tread.
            const bool Grounded=GetWorld()->LineTraceSingleByChannel(Floor,FVector(-2190,Y,FMath::Min(390.f,ApproximateFloor+70.f)),FVector(-2190,Y,40),ECC_Pawn,Query);
            const float FloorZ=Grounded?Floor.ImpactPoint.Z:ApproximateFloor;
            Light->SetWorldLocation(FVector(-2190,Y,FloorZ+42.f));
            UE_LOG(LogTemp,Display,TEXT("SMART_STAIR_FILL index=%d grounded=%d floor=%.2f position=%s"),I,Grounded,FloorZ,*Light->GetComponentLocation().ToString());
            SmartStairFills.Add(Light);SmartStairLevels.Add(0);
        }
    }
    if(SmartPathFills.IsEmpty())
    {
        // Low-level bounced-light stand-ins at verified walkable route points.
        // Unlike a single room-wide unshadowed fill, each is occluded by walls
        // and doors and enabled only when reachable on the player's floor.
        struct FNode{FVector Position;int32 Room;};
        const FNode Nodes[]={
            {{-2260,9455,385},1},{{-2120,9740,385},1},{{-2120,9490,385},1},
            {{-2370,9460,385},1},{{-2390,9620,385},6},
            {{-2010,9290,385},8},{{-2000,9120,385},3},{{-1810,9160,385},3},{{-1800,9500,385},8},
            {{-2180,10070,388},8},{{-2000,10070,388},8},
            {{-1890,10230,388},4},{{-1880,10510,388},4},
            {{-1990,9800,125},8},{{-1990,10020,125},8},{{-1990,10240,125},8},{{-1990,10530,125},8},
            {{-1790,10225,125},5},{{-2180,9870,125},0},{{-2310,10400,125},7},
            {{-2050,9490,125},2},{{-1740,9440,125},2},{{-2200,9170,125},2},{{-1920,9170,125},2}};
        for(int32 I=0;I<UE_ARRAY_COUNT(Nodes);++I)
        {
            auto* Holder=GetWorld()->SpawnActor<AActor>();Holder->SetActorHiddenInGame(false);
            auto* Light=NewObject<UPointLightComponent>(Holder,*FString::Printf(TEXT("NightPathBounce_%d"),I));
            Holder->SetRootComponent(Light);Holder->AddInstanceComponent(Light);
            Light->SetMobility(EComponentMobility::Movable);Light->IntensityUnits=ELightUnits::Lumens;
            Light->SetAttenuationRadius(300);Light->SetSourceRadius(16);Light->SetLightColor(FLinearColor(1,.65f,.36f));
            Light->SetIntensity(0);Light->SetVisibility(false);Light->SetCastShadows(true);Light->RegisterComponent();
            Light->SetWorldLocation(Nodes[I].Position);
            FSmartPathFill Fill;Fill.Light=Light;Fill.Room=Nodes[I].Room;SmartPathFills.Add(Fill);
        }
    }
    auto Add=[&](FName Name,int32 Room){if(!SmartFixtures.Contains(Name)){FSmartFixture S;S.Room=Room;S.Restore=RoomBrightness[Room];S.Level=S.Target=RoomLightOn[Room]?RoomBrightness[Room]:0;SmartFixtures.Add(Name,S);}};
    for(const auto& Light:ControlledLights)if(Light.Component.IsValid())
    {Add(Light.Component->GetOwner()->GetFName(),Light.Room);SmartOriginalColors.Add(Light.Component.Get(),Light.Component->GetLightColor());}
    for(const auto& Glow:ControlledGlows)if(Glow.Mesh.IsValid())Add(Glow.Mesh->GetOwner()->GetFName(),Glow.Room);
    SmartLightingScene=Scene;
    SmartNightStartedAt=GetWorld()->GetTimeSeconds();
    TickSmartLighting(0);
    if(Overlay)Overlay->InvalidateLayoutAndVolatility();
    UE_LOG(LogTemp,Display,TEXT("SMART_SCENE mode=%d fixtures=%d"),Scene,SmartFixtures.Num());
    return true;
}

bool ACourseworkHUD::SetSmartFixtureManual(FName Actor,float Level)
{
    if(!SmartLightingScene||!SmartFixtures.Contains(Actor)||!FMath::IsFinite(Level))return false;
    Level=FMath::Clamp(Level,0.f,1.f);SmartFixtureOverrides.Add(Actor,Level);
    auto& S=SmartFixtures[Actor];if(Level>0)S.Restore=Level;else if(S.Target>0)S.Restore=S.Target;
    S.Level=S.Target=Level;ApplySmartLighting();
    UE_LOG(LogTemp,Display,TEXT("SMART_MANUAL actor=%s level=%.2f"),*Actor.ToString(),Level);return true;
}

void ACourseworkHUD::TickSmartLighting(float Dt)
{
    if(!SmartLightingScene||!bLightingReady)return;
    if(IsSafetyDrillRunning()){SetSmartLightingScene(0);return;}
    auto* PC=GetOwningPlayerController();if(!PC||!PC->GetPawn())return;
    const FVector Player=PC->GetPawn()->GetActorLocation();const int32 PreviousZone=SmartPlayerZone;SmartPlayerZone=SmartZoneAt(Player);
    const double Now=GetWorld()->GetTimeSeconds();
    const bool Dark=NeedsNightLighting();
    // The original actor's origin is at the feet; zone heights use pawn centres.
    const int32 ResidentZone=OriginalResident.IsValid()?SmartZoneAt(GetResidentPosition()):INDEX_NONE;
    if(SmartLightingScene==1&&Dark)UpdateNightGuidance(Dt,Player,Now);
    int32 ActivePathFills=0;
    for(auto& Fill:SmartPathFills)if(auto* Light=Fill.Light.Get())
    {
        const FVector Source=Light->GetComponentLocation();
        const float Target=SmartLightingScene==1&&Dark&&!SmartManualRooms.Contains(Fill.Room)&&Fill.Until>Now?1.f:0.f;
        Fill.Level=SmartManualRooms.Contains(Fill.Room)?0:CourseworkNightGuidance::Fade(Fill.Level,Target,Dt,true);
        const float Intensity=6.f*Fill.Level;
        if(!FMath::IsNearlyEqual(Light->Intensity,Intensity,.0001f))Light->SetIntensity(Intensity);
        if(Light->IsVisible()!=(Intensity>.0001f))Light->SetVisibility(Intensity>.0001f);
        if(Intensity>.01f)++ActivePathFills;
    }
    if(SmartLightingScene==1&&PreviousZone!=SmartPlayerZone)
            UE_LOG(LogTemp,Display,TEXT("SMART_NIGHT_ZONE from=%d to=%d position=%s dark=%d hour=%.3f elevation=%.3f path_fills=%d"),PreviousZone,SmartPlayerZone,*Player.ToString(),Dark,GetDaylightHour(),GetDaylightElevation(),ActivePathFills);
    for(int32 I=0;I<SmartStairFills.Num();++I)if(auto* Light=SmartStairFills[I].Get())
    {
        const FName Key(*FString::Printf(TEXT("stair_bounce_%d"),I));
        const bool ManualHall=SmartManualRooms.Contains(8);
        const float Target=SmartLightingScene==1&&Dark&&!ManualHall&&SmartTrailUntil.FindRef(Key)>Now?1.f:0.f;
        SmartStairLevels[I]=CourseworkNightGuidance::Fade(SmartStairLevels[I],Target,Dt,true);
        const float Intensity=8.f*SmartStairLevels[I];
        if(!FMath::IsNearlyEqual(Light->Intensity,Intensity,.0001f))Light->SetIntensity(Intensity);
        if(Light->IsVisible()!=(Intensity>.0001f))Light->SetVisibility(Intensity>.0001f);
    }
    if(SmartLightingScene==2)
    {
        const FVector Desk(-1734,10661,413);
        const float Distance=FVector::Dist2D(Player,Desk);
        FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(ReadingDeskPresence),true,PC->GetPawn());
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,PC->GetPawn()->GetPawnViewLocation(),Desk+FVector(0,0,55),ECC_Visibility,Query)
            &&(!Hit.GetActor()||Hit.GetActor()->GetFName()!=StudyDesk);
        if(!Blocked&&SmartPlayerZone==4&&FMath::Abs(Player.Z-413)<60&&Distance<(bSmartReadingOccupied?235.f:185.f))SmartReadingUntil=Now+5;
        bSmartReadingOccupied=Now<SmartReadingUntil;
    }
    // Daylight supplement is a teaching estimate, not a lux sensor reading.
    // Follow the visible sun while a clock seek is still blending the sky.
    const float Sun=IsDaylightActive()?FMath::Clamp(Daylight->GetRenderedElevation()/50.f,0.f,1.f)*GetWeatherDaylight():0.f;
    const float ReadingLevel=FMath::Lerp(.85f,.30f,Sun);
    for(auto& Pair:SmartFixtures)
    {
        auto& S=Pair.Value;const float Baseline=RoomLightOn[S.Room]?RoomBrightness[S.Room]:0;
        float Target=Baseline;
        if(SmartLightingScene==1)
        {
            // During daylight there is no reason for the sleep scene to black
            // out the house. At night retain the resident and gently light
            // occupied rooms, including the bedroom-to-door connection.
            Target=!Dark||ResidentZone==S.Room?Baseline:0;
            if(Dark&&SmartRoomUntil.FindRef(S.Room)>Now)
            {
                const bool Ceiling=Pair.Key==FName(TEXT("SM_Lamp_D_6"))||Pair.Key==FName(TEXT("SM_Lamp_D2"))||Pair.Key==FName(TEXT("SM_Lamp_B1_2"));
                const bool Auxiliary=Pair.Key.ToString().StartsWith(TEXT("PointLight"))||Pair.Key.ToString().StartsWith(TEXT("SpotLight"));
                if(S.Room!=8&&Auxiliary&&!Ceiling)Target=FMath::Max(Target,.08f);
            }
            if(Dark&&SmartTrailUntil.FindRef(Pair.Key)>Now)
            {
                const bool IndirectFill=Pair.Key==FName(TEXT("PointLight12"))||Pair.Key==FName(TEXT("PointLight15"));
                Target=FMath::Max(Target,IndirectFill?.28f:S.Room==8?.32f:.22f);
            }
        }
        else if(S.Room==4)
        {
            if(bSmartReadingOccupied)Target=Pair.Key==StudyDesk?ReadingLevel:.10f;
            // Leaving the desk restores its ordinary room configuration.
            else Target=Baseline;
        }
        if(SmartManualRooms.Contains(S.Room))Target=Baseline;
        if(const auto* Override=SmartFixtureOverrides.Find(Pair.Key))Target=*Override;
        S.Target=Target;
        // Fixed-rate fade avoids near-zero lights staying visible indefinitely.
        const bool GentleNight=SmartLightingScene==1&&Now-SmartNightStartedAt>2&&S.Level<=.4f&&Target<=.4f;
        S.Level=SmartManualRooms.Contains(S.Room)?Target:FMath::FInterpConstantTo(S.Level,Target,Dt,GentleNight?(Target>S.Level?1.f:.16f):.65f);
    }
    ApplySmartLighting();
}

void ACourseworkHUD::ApplySmartLighting()
{
    if(!SmartLightingScene)return;
    for(const auto& Light:ControlledLights)if(auto* C=Light.Component.Get())
    {
        const FName Name=C->GetOwner()->GetFName();const float Level=GetSmartFixtureLevel(Name);
        const float Intensity=Light.Intensity*Level;
        if(!FMath::IsNearlyEqual(C->Intensity,Intensity,.0001f))C->SetIntensity(Intensity);
        if(C->IsVisible()!=(Level>.0001f))C->SetVisibility(Level>.0001f);
        if(const auto* Original=SmartOriginalColors.Find(C))
        {
            const bool Manual=SmartManualRooms.Contains(Light.Room)||SmartFixtureOverrides.Contains(Name);
            const bool Night=NeedsNightLighting();
            const bool Warm=SmartLightingScene==1&&Night&&(SmartTrailUntil.FindRef(Name)>GetWorld()->GetTimeSeconds()||SmartRoomUntil.FindRef(Light.Room)>GetWorld()->GetTimeSeconds())&&!Manual;
            const FLinearColor Color=Warm?FLinearColor(1,.57f,.26f):*Original;
            // LightColor is stored as 8-bit sRGB; tolerate its quantization so
            // a stable warm light does not invalidate shadow caches each frame.
            if(!C->GetLightColor().Equals(Color,.01f))C->SetLightColor(Color);
        }
    }
    for(const auto& Glow:ControlledGlows)if(Glow.Material.IsValid()&&Glow.Mesh.IsValid())
    {
        if(Glow.Mesh->GetMaterial(Glow.Slot)!=Glow.Material.Get())Glow.Mesh->SetMaterial(Glow.Slot,Glow.Material.Get());
        Glow.Material->SetScalarParameterValue(Glow.Parameter,Glow.Strength*GetSmartFixtureLevel(Glow.Mesh->GetOwner()->GetFName()));
    }
}

FString ACourseworkHUD::GetSmartLightingStatus()const
{
    if(SmartLightingScene==1)return NeedsNightLighting()?TEXT("沿途引路已开启"):TEXT("等待天黑");
    if(SmartLightingScene==2&&(SmartFixtureOverrides.Contains(StudyDesk)||SmartManualRooms.Contains(4)))return TEXT("已手动接管");
    if(SmartLightingScene==2)return bSmartReadingOccupied?TEXT("正在补光"):TEXT("等待靠近书桌");
    return TEXT("日常手动");
}
FString ACourseworkHUD::GetSmartLightingDetail()const
{
    if(SmartLightingScene==1)
    {
        if(!NeedsNightLighting())return TEXT("白天暂停引路，保留日常照明配置");
        return TEXT("走近时提前补亮床边出口与沿途，离开后延时渐暗");
    }
    if(SmartLightingScene==2&&(SmartFixtureOverrides.Contains(StudyDesk)||SmartManualRooms.Contains(4)))return TEXT("保留你的灯光调整，退出场景后恢复日常配置");
    if(SmartLightingScene==2)return bSmartReadingOccupied?TEXT("只补亮书房台灯，日照充足时降低补光"):TEXT("走近二楼书房桌面启用，离开后恢复日常照明");
    return TEXT("选择生活场景，让灯光配合你的活动");
}
