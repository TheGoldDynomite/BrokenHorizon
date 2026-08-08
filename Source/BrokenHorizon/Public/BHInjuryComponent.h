#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BHInjuryComponent.generated.h"

class UBHHealthComponent;
struct FHitResult;

UENUM(BlueprintType)
enum class EBHPlayerHitZone : uint8
{
    Head,
    Torso,
    Arm,
    Leg
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
    FBHOnInjuryStateChanged,
    bool,
    bBleeding,
    float,
    BleedRate,
    bool,
    bArmInjured,
    bool,
    bLegInjured,
    int32,
    FieldDressings
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
    FBHOnMedicalStateChanged,
    int32,
    Medkits,
    float,
    HelmetDurabilityPercentage,
    float,
    BodyArmorDurabilityPercentage,
    bool,
    bTreatmentActive,
    float,
    TreatmentProgress
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FBHOnTreatmentCompleted
);

UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class BROKENHORIZON_API UBHInjuryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBHInjuryComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    EBHPlayerHitZone ResolveHitZone(
        const FHitResult& HitResult
    ) const;

    float CalculateDamageForHit(
        const FHitResult& HitResult,
        float RawDamage,
        EBHPlayerHitZone& OutHitZone
    );

    void RegisterBallisticHit(
        EBHPlayerHitZone HitZone,
        float DamageApplied,
        AActor* DamageCauser
    );

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    bool UseFieldDressing();

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    bool ConsumeFieldDressingForSquadAid();

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    bool ConsumeMedkitForSquadAid();

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    void ClearBleedingForSquadAid();

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    bool StartMedkitTreatment();

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    void CancelMedkitTreatment();

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    void AddMedicalSupplies(
        int32 MedkitsToAdd,
        int32 FieldDressingsToAdd
    );

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    bool RepairArmor(
        float HelmetDurabilityToRestore,
        float BodyArmorDurabilityToRestore
    );

    void RestorePersistentSupplyState(
        int32 SavedMedkits,
        int32 SavedFieldDressings,
        float SavedHelmetDurability,
        float SavedBodyArmorDurability
    );

    void RestorePersistentInjuryState(
        bool bSavedBleeding,
        float SavedBleedRate,
        bool bSavedArmInjured,
        bool bSavedLegInjured
    );

    UFUNCTION(BlueprintCallable, Category = "Injuries")
    void ResetInjuries();

    UFUNCTION(BlueprintPure, Category = "Injuries")
    bool IsBleeding() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetBleedRate() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    bool IsArmInjured() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    bool IsLegInjured() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    int32 GetFieldDressingCount() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    int32 GetMedkitCount() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetHelmetDurabilityPercentage() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetBodyArmorDurabilityPercentage() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetHelmetDurability() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetBodyArmorDurability() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    bool IsMedkitTreatmentActive() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetMedkitTreatmentProgress() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetWeaponSpreadMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetMovementSpeedMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Injuries")
    float GetWeaponSwayDegrees() const;

    UPROPERTY(BlueprintAssignable, Category = "Injuries")
    FBHOnInjuryStateChanged OnInjuryStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Injuries")
    FBHOnMedicalStateChanged OnMedicalStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Injuries")
    FBHOnTreatmentCompleted OnTreatmentCompleted;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Damage",
        meta = (ClampMin = "0.0")
    )
    float HeadDamageMultiplier = 3.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Damage",
        meta = (ClampMin = "0.0")
    )
    float TorsoDamageMultiplier = 1.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Damage",
        meta = (ClampMin = "0.0")
    )
    float ArmDamageMultiplier = 0.70f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Damage",
        meta = (ClampMin = "0.0")
    )
    float LegDamageMultiplier = 0.75f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Armor"
    )
    bool bHasHelmet = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Armor",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float HelmetDamageScale = 0.55f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Armor",
        meta = (ClampMin = "0.0")
    )
    float MaximumHelmetDurability = 45.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Armor"
    )
    bool bHasBodyArmor = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Armor",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float BodyArmorDamageScale = 0.75f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Armor",
        meta = (ClampMin = "0.0")
    )
    float MaximumBodyArmorDurability = 100.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Bleeding",
        meta = (ClampMin = "0.0")
    )
    float MinimumBleedingHitDamage = 5.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Bleeding",
        meta = (ClampMin = "0.0")
    )
    float HeadBleedRate = 0.75f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Bleeding",
        meta = (ClampMin = "0.0")
    )
    float TorsoBleedRate = 0.80f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Bleeding",
        meta = (ClampMin = "0.0")
    )
    float ArmBleedRate = 0.45f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Bleeding",
        meta = (ClampMin = "0.0")
    )
    float LegBleedRate = 0.60f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Bleeding",
        meta = (ClampMin = "0.05", Units = "s")
    )
    float BleedTickInterval = 0.50f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Treatment",
        meta = (ClampMin = "0")
    )
    int32 StartingFieldDressings = 3;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Treatment",
        meta = (ClampMin = "0")
    )
    int32 StartingMedkits = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Treatment",
        meta = (ClampMin = "0.0")
    )
    float MedkitHealingAmount = 45.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Treatment",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float MedkitTreatmentDuration = 3.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Treatment"
    )
    bool bMedkitTreatsLimbInjuries = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Penalties",
        meta = (ClampMin = "1.0")
    )
    float ArmInjurySpreadMultiplier = 1.65f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Penalties",
        meta = (ClampMin = "0.1", ClampMax = "1.0")
    )
    float LegInjuryMovementMultiplier = 0.70f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Injuries|Penalties",
        meta = (ClampMin = "0.0", Units = "Degrees")
    )
    float ArmInjuryWeaponSwayDegrees = 0.85f;

private:
    UFUNCTION()
    void OnRep_InjuryMedicalState();

    void BroadcastInjuryState();
    void BroadcastMedicalState();
    void CompleteMedkitTreatment();
    float ApplyArmorProtection(
        float UnarmoredDamage,
        float DamageScale,
        float& CurrentDurability
    );
    float GetBleedRateForZone(EBHPlayerHitZone HitZone) const;
    float GetEffectiveMedkitTreatmentDuration() const;

    UPROPERTY()
    TObjectPtr<UBHHealthComponent> HealthComponent;
    TWeakObjectPtr<AActor> LastDamageCauser;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    float CurrentBleedRate = 0.0f;
    float BleedTickAccumulator = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    float CurrentHelmetDurability = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    float CurrentBodyArmorDurability = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    float MedkitTreatmentElapsed = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    int32 FieldDressingCount = 0;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    int32 MedkitCount = 0;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    bool bBleeding = false;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    bool bArmInjured = false;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    bool bLegInjured = false;

    UPROPERTY(ReplicatedUsing = OnRep_InjuryMedicalState)
    bool bMedkitTreatmentActive = false;
};
