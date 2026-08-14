#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHAntiVehicleProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHAntiVehicleProjectile : public AActor
{
    GENERATED_BODY()

public:
    ABHAntiVehicleProjectile();

    UFUNCTION(BlueprintCallable, Category = "Anti Vehicle")
    void Launch(const FVector& Direction, float Speed = 2400.0f);

protected:
    virtual void BeginPlay() override;
    UFUNCTION()
    void HandleProjectileHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit
    );

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anti Vehicle")
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anti Vehicle|Presentation")
    TObjectPtr<UStaticMeshComponent> ProjectileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anti Vehicle")
    TObjectPtr<UProjectileMovementComponent> Movement;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anti Vehicle", meta = (ClampMin = "1.0"))
    float AntiVehicleDamage = 125.0f;
};
