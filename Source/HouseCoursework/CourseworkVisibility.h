#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

namespace CourseworkVisibility
{
inline bool IsOpaqueViewBlocked(UWorld* World,AActor* Pawn,AActor* Lamp,const FVector& Eye,const FVector& Point)
{
    // Authored thin architectural surfaces have one-sided collision. Check
    // both directions so the back of an opaque wall also occludes a lamp.
    FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LampOpaqueOcclusion),true,Pawn);Query.AddIgnoredActor(Lamp);Query.bReturnFaceIndex=true;
    for(bool Reverse:{false,true})
    {
        TArray<FHitResult> Surfaces;
        World->LineTraceMultiByObjectType(Surfaces,Reverse?Point:Eye,Reverse?Eye:Point,Objects,Query);
        for(const auto& Surface:Surfaces)
        {
            auto* Mesh=Cast<UStaticMeshComponent>(Surface.GetComponent());
            if(!Mesh||!Mesh->IsVisible()||Mesh->bHiddenInGame||FVector::Dist(Surface.ImpactPoint,Eye)<1.f||FVector::Dist(Surface.ImpactPoint,Point)<1.f)continue;
            int32 Section=0;const auto* Material=Mesh->GetMaterialFromCollisionFaceIndex(Surface.FaceIndex,Section);
            if(Material&&(Material->GetBlendMode()==BLEND_Translucent||Material->GetBlendMode()==BLEND_Additive||Material->GetBlendMode()==BLEND_Modulate))continue;
            return true;
        }
    }
    return false;
}
}
