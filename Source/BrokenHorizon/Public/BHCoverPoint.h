#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHCoverPoint.generated.h"

class USceneComponent;

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHCoverPoint : public AActor
{
    GENERATED_BODY()

public:
    ABHCoverPoint();

    UFUNCTION(BlueprintPure, Category = "Cover")
    bool IsAvailableFor(const AActor* RequestingActor) const;

    bool TryClaim(AActor* RequestingActor);
    void Release(AActor* RequestingActor);

    FVector GetAnchorLocation() const;
    FVector GetPeekLocation(bool bRightSide) const;

protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Cover"
    )
    TObjectPtr<USceneComponent> CoverRoot;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Cover",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float PeekLateralOffset = 180.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Cover",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float PeekForwardOffset = 20.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Cover"
    )
    bool bCoverEnabled = true;

private:
    TWeakObjectPtr<AActor> ClaimedBy;
};
