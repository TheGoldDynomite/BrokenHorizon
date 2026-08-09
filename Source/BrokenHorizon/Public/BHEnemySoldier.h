#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BHEnemySoldier.generated.h"

class ABHPatrolPoint;
class ABHAmmoSupply;
class ABHFragGrenade;
class ABHImpactEffect;
class UAnimMontage;
class UAnimSequenceBase;
class UBHHealthComponent;
class UNavigationInvokerComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class UTextRenderComponent;
struct FHitResult;

UENUM(BlueprintType)
enum class EBHCombatantArchetype : uint8
{
    Rifleman,
    Scout,
    Gunner
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHCombatantArchetypeProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EBHCombatantArchetype Archetype = EBHCombatantArchetype::Rifleman;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MaximumHealth = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MovementSpeed = 300.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float DesiredEngagementDistance = 1200.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MaximumEngagementDistance = 2500.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float CombatRepositionInterval = 2.25f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float CombatRepositionRadius = 450.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 MinimumBurstShots = 2;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 MaximumBurstShots = 4;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MinimumBurstRecovery = 1.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MaximumBurstRecovery = 2.25f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float CoverSearchRadius = 2200.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float CoverHoldDuration = 6.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float SuppressionCoverThreshold = 0.35f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float RetreatHealthThreshold = 0.35f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float RetreatSuppressionThreshold = 0.75f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float RetreatReadinessThreshold = 0.30f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ShotDamage = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float FireInterval = 0.75f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 MagazineCapacity = 30;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 StartingReserveAmmo = 60;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ReloadDuration = 2.6f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 MaximumFragGrenades = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float GrenadeUseChance = 0.35f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float SightRadius = 2500.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float LoseSightRadius = 3000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float HearingRange = 3500.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bHasBodyArmor = false;
};

UENUM(BlueprintType)
enum class EBHHitZone : uint8
{
    Head,
    Torso,
    Arm,
    Leg
};

UENUM(BlueprintType)
enum class EBHCombatFaction : uint8
{
    Friendly,
    Hostile
};

UENUM(BlueprintType)
enum class EBHEnemyBarkType : uint8
{
    Alert,
    Contact,
    Reload,
    Grenade,
    Casualty,
    Retreat,
    Search,
    Surrender
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FBHOnFriendlyCasualtyExpired,
    AActor*,
    Operative
);

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHEnemySoldier : public ACharacter
{
    GENERATED_BODY()

public:
    ABHEnemySoldier();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintPure, Category = "Enemy|Health")
    UBHHealthComponent* GetHealthComponent() const;

    UFUNCTION(BlueprintPure, Category = "Enemy")
    bool IsDead() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Archetype")
    void SetCombatantArchetype(EBHCombatantArchetype NewArchetype);

    UFUNCTION(BlueprintPure, Category = "Enemy|Archetype")
    EBHCombatantArchetype GetCombatantArchetype() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Archetype")
    FText GetCombatantArchetypeDisplayName() const;

    static FBHCombatantArchetypeProfile BuildCombatantArchetypeProfile(
        EBHCombatantArchetype Archetype
    );

    static EBHCombatantArchetype ChooseFormationArchetype(
        int32 FormationIndex,
        int32 FormationSize
    );

    static bool ShouldNotifyControllerOfDamage(
        float RemainingHealth,
        bool bAlreadyDead
    );

    static bool ShouldSurrender(
        float SuppressionLevel,
        float CombatReadiness,
        bool bOutOfAmmunition,
        int32 NearbyAllies
    );

    static float CalculateSurrenderEscapeRemaining(
        float CurrentRemainingSeconds,
        float DeltaSeconds,
        bool bFriendlyPlayerNearby,
        float CustodyGraceSeconds
    );

UFUNCTION(BlueprintPure, Category = "Enemy|Morale")
bool IsSurrendered() const;

FName GetSurrenderSectorID() const;

void RestoreSurrenderPersistence(bool bNewSurrendered, bool bNewCustodySecured, float NewEscapeSecondsRemaining);

    UFUNCTION(BlueprintPure, Category = "Enemy|Morale")
    bool IsSurrenderSecured() const;

    void SetSurrendered(bool bNewSurrendered);

    UFUNCTION(BlueprintCallable, Category = "Enemy|Morale")
    bool SecureSurrender();

    void UpdateSurrenderEscapeState(
        float DeltaSeconds,
        bool bFriendlyPlayerNearby
    );

    UFUNCTION(BlueprintPure, Category = "Enemy|Casualty")
    bool IsIncapacitated() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Casualty")
    bool StabilizeIncapacitatedSoldier();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Casualty")
    void RestoreIncapacitatedState();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Casualty")
    void RestoreIncapacitatedStateWithRemainingTime(
        float SavedRemainingSeconds
    );

    UFUNCTION(BlueprintPure, Category = "Enemy|Casualty")
    float GetIncapacitationSecondsRemaining() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Casualty")
    bool RequiresMedicalEvacuation() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Casualty")
    FName GetFieldOperativeID() const;

    void SetFieldOperativeID(FName NewFieldOperativeID);

    void RestoreMedicalEvacuationState(
        bool bSavedRequiresMedicalEvacuation
    );

    UPROPERTY(BlueprintAssignable, Category = "Enemy|Casualty")
    FBHOnFriendlyCasualtyExpired OnFriendlyCasualtyExpired;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Faction")
    void SetCombatFaction(EBHCombatFaction NewCombatFaction);

    UFUNCTION(BlueprintPure, Category = "Enemy|Faction")
    EBHCombatFaction GetCombatFaction() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Faction")
    bool IsHostileTo(const AActor* OtherActor) const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Objective")
    void SetObjectiveIdToCompleteOnDeath(FName ObjectiveID);

    UFUNCTION(BlueprintPure, Category = "Enemy|Objective")
    FName GetObjectiveIdToCompleteOnDeath() const;

    EBHHitZone ResolveHitZone(const FHitResult& HitResult) const;
    float GetHitZoneDamageMultiplier(EBHHitZone HitZone) const;
    bool IsArmorMitigatingHitZone(EBHHitZone HitZone) const;
    static bool DoesArmorMitigateHitZone(
        EBHHitZone HitZone,
        bool bHelmetEquipped,
        float InHelmetDamageScale,
        bool bBodyArmorEquipped,
        float InBodyArmorDamageScale
    );

    const TArray<TObjectPtr<ABHPatrolPoint>>& GetPatrolPoints() const;

    void SetPatrolPoints(
        const TArray<ABHPatrolPoint*>& NewPatrolPoints
    );

    float GetPatrolAcceptanceRadius() const;
    float GetPatrolWaitDuration() const;
    float GetInvestigateDuration() const;
    float GetSearchDuration() const;
    float GetMinimumEngagementDistance() const;
    float GetDesiredEngagementDistance() const;
    float GetMaximumEngagementDistance() const;
    float GetCombatRepositionInterval() const;
    float GetCombatRepositionRadius() const;
    int32 GetMinimumBurstShots() const;
    int32 GetMaximumBurstShots() const;
    float GetMinimumBurstRecovery() const;
    float GetMaximumBurstRecovery() const;
    float GetCoverSearchRadius() const;
    float GetCoverAcceptanceRadius() const;
    float GetCoverHoldDuration() const;
    float GetCoverHideDuration() const;
    float GetCoverPeekDuration() const;
    float GetCoverReevaluationInterval() const;
    float GetSuppressionDecayRate() const;
    float GetSuppressionCoverThreshold() const;
    float GetSuppressionSpreadPenalty() const;
    float GetRetreatHealthThreshold() const;
    float GetRetreatSuppressionThreshold() const;
    float GetRetreatReadinessThreshold() const;
    float GetRetreatDistance() const;
    float GetRetreatDuration() const;
    float GetNormalMovementSpeed() const;
    float GetRetreatMovementSpeed() const;
    float GetAllyCasualtyMoraleRadius() const;
    float GetAllyCasualtySuppression() const;
    float GetSightRadius() const;
    float GetLoseSightRadius() const;
    float GetPeripheralVisionAngle() const;
    float GetSightMemoryDuration() const;
    float GetHearingRange() const;
    float GetHearingMemoryDuration() const;
    float GetSquadAlertRadius() const;
    float GetRotationInterpSpeed() const;
    float GetPatrolRetryInterval() const;
    bool IsDebugEnabled() const;
    bool IsReloading() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Presentation|Voice")
    bool PlayBark(EBHEnemyBarkType BarkType);

    static bool CanPlayBark(
        float CurrentTime,
        float LastBarkTime,
        float MinimumInterval
    );

    UFUNCTION(BlueprintPure, Category = "Enemy|Combat|Ammunition")
    int32 GetCurrentMagazineAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Combat|Ammunition")
    int32 GetCurrentReserveAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Combat|Ammunition")
    bool HasCombatAmmunition() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Combat|Ammunition")
    bool IsOutOfAmmunition() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat|Ammunition")
    void RefillAmmunition();

    void RestorePersistentCombatState(
        float SavedHealth,
        int32 SavedMagazineAmmo,
        int32 SavedReserveAmmo,
        int32 SavedFragGrenades = -1,
        float SavedCombatReadiness = 1.0f
    );

    UFUNCTION(BlueprintCallable, Category = "Enemy|Persistence")
    void RestorePersistentDeathState();

    UFUNCTION(BlueprintPure, Category = "Enemy|Combat|Service")
    float GetCombatReadiness() const;

    float GetSurrenderAllyRadius() const;
    float GetSurrenderPlayerCaptureRadius() const;
    float GetSurrenderCustodyGraceSeconds() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Morale")
    float GetSurrenderEscapeSecondsRemaining() const;

    UFUNCTION(BlueprintPure, Category = "Enemy|Combat|Service")
    bool NeedsCombatService() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat|Service")
    bool ServiceCombatLoadout();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat|Ammunition")
    ABHAmmoSupply* DropRemainingAmmunition();

    int32 GetMagazineCapacity() const;
    int32 GetStartingReserveAmmo() const;
    float GetReloadDuration() const;
    UFUNCTION(BlueprintPure, Category = "Enemy|Combat|Grenades")
    int32 GetCurrentFragGrenades() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat|Grenades")
    void RefillFragGrenades();

    int32 GetMaximumFragGrenades() const;
    float GetMinimumGrenadeRange() const;
    float GetMaximumGrenadeRange() const;
    float GetGrenadeDecisionInterval() const;
    float GetGrenadeUseChance() const;
    float GetGrenadeFriendlySafetyRadius() const;

    bool FireAt(
        AActor* TargetActor,
        float AdditionalSpreadDegrees = 0.0f
    );
    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat|Grenades")
    bool ThrowFragGrenadeAt(const FVector& TargetLocation);

    void ApplySuppression(
        float Amount,
        AActor* SourceActor
    );

    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Death")
    void OnEnemyDeathCosmetics(AActor* DamageCauser);

    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Presentation")
    void OnEnemyHitCosmetics(
        float DamageApplied,
        AActor* DamageCauser
    );

    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Presentation")
    void OnEnemyFireCosmetics(AActor* TargetActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Presentation")
    void OnEnemyReloadStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Presentation")
    void OnEnemyGrenadeThrown(const FVector& TargetLocation);

    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Morale|Presentation")
    void OnEnemySurrendered(bool bIsSurrendered);

    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Morale|Presentation")
    void OnEnemyCustodySecured(bool bIsSecured);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleDamaged(float DamageApplied, AActor* DamageCauser);

    UFUNCTION()
    void HandleDeath(AActor* DamageCauser);

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Enemy|Health"
    )
    TObjectPtr<UBHHealthComponent> HealthComponent;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Enemy|Navigation"
    )
    TObjectPtr<UNavigationInvokerComponent> NavigationInvoker;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Enemy|Archetype|Presentation"
    )
    TObjectPtr<UTextRenderComponent> ArchetypeLabel;

    UPROPERTY(
        ReplicatedUsing = OnRep_CombatantArchetype,
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Enemy|Archetype"
    )
    EBHCombatantArchetype CombatantArchetype =
        EBHCombatantArchetype::Rifleman;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Damage Model",
        meta = (ClampMin = "0.0")
    )
    float HeadDamageMultiplier = 4.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Damage Model",
        meta = (ClampMin = "0.0")
    )
    float TorsoDamageMultiplier = 1.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Damage Model",
        meta = (ClampMin = "0.0")
    )
    float ArmDamageMultiplier = 0.65f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Damage Model",
        meta = (ClampMin = "0.0")
    )
    float LegDamageMultiplier = 0.70f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Damage Model|Armor"
    )
    bool bHasHelmet = false;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Damage Model|Armor",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float HelmetDamageScale = 0.55f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Damage Model|Armor"
    )
    bool bHasBodyArmor = false;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Damage Model|Armor",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float BodyArmorDamageScale = 0.75f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Casualty"
    )
    bool bAllowFriendlyIncapacitation = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Casualty",
        meta = (ClampMin = "1.0")
    )
    float StabilizedHealth = 35.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Casualty",
        meta = (ClampMin = "30.0", Units = "s")
    )
    float IncapacitationDuration = 300.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Casualty",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float PostCasualtyCombatReadiness = 0.55f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Morale",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float SurrenderAllyRadius = 1800.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Morale",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float SurrenderPlayerCaptureRadius = 850.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Morale",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float SurrenderCustodyGraceSeconds = 30.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Casualty",
        meta = (ClampMin = "0.0", Units = "Degrees")
    )
    float MaximumReadinessSpreadPenalty = 4.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Casualty",
        meta = (ClampMin = "1.0")
    )
    float MaximumReadinessFireIntervalMultiplier = 1.35f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Enemy|Combat"
    )
    TObjectPtr<USceneComponent> MuzzlePoint;

    UPROPERTY(
        ReplicatedUsing = OnRep_CombatFaction,
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Enemy|Faction"
    )
    EBHCombatFaction CombatFaction = EBHCombatFaction::Hostile;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Enemy|Patrol"
    )
    TArray<TObjectPtr<ABHPatrolPoint>> PatrolPoints;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Patrol",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float PatrolAcceptanceRadius = 75.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Patrol",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float PatrolWaitDuration = 1.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Patrol",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float PatrolRetryInterval = 1.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float InvestigateDuration = 3.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float SearchDuration = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float SightRadius = 2500.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float LoseSightRadius = 3000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness",
        meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees")
    )
    float PeripheralVisionAngle = 70.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float SightMemoryDuration = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float HearingRange = 3500.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float HearingMemoryDuration = 4.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Awareness|Coordination",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float SquadAlertRadius = 2400.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Movement",
        meta = (ClampMin = "0.1")
    )
    float RotationInterpSpeed = 8.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Movement",
        meta = (ClampMin = "0.0", Units = "cm/s")
    )
    float NormalMovementSpeed = 300.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Tactics",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float MinimumEngagementDistance = 550.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Tactics",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float DesiredEngagementDistance = 1200.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Tactics",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float MaximumEngagementDistance = 2500.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Tactics",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float CombatRepositionInterval = 2.25f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Tactics",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float CombatRepositionRadius = 450.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Burst",
        meta = (ClampMin = "1")
    )
    int32 MinimumBurstShots = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Burst",
        meta = (ClampMin = "1")
    )
    int32 MaximumBurstShots = 4;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Burst",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float MinimumBurstRecovery = 1.5f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Burst",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float MaximumBurstRecovery = 2.25f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Cover",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float CoverSearchRadius = 2200.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Cover",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float CoverAcceptanceRadius = 75.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Cover",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float CoverHoldDuration = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Cover",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float CoverHideDuration = 0.8f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Cover",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float CoverPeekDuration = 1.6f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Cover",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float CoverReevaluationInterval = 2.5f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Suppression",
        meta = (ClampMin = "0.0")
    )
    float SuppressionDecayRate = 0.22f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Suppression",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float SuppressionCoverThreshold = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Suppression",
        meta = (ClampMin = "0.0", ClampMax = "30.0", Units = "Degrees")
    )
    float SuppressionSpreadPenalty = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Morale",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float RetreatHealthThreshold = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Morale",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float RetreatSuppressionThreshold = 0.75f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Morale",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float RetreatReadinessThreshold = 0.30f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Morale",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float RetreatDistance = 1800.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Morale",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float RetreatDuration = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Morale",
        meta = (ClampMin = "0.0", Units = "cm/s")
    )
    float RetreatMovementSpeed = 525.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Morale",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float AllyCasualtyMoraleRadius = 1800.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Morale",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float AllyCasualtySuppression = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat",
        meta = (ClampMin = "0.0")
    )
    float ShotDamage = 10.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat",
        meta = (ClampMin = "0.05", Units = "s")
    )
    float FireInterval = 0.75f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Ammunition",
        meta = (ClampMin = "1")
    )
    int32 MagazineCapacity = 30;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Ammunition",
        meta = (ClampMin = "0")
    )
    int32 StartingReserveAmmo = 60;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Ammunition",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float ReloadDuration = 2.6f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Ammunition"
    )
    TSubclassOf<ABHAmmoSupply> BattlefieldAmmoSupplyClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Ammunition"
    )
    bool bDropAmmoOnDeath = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Ammunition",
        meta = (ClampMin = "1")
    )
    int32 MaximumDroppedAmmo = 30;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Grenades"
    )
    TSubclassOf<ABHFragGrenade> FragGrenadeClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Grenades",
        meta = (ClampMin = "0")
    )
    int32 MaximumFragGrenades = 1;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Grenades",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float MinimumGrenadeRange = 700.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Grenades",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float MaximumGrenadeRange = 1700.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Grenades",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float GrenadeDecisionInterval = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Grenades",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float GrenadeUseChance = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Grenades",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float GrenadeFriendlySafetyRadius = 700.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float ShotRange = 5000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat",
        meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "Degrees")
    )
    float ShotSpreadDegrees = 2.5f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Incoming Fire",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float NearMissRadius = 200.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Incoming Fire",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float NearMissMinimumIntensity = 0.20f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Suppression",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float NearMissSuppressionAmount = 0.30f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation"
    )
    FName MuzzleSocketName = TEXT("Muzzle");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation"
    )
    TObjectPtr<UAnimMontage> FireMontage;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation"
    )
    TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation"
    )
    TObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Audio")
    TObjectPtr<USoundBase> IndoorFireTailSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Audio")
    TObjectPtr<USoundBase> OutdoorFireTailSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice")
    TObjectPtr<USoundBase> AlertBark;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice")
    TObjectPtr<USoundBase> ContactBark;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice")
    TObjectPtr<USoundBase> ReloadBark;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice")
    TObjectPtr<USoundBase> GrenadeBark;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice")
    TObjectPtr<USoundBase> CasualtyBark;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice")
    TObjectPtr<USoundBase> RetreatBark;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice")
    TObjectPtr<USoundBase> SearchBark;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice")
    TObjectPtr<USoundBase> SurrenderBark;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation|Voice", meta = (ClampMin = "0.0", Units = "s"))
    float MinimumBarkInterval = 2.5f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation"
    )
    TSubclassOf<ABHImpactEffect> ImpactActorClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation|Reactions"
    )
    TArray<TObjectPtr<UAnimSequenceBase>> HitReactionAnimations;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation|Reactions"
    )
    TObjectPtr<UAnimSequenceBase> DeathAnimation;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation|Reactions"
    )
    FName ReactionSlotName = TEXT("DefaultSlot");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation|Reactions",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float MinimumHitReactionInterval = 0.12f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation|Death"
    )
    bool bEnableRagdollOnDeath = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation|Death",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float DeathRagdollDelay = 0.2f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation|Death",
        meta = (ClampMin = "0.0")
    )
    float DeathImpulse = 350.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Presentation|Death",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float CorpseLifeSpan = 20.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Objective"
    )
    FName ObjectiveIdToCompleteOnDeath = NAME_None;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Enemy|Debug"
    )
    bool bEnableDebug = false;

private:
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastFirePresentation(
        AActor* TargetActor,
        const FHitResult& HitResult,
        bool bPlayImpact
    );

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastHitPresentation(
        float DamageApplied,
        AActor* DamageCauser
    );

    UFUNCTION(NetMulticast, Reliable)
    void MulticastDeathPresentation(
        AActor* DamageCauser,
        bool bFriendlyIncapacitation
    );

    UFUNCTION(NetMulticast, Reliable)
    void MulticastRestoreFromIncapacitation();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayBark(EBHEnemyBarkType BarkType);

    UFUNCTION()
    void OnRep_CombatFaction();

    UFUNCTION()
    void OnRep_CombatantArchetype();

    UFUNCTION()
    void OnRep_Incapacitated();

    UFUNCTION()
    void OnRep_Surrendered();

    UFUNCTION()
    void OnRep_SurrenderSecured();

    void UpdateCombatFactionTags();
    void CaptureArchetypeBaseline();
    void ApplyCombatantArchetype(bool bResetResources);
    void RefreshArchetypePresentation();

    FTransform GetPresentationMuzzleTransform() const;
    void PlayFirePresentation(AActor* TargetActor);
    void PlayImpactPresentation(const FHitResult& HitResult);
    bool IsHitPartOfTarget(
        AActor* HitActor,
        AActor* TargetActor
    ) const;
    bool HasClearLineOfFireTo(
        const FVector& TraceStart,
        const FVector& TargetLocation,
        AActor* TargetActor
    ) const;
    void PlayHitReaction();
    void PlayDeathReaction(AActor* DamageCauser);
    void EnableDeathRagdoll();
    void EnterFriendlyIncapacitation(
        AActor* DamageCauser,
        bool bPlayPresentation
    );
    void RestoreFromDeathPresentation();
    void StartIncapacitationTimer();
    void UpdateIncapacitationTimer();
    void ExpireFriendlyIncapacitation();
    USoundBase* ResolveBarkSound(EBHEnemyBarkType BarkType) const;
    bool IsMuzzleEnvironmentEnclosed(const FVector& MuzzleLocation) const;
    bool UpdateReloadState(float CurrentTime);
    void BeginReload(float CurrentTime);
    FName ResolveSurrenderSectorID() const;
    void ReportSurrenderDetained();
    void ReportSurrenderConductOutcome(bool bKilled);

    float LastFireTime = -BIG_NUMBER;
    float ReloadEndTime = -BIG_NUMBER;
    float LastHitReactionTime = -BIG_NUMBER;
    float LastBarkTime = -BIG_NUMBER;
    int32 CurrentMagazineAmmo = 0;
    int32 CurrentReserveAmmo = 0;
    UPROPERTY(
        VisibleInstanceOnly,
        Category = "Enemy|Combat|Grenades"
    )
    int32 CurrentFragGrenades = 0;
    bool bReloading = false;
    bool bOutOfAmmoReported = false;
    bool bAmmoDropSpawned = false;
    bool bDeathHandled = false;
    bool bSurrenderConductReported = false;
    bool bSurrenderDetentionReported = false;
    UPROPERTY(ReplicatedUsing = OnRep_Incapacitated)
    bool bIncapacitated = false;
    UPROPERTY(
        Replicated,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Enemy|Casualty",
        meta = (AllowPrivateAccess = "true")
    )
    bool bRequiresMedicalEvacuation = false;
    UPROPERTY(
        Replicated,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Enemy|Casualty",
        meta = (AllowPrivateAccess = "true")
    )
    FName FieldOperativeID = NAME_None;
    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Enemy|Combat|Service",
        meta = (AllowPrivateAccess = "true")
    )
    float CombatReadiness = 1.0f;
    UPROPERTY(Replicated)
    float IncapacitationSecondsRemaining = 0.0f;
    UPROPERTY(ReplicatedUsing = OnRep_Surrendered, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Enemy|Morale",
        meta = (AllowPrivateAccess = "true"))
    bool bSurrendered = false;
    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Enemy|Morale",
        meta = (ClampMin = "0.0", Units = "s", AllowPrivateAccess = "true"))
    float SurrenderEscapeSecondsRemaining = 0.0f;
    UPROPERTY(ReplicatedUsing = OnRep_SurrenderSecured,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Enemy|Morale",
        meta = (AllowPrivateAccess = "true"))
    bool bSurrenderSecured = false;
    bool bLoggedFactionDamage = false;
    FBHCombatantArchetypeProfile ArchetypeBaseline;
    bool bHasArchetypeBaseline = false;
    TWeakObjectPtr<AActor> LastDamageCauser;
    FTransform InitialMeshRelativeTransform =
        FTransform::Identity;
    FTimerHandle DeathRagdollTimerHandle;
    FTimerHandle IncapacitationTimerHandle;
};
