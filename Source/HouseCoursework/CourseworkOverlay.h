#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CourseworkOverlay.generated.h"
class ACourseworkHUD;
class UTextBlock;
class UButton;
class USlider;
class UHorizontalBox;
class UBorder;
class UVerticalBox;
class USizeBox;
class UCourseworkMapWidget;
class UCourseworkControlWidget;

UCLASS()
class HOUSECOURSEWORK_API UCourseworkOverlay : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetOwnerHUD(ACourseworkHUD* Value);
    void RefreshPanel();
    void SetInteraction(const FString& Text);
    void SetLightInteraction(const FString& Name,const FString& Status,float Brightness,bool Dimming);
    void SetToast(const FString& Text);
    void ShowLightingRoom(int32 Room);
    bool ExerciseControlPointer(FVector2D Position);
    bool ExerciseTimeSelection(float Hour);
    bool ExerciseControlDrag(FVector2D Position,int32 Phase);
    bool ExerciseEnergyMapButton();
    bool ExerciseMapDestination(int32 Destination);
    bool CheckLightFeedback(int32 Room,bool Dimming)const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UPROPERTY() TObjectPtr<ACourseworkHUD> OwnerHUD;
    UPROPERTY() TObjectPtr<UTextBlock> Interaction;
    UPROPERTY() TObjectPtr<UTextBlock> Toast;
    UPROPERTY() TObjectPtr<UBorder> SafetyBanner;
    UPROPERTY() TObjectPtr<UTextBlock> SafetyText;
    UPROPERTY() TObjectPtr<UTextBlock> PanelTitle;
    UPROPERTY() TObjectPtr<UTextBlock> PanelBody;
    UPROPERTY() TObjectPtr<UBorder> Panel;
    UPROPERTY() TObjectPtr<UVerticalBox> Buttons;
    UPROPERTY() TObjectPtr<USizeBox> PanelWidth;
    UPROPERTY() TObjectPtr<USizeBox> MapArea;
    UPROPERTY() TObjectPtr<UCourseworkMapWidget> MiniMap;
    UPROPERTY() TObjectPtr<UCourseworkMapWidget> ExpandedMap;
    UPROPERTY() TObjectPtr<UBorder> InteractionCard;
    UPROPERTY() TObjectPtr<UTextBlock> InteractionName;
    UPROPERTY() TObjectPtr<class UProgressBar> InteractionDimBar;
    UPROPERTY() TObjectPtr<USizeBox> InteractionDimWidth;
    UPROPERTY() TObjectPtr<UBorder> InteractionKey;
    bool bInteractionWanted = false;
    float InteractionOpacity = 0;
    UPROPERTY() TObjectPtr<UBorder> PanelScrim;
    UPROPERTY() TObjectPtr<UHorizontalBox> PanelTabs;
    UPROPERTY() TObjectPtr<UTextBlock> LightValue;
    UPROPERTY() TObjectPtr<USlider> LightSlider;
    UPROPERTY() TObjectPtr<UCourseworkControlWidget> ControlMenu;
    UPROPERTY() TObjectPtr<UBorder> StartupCover;
    int32 LightingRoom=0;
    UButton* MakePanelButton(const FString& Label,FName Handler,bool Accent=false);
    void RefreshLightReadout();
    UFUNCTION() void Lights();
    UFUNCTION() void LivingLights();
    UFUNCTION() void BedroomLights();
    UFUNCTION() void ToggleLights();
    UFUNCTION() void LightBrightnessChanged(float Value);
    UFUNCTION() void ResetLights();
    void AddButton(const FString& Label, FName Handler);
    UFUNCTION() void ClosePanel();
    UFUNCTION() void Outside();
    UFUNCTION() void Living();
    UFUNCTION() void Kitchen();
    UFUNCTION() void Upstairs();
    UFUNCTION() void Entrance();
    UFUNCTION() void Help();
    UFUNCTION() void Settings();
    UFUNCTION() void Back();
    UFUNCTION() void Quit();
    UFUNCTION() void SensitivityChanged(float Value);
    UFUNCTION() void SpeedChanged(float Value);
    UFUNCTION() void FloorOne();
    UFUNCTION() void FloorTwo();
};
