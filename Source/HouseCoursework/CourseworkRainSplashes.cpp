#include "CourseworkRain.h"
#include "Components/InstancedStaticMeshComponent.h"

void ACourseworkRain::UpdateSplashes(float Amount,float Dt,FVector View)
{
    if(!SplashMesh)return;
    if(Amount<=.01f){VisibleSplashes=0;SplashMesh->SetVisibility(false);for(auto& S:SplashPool)S.Active=false;return;}
    TArray<int32> Eligible;
    for(int32 I=0;I<Columns.Num();++I)if(Columns[I].Hard&&FVector::DistSquared(Columns[I].Impact,View)<950*950)Eligible.Add(I);
    VisibleSplashes=0;
    for(int32 I=0;I<SplashPool.Num();++I)
    {
        auto& S=SplashPool[I];S.Age+=Dt;
        if(S.Age>=S.Life||!S.Active)
        {
            S.Active=Amount>.05f&&Eligible.Num()>0&&VisualRandom.FRand()<Amount;
            S.Age=VisualRandom.FRandRange(-.12f,0.f);S.Life=VisualRandom.FRandRange(.22f,.32f);
            if(S.Active){const auto& C=Columns[Eligible[VisualRandom.RandRange(0,Eligible.Num()-1)]];S.Position=C.Impact+3*C.Normal;S.Normal=C.Normal;}
        }
        const bool Active=S.Active&&S.Age>=0&&S.Age<S.Life&&Amount>.01f;
        VisibleSplashes+=Active?1:0;const FQuat Surface=FRotationMatrix::MakeFromZ(S.Normal).ToQuat();
        const float T=FMath::Clamp(S.Age/FMath::Max(.01f,S.Life),0.f,1.f),Fade=Active?1-T:0;
        for(int32 J=0;J<9;++J)
        {
            const float Angle=2*PI*(J%6)/6.f;const FVector Radial(FMath::Cos(Angle),FMath::Sin(Angle),0);FVector Local;FVector Scale;
            if(J<6)
            {
                const float Radius=.5f+3*T;Local=Radial*Radius;Local.Z=.15f;
                Scale=FVector((1+T)*.018f,.002f*Fade,1);
            }
            else
            {
                Local=Radial*(1+T*3);Local.Z=FMath::Max(0.f,45*S.Age-250*S.Age*S.Age);
                Scale=FVector(.006f*Fade,.009f*Fade,1);
            }
            FQuat Rotation=Surface*FQuat(FVector::UpVector,Angle+PI/2);
            if(!Active)Scale=FVector::ZeroVector;
            SplashMesh->UpdateInstanceTransform(I*9+J,FTransform(Rotation,S.Position+Surface.RotateVector(Local),Scale),false,false,true);
        }
    }
    SplashMesh->SetVisibility(VisibleSplashes>0);if(VisibleSplashes>0)SplashMesh->MarkRenderStateDirty();
}
bool ACourseworkRain::CheckSplashes()const
{
    if(!SplashMesh)return false;
    for(const auto& S:SplashPool)if(S.Active){if(S.Normal.Z<=.45f)return false;bool Found=false;for(const auto& C:Columns)if(C.Hard&&FVector::DistSquared(S.Position,C.Impact+3*C.Normal)<1)Found=true;if(!Found)return false;}
    return true;
}
