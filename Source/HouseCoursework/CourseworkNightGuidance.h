#pragma once
#include "CoreMinimal.h"

namespace CourseworkNightGuidance
{
// Real walking velocity supplies Heading; camera rotation never does.
inline float AheadScore(FVector Delta,FVector Heading,bool Walking,float Range=340.f)
{
 const float Distance=Delta.Size2D();
 if(!Walking||Distance<60||Distance>Range)return MAX_flt;
 const float Forward=FVector::DotProduct(Delta.GetSafeNormal2D(),Heading.GetSafeNormal2D());
 if(Forward<.35f)return MAX_flt;
 return Distance*(1.f+2.f*(1.f-Forward));
}
inline float Fade(float Current,float Target,float Dt,bool Gentle)
{
 const float Rate=Gentle?(Target>Current?1.2f:.45f):1.2f;
 return FMath::FInterpConstantTo(Current,Target,Dt,Rate);
}
}
