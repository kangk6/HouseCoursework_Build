#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

void ACourseworkHUD::SetAllRoomLighting(bool Enabled)
{for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,Enabled,RoomBrightness[I]);}
void ACourseworkHUD::ResetAllRoomLighting()
{for(int32 I=0;I<CourseworkRooms::Count;++I)ResetRoomLighting(I);}
FString ACourseworkHUD::GetLightingSaveStatus() const
{if(bEnergyComparisonActive)return TEXT("方案对比中 · 调整暂不保存");return !bLightingPersistenceEnabled?TEXT("关灯保留亮度"):bLightingSaveFailed?TEXT("保存失败，请检查磁盘"):bLightingDirty?TEXT("正在保存 · 关灯保留亮度"):TEXT("自动保存 · 关灯保留亮度");}

void ACourseworkHUD::LoadRoomLighting()
{
    FString Test,Profile;
    FParse::Value(FCommandLine::Get(),TEXT("HouseTest="),Test);
    FParse::Value(FCommandLine::Get(),TEXT("HouseLightingProfile="),Profile);
    bool SafeProfile=!Profile.IsEmpty() && Profile.Len()<64;
    for(TCHAR C:Profile)if(!FChar::IsAlnum(C)&&C!='_'&&C!='-')SafeProfile=false;
    // Acceptance runs never read or overwrite the user's actual lighting state.
    bLightingPersistenceEnabled=Test.IsEmpty() || SafeProfile;
    if(!bLightingPersistenceEnabled)return;
    LightingSavePath=FPaths::ProjectSavedDir()/TEXT("Lighting")/(SafeProfile?Profile+TEXT(".json"):TEXT("room_lighting.json"));
    FString Text;if(!FFileHelper::LoadFileToString(Text,*LightingSavePath))return;
    TSharedPtr<FJsonObject> Root;const auto Reader=TJsonReaderFactory<>::Create(Text);
    double Version=0;const TArray<TSharedPtr<FJsonValue>>* Rooms=nullptr;
    if(!FJsonSerializer::Deserialize(Reader,Root)||!Root.IsValid()||!Root->TryGetNumberField(TEXT("version"),Version)||Version!=1||!Root->TryGetArrayField(TEXT("rooms"),Rooms))
    {UE_LOG(LogTemp,Warning,TEXT("LIGHTING_SAVE_INVALID using defaults"));return;}
    bApplyingLightingSave=true;TSet<FString> Seen;
    for(const auto& Value:*Rooms)
    {
        if(!Value.IsValid()||Value->Type!=EJson::Object)continue;
        const auto Item=Value->AsObject();FString Id;bool Enabled=false;double Brightness=1;
        if(!Item->TryGetStringField(TEXT("id"),Id)||Seen.Contains(Id)||!Item->TryGetBoolField(TEXT("enabled"),Enabled)||!Item->TryGetNumberField(TEXT("brightness"),Brightness)||!FMath::IsFinite(Brightness))continue;
        Seen.Add(Id);
        for(int32 I=0;I<CourseworkRooms::Count;++I)if(Id==CourseworkRooms::Ids[I])SetRoomLighting(I,Enabled,float(Brightness));
    }
    bApplyingLightingSave=false;
    UE_LOG(LogTemp,Display,TEXT("LIGHTING_SAVE_LOADED rooms=%d"),Seen.Num());
}
void ACourseworkHUD::QueueLightingSave()
{
    if(!bLightingReady||!bLightingPersistenceEnabled||bApplyingLightingSave||bEnergyComparisonActive)return;
    bLightingDirty=true;
    GetWorld()->GetTimerManager().SetTimer(LightingSaveTimer,this,&ACourseworkHUD::SaveRoomLighting,.5f,false);
}
void ACourseworkHUD::SaveRoomLighting()
{
    if(!bLightingReady||!bLightingPersistenceEnabled||!bLightingDirty||bEnergyComparisonActive)return;
    TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("version"),1);
    TArray<TSharedPtr<FJsonValue>> Rooms;
    for(int32 I=0;I<CourseworkRooms::Count;++I)
    {
        TSharedRef<FJsonObject> Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("id"),CourseworkRooms::Ids[I]);
        Item->SetBoolField(TEXT("enabled"),RoomLightOn[I]);Item->SetNumberField(TEXT("brightness"),RoomBrightness[I]);Rooms.Add(MakeShared<FJsonValueObject>(Item));
    }
    Root->SetArrayField(TEXT("rooms"),Rooms);FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(LightingSavePath),true);
    const FString Temporary=LightingSavePath+TEXT(".tmp");
    bLightingSaveFailed=!FFileHelper::SaveStringToFile(Text,*Temporary,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)||!IFileManager::Get().Move(*LightingSavePath,*Temporary,true,true);
    if(!bLightingSaveFailed)bLightingDirty=false;
    else UE_LOG(LogTemp,Warning,TEXT("LIGHTING_SAVE_FAILED"));
    if(Overlay)Overlay->InvalidateLayoutAndVolatility();
}

void ACourseworkHUD::TickLightingStateAcceptance()
{
    if(!bLightingReady||GetWorld()->GetRealTimeSeconds()<1.5)return;
    const bool Debounce=TestMode==TEXT("lighting_state_debounce"),Clamp=TestMode==TEXT("lighting_state_clamp");
    const bool Write=TestMode==TEXT("lighting_state_write")||Debounce,Read=TestMode==TEXT("lighting_state_read"),Reset=TestMode==TEXT("lighting_state_reset");
    if(Debounce&&AcceptancePhase==0){for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,I%2==0,.2f+.07f*I);AcceptancePhase=1;TestCaseStart=GetWorld()->GetRealTimeSeconds();return;}
    if(Debounce&&GetWorld()->GetRealTimeSeconds()-TestCaseStart<.9)return;
    bool Passed=bLightingPersistenceEnabled;
    if(Debounce)Passed &=!bLightingDirty&&!bLightingSaveFailed&&IFileManager::Get().FileExists(*LightingSavePath);
    if(Write&&!Debounce)for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,I%2==0,.2f+.07f*I);
    if(Reset)ResetAllRoomLighting();
    for(int32 I=0;I<CourseworkRooms::Count;++I)
    {
        const bool ExpectedOn=(Write||Read)?I%2==0:Clamp?I!=0:true;const float ExpectedBrightness=(Write||Read)?.2f+.07f*I:Clamp&&I==0?.01f:1.f;
        Passed &=RoomLightOn[I]==ExpectedOn && FMath::IsNearlyEqual(RoomBrightness[I],ExpectedBrightness,.001f);
        const float Factor=ExpectedOn?ExpectedBrightness:0.f;
        for(const auto& Light:ControlledLights)if(Light.Room==I)Passed &=Light.Component.IsValid()&&FMath::IsNearlyEqual(Light.Component->Intensity,Light.Intensity*Factor,.01f);
        for(const auto& Glow:ControlledGlows)if(Glow.Room==I)Passed &=Glow.Material.IsValid()&&Glow.Mesh.IsValid()&&Glow.Mesh->GetMaterial(Glow.Slot)==Glow.Material.Get()&&FMath::IsNearlyEqual(Glow.Material->K2_GetScalarParameterValue(Glow.Parameter),Factor,.01f);
    }
    // Quit immediately after changing values: EndPlay must flush the debounce.
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"passed\":%s,\"mode\":\"%s\",\"rooms\":%d}"),Passed?TEXT("true"):TEXT("false"),*TestMode,CourseworkRooms::Count),*(FPaths::ProjectSavedDir()/TEXT("LightingStateAcceptance.json")));
    TestMode.Empty();QuitApplication();
}
