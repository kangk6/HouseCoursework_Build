#include "CourseworkRain.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool ACourseworkRain::IsHardMaterial(const UMaterialInterface* Material)
{
    for(const UMaterialInterface* M=Material;M;)
    {
        const FString N=M->GetName();
        if(N.StartsWith(TEXT("MI_Roof_"))||N==TEXT("M_Road_A")||N==TEXT("M_RoadWet")||N.StartsWith(TEXT("MI_SideWalk_"))||N.StartsWith(TEXT("MI_Concrete_")))return true;
        const auto* Instance=Cast<UMaterialInstance>(M);M=Instance?Instance->Parent:nullptr;
    }
    return false;
}
void ACourseworkRain::RegisterWetSurfaces()
{
    auto* Road=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Coursework/RainPolish/M_RoadWet.M_RoadWet"));
    if(!Road){UE_LOG(LogTemp,Error,TEXT("RAIN_POLISH_REQUIRED_ASSET missing road wet material"));return;}
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
        for(auto* Mesh:Meshes)
        {
            if(!Mesh->GetStaticMesh()||!Mesh->IsVisible()||Mesh->bHiddenInGame)continue;
            const FString N=Mesh->GetStaticMesh()->GetName();
            if(!N.StartsWith(TEXT("SM_Road_"))&&!N.StartsWith(TEXT("SM_DriveWay_"))&&!N.StartsWith(TEXT("SM_Sidewalk_"))&&!N.StartsWith(TEXT("SM_Roof_"))&&!N.StartsWith(TEXT("SM_Porch_Roof_")))continue;
            for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
            {
                auto* Original=Mesh->GetMaterial(Slot);if(!Original||!IsHardMaterial(Original))continue;
                FWetSurface S;S.Mesh=Mesh;S.Slot=Slot;S.MaterialIndex=WetMaterials.Num();S.Road=Original->GetBaseMaterial()->GetName()==TEXT("M_Road_A");
                UMaterialInterface* Parent=Original;
                while(auto* Dynamic=Cast<UMaterialInstanceDynamic>(Parent))Parent=Dynamic->Parent;
                auto* MID=UMaterialInstanceDynamic::Create(S.Road?Road:Parent,this);
                MID->CopyMaterialUniformParameters(Original);
                TArray<FMaterialParameterInfo> Infos;TArray<FGuid> Ids;Original->GetAllScalarParameterInfo(Infos,Ids);
                for(const auto& Info:Infos)
                {
                    const FString Name=Info.Name.ToString();const bool Rough=Name==TEXT("Roughness")||Name.EndsWith(TEXT("_Roughness_1"));
                    const bool Bright=Name==TEXT("Brightness")||Name==TEXT("Global_Brightness")||Name==TEXT("Mat1_Brightness")||Name==TEXT("Mat2_Brightness");
                    float Value=0;if(!S.Road&&(Rough||Bright)&&Original->GetScalarParameterValue(Info,Value))S.Parameters.Add({Info.Name,Value,Rough});
                }
                OriginalSurfaces.Add(Original);WetMaterials.Add(MID);WetSurfaces.Add(MoveTemp(S));Mesh->SetMaterial(Slot,MID);
            }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("RAIN_WET_SURFACES registered=%d"),WetSurfaces.Num());
}
void ACourseworkRain::UpdateWetSurfaces(float Amount,float Dt)
{
    // Visual surface water uses real running time, not accelerated clock seeks.
    // Full rain wets in ~30 seconds; clear weather dries over ~3 minutes.
    Wetness=FMath::Clamp(Wetness+Dt*(Amount/30.f-(1-Amount)/180.f),0.f,1.f);
    if(Wetness>.0001f&&WetSurfaces.IsEmpty())RegisterWetSurfaces();
    SurfaceElapsed+=Dt;if(SurfaceElapsed<.1f&&Wetness>0)return;SurfaceElapsed=0;
    if(Wetness==0){RestoreDrySurfaces();return;}
    for(const auto& S:WetSurfaces)if(auto* M=WetMaterials[S.MaterialIndex].Get())
    {
        if(S.Road)M->SetScalarParameterValue(TEXT("CourseworkWetness"),Wetness);
        else for(const auto& P:S.Parameters)
        {
            // Preserve authored texture detail, normals, metallic and UVs.
            const float Value=P.Roughness?FMath::Lerp(P.Original,FMath::Min(P.Original,FMath::Max(.13f,P.Original*.38f)),Wetness):P.Original*FMath::Lerp(1.f,.72f,Wetness);
            M->SetScalarParameterValue(P.Name,Value);
        }
    }
}
void ACourseworkRain::RestoreDrySurfaces()
{
    for(const auto& S:WetSurfaces)if(auto* Mesh=S.Mesh.Get())if(Mesh->GetMaterial(S.Slot)==WetMaterials[S.MaterialIndex])Mesh->SetMaterial(S.Slot,OriginalSurfaces[S.MaterialIndex]);
    WetSurfaces.Empty();WetMaterials.Empty();OriginalSurfaces.Empty();
}
bool ACourseworkRain::CheckWetSurfaces()const
{
    if(WetSurfaces.Num()<6||WetMaterials.Num()!=WetSurfaces.Num()||OriginalSurfaces.Num()!=WetMaterials.Num())return false;
    bool Road=false,Roof=false,Paving=false;
    for(const auto& S:WetSurfaces)
    {
        auto* Mesh=S.Mesh.Get();auto* M=WetMaterials[S.MaterialIndex].Get();auto* Original=OriginalSurfaces[S.MaterialIndex].Get();
        if(!Mesh||!M||Mesh->GetMaterial(S.Slot)!=M||!IsHardMaterial(Original))return false;
        Road|=S.Road;Roof|=Mesh->GetStaticMesh()->GetName().Contains(TEXT("Roof"));Paving|=Mesh->GetStaticMesh()->GetName().Contains(TEXT("DriveWay"))||Mesh->GetStaticMesh()->GetName().Contains(TEXT("Sidewalk"));
        TArray<FMaterialParameterInfo> Infos;TArray<FGuid> Ids;Original->GetAllTextureParameterInfo(Infos,Ids);
        for(const auto& Info:Infos){UTexture* A=nullptr;UTexture* B=nullptr;if(Original->GetTextureParameterValue(Info,A)&&(!M->GetTextureParameterValue(Info,B)||A!=B))return false;}
        for(const auto& P:S.Parameters){float V=0;if(!M->GetScalarParameterValue(FMaterialParameterInfo(P.Name),V)||V>P.Original+.0001f)return false;}
    }
    return Road&&Roof&&Paving;
}
