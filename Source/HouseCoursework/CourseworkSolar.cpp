#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RotationMatrix.h"

void ACourseworkHUD::InitializeSolar()
{
    if(SolarInstallation)return;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if(!Cube||!Base){UE_LOG(LogTemp,Error,TEXT("SOLAR_INSTALL missing cooked cube/material"));return;}
    SolarInstallation=GetWorld()->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(SolarInstallation);SolarInstallation->AddInstanceComponent(Root);SolarInstallation->SetRootComponent(Root);Root->RegisterComponent();
    auto Group=[&](FLinearColor Color,float Roughness)
    {
        auto* C=NewObject<UInstancedStaticMeshComponent>(SolarInstallation);SolarInstallation->AddInstanceComponent(C);
        C->SetupAttachment(Root);C->SetStaticMesh(Cube);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
        auto* M=UMaterialInstanceDynamic::Create(Base,C);M->SetVectorParameterValue(TEXT("Color"),Color);M->SetScalarParameterValue(TEXT("Roughness"),Roughness);
        C->SetMaterial(0,M);C->RegisterComponent();return C;
    };
    auto* Frame=Group(FLinearColor(.19f,.22f,.24f),.35f);
    auto* Cells=Group(FLinearColor(.012f,.027f,.052f),.23f);
    auto* Lines=Group(FLinearColor(.12f,.17f,.22f),.45f);Lines->SetCastShadow(false);
    // Two rows by three columns, clear of roof ends. Complex traces select the
    // authored roof surface, never the ground, furniture or a collision box.
    for(double X:{-2225.,-2375.})for(double Y:{9520.,9800.,10080.})
    {
        FHitResult Best;bool Found=false;double Highest=-DBL_MAX;
        for(TActorIterator<AActor> It(GetWorld());It;++It)
        {
            TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);
            for(auto* C:Parts)if(C->GetStaticMesh()&&C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_Roof_C")))
            {
                FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(SolarRoof),true);
                if(C->LineTraceComponent(Hit,FVector(X,Y,1300),FVector(X,Y,550),Q)&&Hit.ImpactPoint.Z>Highest&&Hit.ImpactNormal.Z>.35)
                {Highest=Hit.ImpactPoint.Z;Best=Hit;Found=true;}
            }
        }
        if(!Found){UE_LOG(LogTemp,Error,TEXT("SOLAR_INSTALL no sloped roof at %.0f %.0f"),X,Y);continue;}
        const FVector N=Best.ImpactNormal.GetSafeNormal();const FQuat R=FRotationMatrix::MakeFromXZ(FVector::YAxisVector,N).ToQuat();
        const FVector Center=Best.ImpactPoint+N*7;
        auto Part=[&](UInstancedStaticMeshComponent* G,FVector Offset,FVector Size){G->AddInstance(FTransform(R,Center+R.RotateVector(Offset),Size/100.),true);};
        Part(Frame,FVector::ZeroVector,FVector(170,110,3));
        Part(Cells,FVector(0,0,1.8),FVector(165,105,.6));
        for(int32 I=1;I<6;++I)Part(Lines,FVector(-82.5+I*27.5,0,2.2),FVector(.25,105,.1));
        for(int32 I=1;I<10;++I)Part(Lines,FVector(0,-52.5+I*10.5,2.2),FVector(165,.2,.1));
        for(float T:{-55.f,55.f})Part(Frame,FVector(T,0,-4),FVector(4,114,6));
        SolarPanels.Add({Center,N,R});
        UE_LOG(LogTemp,Display,TEXT("SOLAR_PANEL center=%s normal=%s rated_w=360"),*Center.ToString(),*N.ToString());
    }
    UE_LOG(LogTemp,Display,TEXT("SOLAR_READY panels=%d rated_w=%.0f"),SolarPanels.Num(),GetSolarRatedWatts());
}

bool ACourseworkHUD::IsEnergySuspended()const
{
    return !bLightingReady||!IsDaylightActive()||bStartupCover||IsTimeSuspended();
}
void ACourseworkHUD::TickSolar(float DeltaSeconds)
{
    if(!bLightingReady)return;
    double Power=0;
    if(Daylight&&Daylight->IsActive())
    {
        const FVector ToSun=-Daylight->GetSunRotation().Vector();
        for(const auto& Panel:SolarPanels)Power+=CourseworkSolar::Power(ToSun,Panel.Normal,360.,Weather.Direct(),Weather.Diffuse());
    }
    SolarFlow=FCourseworkSolarFlow::Balance(Power,GetHouseholdWatts());
    // Fixed solar time still consumes energy. Seeking supplies no elapsed time.
    // DeltaSeconds follows game time; native pause policy additionally gates it.
    if(!IsEnergySuspended()&&FMath::IsFinite(DeltaSeconds)&&DeltaSeconds>0)
        SolarMeter.Add(SolarFlow,double(DeltaSeconds)*1440./Clock.DayMinutes);
}
