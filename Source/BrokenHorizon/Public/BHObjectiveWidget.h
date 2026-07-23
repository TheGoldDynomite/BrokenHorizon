#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BHObjectiveWidget.generated.h"

class UTextBlock;

UCLASS()
class BROKENHORIZON_API UBHObjectiveWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable)
    void SetObjectiveText(const FText& NewText);

protected:

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> ObjectiveText;
};