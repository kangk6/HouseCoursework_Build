#include "CourseworkDaylight.h"
#include "Components/DirectionalLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void ACourseworkDaylight::UpdateCelestial(float SolarElevation,float Cloud)
{
 if(!CelestialMaterial)return;
 const float Clear=1-FMath::SmoothStep(.60f,.98f,Cloud);
 CelestialSunAmount=PreviewWeight*FMath::SmoothStep(-1.f,2.f,SolarElevation)*Clear;
 CelestialMoonAmount=PreviewWeight*FMath::SmoothStep(-1.f,4.f,-SolarElevation)*Clear;
 CelestialStarAmount=PreviewWeight*FMath::SmoothStep(3.f,14.f,-SolarElevation)*Clear;
 const FVector SunDirection=-Sun->GetForwardVector(),MoonDirection=-SunDirection;
 Moon->SetWorldRotation((-MoonDirection).Rotation());
 Moon->SetIntensity(.12f*CelestialMoonAmount*FMath::SmoothStep(0.f,20.f,-SolarElevation));
 Moon->SetLightColor(FLinearColor(.72f,.8f,1.f));Moon->SetVisibility(true);
 const FLinearColor SunTint=FMath::Lerp(FLinearColor(1,.38f,.12f),FLinearColor(1,.95f,.8f),FMath::Clamp(SolarElevation/35.f,0.f,1.f));
 for(const auto& S:WeatherSkies)if(auto* M=S.Material.Get())
 {
  M->SetScalarParameterValue(TEXT("Sun brightness"),0);M->SetScalarParameterValue(TEXT("Stars brightness"),0);
  M->SetVectorParameterValue(TEXT("CelestialSunDir"),FLinearColor(SunDirection.X,SunDirection.Y,SunDirection.Z));
  M->SetVectorParameterValue(TEXT("CelestialMoonDir"),FLinearColor(MoonDirection.X,MoonDirection.Y,MoonDirection.Z));
  M->SetVectorParameterValue(TEXT("CelestialSunColor"),SunTint);
  M->SetScalarParameterValue(TEXT("CelestialSunAmount"),CelestialSunAmount);
  M->SetScalarParameterValue(TEXT("CelestialMoonAmount"),CelestialMoonAmount);
  M->SetScalarParameterValue(TEXT("CelestialStarAmount"),CelestialStarAmount);
  M->SetScalarParameterValue(TEXT("CelestialTwilightAmount"),PreviewWeight*FMath::SmoothStep(-10.f,-1.f,SolarElevation)*(1-FMath::SmoothStep(2.f,14.f,SolarElevation)));
  M->SetScalarParameterValue(TEXT("CelestialSpin"),RenderedHour/24.f*2*PI);
 }
}
bool ACourseworkDaylight::HasCelestialSky()const{return CelestialMaterial&&WeatherSkies.Num()>0;}
FVector ACourseworkDaylight::GetMoonDirection()const{return -Moon->GetForwardVector();}
float ACourseworkDaylight::GetMoonIntensity()const{return Moon->Intensity;}
bool ACourseworkDaylight::CheckCelestialParameters()const
{
 if(!HasCelestialSky())return false;
 for(const auto& S:WeatherSkies)if(auto* M=S.Material.Get())
 {
  if(M->K2_GetScalarParameterValue(TEXT("Sun brightness"))!=0||M->K2_GetScalarParameterValue(TEXT("Stars brightness"))!=0)return false;
  if(!FMath::IsNearlyEqual(M->K2_GetScalarParameterValue(TEXT("CelestialMoonAmount")),CelestialMoonAmount))return false;
  const auto D=M->K2_GetVectorParameterValue(TEXT("CelestialSunDir"));
  if(!FVector(D.R,D.G,D.B).Equals(-Sun->GetForwardVector(),.001f))return false;
 }
 return Sun->AtmosphereSunDiskColorScale.Equals(FLinearColor::Black)&&GetMoonDirection().Equals(Sun->GetForwardVector(),.001f);
}
