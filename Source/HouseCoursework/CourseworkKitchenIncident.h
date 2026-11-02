#pragma once
#include "CoreMinimal.h"

// Relative teaching model. Electrical isolation removes the heat source; it
// never extinguishes already ignited fuel. Sensor response has a short lag.
struct FCourseworkKitchenIncident
{
 float Heat=.65f,Smoke=0,Sensor=0,Fire=0,Elapsed=0,IsolationAt=-1;
 bool Isolated=false,Ignited=false;
 void Isolate(){if(!Isolated){Isolated=true;IsolationAt=Elapsed;}}
 bool Cleared()const{return Isolated&&!Ignited&&Heat<.45f&&Smoke<3&&Sensor<3;}
 void Advance(float Seconds)
 {
  float Remaining=FMath::Clamp(Seconds,0.f,120.f);
  while(Remaining>0)
  {
   const float Dt=FMath::Min(Remaining,.05f);Remaining-=Dt;Elapsed+=Dt;
   Heat=FMath::Clamp(Heat+(Isolated?-.08f:.025f)*Dt,0.f,1.3f);
   if(!Isolated&&Heat>=1)Ignited=true;
   if(Ignited)Fire=FMath::Clamp(Fire+(Isolated?.005f:.03f)*Dt,.3f,1.f);
   const float Generation=(!Isolated?15.f*Heat:0)+Fire*8;
   Smoke=FMath::Clamp(Smoke+(Generation-(Generation>0?1.f:3.f))*Dt,0.f,100.f);
   Sensor+=(Smoke-Sensor)*(1-FMath::Exp(-Dt));
  }
 }
};
