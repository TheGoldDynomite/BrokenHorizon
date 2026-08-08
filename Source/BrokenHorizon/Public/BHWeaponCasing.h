#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHWeaponCasing.generated.h"

class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class BROKENHORIZON_API ABHWeaponCasing : public AActor
{
    GENERATED_BODY()

public:
    ABHWeaponCasing();

    void InitializeCasing(
        UStaticMesh* InCasingMesh,
        const FVector& LinearVelocity,
        const FVector& AngularVelocityRadians,
        float LifeSpanSeconds
    );

protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Casing"
    )
    TObjectPtr<USphereComponent> CollisionRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Casing"
    )
    TObjectPtr<UStaticMeshComponent> CasingMeshComponent;
};
