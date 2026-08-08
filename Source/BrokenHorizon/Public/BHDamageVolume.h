#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHDamageVolume.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
class BROKENHORIZON_API ABHDamageVolume : public AActor
{
    GENERATED_BODY()

public:
    ABHDamageVolume();

protected:
    UFUNCTION()
    void HandleBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    TObjectPtr<UBoxComponent> DamageBounds;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Damage",
        meta = (ClampMin = "0.0")
    )
    float DamageAmount = 100.0f;
};
