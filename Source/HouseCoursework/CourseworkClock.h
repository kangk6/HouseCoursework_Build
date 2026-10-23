#pragma once
#include "CoreMinimal.h"

// One simulation clock. Seeking never counts as elapsed simulation time.
// No UObject state or household save data: every session starts at noon.
struct FCourseworkClock
{
    double Hour=12;
    double ElapsedSimulationSeconds=0;
    int64 Day=1;
    float DayMinutes=24;
    bool bRunning=true;

    static double Wrap(double Value)
    {
        const double R=FMath::Fmod(Value,24.0);
        return R<0?R+24:R;
    }
    bool Seek(double Value)
    {
        if(!FMath::IsFinite(Value))return false;
        Hour=Wrap(Value);return true;
    }
    bool SetDayMinutes(float Value)
    {
        if(!FMath::IsFinite(Value))return false;
        DayMinutes=FMath::Clamp(Value,6.f,120.f);return true;
    }
    void Advance(double RealSeconds,bool Suspended)
    {
        if(!bRunning||Suspended||!FMath::IsFinite(RealSeconds)||RealSeconds<=0)return;
        const double Hours=RealSeconds/(double(DayMinutes)*60.0)*24.0;
        const double Next=Hour+Hours;
        Day+=int64(FMath::FloorToDouble(Next/24.0));
        Hour=Wrap(Next);ElapsedSimulationSeconds+=Hours*3600.0;
    }
};
