#include "CourseworkHUD.h"
#include "CourseworkOverlay.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "TimerManager.h"

// Uses the rendered Slate geometry and the widget's actual mouse down/up handlers.
// This is synthetic pointer input, not a claim of a physical OS mouse test.
void ACourseworkHUD::TickControlAcceptance()
{
    const double Now=GetWorld()->GetRealTimeSeconds();if(Now-TestCaseStart<1.5)return;
    TestCaseStart=Now;++TestCase;
    auto* PC=GetOwningPlayerController();auto* Character=Cast<ACharacter>(PC->GetPawn());
    if(TestMode==TEXT("typography"))
    {
        switch(TestCase)
        {
        case 0:
            Character->TeleportTo(FVector(-2100,9870,151.5),FRotator::ZeroRotator,false,false);PC->SetControlRotation(FRotator(-12,167,0));
            SetRoomLighting(0,true,.7f);SetPanel(6);Overlay->ShowLightingRoom(0);break;
        case 1:FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Typography_lighting.png"),true,false);break;
        case 2:SetPanel(4);break;
        case 3:FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Typography_settings.png"),true,false);break;
        default:TestMode.Empty();QuitApplication();break;
        }
        return;
    }
    auto Click=[&](float X,float Y){return Overlay->ExerciseControlPointer(FVector2D(X,Y));};
    auto Shot=[&](const TCHAR* Name){const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/")+Name;FTimerHandle H;GetWorld()->GetTimerManager().SetTimer(H,[Path](){FScreenshotRequest::RequestScreenshot(Path,true,false);},.5f,false);};
    bool Passed=false;FString Name;
    switch(TestCase)
    {
    case 0:
        Name=TEXT("lighting_menu_open");Character->TeleportTo(FVector(-2100,9870,151.5),FRotator::ZeroRotator,false,false);PC->SetControlRotation(FRotator(-12,167,0));SetPanel(6);Overlay->ShowLightingRoom(0);Passed=PanelMode==6&&PC->IsMoveInputIgnored();break;
    case 1:Name=TEXT("pointer_brightness_70");Passed=Click(567.4f,477)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.7f,.002f);Shot(TEXT("Tab_living_70.png"));break;
    case 2:Name=TEXT("pointer_off_preserves_brightness");Passed=Click(510,600)&&!IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.7f,.002f);break;
    case 3:Name=TEXT("zero_slider_preserves_restore_value");Passed=Click(300,477)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.7f,.002f);Shot(TEXT("Tab_living_off.png"));break;
    case 4:Name=TEXT("pointer_on_restores_value");Passed=Click(625,600)&&IsRoomLightOn(0)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.7f,.002f);break;
    case 5:Name=TEXT("brightness_minimum");Passed=Click(303.82f,477)&&FMath::IsNearlyEqual(GetRoomBrightness(0),.01f,.002f);break;
    case 6:Name=TEXT("brightness_maximum");Passed=Click(682,477)&&FMath::IsNearlyEqual(GetRoomBrightness(0),1.f);Shot(TEXT("Tab_living_100.png"));break;
    case 7:{Name=TEXT("room_selection_does_not_teleport");const FVector Before=Character->GetActorLocation();Passed=Click(160,499)&&FVector::Dist(Before,Character->GetActorLocation())<1;break;}
    case 8:Name=TEXT("room_independent_value");Passed=Click(433.7f,477)&&FMath::IsNearlyEqual(GetRoomBrightness(1),.35f,.002f)&&FMath::IsNearlyEqual(GetRoomBrightness(0),1.f);Shot(TEXT("Tab_master_35.png"));break;
    case 9:Name=TEXT("room_reset");ResetRoomLighting(1);Passed=IsRoomLightOn(1)&&FMath::IsNearlyEqual(GetRoomBrightness(1),1.f);break;
    case 10:Name=TEXT("settings_auxiliary_gear");Passed=Click(1512,45)&&PanelMode==4;break;
    case 11:Name=TEXT("sensitivity_slider");Passed=Click(491,353)&&FMath::IsNearlyEqual(GetSensitivity(),1.125f,.002f);break;
    case 12:Name=TEXT("speed_slider");Passed=Click(491,524)&&FMath::IsNearlyEqual(GetSpeedScale(),1.25f,.002f);Shot(TEXT("Tab_settings.png"));break;
    case 13:Name=TEXT("map_auxiliary_entry");Passed=Click(1340,45)&&PanelMode==5;Shot(TEXT("Tab_navigation.png"));break;
    case 14:Name=TEXT("navigation_living");Passed=Overlay->ExerciseMapDestination(1)&&PanelMode==0&&FVector::Dist(Character->GetActorLocation(),FVector(-1990,9800,160))<50;break;
    case 15:Name=TEXT("menu_close_releases_input");SetPanel(6);Passed=Click(1480,860)&&PanelMode==0&&!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored()&&!PC->bShowMouseCursor;break;
    case 25:
        Name=TEXT("whole_off_keeps_each_brightness");for(int32 I=0;I<CourseworkRooms::Count;++I)SetRoomLighting(I,true,.2f+.07f*I);
        Passed=Click(1270,144);for(int32 I=0;I<CourseworkRooms::Count;++I)Passed &=!IsRoomLightOn(I)&&FMath::IsNearlyEqual(GetRoomBrightness(I),.2f+.07f*I,.001f);Shot(TEXT("Tab_whole_off.png"));break;
    case 26:
        Name=TEXT("whole_on_restores_each_brightness");Passed=Click(1090,144);for(int32 I=0;I<CourseworkRooms::Count;++I)Passed &=IsRoomLightOn(I)&&FMath::IsNearlyEqual(GetRoomBrightness(I),.2f+.07f*I,.001f);break;
    case 27:
        Name=TEXT("local_value_after_whole_control");Passed=Click(567.4f,477)&&FMath::IsNearlyEqual(GetRoomBrightness(8),.7f,.002f);for(int32 I=0;I<8;++I)Passed &=FMath::IsNearlyEqual(GetRoomBrightness(I),.2f+.07f*I,.001f);break;
    case 28:
        Name=TEXT("local_reset_preserves_other_rooms");ResetRoomLighting(8);Passed=FMath::IsNearlyEqual(GetRoomBrightness(8),1.f);for(int32 I=0;I<8;++I)Passed &=FMath::IsNearlyEqual(GetRoomBrightness(I),.2f+.07f*I,.001f);break;
    case 29:
        Name=TEXT("whole_reset_defaults");ResetAllRoomLighting();Passed=true;for(int32 I=0;I<CourseworkRooms::Count;++I)Passed &=IsRoomLightOn(I)&&FMath::IsNearlyEqual(GetRoomBrightness(I),1.f);Shot(TEXT("Tab_whole_reset.png"));break;
    default:
        if(TestCase<16+CourseworkRooms::Count)
        {
            const int32 Row=TestCase-16,ID=CourseworkRooms::DisplayOrder[Row];
            Name=FString::Printf(TEXT("pointer_room_%d_independent"),ID);
            for(int32 I=0;I<CourseworkRooms::Count;++I)ResetRoomLighting(I);
            SetPanel(6);
            Passed=Click(160,CourseworkRooms::RowY(Row)+31)&&Click(510.1f,477)&&FMath::IsNearlyEqual(GetRoomBrightness(ID),.55f,.002f);
            for(int32 I=0;I<CourseworkRooms::Count;++I)if(I!=ID)Passed &=FMath::IsNearlyEqual(GetRoomBrightness(I),1.f);
            if(Row==CourseworkRooms::Count-1)Shot(TEXT("Tab_all_rooms.png"));
            break;
        }
        ApplySensitivity(1);ApplySpeed(1);for(int32 I=0;I<CourseworkRooms::Count;++I)ResetRoomLighting(I);
        FFileHelper::SaveStringToFile(TEXT("[")+FString::Join(TestResults,TEXT(","))+TEXT("]"),*(FPaths::ProjectSavedDir()/TEXT("ControlPointerAcceptance.json")));TestMode.Empty();QuitApplication();return;
    }
    TestResults.Add(FString::Printf(TEXT("{\"case\":\"%s\",\"passed\":%s}"),*Name,Passed?TEXT("true"):TEXT("false")));
    UE_LOG(LogTemp,Display,TEXT("CONTROL_POINTER_TEST %s"),*TestResults.Last());
}
