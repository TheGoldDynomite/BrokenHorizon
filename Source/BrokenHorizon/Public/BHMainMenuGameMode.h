#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BHMainMenuGameMode.generated.h"

class UBHMainMenuWidget;

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHMainMenuGameMode
    : public AGameModeBase
{
    GENERATED_BODY()

public:
    ABHMainMenuGameMode();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Main Menu"
    )
    TSubclassOf<UBHMainMenuWidget> MainMenuWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UBHMainMenuWidget> MainMenuWidget;

#if !UE_BUILD_SHIPPING
    void RunRenderedSessionReview();
    FTimerHandle RenderedSessionReviewTimer;
#endif
};
