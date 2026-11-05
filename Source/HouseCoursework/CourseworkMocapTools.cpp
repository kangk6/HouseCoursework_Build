#include "CourseworkTools.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#endif
FString UCourseworkTools::BakeResidentMocap(const FString& JsonPath)
{
#if WITH_EDITOR
 FString Text;if(!FFileHelper::LoadFileToString(Text,*JsonPath))return TEXT("ERROR json missing");TSharedPtr<FJsonObject> Doc;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Doc))return TEXT("ERROR json");
 const auto& Frames=Doc->GetArrayField(TEXT("frames"));if(Frames.Num()<2)return TEXT("ERROR frame count");
 auto* Skeleton=LoadObject<USkeleton>(nullptr,TEXT("/Game/Coursework/ResidentAnimated/harper-idle-linear_Skeleton.harper-idle-linear_Skeleton"));
 auto Save=[](UObject* Asset){auto* P=Asset->GetOutermost();Asset->PostEditChange();P->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;return UPackage::SavePackage(P,Asset,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A);};
 auto* Package=CreatePackage(TEXT("/Game/Coursework/ResidentMocap/A_ResidentWalk"));Package->FullyLoad();auto* Anim=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Coursework/ResidentMocap/A_ResidentWalk.A_ResidentWalk"));if(!Anim)Anim=NewObject<UAnimSequence>(Package,TEXT("A_ResidentWalk"),RF_Public|RF_Standalone);Anim->SetSkeleton(Skeleton);
 auto& Controller=Anim->GetController();Controller.InitializeModel();Controller.RemoveAllBoneTracks(false);Controller.OpenBracket(FText::FromString(TEXT("Bake CMU walking")),false);Controller.SetFrameRate(FFrameRate(60,1),false);Controller.SetNumberOfFrames(FFrameNumber(Frames.Num()-1),false);
 for(const auto& Pair:Frames[0]->AsObject()->Values)
 {
  const FName Bone(*Pair.Key);if(Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone)==INDEX_NONE)continue;
  TArray<FVector3f> T,S;TArray<FQuat4f> Q;
  for(const auto& V:Frames){const auto D=V->AsObject()->GetObjectField(Pair.Key);const auto& P=D->GetArrayField(TEXT("translation"));const auto& R=D->GetArrayField(TEXT("rotation"));const auto& Z=D->GetArrayField(TEXT("scale"));T.Add(FVector3f(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()));Q.Add(FQuat4f(R[0]->AsNumber(),R[1]->AsNumber(),R[2]->AsNumber(),R[3]->AsNumber()));S.Add(FVector3f(Z[0]->AsNumber(),Z[1]->AsNumber(),Z[2]->AsNumber()));}
  if(!Controller.AddBoneCurve(Bone,false)||!Controller.SetBoneTrackKeys(Bone,T,Q,S,false))return TEXT("ERROR bone keys ")+Pair.Key;
 }
 Controller.NotifyPopulated();Controller.CloseBracket(false);if(!Save(Anim))return TEXT("ERROR save animation");
 auto* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Coursework/ResidentAnimated/harper-idle-linear_Anim.harper-idle-linear_Anim"));
 Package=CreatePackage(TEXT("/Game/Coursework/ResidentMocap/BS_ResidentWalk"));Package->FullyLoad();auto* Existing=LoadObject<UBlendSpace1D>(nullptr,TEXT("/Game/Coursework/ResidentMocap/BS_ResidentWalk.BS_ResidentWalk"));if(Existing)return TEXT("SAVED mocap existing blend");auto* Blend=NewObject<UBlendSpace1D>(Package,TEXT("BS_ResidentWalk"),RF_Public|RF_Standalone);Blend->SetSkeleton(Skeleton);auto& Axis=const_cast<FBlendParameter&>(Blend->GetBlendParameter(0));Axis.Min=0;Axis.Max=170;Axis.GridNum=4;Blend->InterpolationParam[0].InterpolationTime=.15f;Blend->TargetWeightInterpolationSpeedPerSec=6;Blend->bScaleAnimation=false;Blend->AddSample(Idle,FVector::ZeroVector);Blend->AddSample(Anim,FVector(170,0,0));Blend->ValidateSampleData();Blend->ResampleData();if(!Save(Blend))return TEXT("ERROR save blend");
 return FString::Printf(TEXT("SAVED mocap %d frames"),Frames.Num());
#else
 return TEXT("Editor only");
#endif
}
