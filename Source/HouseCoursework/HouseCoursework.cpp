#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "CourseworkHUD.h"

class FHouseCourseworkModule : public FDefaultGameModuleImpl
{
    FTSTicker::FDelegateHandle Handle;
public:
    virtual void StartupModule() override
    {
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
    virtual void ShutdownModule() override { FTSTicker::GetCoreTicker().RemoveTicker(Handle); }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FHouseCourseworkModule, HouseCoursework, "HouseCoursework");
