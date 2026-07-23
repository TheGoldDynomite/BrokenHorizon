#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BHInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBHInteractable : public UInterface
{
    GENERATED_BODY()
};

class BROKENHORIZON_API IBHInteractable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void Interact(AActor* InteractingActor);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FText GetInteractionText() const;
};