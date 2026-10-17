#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourseworkTools.generated.h"
UCLASS()
class HOUSECOURSEWORK_API UCourseworkTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString PrepareVentilationLeaves();
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString PrepareCurtainWind();
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString ExportWindowGeometry(const FString& OutputPath);
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString PrepareWindowParts(const FString& RecipePath);
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString BindResidentAnimation(const FString& AnimationPath, const FString& SkeletonPath);
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString PrepareKitchenSmoke();
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString PrepareKitchenFire();
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString PrepareResidentLocomotion(const FString& BriskPath);
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString ReplaceLegacyInteractionPrompt(const FString& AssetPath);
    UFUNCTION(BlueprintCallable, Category="Coursework|Editor")
    static FString ExportFloorplan(const FString& OutputPath);
};
