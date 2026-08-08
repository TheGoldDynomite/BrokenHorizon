#pragma once

#include "CoreMinimal.h"
#include "BHCoverPoint.h"
#include "BHInteractable.h"
#include "BHWarTypes.h"
#include "BHFieldFortification.generated.h"

class ABHCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EBHFortificationPlan : uint8
{
    HastyBarricade UMETA(DisplayName = "Hasty Barricade"),
    ReinforcedBulwark UMETA(DisplayName = "Reinforced Bulwark"),
    FiringPosition UMETA(DisplayName = "Firing Position"),
    FieldSupplyCache UMETA(DisplayName = "Field Supply Cache"),
    ObservationPost UMETA(DisplayName = "Observation Post"),
    FieldRallyPoint UMETA(DisplayName = "Field Rally Point")
};

USTRUCT(BlueprintType)
struct FBHFortificationPlanProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ConstructionSupplyCost = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ConstructionWorkDuration = 5.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MaximumHealth = 500.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float AICoverQuality = 0.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float WeaponBraceQuality = 0.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float StrategicDefenseValue = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector MeshScale = FVector(0.45f, 2.4f, 0.65f);
};

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHFieldFortification
    : public ABHCoverPoint,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHFieldFortification();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual float TakeDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation() const override;

    UFUNCTION(BlueprintCallable, Category = "Field Fortification")
    void ConfigureFortification(
        FName NewPersistenceID,
        FName NewSectorID
    );

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    FName GetPersistenceID() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    FName GetSectorID() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    bool IsConstructed() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    float GetHealthFraction() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    float GetStrategicDefenseValue() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    float GetAICoverQuality() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    float GetWeaponBraceQuality() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    float GetCurrentPlanProfileDefense() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    EBHFortificationPlan GetSelectedPlan() const;

    UFUNCTION(BlueprintCallable, Category = "Field Fortification")
    void SetSelectedPlan(EBHFortificationPlan NewPlan);

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    int32 GetSupplyCacheChargesRemaining() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    int32 GetMaxSupplyCacheCharges() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    int32 GetRallyDeploymentsRemaining() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    int32 GetMaxRallyDeployments() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    int32 GetLastObservationTurn() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    float GetObservationProgress() const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Field Fortification|Logistics"
    )
    int32 ConsumeRallyDeployments(int32 DeploymentCount = 1);

    UFUNCTION(
        BlueprintCallable,
        Category = "Field Fortification|Logistics"
    )
    int32 ConsumeSupplyCacheCharges(int32 ChargeCount = 1);

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    bool IsDismantling() const;

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    int32 GetActiveWorkerCount() const;

    UFUNCTION(BlueprintCallable, Category = "Field Fortification")
    void ReportObservationProgress(int32 CurrentTurn);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Field Fortification|Persistence")
    void RestorePersistentState(
        const FTransform& SavedTransform,
        bool bSavedConstructed,
        float SavedHealthFraction,
        EBHFortificationPlan SavedPlan,
        float SavedWorkProgress,
        bool bSavedDismantleWork,
        int32 SavedActiveWorkerCount,
        int32 SavedSupplyCacheCharges,
        int32 SavedLastObservationTurn,
        int32 SavedRallyDeploymentsRemaining,
        float SavedObservationProgress
    );

    UFUNCTION(BlueprintPure, Category = "Field Fortification")
    float GetConstructionProgress() const;

    UFUNCTION(BlueprintCallable, Category = "Field Fortification")
    void SetConstructionWorkers(int32 WorkerCount);

    static bool CanConstructForSector(
        const FBHWarSectorState& Sector,
        float RequiredSupply
    );

    static float CalculateRepairSupplyCost(
        float HealthFraction,
        float FullRepairSupplyCost
    );

    static float CalculateWorkProgress(
        float WorkHours,
        float WorkDuration
    );

    static float CalculateAssistedWorkRate(
        int32 ActiveWorkers,
        float WorkProgress,
        float BaseWorkRate
    );

    static float CalculateDismantleRecovery(
        float ConstructionCost,
        float HealthFraction,
        float RecoveryMultiplier
    );

    static bool ShouldWorkerRemainAssigned(
        bool bPlayerActive,
        bool bPlayerIncapacitated,
        bool bPlayerBusy,
        float DistanceToFortification,
        float WorkRadius
    );

    static FBHFortificationPlanProfile BuildPlanProfile(
        EBHFortificationPlan Plan
    );

    static bool IsRallyDeploymentSafe(
        bool bSectorFriendly,
        EBHFortificationPlan Plan,
        float HealthFraction,
        int32 HostileWavePressure,
        float DistanceToHostileFront,
        float RallyDistanceToFriendly
    );

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Fortification|Economy", meta = (ClampMin = "0.0", Units = "s"))
    float ConstructionUpdateIntervalSeconds = 1.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Logistics",
        meta = (ClampMin = "0")
    )
    int32 MaxSupplyCacheCharges = 6;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Tactical",
        meta = (ClampMin = "0")
    )
    int32 MaxRallyDeployments = 3;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Economy",
        meta = (ClampMin = "0.0")
    )
    float WorkAccelerationPerAdditionalWorker = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Logistics",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float RallyDamagePenaltyMultiplier = 0.5f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Components"
    )
    TObjectPtr<UStaticMeshComponent> BarricadeMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Components"
    )
    TObjectPtr<UTextRenderComponent> StatusLabel;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Field Fortification|Persistence")
    FName PersistenceID = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Field Fortification|War")
    FName SectorID = NAME_None;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Economy",
        meta = (ClampMin = "0.0")
    )
    float ConstructionSupplyCost = 12.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Economy",
        meta = (ClampMin = "0.0")
    )
    float FullRepairSupplyCost = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Durability",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float DismantleRecoveryMultiplier = 0.25f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Field Fortification|Durability",
        meta = (ClampMin = "1.0")
    )
    float MaximumHealth = 600.0f;

private:
    UFUNCTION()
    void OnRep_FortificationState();
    UFUNCTION()
    void OnRep_FortificationWork();

    void RefreshPresentation();
    void NotifyPlayer(ABHCharacter* Character, const FText& Message) const;
    void ReconcileWorkProgress(float DeltaSeconds);
    FBHFortificationPlanProfile GetActivePlanProfile() const;
    float GetCurrentPlanHealth() const;
    void ApplyPlanProfile(const FBHFortificationPlanProfile& PlanProfile);

    UPROPERTY(
        ReplicatedUsing = OnRep_FortificationState
    )
    bool bConstructed = false;

    UPROPERTY(
        ReplicatedUsing = OnRep_FortificationState
    )
    float CurrentHealth = 0.0f;

    UPROPERTY(
        Replicated,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Plan",
        meta = (AllowPrivateAccess = "true")
    )
    EBHFortificationPlan SelectedPlan = EBHFortificationPlan::HastyBarricade;

    UPROPERTY(
        ReplicatedUsing = OnRep_FortificationWork,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Build",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0")
    )
    float WorkProgress = 0.0f;

    UPROPERTY(
        Replicated,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Build",
        meta = (AllowPrivateAccess = "true")
    )
    int32 ActiveWorkerCount = 0;

    UPROPERTY(
        Replicated,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Build",
        meta = (AllowPrivateAccess = "true")
    )
    bool bDismantleWork = false;

    UPROPERTY(
        Replicated,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Logistics",
        meta = (AllowPrivateAccess = "true")
    )
    int32 SupplyCacheChargesRemaining = 0;

    UPROPERTY(
        Replicated,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Recon",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0")
    )
    float ObservationProgress = 0.0f;

    UPROPERTY(
        Replicated,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Recon",
        meta = (AllowPrivateAccess = "true")
    )
    int32 LastObservationTurn = INDEX_NONE;

    UPROPERTY(
        Replicated,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Fortification|Tactical",
        meta = (AllowPrivateAccess = "true")
    )
    int32 RallyDeploymentsRemaining = 0;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Field Fortification|Placement",
        meta = (ClampMin = "1")
    )
    int32 MaxFortificationsPerSector = 1;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Field Fortification|Placement",
        meta = (ClampMin = "0.0")
    )
    float HeavyCombatPlacementPressure = 80.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Field Fortification|Logistics",
        meta = (ClampMin = "0.0")
    )
    float SupplyCacheRefillCostPerCharge = 1.5f;

    float WorkAccumulator = 0.0f;
    FBHFortificationPlanProfile ActivePlanProfile;
};
