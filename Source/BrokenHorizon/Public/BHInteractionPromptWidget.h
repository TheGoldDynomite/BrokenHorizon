#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHInteractionPromptWidget.generated.h"

class UTextBlock;

UCLASS(Blueprintable)
class BROKENHORIZON_API UBHInteractionPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetInteractionText(const FText& NewText);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> InteractionText;
};