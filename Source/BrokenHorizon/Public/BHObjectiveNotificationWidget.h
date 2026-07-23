#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "BHObjectiveNotificationWidget.generated.h"

class UTextBlock;

UCLASS()
class BROKENHORIZON_API UBHObjectiveNotificationWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void ShowNotification(const FText& Message);

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> NotificationText;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Objectives|Notification",
        meta = (ClampMin = "0.0"))
    float DisplayDuration = 2.5f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Objectives|Notification",
        meta = (ClampMin = "0.01"))
    float FadeDuration = 0.5f;

private:
    void StartFadeOut();
    void UpdateFadeOut();
    void HideNotification();
    void ClearNotificationTimers();

    FTimerHandle DisplayTimerHandle;
    FTimerHandle FadeTimerHandle;

    float FadeStartTime = 0.0f;
    float FadeUpdateInterval = 0.02f;
};
