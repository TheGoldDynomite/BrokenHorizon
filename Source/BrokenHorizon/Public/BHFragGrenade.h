#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "BHFragGrenade.generated.h"

class URadialForceComponent;
class USphereComponent;
class UStaticMeshComponent;
class ABHEnemyAIController;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHFragGrenade : public AActor
{
    GENERATED_BODY()

public:
    ABHFragGrenade();

    void Throw(
        const FVector& InitialVelocity,
        float CookDuration = 0.0f
    );

    UFUNCTION(BlueprintPure, Category = "Grenade")
    float GetFuseDuration() const;

    UFUNCTION(BlueprintPure, Category = "Grenade")
    float GetFuseDurationAfterCook(float CookDuration) const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Grenade|Components"
    )
    TObjectPtr<USphereComponent> CollisionRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Grenade|Components"
    )
    TObjectPtr<UStaticMeshComponent> GrenadeMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Grenade|Components"
    )
    TObjectPtr<URadialForceComponent> BlastImpulse;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|Damage",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float FuseDuration = 3.5f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|Damage",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float MinimumRemainingFuse = 0.6f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|Damage",
        meta = (ClampMin = "0.0")
    )
    float MaximumDamage = 120.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|Damage",
        meta = (ClampMin = "0.0")
    )
    float MinimumDamage = 20.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|Damage",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float InnerDamageRadius = 200.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|Damage",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float OuterDamageRadius = 550.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|Damage",
        meta = (ClampMin = "0.01")
    )
    float DamageFalloff = 1.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|AI",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float ExplosionNoiseRange = 6000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Grenade|AI",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float ThreatWarningLeadTime = 1.75f;

private:
    virtual void UpdateThreatWarnings();
    virtual void Explode();

    FTimerHandle FuseTimerHandle;
    FTimerHandle ThreatWarningTimerHandle;
    TSet<TWeakObjectPtr<ABHEnemyAIController>> AlertedControllers;
    bool bHasExploded = false;
};
