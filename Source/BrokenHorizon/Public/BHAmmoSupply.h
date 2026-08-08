#pragma once

#include "CoreMinimal.h"
#include "BHSupplyBase.h"
#include "BHAmmoSupply.generated.h"

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHAmmoSupply : public ABHSupplyBase
{
    GENERATED_BODY()

public:
    ABHAmmoSupply();

    void ConfigureRuntimePickup(int32 NewReserveAmmoAmount);

    UFUNCTION(BlueprintPure, Category = "Supply|Ammo")
    int32 GetReserveAmmoAmount() const;

protected:
    virtual bool TryApplyToCharacter(
        ABHCharacter* Character
    ) override;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Supply|Ammo",
        meta = (ClampMin = "1")
    )
    int32 ReserveAmmoAmount = 30;
};
