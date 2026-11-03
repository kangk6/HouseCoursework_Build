#include "CourseworkHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

void ACourseworkHUD::InitializeFireSpread()
{
 // Authored fuel sites inside this kitchen, not unrestricted propagation.
 const FVector Offsets[]={{-40,85,-18},{-25,0,70},{-40,-85,-18}};
 for(const FVector& Offset:Offsets)
 {
  auto* Fire=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),SafetyFireTemplate,AlarmLocation+Offset,FRotator::ZeroRotator,FVector(.01f),false,EPSCPoolMethod::None,false);
  if(Fire)Fire->SetComponentTickEnabled(false);SpreadFires.Add(Fire);
 }
 auto* Rig=GetWorld()->SpawnActor<AActor>();
 for(int32 I=0;I<2;I++)
 {
  auto* Light=NewObject<UPointLightComponent>(Rig);Rig->AddInstanceComponent(Light);Light->SetMobility(EComponentMobility::Movable);Light->SetLightColor(FLinearColor(1,.19f,.025f));Light->SetCastShadows(false);Light->SetAttenuationRadius(220);Light->SetIntensity(0);Light->RegisterComponent();Light->SetWorldLocation(AlarmLocation+Offsets[I]+FVector(-25,0,20));SpreadLights.Add(Light);
 }
 SpreadSmoke=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),SafetySmokeTemplate,AlarmLocation+FVector(-45,0,45),FRotator::ZeroRotator,FVector(.01f),false,EPSCPoolMethod::None,false);
 if(SpreadSmoke){if(SafetySmokeMaterial)SpreadSmoke->SetMaterial(0,SafetySmokeMaterial);SpreadSmoke->SetComponentTickEnabled(false);}
}
void ACourseworkHUD::ResetFireSpread()
{
 for(UParticleSystemComponent* Fire:SpreadFires)if(Fire){Fire->DeactivateImmediate();Fire->SetComponentTickEnabled(false);}
 if(SpreadSmoke){SpreadSmoke->DeactivateImmediate();SpreadSmoke->SetComponentTickEnabled(false);}
 for(UPointLightComponent* Light:SpreadLights)if(Light)Light->SetIntensity(0);
}
void ACourseworkHUD::UpdateFireSpread()
{
 for(int32 I=0;I<SpreadFires.Num();I++)if(UParticleSystemComponent* Fire=SpreadFires[I])
 {
  const float Growth=KitchenIncident.Spread(I);
  if(Growth>0){if(!Fire->IsActive()){Fire->SetComponentTickEnabled(true);Fire->ActivateSystem(true);}Fire->SetWorldScale3D(FVector(.12f+.55f*Growth));}
 }
 for(int32 I=0;I<SpreadLights.Num();I++)if(UPointLightComponent* Light=SpreadLights[I])Light->SetIntensity(350*KitchenIncident.Spread(I)*(.88f+.12f*FMath::Sin(GetWorld()->GetTimeSeconds()*9.3f+I)));
 const float Upper=KitchenIncident.Spread(1);
 if(SpreadSmoke&&Upper>0){if(!SpreadSmoke->IsActive()){SpreadSmoke->SetComponentTickEnabled(true);SpreadSmoke->ActivateSystem(true);}SpreadSmoke->SetWorldScale3D(FVector(.3f+.4f*Upper,.3f+.4f*Upper,.25f+.3f*Upper));}
}
