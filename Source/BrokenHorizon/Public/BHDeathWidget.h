#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHDeathWidget.generated.h"

class UTextBlock;

UCLASS()
class BROKENHORIZON_API UBHDeathWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Death")
    void ShowDeathScreen();

    UFUNCTION(BlueprintCallable, Category = "Death")
    void ShowDeathScreenWithRespawnDelay(float RespawnDelay);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DeathText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
    FText DeathMessage;

    UFUNCTION(BlueprintImplementableEvent, Category = "Death")
    void OnDeathFeedback(float RespawnDelay);
};
