#pragma once

#include "CoreMinimal.h"
#include "BHSupplyBase.h"
#include "BHArmorSupply.generated.h"

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHArmorSupply : public ABHSupplyBase
{
    GENERATED_BODY()

public:
    ABHArmorSupply();

protected:
    virtual bool TryApplyToCharacter(
        ABHCharacter* Character
    ) override;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Supply|Armor",
        meta = (ClampMin = "0.0")
    )
    float HelmetDurabilityAmount = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Supply|Armor",
        meta = (ClampMin = "0.0")
    )
    float BodyArmorDurabilityAmount = 50.0f;
};
