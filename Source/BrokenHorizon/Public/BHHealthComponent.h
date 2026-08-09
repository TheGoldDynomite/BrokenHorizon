#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BHHealthComponent.generated.h"

class AController;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FBHOnHealthChanged,
    float,
    CurrentHealth,
    float,
    MaxHealth
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FBHOnDamaged,
    float,
    DamageApplied,
    AActor*,
    DamageCauser
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnHealed,
    float,
    HealingApplied
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnDeath,
    AActor*,
    DamageCauser
);

UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class BROKENHORIZON_API UBHHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBHHealthComponent();

    static float CalculateFriendlyFireDamage(
        float DamageAmount,
        bool bFriendlyFire,
        float FriendlyFireMultiplier
    );

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintCallable, Category = "Health")
    float ApplyDamage(
        float DamageAmount,
        AActor* DamageCauser
    );

    UFUNCTION(BlueprintCallable, Category = "Health")
    float Heal(float HealingAmount);

    UFUNCTION(BlueprintCallable, Category = "Health")
    void ResetHealth();

    UFUNCTION(BlueprintCallable, Category = "Health")
    void ReviveAtHealth(float RevivedHealth);

    UFUNCTION(BlueprintCallable, Category = "Health|Configuration")
    void ConfigureMaximumHealth(
        float NewMaximumHealth,
        bool bResetCurrentHealth = true
    );

    UFUNCTION(BlueprintCallable, Category = "Health")
    void RestorePersistentHealthState(float SavedHealth);
    UFUNCTION(BlueprintCallable, Category = "Health")
    void RestorePersistentDeathState();

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetCurrentHealth() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetMaxHealth() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsDead() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsFullHealth() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetHealthPercentage() const;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FBHOnHealthChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FBHOnDamaged OnDamaged;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FBHOnHealed OnHealed;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FBHOnDeath OnDeath;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    UFUNCTION()
    void HandleOwnerDamage(
        AActor* DamagedActor,
        float Damage,
        const UDamageType* DamageType,
        AController* InstigatedBy,
        AActor* DamageCauser
    );

    UFUNCTION()
    void OnRep_CurrentHealth(float PreviousHealth);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Health",
        meta = (ClampMin = "1.0")
    )
    float MaxHealth = 100.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        ReplicatedUsing = OnRep_CurrentHealth,
        Category = "Health",
        meta = (ClampMin = "0.0")
    )
    float CurrentHealth = 100.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
    bool bIsDead = false;

private:
    bool HasMutationAuthority() const;
};
