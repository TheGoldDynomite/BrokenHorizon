#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BHSaveLoadTestObserver.generated.h"

// Scoped native test callback for the real dynamic health-restoration event.
UCLASS(Transient)
class UBHSaveLoadTestObserver : public UObject
{
    GENERATED_BODY()

public:
    TFunction<void(float, float)> OnObservedHealth;

    UFUNCTION()
    void HandleHealthChanged(float CurrentHealth, float MaxHealth)
    {
        if (OnObservedHealth) { OnObservedHealth(CurrentHealth, MaxHealth); }
    }
};