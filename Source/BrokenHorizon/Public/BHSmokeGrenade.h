#pragma once

#include "CoreMinimal.h"
#include "BHFragGrenade.h"
#include "BHSmokeGrenade.generated.h"

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHSmokeGrenade : public ABHFragGrenade
{
    GENERATED_BODY()

public:
    ABHSmokeGrenade();

protected:
    void Explode() override;
};
