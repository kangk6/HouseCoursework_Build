#include "CourseworkNightGuidance.h"
#include "CourseworkHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

void ACourseworkHUD::UpdateNightGuidance(float Dt,FVector Player,double Now)
{
 auto* Pawn=GetOwningPlayerController()->GetPawn();
 const FVector Velocity=Pawn->GetVelocity();bSmartNightWalking=Velocity.Size2D()>25.f;
 if(bSmartNightWalking)
 {
  const FVector Direction=Velocity.GetSafeNormal2D();
  SmartNightHeading=SmartNightHeading.IsNearlyZero()?Direction:FMath::VInterpTo(SmartNightHeading,Direction,Dt,7.f);
  if(SmartNightHeading.Size2D()<.1f)SmartNightHeading=Direction;
 }
 auto Reach=[&](FVector Destination)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(NightGuideReach),true,Pawn);if(OriginalResident.IsValid())Query.AddIgnoredActor(OriginalResident.Get());
  const FVector Eye=Player-FVector(0,0,30);FHitResult Hit;
  return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,Destination,ECC_Visibility,Query)
      &&!GetWorld()->LineTraceSingleByChannel(Hit,Destination,Eye,ECC_Visibility,Query);
 };
 if(SmartPlayerZone!=INDEX_NONE)SmartRoomUntil.Add(SmartPlayerZone,Now+6);

 // Keep local coverage, but predict only the nearest suitable forward node.
 int32 Ahead=INDEX_NONE;float BestAhead=MAX_flt;
 for(int32 I=0;I<SmartPathFills.Num();++I)if(auto* Light=SmartPathFills[I].Light.Get())
 {
  const FVector Source=Light->GetComponentLocation(),Delta=Source-Player;
  if(FMath::Abs(Delta.Z)>=105||Delta.Size2D()>340||!Reach(Source+FVector(0,0,20)))continue;
  if(Delta.Size2D()<170)SmartPathFills[I].Until=Now+6;
  else {const float Score=CourseworkNightGuidance::AheadScore(Delta,SmartNightHeading,bSmartNightWalking);if(Score<BestAhead){BestAhead=Score;Ahead=I;}}
 }
 if(Ahead!=INDEX_NONE)SmartPathFills[Ahead].Until=Now+6;

 bool DoorApproach=false;
 for(const auto& Pair:ClosedTransforms)if(auto* C=Cast<UStaticMeshComponent>(Pair.Key.Get()))
  if((C->GetOwner()->GetFName()==FName(TEXT("BP_Door_Inside_A3_8"))||C->GetOwner()->GetFName()==FName(TEXT("BP_Door_Inside_A5_14")))&&FVector::Dist2D(Player,C->Bounds.Origin)<145)DoorApproach=true;
 const bool OnStairs=SmartPlayerZone==8&&Player.X> -2260&&Player.X< -2100&&Player.Y>10040&&Player.Y<10665;
 const bool StairApproach=(FMath::Abs(Player.Z-150)<100&&FVector::Dist2D(Player,FVector(-2190,10540,150))<240&&Reach({-2190,10540,150}))
   ||(FMath::Abs(Player.Z-413)<100&&FVector::Dist2D(Player,FVector(-2190,10070,413))<200&&Reach({-2190,10070,383}));
 if(OnStairs||StairApproach)
 {
  int32 Current=INDEX_NONE,Next=INDEX_NONE;float Nearest=MAX_flt,Forward=MAX_flt;
  for(int32 I=0;I<SmartStairFills.Num();++I)if(auto* Light=SmartStairFills[I].Get())
  {
   const FVector Delta=Light->GetComponentLocation()-Player;const float Distance=Delta.Size();if(Distance>310)continue;
   if(Distance<Nearest){Nearest=Distance;Current=I;}
   const float Score=CourseworkNightGuidance::AheadScore(Delta,SmartNightHeading,bSmartNightWalking,310);if(Score<Forward){Forward=Score;Next=I;}
  }
  if(Current!=INDEX_NONE)SmartTrailUntil.Add(FName(*FString::Printf(TEXT("stair_bounce_%d"),Current)),Now+6);
  if(Next==INDEX_NONE||Next==Current)
  {
   float OtherDistance=MAX_flt;
   for(int32 I=0;I<SmartStairFills.Num();++I)if(I!=Current&&SmartStairFills[I].IsValid())
   {const float D=FVector::Dist(SmartStairFills[I]->GetComponentLocation(),Player);if(D<OtherDistance){OtherDistance=D;Next=I;}}
  }
  if(Next!=INDEX_NONE)SmartTrailUntil.Add(FName(*FString::Printf(TEXT("stair_bounce_%d"),Next)),Now+6);
  // The middle flight needs both landings; this is a safety overlap, not
  // sensing an unrelated room through the floor slab.
  if(Player.Z>210)SmartTrailUntil.Add(TEXT("SM_Lamp_E3"),Now+6);
  if(Player.Z<360)SmartTrailUntil.Add(TEXT("SM_Lamp_E2"),Now+6);
  if(Player.Y<10180)SmartTrailUntil.Add(TEXT("SM_Lamp_C1_2"),Now+6);
  SmartTrailUntil.Add(TEXT("PointLight12"),Now+6);
 }
 else if(SmartPlayerZone==8||DoorApproach||SmartPlayerZone==6)
 {
  struct FGuide{FName Lamp;FVector Anchor;};
  const FGuide Guides[]={
   {TEXT("SM_Lamp_E3"),{-2114,9980,413}},{TEXT("SM_Lamp_C1_2"),{-2190,10070,413}},
   {TEXT("BP_Lamp_E2_ON_2"),{-2000,10070,413}},{TEXT("SM_Lamp_E2"),{-1990,10240,150}}};
  int32 Current=INDEX_NONE,Next=INDEX_NONE;float Nearest=MAX_flt,Forward=MAX_flt;
  for(int32 I=0;I<UE_ARRAY_COUNT(Guides);++I)
  {
   const FVector Delta=Guides[I].Anchor-Player;const float Distance=Delta.Size2D();
   if(FMath::Abs(Delta.Z)>100||Distance>340||!Reach(Guides[I].Anchor-FVector(0,0,30)))continue;
   if(Distance<Nearest){Nearest=Distance;Current=I;}
  }
  for(int32 I=0;I<UE_ARRAY_COUNT(Guides);++I)if(I!=Current)
  {
   const FVector Delta=Guides[I].Anchor-Player;
   if(FMath::Abs(Delta.Z)>100||!Reach(Guides[I].Anchor-FVector(0,0,30)))continue;
   const float Score=bSmartNightWalking?CourseworkNightGuidance::AheadScore(Delta,SmartNightHeading,true):(Delta.Size2D()<160?Delta.Size2D():MAX_flt);
   if(Score<Forward){Forward=Score;Next=I;}
  }
  auto PrepareGuide=[&](int32 Index)
  {
   if(Index==INDEX_NONE)return;
   SmartTrailUntil.Add(Guides[Index].Lamp,Now+6);
   if(!DoorApproach)return;
   // Pair the selected fixture with its local floor coverage, including an
   // open bedroom exit. The guide itself has already passed floor and LOS tests.
   int32 FillIndex=INDEX_NONE;float BestDistance=180.f;
   for(int32 I=0;I<SmartPathFills.Num();++I)if(SmartPathFills[I].Room==8&&SmartPathFills[I].Light.IsValid())
   {
    const FVector Location=SmartPathFills[I].Light->GetComponentLocation();
    const float D=FVector::Dist2D(Location,Guides[Index].Anchor);
    if(FMath::Abs(Location.Z-Guides[Index].Anchor.Z)<105&&D<BestDistance){BestDistance=D;FillIndex=I;}
   }
   if(FillIndex!=INDEX_NONE)SmartPathFills[FillIndex].Until=Now+6;
  };
  PrepareGuide(Current);PrepareGuide(Next);
  if(Current!=INDEX_NONE&&Player.Z>310)SmartTrailUntil.Add(TEXT("PointLight15"),Now+6);
 }

 // Select one visible fixture family per occupied room. Keep a 75cm switching
 // margin so small movements near two bedside lights do not alternate them.
 FName Best=NAME_None;float Distance=MAX_flt,Previous=MAX_flt;
 for(const auto& Fixture:LightFixtures)if(Fixture.Room==SmartPlayerZone&&Fixture.Room!=8&&Fixture.Mesh.IsValid())
 {
  const FName Name=Fixture.Mesh->GetOwner()->GetFName();
  if(Name==FName(TEXT("SM_Lamp_D_6"))||Name==FName(TEXT("SM_Lamp_D2"))||Name==FName(TEXT("SM_Lamp_B1_2")))continue;
  const float D=FVector::Dist2D(Player,Fixture.Mesh->Bounds.Origin);
  if(Name==SmartNightPreferredFixture)Previous=FMath::Min(Previous,D);
  if(D<Distance){Distance=D;Best=Name;}
 }
 if(Previous<Distance+75)Best=SmartNightPreferredFixture;
 SmartNightPreferredFixture=Best;
 if(!Best.IsNone())SmartTrailUntil.Add(Best,Now+6);
}
