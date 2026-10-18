#include "CourseworkHUD.h"
#include "CourseworkDaylight.h"
#include "CourseworkOverlay.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"

// Sample every rendered frame while using the actual slider down/move/up handlers.
void ACourseworkHUD::TickDaylightSweep()
{
    if(!bLightingReady)return;
    struct FRun{int32 Stage=0,Scenario=0;double At=0,LastFrame=0;double LastShot=-1;int32 ShotCount=0;TArray<FString> Frames;};
    static FRun R;const double Now=GetWorld()->GetRealTimeSeconds();
    auto* PC=GetOwningPlayerController();auto* C=Cast<ACharacter>(PC->GetPawn());
    auto Click=[&](float H,int32 Phase){return Overlay->ExerciseControlDrag(FVector2D(300+540*(H-6)/16.f,470),Phase);};
    const float From[]={12,17.4f,21},To[]={16,18.6f,14};
    if(TestCase<0)
    {
        TestCase=0;R=FRun();R.At=Now;ObserveDaylight();C->GetCharacterMovement()->DisableMovement();PC->PlayerCameraManager->UpdateCamera(0);
        SetAllRoomLighting(false);SetPanel(9);SetDaylightHour(From[0]);IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Screenshots/Sweep")),true);return;
    }
    const double Elapsed=Now-R.At;
    if(R.Stage==0&&Elapsed>=2.2){Click(From[R.Scenario],0);R.Stage=1;R.At=Now;}
    else if(R.Stage==1)
    {
        const float T=FMath::Clamp(float(Elapsed/1.5),0.f,1.f);Click(FMath::Lerp(From[R.Scenario],To[R.Scenario],T),1);
        if(T>=1){Click(To[R.Scenario],2);R.Stage=2;R.At=Now;}
    }
    else if(R.Stage==2&&Elapsed>1.8)
    {
        ++R.Scenario;R.Stage=0;R.At=Now;
        if(R.Scenario>=3)
        {
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"realtime_sky\":%s,\"samples\":["),Daylight->UsesRealtimeSky()?TEXT("true"):TEXT("false"))+FString::Join(R.Frames,TEXT(","))+TEXT("]}"),*(FPaths::ProjectSavedDir()/TEXT("DaylightSweep.json")));
            RestoreDaylight();SetPanel(0);TestMode.Empty();QuitApplication();return;
        }
        SetDaylightHour(From[R.Scenario]);
    }
    if(auto* V=GetWorld()->GetGameViewport()->Viewport)
    {
        TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;auto Window=GetWorld()->GetGameViewport()->GetWindow();
        if(Window.IsValid()&&FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)&&Pixels.Num())
        {
            double L=0;int32 Count=0;
            for(int32 Y=int32(Size.Y*.36);Y<int32(Size.Y*.7);++Y)for(int32 X=int32(Size.X*.93);X<int32(Size.X*.985);++X)
            {const auto& P=Pixels[Y*Size.X+X];L+=.2126*P.R+.7152*P.G+.0722*P.B;++Count;}
            L/=FMath::Max(1,Count);
            if((R.Stage==1||R.Stage==2)&&Now-R.LastShot>.12)
            {R.LastShot=Now;TArray<FColor> Small;FImageUtils::ImageResize(Size.X,Size.Y,Pixels,640,360,Small,true);TArray<uint8> PNG;FImageUtils::CompressImageArray(640,360,Small,PNG);
            FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Sweep/Sweep_%04d.png"),R.ShotCount++)));}
            const float H=Daylight->GetRenderedHour(),Sun=Daylight->GetSunIntensity(),Sky=Daylight->GetSkyIntensity();
            R.Frames.Add(FString::Printf(TEXT("{\"scenario\":%d,\"stage\":%d,\"elapsed\":%.5f,\"dt\":%.5f,\"target\":%.5f,\"rendered_hour\":%.5f,\"sun\":%.5f,\"sky\":%.5f,\"luma\":%.5f}"),R.Scenario,R.Stage,Now-R.At,R.LastFrame>0?Now-R.LastFrame:0,GetDaylightHour(),H,Sun,Sky,L));
            R.LastFrame=Now;
        }
    }
}
