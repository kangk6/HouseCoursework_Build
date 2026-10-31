#pragma once
#include "CoreMinimal.h"

// Dimensionless wind-driven teaching network. Openings use an orifice-like
// square-root pressure law, not calibrated m3/s, CFD or a thermal/stack model.
namespace CourseworkAirNetwork
{
struct FEdge
{
 int32 A, B; // B == INDEX_NONE means an exterior pressure boundary.
 float Conductance, OutsidePressure=0, Flow=0; // positive A -> B
};
inline float Solve(int32 Count,TArray<FEdge>& Edges,TArray<float>& Pressure)
{
 Pressure.Init(0,Count);
 // Anchor only components connected to an exterior opening. A sealed component
 // has no pressure reference and must not make the Newton matrix singular.
 TArray<bool> Anchored;Anchored.Init(false,Count);
 for(const auto& E:Edges)if(E.B==INDEX_NONE&&E.Conductance>0)Anchored[E.A]=true;
 for(int32 K=0;K<Count;++K)for(const auto& E:Edges)if(E.B!=INDEX_NONE&&E.Conductance>0&&(Anchored[E.A]||Anchored[E.B]))Anchored[E.A]=Anchored[E.B]=true;
 TArray<double> P;P.Init(0,Count);
 auto Q=[](double C,double D){return C*D/FMath::Sqrt(FMath::Max(FMath::Abs(D),1.e-4));};
 auto Evaluate=[&](const TArray<double>& Values,TArray<double>& Balance)
 {
  Balance.Init(0,Count);
  for(const auto& E:Edges){const double F=Q(E.Conductance,Values[E.A]-(E.B==INDEX_NONE?E.OutsidePressure:Values[E.B]));Balance[E.A]+=F;if(E.B!=INDEX_NONE)Balance[E.B]-=F;}
  double Norm=0;for(double B:Balance)Norm+=B*B;return Norm;
 };
 TArray<double> Balance;double Norm=Evaluate(P,Balance);
 for(int32 Iter=0;Iter<64&&Norm>1.e-16;++Iter)
 {
  TArray<double> J,Step;J.Init(0,Count*Count);Step.Init(0,Count);
  for(const auto& E:Edges)
  {
   const double D=FMath::Abs(P[E.A]-(E.B==INDEX_NONE?E.OutsidePressure:P[E.B]));
   const double G=E.Conductance*(D<1.e-4?100.:.5/FMath::Sqrt(D));
   J[E.A*Count+E.A]+=G;
   if(E.B!=INDEX_NONE){J[E.B*Count+E.B]+=G;J[E.A*Count+E.B]-=G;J[E.B*Count+E.A]-=G;}
  }
  for(int32 R=0;R<Count;++R){Step[R]=-Balance[R];if(!Anchored[R]){for(int32 K=0;K<Count;++K){J[R*Count+K]=0;J[K*Count+R]=0;}J[R*Count+R]=1;Step[R]=0;}}
  // Dense pivoted solve is small (ten zones) and avoids the very slow convergence
  // of coordinate iteration on a high-conductance dead-end room.
  bool Valid=true;
  for(int32 C=0;C<Count;++C)
  {
   int32 Pivot=C;for(int32 R=C+1;R<Count;++R)if(FMath::Abs(J[R*Count+C])>FMath::Abs(J[Pivot*Count+C]))Pivot=R;
   if(FMath::Abs(J[Pivot*Count+C])<1.e-12){Valid=false;break;}
   if(Pivot!=C){for(int32 K=C;K<Count;++K)Swap(J[C*Count+K],J[Pivot*Count+K]);Swap(Step[C],Step[Pivot]);}
   for(int32 R=C+1;R<Count;++R){const double Factor=J[R*Count+C]/J[C*Count+C];for(int32 K=C;K<Count;++K)J[R*Count+K]-=Factor*J[C*Count+K];Step[R]-=Factor*Step[C];}
  }
  if(!Valid)break;
  for(int32 R=Count-1;R>=0;--R){for(int32 C=R+1;C<Count;++C)Step[R]-=J[R*Count+C]*Step[C];Step[R]/=J[R*Count+R];}
  bool Accepted=false;
  for(double Scale=1;Scale>=1./4096;Scale*=.5)
  {
   TArray<double> Candidate=P,NextBalance;
   for(int32 R=0;R<Count;++R)Candidate[R]+=Scale*Step[R];
   const double NextNorm=Evaluate(Candidate,NextBalance);
   if(NextNorm<Norm){P=MoveTemp(Candidate);Balance=MoveTemp(NextBalance);Norm=NextNorm;Accepted=true;break;}
  }
  if(!Accepted)break;
 }
 double Residual=0;Evaluate(P,Balance);for(double B:Balance)Residual=FMath::Max(Residual,FMath::Abs(B));
 for(int32 R=0;R<Count;++R)Pressure[R]=float(P[R]);
 for(auto& E:Edges)E.Flow=float(Q(E.Conductance,P[E.A]-(E.B==INDEX_NONE?E.OutsidePressure:P[E.B])));
 return float(Residual);
}
// Pose is the same smoothed fraction used to draw the opening animation.
inline float Aperture(float Pose,float Angle)
{
 Pose=FMath::Clamp(Pose,0.f,1.f);
 return FMath::IsNearlyZero(Angle)?Pose:FMath::Sin(FMath::DegreesToRadians(FMath::Min(90.f,FMath::Abs(Angle))*Pose));
}
inline float Response(float Exchange){return FMath::Clamp(Exchange/(Exchange+.16f),0.f,1.f);}
}
