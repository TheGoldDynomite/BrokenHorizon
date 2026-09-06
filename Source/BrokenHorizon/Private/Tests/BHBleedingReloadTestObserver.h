#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BHBleedingReloadTestObserver.generated.h"

UCLASS(Transient)
class UBHBleedingReloadTestObserver : public UObject
{
    GENERATED_BODY()

public:
    TFunction<void(float, AActor*)> OnDamage;
    TFunction<void(AActor*)> OnDeath;

    UFUNCTION()
    void HandleDamage(float DamageApplied, AActor* DamageCauser)
    { if (OnDamage) { OnDamage(DamageApplied, DamageCauser); } }

    UFUNCTION()
    void HandleDeath(AActor* DamageCauser)
    { if (OnDeath) { OnDeath(DamageCauser); } }
};
