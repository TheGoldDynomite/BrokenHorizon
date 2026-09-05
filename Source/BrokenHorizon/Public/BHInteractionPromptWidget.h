#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHInteractionPromptWidget.generated.h"

class SWidget;
class UTextBlock;

UCLASS(Blueprintable)
class BROKENHORIZON_API UBHInteractionPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetInteractionText(const FText& NewText);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void ClearInteractionText();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    virtual void NativeConstruct() override;

    void BuildNativeLayout();

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> InteractionText;

    FText ActiveText;
    bool bHasActiveText = false;
};
