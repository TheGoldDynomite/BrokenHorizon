#pragma once

#include "CoreMinimal.h"
#include "BHInteractable.h"
#include "GameFramework/Actor.h"
#include "BHEngineeringCharge.generated.h"

class ABHCharacter;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EBHEngineeringChargeMode : uint8
{
    Breach,
    AreaDenial
};

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHEngineeringCharge
    : public AActor,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHEngineeringCharge();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual float TakeDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    void InitializeCharge(
        ABHCharacter* PlacingCharacter,
        AActor* PlacementTarget,
        EBHEngineeringChargeMode NewMode
    );

    bool Detonate(ABHCharacter* RequestingCharacter);

    UFUNCTION(BlueprintPure, Category = "Engineering Charge")
    bool IsArmed() const;

    UFUNCTION(BlueprintPure, Category = "Engineering Charge")
    EBHEngineeringChargeMode GetChargeMode() const;

    static float GetOuterDamageRadius(EBHEngineeringChargeMode Mode);
    static float GetMaximumDamage(EBHEngineeringChargeMode Mode);
    static bool CanCommandDetonate(
        bool bArmed,
        bool bAlreadyDetonated,
        bool bRequesterOwnsCharge
    );

    virtual void Interact_Implementation(AActor* InteractingActor) override;
    virtual FText GetInteractionText_Implementation() const override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UBoxComponent> CollisionRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMeshComponent> ChargeMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UTextRenderComponent> ChargeLabel;

    UPROPERTY(ReplicatedUsing = OnRep_ChargeState)
    EBHEngineeringChargeMode ChargeMode =
        EBHEngineeringChargeMode::AreaDenial;

    UPROPERTY(ReplicatedUsing = OnRep_ChargeState)
    bool bArmed = false;

    UPROPERTY(ReplicatedUsing = OnRep_ChargeState)
    bool bDetonated = false;

    UPROPERTY(Replicated)
    TObjectPtr<AActor> AttachedTarget;

    UPROPERTY(EditDefaultsOnly, Category = "Engineering Charge", meta = (ClampMin = "0.1", Units = "s"))
    float ArmingDelay = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Engineering Charge", meta = (ClampMin = "1.0"))
    float MaximumHealth = 25.0f;

private:
    UFUNCTION()
    void OnRep_ChargeState();

    void FinishArming();
    void RefreshPresentation();
    void NotifyOwningCharacterRemoved();

    UPROPERTY()
    TObjectPtr<ABHCharacter> PlacedByCharacter;

    float CurrentHealth = 25.0f;
    FTimerHandle ArmingTimerHandle;
};
