#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CourseworkMapWidget.generated.h"
class ACourseworkHUD;

UCLASS()
class HOUSECOURSEWORK_API UCourseworkMapWidget : public UWidget
{
    GENERATED_BODY()
public:
    void Configure(ACourseworkHUD* HUD, bool bExpanded);
    bool ExerciseLayerButton();
    bool ExerciseDestination(int32 Destination);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    UPROPERTY() TObjectPtr<ACourseworkHUD> OwnerHUD;
    bool bLarge = false;
    TSharedPtr<SWidget> MapSlate;
};
