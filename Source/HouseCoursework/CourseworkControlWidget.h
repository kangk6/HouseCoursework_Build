#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CourseworkControlWidget.generated.h"
class ACourseworkHUD;
class SCourseworkControl;

// Edge-to-edge Tab menu. Gameplay/map overlays remain separate.
UCLASS()
class HOUSECOURSEWORK_API UCourseworkControlWidget : public UWidget
{
    GENERATED_BODY()
public:
    void Configure(ACourseworkHUD* HUD);
    void SelectRoom(int32 Room);
    int32 GetRoom() const;
    bool ExercisePointer(FVector2D DesignPosition);
    bool ExerciseDrag(FVector2D DesignPosition,int32 Phase);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool ReleaseChildren) override;
private:
    UPROPERTY() TObjectPtr<ACourseworkHUD> Owner;
    TSharedPtr<SCourseworkControl> Menu;
};
