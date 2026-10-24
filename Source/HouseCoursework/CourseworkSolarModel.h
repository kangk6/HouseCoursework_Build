#pragma once
#include "CoreMinimal.h"

// Teaching model, watts and kWh. Render intensity is never an irradiance input.
struct FCourseworkSolarFlow
{
    double Generation=0,Load=0,SelfUse=0,Import=0,Export=0;
    static FCourseworkSolarFlow Balance(double PV,double Demand)
    {
        FCourseworkSolarFlow R;
        R.Generation=FMath::IsFinite(PV)?FMath::Max(0.,PV):0;
        R.Load=FMath::IsFinite(Demand)?FMath::Max(0.,Demand):0;
        R.SelfUse=FMath::Min(R.Generation,R.Load);
        R.Import=R.Load-R.SelfUse;R.Export=R.Generation-R.SelfUse;
        return R;
    }
};
struct FCourseworkSolarMeter
{
    double Seconds=0,Generation=0,Load=0,SelfUse=0,Import=0,Export=0;
    void Add(const FCourseworkSolarFlow& Flow,double SimulationSeconds)
    {
        if(!FMath::IsFinite(SimulationSeconds)||SimulationSeconds<=0)return;
        Seconds+=SimulationSeconds;const double K=SimulationSeconds/3600000.;
        Generation+=Flow.Generation*K;Load+=Flow.Load*K;SelfUse+=Flow.SelfUse*K;
        Import+=Flow.Import*K;Export+=Flow.Export*K;
    }
};
namespace CourseworkSolar
{
// Fixed clear-sky approximation. Altitude attenuates DNI; tilt sets diffuse view.
// No seasonal/weather/temperature/occlusion forecast is implied.
inline double Power(FVector ToSun,FVector Normal,double RatedWatts,double DirectFactor=1,double DiffuseFactor=1)
{
    if(ToSun.ContainsNaN()||Normal.ContainsNaN()||!FMath::IsFinite(RatedWatts)||RatedWatts<=0)return 0;
    ToSun=ToSun.GetSafeNormal();Normal=Normal.GetSafeNormal();
    const double Height=ToSun.Z;if(Height<=0||Normal.IsNearlyZero())return 0;
    const double DNI=900.*FMath::SmoothStep(0.,.25,Height)*FMath::Clamp(DirectFactor,0.,1.);
    const double Diffuse=100.*Height*(1.+FMath::Clamp(Normal.Z,-1.,1.))*.5*FMath::Clamp(DiffuseFactor,0.,2.);
    const double POA=DNI*FMath::Max(0.,FVector::DotProduct(ToSun,Normal))+Diffuse;
    // Nameplate already includes panel conversion. 0.90 combines remaining losses.
    return FMath::Min(RatedWatts,RatedWatts*POA/1000.*.90);
}
}
