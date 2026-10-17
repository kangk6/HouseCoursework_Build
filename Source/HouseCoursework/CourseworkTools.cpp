#include "CourseworkTools.h"
#include "Misc/FileHelper.h"
#if WITH_EDITOR
#include "EngineUtils.h"
#include "MeshDescription.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "StaticMeshAttributes.h"
#include "Serialization/JsonSerializer.h"
#include "PhysicsEngine/BodySetup.h"
#include "Editor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Distributions/DistributionVectorConstant.h"
#include "Distributions/DistributionFloatConstant.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/BlendSpace1D.h"
#include "Particles/Light/ParticleModuleLight.h"
#endif

FString UCourseworkTools::PrepareResidentLocomotion(const FString& BriskPath)
{
#if WITH_EDITOR
    auto* Brisk=LoadObject<UAnimSequence>(nullptr,*BriskPath);
    auto* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Coursework/ResidentAnimated/harper-idle-linear_Anim.harper-idle-linear_Anim"));
    if(!Brisk||!Idle||Brisk->GetSkeleton()!=Idle->GetSkeleton())return TEXT("ERROR locomotion skeleton");
    const FString Name=TEXT("/Game/Coursework/ResidentBrisk/BS_ResidentEvacuation");auto* Package=CreatePackage(*Name);
    auto* Blend=NewObject<UBlendSpace1D>(Package,TEXT("BS_ResidentEvacuation"),RF_Public|RF_Standalone);
    Blend->SetSkeleton(Idle->GetSkeleton());
    auto& Axis=const_cast<FBlendParameter&>(Blend->GetBlendParameter(0));Axis.DisplayName=TEXT("Ground speed cm/s");Axis.Min=0;Axis.Max=170;Axis.GridNum=4;
    Blend->InterpolationParam[0].InterpolationTime=.18f;
    Blend->TargetWeightInterpolationSpeedPerSec=6.f;
    Blend->bScaleAnimation=true;
    Blend->AddSample(Idle,FVector::ZeroVector);Blend->AddSample(Brisk,FVector(170,0,0));
    Blend->ValidateSampleData();Blend->ResampleData();Blend->PostEditChange();Package->MarkPackageDirty();
    const FString File=FPackageName::LongPackageNameToFilename(Name,FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(Package,Blend,*File,Args)?TEXT("SAVED locomotion blend"):TEXT("ERROR saving blend");
#else
    return TEXT("Editor-only operation");
#endif
}

FString UCourseworkTools::PrepareKitchenFire()
{
#if WITH_EDITOR
    auto* Original=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/StarterContent/Particles/P_Fire.P_Fire"));
    if(!Original)return TEXT("ERROR missing fire");
    const FString Name=TEXT("/Game/Coursework/Safety/P_KitchenFire");auto* Package=CreatePackage(*Name);
    auto* Fire=DuplicateObject<UParticleSystem>(Original,Package,TEXT("P_KitchenFire"));Fire->SetFlags(RF_Public|RF_Standalone);
    for(const auto& Emitter:Fire->Emitters)if(Emitter)for(const auto& LOD:Emitter->LODLevels)if(LOD)
        for(const auto& Module:LOD->Modules)if(Cast<UParticleModuleLight>(Module))Module->bEnabled=false;
    Fire->PostEditChange();Package->MarkPackageDirty();
    const FString File=FPackageName::LongPackageNameToFilename(Name,FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(Package,Fire,*File,Args)?TEXT("SAVED kitchen fire"):TEXT("ERROR saving fire");
#else
    return TEXT("Editor-only operation");
#endif
}

FString UCourseworkTools::BindResidentAnimation(const FString& AnimationPath,const FString& SkeletonPath)
{
#if WITH_EDITOR
    auto* Animation=LoadObject<UAnimSequence>(nullptr,*AnimationPath);
    auto* Skeleton=LoadObject<USkeleton>(nullptr,*SkeletonPath);
    if(!Animation||!Skeleton)return TEXT("ERROR missing animation or skeleton");
    if(!Animation->ReplaceSkeleton(Skeleton,false))return TEXT("ERROR incompatible skeleton");
    Animation->MarkPackageDirty();
    return FString::Printf(TEXT("BOUND %.3f"),Animation->GetPlayLength());
#else
    return TEXT("Editor-only operation");
#endif
}

FString UCourseworkTools::PrepareKitchenSmoke()
{
#if WITH_EDITOR
    auto* Original=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/StarterContent/Particles/P_Smoke.P_Smoke"));
    if(!Original)return TEXT("ERROR: missing original smoke");
    const FString PackageName=TEXT("/Game/Coursework/Safety/P_KitchenSmoke");
    UPackage* Package=CreatePackage(*PackageName);
    auto* Smoke=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/Coursework/Safety/P_KitchenSmoke.P_KitchenSmoke"));
    if(!Smoke)Smoke=DuplicateObject<UParticleSystem>(Original,Package,TEXT("P_KitchenSmoke"));
    Smoke->SetFlags(RF_Public|RF_Standalone);
    auto SetColor=[](FRawDistributionVector& Color,UObject* Module)
    {
        auto* Grey=NewObject<UDistributionVectorConstant>(Module);Grey->Constant=FVector(.65f);
        Color=FRawDistributionVector();Color.Distribution=Grey;Color.Initialize();
    };
    TArray<FString> Notes;
    for(const auto& Emitter:Smoke->Emitters)if(Emitter)
        for(const auto& LOD:Emitter->LODLevels)if(LOD)
        {
            if(LOD->SpawnModule)
            {
                auto* Rate=NewObject<UDistributionFloatConstant>(LOD->SpawnModule);Rate->Constant=22.f;
                LOD->SpawnModule->Rate=FRawDistributionFloat();LOD->SpawnModule->Rate.Distribution=Rate;LOD->SpawnModule->Rate.Initialize();
            }
            for(const auto& Module:LOD->Modules)
            {
                if(auto* Color=Cast<UParticleModuleColor>(Module))SetColor(Color->StartColor,Color);
                if(auto* Color=Cast<UParticleModuleColorOverLife>(Module))
                {
                    Notes.Add(FString::Printf(TEXT("alpha=%.3f"),Color->AlphaOverLife.GetValue(.3f)));
                    SetColor(Color->ColorOverLife,Color);
                }
            }
        }
    Smoke->PostEditChange();Package->MarkPackageDirty();
    const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(Package,Smoke,*Filename,Args)?TEXT("Saved indoor smoke ")+FString::Join(Notes,TEXT(" ")):TEXT("ERROR: save failed");
#else
    return TEXT("Editor-only operation");
#endif
}

FString UCourseworkTools::ExportFloorplan(const FString& OutputPath)
{
#if WITH_EDITOR
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return TEXT("ERROR: no world");
    TArray<FString> Lines;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<UStaticMeshComponent*> Components; It->GetComponents(Components);
        for (auto* C : Components)
        {
            UStaticMesh* Mesh = C->GetStaticMesh();
            if (!Mesh || !Mesh->GetPathName().Contains(TEXT("/Wall/"))) continue;
            const FVector Center = C->Bounds.Origin;
            if (Center.X < -2550 || Center.X > -1550 || Center.Y < 8850 || Center.Y > 10750) continue;
            auto* Data = Mesh->GetRenderData();
            if (!Data || Data->LODResources.IsEmpty()) continue;
            const auto& LOD = Data->LODResources[0];
            const auto& Vertices = LOD.VertexBuffers.PositionVertexBuffer;
            const auto Indices = LOD.IndexBuffer.GetArrayView();
            for (int32 Floor = 0; Floor < 2; ++Floor)
            {
                const float Height = Floor == 0 ? 160.f : 420.f;
                for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
                {
                    FVector P[3];
                    for (int32 J=0; J<3; ++J) P[J]=C->GetComponentTransform().TransformPosition(FVector(Vertices.VertexPosition(Indices[I+J])));
                    TArray<FVector> Crossings;
                    for (int32 J=0; J<3; ++J)
                    {
                        const FVector& A=P[J]; const FVector& B=P[(J+1)%3];
                        if ((A.Z < Height && B.Z >= Height) || (B.Z < Height && A.Z >= Height))
                            Crossings.Add(FMath::Lerp(A,B,(Height-A.Z)/(B.Z-A.Z)));
                    }
                    if (Crossings.Num()==2 && FVector::DistSquared(Crossings[0],Crossings[1])>0.25f)
                        Lines.Add(FString::Printf(TEXT("[%d,%.2f,%.2f,%.2f,%.2f]"),Floor,Crossings[0].X,Crossings[0].Y,Crossings[1].X,Crossings[1].Y));
                }
            }
        }
    }
    if (!FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(Lines,TEXT(","))+TEXT("]"),*OutputPath)) return TEXT("ERROR: save failed");
    return FString::Printf(TEXT("Exported %d wall cross-sections"),Lines.Num());
#else
    return TEXT("Editor only");
#endif
}

FString UCourseworkTools::ReplaceLegacyInteractionPrompt(const FString& AssetPath)
{
#if WITH_EDITOR
    auto* Blueprint=LoadObject<UBlueprint>(nullptr,*AssetPath);
    if (!Blueprint) return TEXT("ERROR: character blueprint missing");
    int32 Count=0;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (auto* Graph:Graphs) for (const auto& Node:Graph->Nodes)
    {
        auto* Call=Cast<UK2Node_CallFunction>(Node.Get());
        if (!Call || (Call->FunctionReference.GetMemberName()!=TEXT("PrintString") && Call->FunctionReference.GetMemberName()!=TEXT("PrintText"))) continue;
        auto* TextPin=Call->FindPin(TEXT("InString"));
        if (!TextPin) TextPin=Call->FindPin(TEXT("InText"));
        auto* ScreenPin=Call->FindPin(TEXT("bPrintToScreen"));
        bool bLegacyPrompt = TextPin && (TextPin->DefaultValue.Contains(TEXT("Press E")) || TextPin->DefaultTextValue.ToString().Contains(TEXT("Press E")));
        if (TextPin && !bLegacyPrompt)
        {
            TArray<UEdGraphPin*> Search = TextPin->LinkedTo;
            TSet<UEdGraphNode*> Visited;
            for (int32 Depth = 0; Depth < 3 && Search.Num(); ++Depth)
            {
                TArray<UEdGraphPin*> Next;
                for (auto* Linked : Search)
                {
                    auto* Source = Linked->GetOwningNode();
                    if (Visited.Contains(Source)) continue;
                    Visited.Add(Source);
                    for (auto* Pin : Source->Pins)
                    {
                        if (Pin->Direction != EGPD_Input) continue;
                        bLegacyPrompt |= Pin->DefaultValue.Contains(TEXT("Press E")) || Pin->DefaultTextValue.ToString().Contains(TEXT("Press E"));
                        Next.Append(Pin->LinkedTo);
                    }
                }
                Search=MoveTemp(Next);
            }
        }
        if (TextPin && ScreenPin && bLegacyPrompt)
        {
            if (ScreenPin->LinkedTo.Num()!=0) continue;
            ScreenPin->DefaultValue=TEXT("false"); ++Count;
        }
    }
    if (Count)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        Blueprint->MarkPackageDirty();
    }
    return FString::Printf(TEXT("Changed %d legacy Press E nodes; compile status %d"),Count,int32(Blueprint->Status));
#else
    return TEXT("Editor-only operation");
#endif
}

FString UCourseworkTools::ExportWindowGeometry(const FString& OutputPath)
{
#if WITH_EDITOR
    UWorld* World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!World)return TEXT("ERROR world");
    TSet<UStaticMesh*> Seen;TArray<FString> Meshes;
    for(TActorIterator<AActor> It(World);It;++It){TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* C:Parts){
        UStaticMesh* Mesh=C->GetStaticMesh();if(!Mesh||(!Mesh->GetName().StartsWith(TEXT("SM_Window_"))&&!Mesh->GetName().StartsWith(TEXT("SM_Corner_")))||Seen.Contains(Mesh))continue;
        const auto P=C->Bounds.Origin;if(P.X< -2600||P.X> -1500||P.Y<8900||P.Y>10800)continue;Seen.Add(Mesh);
        auto* D=Mesh->GetMeshDescription(0);if(!D)continue;TArray<FString> Triangles;
        for(const auto ID:D->Triangles().GetElementIDs()){
            TArray<FString> Points;for(auto V:D->GetTriangleVertices(ID)){const auto Q=D->GetVertexPosition(V);Points.Add(FString::Printf(TEXT("[%.5f,%.5f,%.5f]"),Q.X,Q.Y,Q.Z));}
            Triangles.Add(FString::Printf(TEXT("{\"id\":%d,\"group\":%d,\"p\":[%s]}"),ID.GetValue(),D->GetTrianglePolygonGroup(ID).GetValue(),*FString::Join(Points,TEXT(","))));
        }
        TArray<FString> Materials;for(const auto& M:Mesh->GetStaticMaterials())Materials.Add(FString::Printf(TEXT("\"%s\""),M.MaterialInterface?*M.MaterialInterface->GetName():TEXT("none")));
        Meshes.Add(FString::Printf(TEXT("{\"path\":\"%s\",\"materials\":[%s],\"triangles\":[%s]}"),*Mesh->GetPathName(),*FString::Join(Materials,TEXT(",")),*FString::Join(Triangles,TEXT(","))));
    }}
    return FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(Meshes,TEXT(","))+TEXT("]"),*OutputPath)?TEXT("SAVED window geometry"):TEXT("ERROR save");
#else
    return TEXT("Editor only");
#endif
}
FString UCourseworkTools::PrepareWindowParts(const FString& RecipePath)
{
#if WITH_EDITOR
    FString Raw;if(!FFileHelper::LoadFileToString(Raw,*RecipePath))return TEXT("ERROR recipe");TArray<TSharedPtr<FJsonValue>> Rows;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Rows))return TEXT("ERROR json");int32 Count=0;
    for(const auto& Row:Rows){const auto O=Row->AsObject();const FString Source=O->GetStringField(TEXT("source")),Destination=O->GetStringField(TEXT("destination"));
        auto* Original=LoadObject<UStaticMesh>(nullptr,*Source);if(!Original)return TEXT("ERROR missing source");auto* Package=CreatePackage(*Destination);
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Destination);
        if(Mesh){Mesh->GetStaticMaterials()=Original->GetStaticMaterials();Mesh->CreateMeshDescription(0,*Original->GetMeshDescription(0));}
        else Mesh=DuplicateObject<UStaticMesh>(Original,Package,*FPackageName::GetLongPackageAssetName(Destination));Mesh->SetFlags(RF_Public|RF_Standalone);
        auto* D=Mesh->GetMeshDescription(0);if(!D)return TEXT("ERROR description");TSet<int32> Keep;for(const auto& V:O->GetArrayField(TEXT("triangles")))Keep.Add(int32(V->AsNumber()));
        TArray<FTriangleID> Remove;for(auto ID:D->Triangles().GetElementIDs())if(!Keep.Contains(ID.GetValue()))Remove.Add(ID);
        for(auto ID:Remove)D->DeleteTriangle(ID);FElementIDRemappings Remap;D->Compact(Remap);Mesh->CommitMeshDescription(0);
        // Source modules contain broad collision boxes enclosing the wall opening.
        // Rebuild accurate triangle collision for the separated frame and sash.
        Mesh->CreateBodySetup();Mesh->GetBodySetup()->RemoveSimpleCollision();Mesh->GetBodySetup()->CollisionTraceFlag=CTF_UseComplexAsSimple;Mesh->GetBodySetup()->bDoubleSidedGeometry=true;
        for(int32 L=Mesh->GetNumSourceModels()-1;L>0;--L)Mesh->RemoveSourceModel(L);
        Mesh->Build(false);Mesh->PostEditChange();Package->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        const FString File=FPackageName::LongPackageNameToFilename(Destination,FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(Package,Mesh,*File,Args))return TEXT("ERROR saving part");++Count;
    }
    return FString::Printf(TEXT("SAVED %d window parts"),Count);
#else
    return TEXT("Editor only");
#endif
}

FString UCourseworkTools::PrepareVentilationLeaves()
{
#if WITH_EDITOR
 auto* Source=LoadObject<UMaterialInstanceConstant>(nullptr,TEXT("/Game/HouseFurniture/Materials/Generic/MI_Plant_A_Leaves.MI_Plant_A_Leaves"));if(!Source)return TEXT("ERROR leaf source");
 const FString BasePath=TEXT("/Game/Coursework/Ventilation/M_LeavesWind"),MIPath=TEXT("/Game/Coursework/Ventilation/MI_LeavesWind");
 auto* BasePackage=CreatePackage(*BasePath);auto* M=DuplicateObject<UMaterial>(Source->GetBaseMaterial(),BasePackage,TEXT("M_LeavesWind"));M->SetFlags(RF_Public|RF_Standalone);
 auto* Custom=NewObject<UMaterialExpressionCustom>(M);Custom->Material=M;Custom->OutputType=CMOT_Float3;
 Custom->Code=TEXT("return float3(-1,0.18,0)*WindAmount*(0.65*sin(WindTime*1.7+WindPos.x*0.013+WindPos.y*0.009)+0.35*sin(WindTime*2.9+WindPos.z*0.04));");
 auto* Time=NewObject<UMaterialExpressionTime>(M);auto* Pos=NewObject<UMaterialExpressionWorldPosition>(M);auto* Amount=NewObject<UMaterialExpressionScalarParameter>(M);Amount->ParameterName=TEXT("CourseworkWindAmount");Amount->DefaultValue=0;
 Custom->Inputs.Empty();FCustomInput T;T.InputName=TEXT("WindTime");T.Input.Connect(0,Time);Custom->Inputs.Add(T);FCustomInput P;P.InputName=TEXT("WindPos");P.Input.Connect(0,Pos);Custom->Inputs.Add(P);FCustomInput A;A.InputName=TEXT("WindAmount");A.Input.Connect(0,Amount);Custom->Inputs.Add(A);
 auto& Expressions=M->GetExpressionCollection();Expressions.AddExpression(Custom);Expressions.AddExpression(Time);Expressions.AddExpression(Pos);Expressions.AddExpression(Amount);
 auto& WPO=M->GetEditorOnlyData()->WorldPositionOffset;
 if(WPO.Expression){auto* Add=NewObject<UMaterialExpressionAdd>(M);Add->A=WPO;Add->B.Connect(0,Custom);Expressions.AddExpression(Add);WPO.Connect(0,Add);}else WPO.Connect(0,Custom);
 M->PostEditChange();BasePackage->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 if(!UPackage::SavePackage(BasePackage,M,*FPackageName::LongPackageNameToFilename(BasePath,FPackageName::GetAssetPackageExtension()),Args))return TEXT("ERROR leaf material save");
 auto* MIPackage=CreatePackage(*MIPath);auto* MI=DuplicateObject<UMaterialInstanceConstant>(Source,MIPackage,TEXT("MI_LeavesWind"));MI->SetFlags(RF_Public|RF_Standalone);MI->SetParentEditorOnly(M);MI->PostEditChange();MIPackage->MarkPackageDirty();
 return UPackage::SavePackage(MIPackage,MI,*FPackageName::LongPackageNameToFilename(MIPath,FPackageName::GetAssetPackageExtension()),Args)?TEXT("SAVED independent leaf wind material"):TEXT("ERROR leaf instance save");
#else
 return TEXT("Editor only");
#endif
}

FString UCourseworkTools::PrepareCurtainWind()
{
#if WITH_EDITOR
 const TCHAR* Paths[]={TEXT("/Game/HouseFurniture/Materials/MI_Curtains_A1"),TEXT("/Game/HouseFurniture/Materials/MI_Curtains_A2"),TEXT("/Game/HouseFurniture/Materials/MI_Curtains_A3_Illum1"),TEXT("/Game/HouseFurniture/Materials/Generic/MI_Fabric_C1"),TEXT("/Game/HouseFurniture/Materials/Generic/MI_Fabric_C7")};
 for(const auto* Path:Paths){auto* Source=LoadObject<UMaterialInstanceConstant>(nullptr,Path);if(!Source)return FString(TEXT("ERROR curtain source "))+Path;
 const FString Name=Source->GetName(),BasePath=TEXT("/Game/Coursework/Ventilation/Curtains/M_")+Name,MIPath=TEXT("/Game/Coursework/Ventilation/Curtains/")+Name+TEXT("_Wind");
 auto* Package=CreatePackage(*BasePath);auto* M=DuplicateObject<UMaterial>(Source->GetBaseMaterial(),Package,*(TEXT("M_")+Name));M->SetFlags(RF_Public|RF_Standalone);
 auto* C=NewObject<UMaterialExpressionCustom>(M);C->Material=M;C->OutputType=CMOT_Float3;C->Code=TEXT("float h=saturate((Top-P.z)/max(Height,1)); h=h*h; float w=sin(T*1.35+P.x*.025+P.y*.019)+.28*sin(T*2.3+P.z*.038); return float3(Dx,Dy,0)*Amount*h*w;");
 auto& Expressions=M->GetExpressionCollection();Expressions.AddExpression(C);
 auto Input=[&](const TCHAR* N,UMaterialExpression* E){FCustomInput I;I.InputName=N;I.Input.Connect(0,E);C->Inputs.Add(I);Expressions.AddExpression(E);};
 auto* Pos=NewObject<UMaterialExpressionWorldPosition>(M);auto* Time=NewObject<UMaterialExpressionTime>(M);Input(TEXT("P"),Pos);Input(TEXT("T"),Time);
 struct FParam{const TCHAR* Input;const TCHAR* Parameter;float Default;};const FParam Params[]={{TEXT("Top"),TEXT("CurtainTop"),550},{TEXT("Height"),TEXT("CurtainHeight"),180},{TEXT("Dx"),TEXT("CurtainDX"),-1},{TEXT("Dy"),TEXT("CurtainDY"),0},{TEXT("Amount"),TEXT("CurtainWindAmount"),0}};
 for(const auto& P:Params){auto* V=NewObject<UMaterialExpressionScalarParameter>(M);V->ParameterName=P.Parameter;V->DefaultValue=P.Default;Input(P.Input,V);}
 auto& WPO=M->GetEditorOnlyData()->WorldPositionOffset;if(WPO.Expression){auto* Add=NewObject<UMaterialExpressionAdd>(M);Add->A=WPO;Add->B.Connect(0,C);Expressions.AddExpression(Add);WPO.Connect(0,Add);}else WPO.Connect(0,C);
 M->PostEditChange();Package->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 if(!UPackage::SavePackage(Package,M,*FPackageName::LongPackageNameToFilename(BasePath,FPackageName::GetAssetPackageExtension()),Args))return TEXT("ERROR curtain base save");
 auto* MIPackage=CreatePackage(*MIPath);auto* MI=DuplicateObject<UMaterialInstanceConstant>(Source,MIPackage,*(Name+TEXT("_Wind")));MI->SetFlags(RF_Public|RF_Standalone);MI->SetParentEditorOnly(M);MI->PostEditChange();MIPackage->MarkPackageDirty();
 if(!UPackage::SavePackage(MIPackage,MI,*FPackageName::LongPackageNameToFilename(MIPath,FPackageName::GetAssetPackageExtension()),Args))return TEXT("ERROR curtain instance save");
 }
 return TEXT("SAVED five independent curtain wind materials");
#else
 return TEXT("Editor only");
#endif
}
