#pragma once
#include "CoreMinimal.h"

// Seeded weather schedule; random decisions happen at hold boundaries. Seconds use the household clock.
struct FCourseworkWeather
{
    FRandomStream Random;
    int32 Kind=0,Seed=0,Changes=0;
    bool Automatic=true;
    double Hold=0,Phase=0,BlendAge=10;
    float Cover=.1f,From=.1f,To=.1f,NoisePhase=0;
    void Initialize(int32 Value,bool Auto=true)
    {
        *this=FCourseworkWeather();Seed=Value;Random.Initialize(Value);Automatic=Auto;
        NoisePhase=Random.FRandRange(0,6.2831853f);Hold=Random.FRandRange(7200,14400);
    }
    static float Base(int32 Type){const float C[]={.1f,.5f,.92f};return C[FMath::Clamp(Type,0,2)];}
    void Choose(int32 Type,bool Manual=true)
    {
        if(Manual)Automatic=false;
        Kind=FMath::Clamp(Type,0,2);From=Cover;To=Base(Kind);BlendAge=0;
        Hold=Random.FRandRange(7200,14400);
    }
    void SetAutomatic(bool Enabled){Automatic=Enabled;Hold=Random.FRandRange(7200,14400);}
    void Advance(double RealSeconds,double SimSeconds,bool Flowing,bool Suspended)
    {
        if(Suspended||!FMath::IsFinite(RealSeconds)||RealSeconds<=0)return;
        if(Automatic&&!Flowing)return;
        if(Flowing&&FMath::IsFinite(SimSeconds)&&SimSeconds>0)
        {
            Phase+=SimSeconds;
            if(Automatic&&BlendAge>=10)
            {
                Hold-=SimSeconds;
                if(Hold<=0)
                {
                    const float Roll=Random.FRand();int32 Next=Kind;
                    if(Kind==0&&Roll<.7f)Next=1;
                    else if(Kind==2&&Roll<.7f)Next=1;
                    else if(Kind==1)Next=Roll<.35f?0:Roll<.7f?2:1;
                    if(Next!=Kind){Choose(Next,false);++Changes;}else Hold=Random.FRandRange(7200,14400);
                }
            }
        }
        BlendAge=FMath::Min(10.,BlendAge+RealSeconds);
        const float W=FMath::SmoothStep(0.f,1.f,float(BlendAge/10));
        // Clouds within the partly cloudy preset drift on 20-40 minute scales.
        const float Drift=Kind==1?.10f*FMath::Sin(float(Phase/1800)+NoisePhase)+.035f*FMath::Sin(float(Phase/370)+NoisePhase):0;
        Cover=FMath::Lerp(From,FMath::Clamp(To+Drift,.1f,.92f),W);
    }
    float Amount()const{return FMath::Clamp((Cover-.1f)/.82f,0.f,1.f);}
    float Direct()const{return FMath::Lerp(1.f,.12f,Amount());}
    float Diffuse()const{const float T=Amount();return FMath::Lerp(1.f,1.4f,T)+.4f*4*T*(1-T);}
    float Daylight()const{return FMath::Lerp(1.f,.35f,Amount());}
};
