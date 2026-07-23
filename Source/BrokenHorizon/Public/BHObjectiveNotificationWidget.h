#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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

    UFUNCTION(BlueprintCallable, Category = "Objectives")
    void HideNotification();

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> NotificationText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objectives", meta = (ClampMin = "0.1"))
    float DisplayDuration = 3.0f;

private:
    FTimerHandle HideTimerHandle;
};
