#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "CourseworkHUD.h"
#include "GameFramework/GameModeBase.h"

class FHouseCourseworkModule : public FDefaultGameModuleImpl
{
    FTSTicker::FDelegateHandle Handle;
    FDelegateHandle InitHandle;
public:
    virtual void StartupModule() override
    {
        // Set the HUD before local-player startup, not half a second after the
        // first frame of the authored night map has already reached the screen.
        InitHandle=FWorldDelegates::OnWorldInitializedActors.AddLambda([](const FActorsInitializedParams& Params)
        {
            UWorld* World=Params.World;
            if(World&&World->IsGameWorld()&&World->GetOutermost()->GetName().Contains(TEXT("HouseFurniture")))
                if(auto* Mode=World->GetAuthGameMode())Mode->HUDClass=ACourseworkHUD::StaticClass();
        });
        Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            if (!GEngine) return true;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !World->IsGameWorld() || !World->GetOutermost()->GetName().Contains(TEXT("HouseFurniture"))) continue;
                auto* PC = World->GetFirstPlayerController();
                if (PC && PC->IsLocalController() && PC->GetPawn() && !Cast<ACourseworkHUD>(PC->GetHUD()))
                    PC->ClientSetHUD(ACourseworkHUD::StaticClass());
            }
            return true;
        }), 0.5f);
    }
    virtual void ShutdownModule() override { FTSTicker::GetCoreTicker().RemoveTicker(Handle);FWorldDelegates::OnWorldInitializedActors.Remove(InitHandle); }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FHouseCourseworkModule, HouseCoursework, "HouseCoursework");
