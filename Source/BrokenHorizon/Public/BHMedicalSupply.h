#pragma once

#include "CoreMinimal.h"
#include "BHSupplyBase.h"
#include "BHMedicalSupply.generated.h"

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHMedicalSupply : public ABHSupplyBase
{
    GENERATED_BODY()

public:
    ABHMedicalSupply();

    void ConfigureRuntimePickup();

protected:
    virtual bool TryApplyToCharacter(
        ABHCharacter* Character
    ) override;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Supply|Medical",
        meta = (ClampMin = "0")
    )
    int32 MedkitAmount = 1;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Supply|Medical",
        meta = (ClampMin = "0")
    )
    int32 FieldDressingAmount = 2;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Supply|Medical",
        meta = (ClampMin = "0.0")
    )
    float HealAmount = 0.0f;
};
