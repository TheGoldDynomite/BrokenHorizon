#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "BHOpenWorldOperationDirector.generated.h"

class ABHCharacter;
class ABHEnemySoldier;
class ABHPatrolPoint;
class ABHRaidSabotageTarget;
class ABHSectorAnchor;
enum class EBHActiveOperationPhase : uint8;

UCLASS()
class BROKENHORIZON_API ABHOpenWorldOperationDirector
    : public AActor
{
    GENERATED_BODY()

public:
    ABHOpenWorldOperationDirector();

    virtual void Tick(float DeltaSeconds) override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    bool StartOperation(
        ABHCharacter* InPlayerCharacter,
        FName InSectorID,
        EBHWarPriorityType InOperationType,
        FName InSupplySourceSectorID,
        bool bSuppressInitialCheckpoint = false
    );

    static int32 ResolveOperationVariationIndex(
        FName SectorID,
        EBHWarPriorityType OperationType
    );

    static bool IsOperationSnapshotCompatible(
        FName SavedOperationID,
        FName CurrentOperationID
    );

    bool IsOperationInProgress() const;

    bool IsOperationActivated() const;

    float GetApproachSecondsRemaining() const;

    float CalculateApproachWindowSecondsForOperation(
        EBHWarPriorityType InOperationType,
        float PlayerDistance
    ) const;

    FVector GetOperationCenter() const;

    FText GetSectorDisplayName() const;

    FText GetOperationStatusText() const;

    int32 GetFriendlySupportCasualties() const;

    int32 GetEnemyCasualties() const;

    int32 GetEnemyRoutedCount() const;

    bool WasRaidDetectedBeforeSabotage() const;

    bool ToggleFriendlySupportHoldOrder();

    bool SetFriendlySupportMoveAndHoldOrder(
        const FVector& CommandLocation,
        float CommandYaw
    );

    bool SetFriendlySupportFollowOrder();

    bool IsFriendlySupportHolding() const;

    bool HasFriendlySupportCommandLocation() const;

    FVector GetFriendlySupportCommandLocation() const;

    int32 GetLivingFriendlySupportCount() const;

    FBHOpenWorldOperationState CaptureSaveState() const;

    void PublishOperationSnapshot(
        EBHActiveOperationPhase PhaseOverride
    );

    bool RestoreOperationState(
        const FBHOpenWorldOperationState& SavedState
    );

    void HandleRaidTargetSabotaged(
        ABHRaidSabotageTarget* SabotagedTarget
    );

#if !UE_BUILD_SHIPPING
    void CompleteOperationForTesting();
    void FailOperationForTesting();
#endif

protected:
    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "1")
    )
    int32 AttackEnemyCount = 3;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "0")
    )
    int32 AttackReinforcementWaveCount = 1;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "1")
    )
    int32 AttackReinforcementsPerWave = 1;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "1")
    )
    int32 DefenseWaveCount = 3;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "1")
    )
    int32 DefenseEnemiesPerWave = 2;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "0")
    )
    int32 MaximumFriendlySupport = 2;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float SpawnRadius = 3000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float DefenseInterWaveDelay = 6.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float AttackInterWaveDelay = 8.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float CasualtyCheckpointDelay = 0.75f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Movement",
        meta = (ClampMin = "1")
    )
    int32 ObjectivePatrolPointCount = 4;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Movement",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float ObjectivePatrolRadius = 1400.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Approach",
        meta = (ClampMin = "30.0", Units = "s")
    )
    float DefenseApproachBaseSeconds = 120.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Approach",
        meta = (ClampMin = "30.0", Units = "s")
    )
    float OffensiveApproachBaseSeconds = 240.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Approach",
        meta = (ClampMin = "100.0", Units = "cm/s")
    )
    float ExpectedApproachTravelSpeed = 1800.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Approach",
        meta = (ClampMin = "1.0")
    )
    float ApproachTravelTimeMultiplier = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Approach",
        meta = (ClampMin = "60.0", Units = "s")
    )
    float MaximumApproachSeconds = 900.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Objective",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float ObjectiveSecureRadius = 650.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Objective",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float ObjectiveSecureDuration = 8.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Objective",
        meta = (ClampMin = "0.0")
    )
    float ObjectiveSecureDecayMultiplier = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Defense",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float DefenseBreachDuration = 20.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Defense",
        meta = (ClampMin = "0.0")
    )
    float DefenseBreachRecoveryMultiplier = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Defense",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float DefenseBreachCheckpointInterval = 5.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Raid",
        meta = (ClampMin = "500.0", Units = "cm")
    )
    float RaidExfiltrationRadius = 3000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "Open World Operation|Morale",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float EnemyRoutWithdrawalDistance = 6500.0f;

private:
    void ResolveSectorAnchor();
    void ResolveEnemyClass();
    void ConfigureForcePackage();
    float CalculateApproachWindowSeconds(
        float PlayerDistance
    ) const;
    void ActivateOperation();
    bool ConfigureAuthoredDefenseAGarrison(bool bActivateGarrison);
    void BuildOperationPatrolPoints();
    void DestroyOperationPatrolPoints();
    void SpawnRaidSabotageTarget();
    void DestroyRaidSabotageTarget();
    void AlertRaidReactionForce();
    void UpdateRaidDetectionState();
    void UpdateEnemyRouts();
    void DestroyTrackedUnits();
    void RequestOperationCheckpoint(float DelaySeconds = 0.0f);
    void SaveOperationProgressNow();
    void SpawnEnemies(int32 EnemyCount);
    void SpawnFriendlySupport(int32 FriendlyCount);
    void ApplyFriendlySupportOrder();
    void ReportFriendlySupportCasualties();
    void UpdateAttackObjectiveSecuring(float DeltaSeconds);
    void UpdateDefenseObjectiveHolding(float DeltaSeconds);
    void UpdateDefenseBreach(float DeltaSeconds);
    bool IsPlayerInsideSecureArea() const;
    TArray<ABHCharacter*> GetPlayerParticipants() const;
    TArray<ABHCharacter*> GetLivingPlayerParticipants() const;
    ABHCharacter* FindLivingPlayerParticipant() const;
    float GetClosestParticipantDistanceToOperation() const;
    bool AreAllLivingParticipantsOutsideRadius(
        float Radius
    ) const;
    bool IsSecureAreaContested() const;
    bool HasPlayerOrFieldOperativeInSecureArea() const;
    bool HasLivingFieldOperativeInSecureArea() const;
    bool HasLivingDefenderInSecureArea() const;
    void CompleteOperation();
    void FailOperation(const FText& FailureReason);
    void PublishCurrentOperationSnapshot();
    void PublishOperationSnapshot(
        EBHActiveOperationPhase PhaseOverride,
        const FBHOpenWorldOperationState& OperationState
    );
    EBHActiveOperationPhase ResolveOperationPhase() const;
    bool HasLivingEnemies() const;
    int32 GetLivingEnemyCount() const;
    int32 GetLivingAllyCount() const;
    int32 GetSecondsUntilNextWave() const;
    void ShowOperationNotification(const FText& Message) const;
    TArray<ABHPatrolPoint*> BuildPatrolPointAssignment(
        int32 UnitIndex
    ) const;
    FTransform BuildSpawnTransform(
        int32 EnemyIndex,
        int32 AttemptIndex = 0
    ) const;
    FTransform BuildFriendlySpawnTransform(
        int32 FriendlyIndex,
        int32 AttemptIndex = 0
    ) const;

    UFUNCTION()
    void HandleFriendlySupportDeath(AActor* DamageCauser);

    UFUNCTION()
    void HandleEnemyDeath(AActor* DamageCauser);

    UPROPERTY()
    TObjectPtr<ABHCharacter> PlayerCharacter;

    UPROPERTY()
    TSubclassOf<ABHEnemySoldier> EnemyClass;

    UPROPERTY()
    TObjectPtr<ABHSectorAnchor> SectorAnchor;

    UPROPERTY()
    TArray<TObjectPtr<ABHEnemySoldier>> TrackedEnemies;

    UPROPERTY()
    TArray<TObjectPtr<ABHEnemySoldier>> TrackedAllies;

    UPROPERTY()
    TArray<TObjectPtr<ABHPatrolPoint>> OperationPatrolPoints;

    UPROPERTY()
    TObjectPtr<ABHRaidSabotageTarget> RaidSabotageTarget;

    FName SectorID = NAME_None;
    FName SupplySourceSectorID = NAME_None;
    FName EnemySourceSectorID = NAME_None;
    EBHWarPriorityType OperationType =
        EBHWarPriorityType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open World Operation|Variation", meta = (AllowPrivateAccess = "true"))
    int32 OperationVariationIndex = 0;

    float EffectiveObjectivePatrolRadius = 1400.0f;
    float EffectiveSpawnRadius = 3000.0f;
    int32 EffectiveAttackEnemyCount = 3;
    int32 EffectiveAttackReinforcementWaveCount = 1;
    int32 EffectiveAttackReinforcementsPerWave = 1;
    int32 EffectiveDefenseWaveCount = 3;
    int32 EffectiveDefenseEnemiesPerWave = 2;
    int32 EffectiveFriendlySupportCount = 0;
    int32 FriendlySupportCasualties = 0;
    int32 EnemyCasualties = 0;
    int32 EnemyRoutedCount = 0;
    int32 CurrentWave = 0;
    float NextWaveTime = 0.0f;
    float ApproachDeadlineTime = -1.0f;
    FVector OperationCenter = FVector::ZeroVector;
    bool bWaitingForWave = false;
    bool bSecuringObjective = false;
    bool bRaidTargetSabotaged = false;
    bool bRaidDetectedBeforeSabotage = false;
    bool bFriendlySupportHolding = false;
    bool bFriendlySupportHasCommandLocation = false;
    FVector FriendlySupportCommandLocation = FVector::ZeroVector;
    float FriendlySupportCommandYaw = 0.0f;
    bool bOperationActivated = false;
    bool bOperationComplete = false;
    bool bSuppressProgressCheckpoint = false;
    bool bCheckpointPending = false;
    float ObjectiveSecureProgress = 0.0f;
    float DefenseBreachProgress = 0.0f;
    float LastCheckpointedDefenseBreachProgress = 0.0f;
    FTimerHandle OperationCheckpointTimerHandle;
};


