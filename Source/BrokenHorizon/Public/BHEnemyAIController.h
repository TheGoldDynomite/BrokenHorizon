#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "TimerManager.h"
#include "BHEnemyAIController.generated.h"

class ABHEnemySoldier;
class ABHCoverPoint;
class ABHPatrolPoint;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;

DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FBHOnHoldMoveFailed,
    ABHEnemySoldier*,
    const FVector&,
    const FVector&
);

UENUM(BlueprintType)
enum class EBHEnemyAIState : uint8
{
    Patrol,
    AlertInvestigate,
    Combat,
    EvadeExplosive,
    Retreat,
    Search,
    ReturnToPatrol,
    Dead
};

UCLASS(Blueprintable)
class BROKENHORIZON_API ABHEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    ABHEnemyAIController();

    FBHOnHoldMoveFailed OnHoldMoveFailed;

    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintPure, Category = "Enemy AI")
    EBHEnemyAIState GetCurrentState() const;

    static EBHEnemyAIState ResolveNavigationFailureFallback(
        EBHEnemyAIState FailedState
    );

    static bool ResolveVisualContactState(
        bool bStartingNewCombat,
        bool bReceivingSquadAlert,
        bool bHasSightOfPreviousTarget,
        bool bHasLineOfSight
    );

    static float CalculateCoverSelectionScore(
        float DistanceToCover,
        float DistanceToTarget,
        float CoverQuality
    );

    static bool CanAcceptSuppressiveFireOrder(
        bool bIsAlive,
        bool bIncapacitated,
        int32 RoundsAvailable,
        bool bTargetHostile
    );

    static float CalculateSuppressiveFireSpread(
        float IncomingSpread,
        bool bSuppressiveOrder
    );

    void SetOperationGarrisonActive(bool bActive);
    void NotifyPawnDamaged(AActor* DamageCauser);
    void NotifySuppressed(AActor* SourceActor, float Amount);
    void NotifyAllyAlert(AActor* TargetActor);
    void SetFollowTarget(
        AActor* NewFollowTarget,
        const FVector& LocalFormationOffset
    );
    void ClearFollowTarget();
    bool HasFollowTarget() const;
    void SetHoldPosition(const FVector& NewHoldLocation);
    void SetMoveAndHoldPosition(
        const FVector& NewHoldLocation,
        float CommandYaw
    );
    void ClearHoldPosition();
    bool HasHoldPosition() const;
    bool NotifyGrenadeThreat(
        const FVector& ThreatLocation,
        float DangerRadius,
        float TimeUntilDetonation
    );
    void HandlePawnDeath();

protected:
    virtual void OnPossess(APawn* InPawn) override;

    virtual void OnMoveCompleted(
        FAIRequestID RequestID,
        const FPathFollowingResult& Result
    ) override;

    UFUNCTION()
    void HandleTargetPerceptionUpdated(
        AActor* Actor,
        FAIStimulus Stimulus
    );

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Enemy AI|Perception"
    )
    TObjectPtr<UAIPerceptionComponent> AIPerception;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Enemy AI"
    )
    EBHEnemyAIState CurrentState = EBHEnemyAIState::Patrol;

private:
    ABHEnemySoldier* GetEnemySoldier() const;
    AActor* ResolveCombatTarget(AActor* Candidate) const;
    bool IsValidCombatTarget(AActor* Candidate) const;
    AActor* FindVisibleCombatTarget();
    bool CanAcquireVisualTarget(AActor* Candidate) const;
    void ConfigurePerception();
    void SetState(EBHEnemyAIState NewState);
    void EnterCombat(AActor* NewTarget);
    bool ShouldRetreat(const ABHEnemySoldier* Enemy) const;
    bool EnterExplosiveEvade(
        const FVector& ThreatLocation,
        float DangerRadius,
        float TimeUntilDetonation
    );
    void ResumeAfterExplosiveEvade();
    void EnterRetreat(AActor* ThreatActor);
    void CompleteAmmunitionWithdrawal();
    void EnterInvestigate(const FVector& StimulusLocation);
    void EnterSearch(const FVector& SearchLocation);
    void EnterReturnToPatrol();
    void BeginPatrolMove();
    void AdvancePatrol();
    void SchedulePatrolAdvance();
    void MoveToNearestPatrolPoint();
    void UpdateFollowMovement();
    void UpdateHoldMovement();
    void HandleHoldMoveFailure(const TCHAR* FailureReason);
    void HandleNavigationMoveFailure();
    void UpdateCombat(float DeltaSeconds);
    bool HasUnobscuredLineOfSight(AActor* Target) const;
    void PursueLastKnownTarget(
        ABHEnemySoldier* Enemy,
        float DeltaSeconds
    );
    void UpdateCombatMovement(
        ABHEnemySoldier* Enemy,
        AActor* Target,
        float Distance
    );
    void UpdateCombatFire(
        ABHEnemySoldier* Enemy,
        AActor* Target,
        float Distance
    );
    bool TryThrowGrenade(
        ABHEnemySoldier* Enemy,
        const FVector& TargetLocation
    );
    bool IsGrenadeTargetSafe(
        const ABHEnemySoldier* Enemy,
        const FVector& TargetLocation
    ) const;
    bool UpdateCoverTactics(
        ABHEnemySoldier* Enemy,
        AActor* Target,
        float DeltaSeconds,
        bool& bOutCanFire
    );
    bool TryClaimCover(
        ABHEnemySoldier* Enemy,
        AActor* Target
    );
    bool IsCoverProtective(
        const ABHCoverPoint* CoverPoint,
        AActor* Target
    ) const;
    bool HasActiveCover() const;
    void ReleaseCover();
    void UpdateSuppression(
        const ABHEnemySoldier* Enemy,
        float DeltaSeconds
    );
    void ResetCombatTactics();
    void UpdateMovementFacing(float DeltaSeconds);
    void FaceLocation(
        const FVector& TargetLocation,
        float DeltaSeconds
    );
    void DrawDebugState() const;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

    TWeakObjectPtr<AActor> CombatTarget;
    TWeakObjectPtr<AActor> FollowTarget;
    FVector FollowLocalOffset = FVector::ZeroVector;
    FVector HoldLocation = FVector::ZeroVector;
    float HoldFacingYaw = 0.0f;
    FVector LastKnownTargetLocation = FVector::ZeroVector;
    int32 CurrentPatrolIndex = 0;
    float StateEndTime = 0.0f;
    float LastConfirmedTargetTime = -BIG_NUMBER;
    float NextCombatPursuitMoveTime = 0.0f;
    bool bHasSightOfTarget = false;
    bool bMoveRequested = false;
    int32 RemainingBurstShots = 0;
    float NextAllowedBurstTime = 0.0f;
    float NextCombatRepositionTime = 0.0f;
    float CombatStrafeEndTime = 0.0f;
    float CombatStrafeDirection = 1.0f;
    float NextPatrolRetryTime = 0.0f;
    float NextFollowMoveTime = 0.0f;
    float NextHoldMoveTime = 0.0f;
    float NextGrenadeDecisionTime = 0.0f;
    FVector LastExplosiveThreatLocation = FVector::ZeroVector;
    float SuppressionLevel = 0.0f;
    float NextCoverEvaluationTime = 0.0f;
    float CoverHoldEndTime = 0.0f;
    float CoverPhaseEndTime = 0.0f;
    bool bPeekingRight = true;
    bool bMovingToCoverAnchor = false;
    bool bHiddenInCover = false;
    bool bMovingToPeek = false;
    bool bPeekingFromCover = false;
    bool bReturningToCover = false;
    bool bOperationGarrisonActive = true;
    bool bReceivingSquadAlert = false;
    bool bLoggedFactionEngagement = false;
    bool bWithdrawingForAmmunition = false;
    bool bHoldingPosition = false;
    bool bHasHoldFacingYaw = false;
    int32 LastMoveResultCode = 0;
    int32 ConsecutiveHoldMoveFailures = 0;
    float AmmunitionWithdrawalEndTime = -BIG_NUMBER;
    TWeakObjectPtr<ABHCoverPoint> ClaimedCoverPoint;
    FTimerHandle PatrolWaitTimerHandle;
};
