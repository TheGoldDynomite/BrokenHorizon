#include "BHEnemyAIController.h"

#include "BHCharacter.h"
#include "BHBattlefieldConditions.h"
#include "BHCoverPoint.h"
#include "BHFieldFortification.h"
#include "BHEnemySoldier.h"
#include "BHTacticalSupportZone.h"
#include "BHHealthComponent.h"
#include "BHPatrolPoint.h"
#include "BHPlayerResolver.h"
#include "BHSupplyConvoyTarget.h"
#include "BrainComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"

namespace
{
constexpr float NavigationBuildRetrySeconds = 0.25f;
constexpr float NavigationStartupDelaySeconds = 0.5f;

constexpr float LocalSearchRadius = 700.0f;

bool IsNavigationBuilding(UWorld* World)
{
    return IsValid(World) &&
        UNavigationSystemV1::IsNavigationBeingBuilt(World);
}

bool TryFindLocalSearchLocation(
    UWorld* World,
    const FVector& Origin,
    FVector& OutLocation
)
{
    if (!IsValid(World) || IsNavigationBuilding(World))
    {
        return false;
    }

    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!IsValid(NavigationSystem))
    {
        return false;
    }

    FNavLocation ProjectedOrigin;
    if (!NavigationSystem->ProjectPointToNavigation(
            Origin,
            ProjectedOrigin,
            FVector(500.0f, 500.0f, 500.0f)
        ))
    {
        return false;
    }

    FNavLocation LocalLocation;
    if (!NavigationSystem->GetRandomReachablePointInRadius(
            ProjectedOrigin.Location,
            LocalSearchRadius,
            LocalLocation
        ))
    {
        return false;
    }

    OutLocation = LocalLocation.Location;
    return true;
}
}

ABHEnemyAIController::ABHEnemyAIController()
{
    PrimaryActorTick.bCanEverTick = true;

    AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(
        TEXT("AIPerception")
    );
    SetPerceptionComponent(*AIPerception);

    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(
        TEXT("SightConfig")
    );
    SightConfig->SightRadius = 2500.0f;
    SightConfig->LoseSightRadius = 3000.0f;
    SightConfig->PeripheralVisionAngleDegrees = 70.0f;
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(
        TEXT("HearingConfig")
    );
    HearingConfig->HearingRange = 3500.0f;
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

    AIPerception->ConfigureSense(*SightConfig);
    AIPerception->ConfigureSense(*HearingConfig);
    AIPerception->SetDominantSense(
        UAISense_Sight::StaticClass()
    );
}

void ABHEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    AIPerception->OnTargetPerceptionUpdated.RemoveDynamic(
        this,
        &ABHEnemyAIController::HandleTargetPerceptionUpdated
    );
    AIPerception->OnTargetPerceptionUpdated.AddDynamic(
        this,
        &ABHEnemyAIController::HandleTargetPerceptionUpdated
    );

    ConfigurePerception();
    CurrentPatrolIndex = 0;
    NextGrenadeDecisionTime =
        GetWorld()->GetTimeSeconds() +
        FMath::FRandRange(3.0f, 6.0f);
    bWithdrawingForAmmunition = false;
    AmmunitionWithdrawalEndTime = -BIG_NUMBER;
    SetState(EBHEnemyAIState::Patrol);
    NextPatrolRetryTime =
        GetWorld()->GetTimeSeconds() + NavigationStartupDelaySeconds;
}

void ABHEnemyAIController::ConfigurePerception()
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy))
    {
        return;
    }

    SightConfig->SightRadius = Enemy->GetSightRadius();
    SightConfig->LoseSightRadius = Enemy->GetLoseSightRadius();
    SightConfig->PeripheralVisionAngleDegrees =
        Enemy->GetPeripheralVisionAngle();
    SightConfig->SetMaxAge(Enemy->GetSightMemoryDuration());

    HearingConfig->HearingRange = Enemy->GetHearingRange();
    HearingConfig->SetMaxAge(Enemy->GetHearingMemoryDuration());

    AIPerception->ConfigureSense(*SightConfig);
    AIPerception->ConfigureSense(*HearingConfig);
    AIPerception->RequestStimuliListenerUpdate();
}

void ABHEnemyAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy) || Enemy->IsDead())
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    UpdateSuppression(Enemy, DeltaSeconds);

    int32 NearbyAllies = 0;
    bool bFriendlyPlayerNearby = false;
    if (Enemy->GetCombatFaction() == EBHCombatFaction::Hostile ||
        Enemy->IsSurrendered())
    {
        for (TActorIterator<ABHEnemySoldier> SoldierIt(World);
            SoldierIt;
            ++SoldierIt)
        {
            ABHEnemySoldier* Candidate = *SoldierIt;
            if (!IsValid(Candidate) ||
                Candidate == Enemy ||
                Candidate->IsDead() ||
                Candidate->GetCombatFaction() !=
                    Enemy->GetCombatFaction())
            {
                continue;
            }

            if (FVector::DistSquared2D(
                    Candidate->GetActorLocation(),
                    Enemy->GetActorLocation()
                ) <= FMath::Square(Enemy->GetSurrenderAllyRadius()))
            {
                ++NearbyAllies;
            }
        }

        for (TActorIterator<ABHCharacter> CharacterIt(World);
            CharacterIt;
            ++CharacterIt)
        {
            ABHCharacter* PlayerCharacter = *CharacterIt;
            if (!IsValid(PlayerCharacter) ||
                !PlayerCharacter->IsPlayerControlled() ||
                PlayerCharacter->IsPlayerIncapacitated())
            {
                continue;
            }

            const UBHHealthComponent* PlayerHealth =
                PlayerCharacter->GetHealthComponent();
            if (!IsValid(PlayerHealth) || PlayerHealth->IsDead())
            {
                continue;
            }

            if (FVector::DistSquared2D(
                    PlayerCharacter->GetActorLocation(),
                    Enemy->GetActorLocation()
                ) <= FMath::Square(
                    Enemy->GetSurrenderPlayerCaptureRadius()
                ))
            {
                bFriendlyPlayerNearby = true;
                break;
            }
        }
    }

    if (Enemy->IsSurrendered())
    {
        StopMovement();
        ClearFocus(EAIFocusPriority::Gameplay);
        Enemy->UpdateSurrenderEscapeState(
            DeltaSeconds,
            bFriendlyPlayerNearby
        );
        if (!Enemy->IsSurrendered())
        {
            EnterRetreat(CombatTarget.Get());
        }
        return;
    }

    if (Enemy->GetCombatFaction() == EBHCombatFaction::Hostile &&
        ABHEnemySoldier::ShouldSurrender(
            SuppressionLevel,
            Enemy->GetCombatReadiness(),
            Enemy->IsOutOfAmmunition(),
            NearbyAllies
        ))
    {
        Enemy->SetSurrendered(true);
        StopMovement();
        ClearFocus(EAIFocusPriority::Gameplay);
        return;
    }

    if (CurrentState == EBHEnemyAIState::Combat &&
        ShouldRetreat(Enemy))
    {
        EnterRetreat(CombatTarget.Get());
    }

    // Sight perception normally supplies this transition.  Keep a small
    // controller-side fallback so a valid local player in clear view still
    // engages if a level's perception source registration is delayed.
    if (CurrentState != EBHEnemyAIState::Combat &&
        CurrentState != EBHEnemyAIState::Retreat &&
        CurrentState != EBHEnemyAIState::EvadeExplosive)
    {
        AActor* PlayerTarget = FindVisibleCombatTarget();

        if (IsValidCombatTarget(PlayerTarget))
        {
            bHasSightOfTarget = true;
            LastKnownTargetLocation =
                PlayerTarget->GetActorLocation();
            EnterCombat(PlayerTarget);
        }
    }

    switch (CurrentState)
    {
        case EBHEnemyAIState::Combat:
            UpdateCombat(DeltaSeconds);
            break;

        case EBHEnemyAIState::Retreat:
            UpdateMovementFacing(DeltaSeconds);

            if (World->GetTimeSeconds() >= StateEndTime)
            {
                if (bWithdrawingForAmmunition)
                {
                    if (World->GetTimeSeconds() >=
                        AmmunitionWithdrawalEndTime)
                    {
                        CompleteAmmunitionWithdrawal();
                        return;
                    }

                    EnterRetreat(nullptr);
                }
                else
                {
                    EnterSearch(LastKnownTargetLocation);
                }
            }
            break;

        case EBHEnemyAIState::EvadeExplosive:
            UpdateMovementFacing(DeltaSeconds);

            if (World->GetTimeSeconds() >= StateEndTime)
            {
                ResumeAfterExplosiveEvade();
            }
            break;

        case EBHEnemyAIState::AlertInvestigate:
            UpdateMovementFacing(DeltaSeconds);

            if (!bMoveRequested &&
                World->GetTimeSeconds() >= StateEndTime)
            {
                EnterReturnToPatrol();
            }
            break;

        case EBHEnemyAIState::Search:
            UpdateMovementFacing(DeltaSeconds);

            if (World->GetTimeSeconds() >= StateEndTime)
            {
                EnterReturnToPatrol();
            }
            break;

        case EBHEnemyAIState::Patrol:
            UpdateMovementFacing(DeltaSeconds);

            if (HasFollowTarget())
            {
                UpdateFollowMovement();
            }
            else if (HasHoldPosition())
            {
                UpdateHoldMovement();
            }
            else if (!bMoveRequested &&
                !World->GetTimerManager().IsTimerActive(
                    PatrolWaitTimerHandle
                ) &&
                World->GetTimeSeconds() >= NextPatrolRetryTime)
            {
                BeginPatrolMove();
            }
            break;

        case EBHEnemyAIState::ReturnToPatrol:
            UpdateMovementFacing(DeltaSeconds);

            if (!bMoveRequested &&
                World->GetTimeSeconds() >= NextPatrolRetryTime)
            {
                MoveToNearestPatrolPoint();
            }
            break;

        default:
            break;
    }

    DrawDebugState();
}

EBHEnemyAIState ABHEnemyAIController::GetCurrentState() const
{
    return CurrentState;
}

void ABHEnemyAIController::SetFollowTarget(
    AActor* NewFollowTarget,
    const FVector& LocalFormationOffset
)
{
    ClearHoldPosition();
    FollowTarget = NewFollowTarget;
    FollowLocalOffset = LocalFormationOffset;
    NextFollowMoveTime = 0.0f;

    if (!IsValid(NewFollowTarget))
    {
        ClearFollowTarget();
        return;
    }

    if (CurrentState == EBHEnemyAIState::Patrol ||
        CurrentState == EBHEnemyAIState::ReturnToPatrol)
    {
        StopMovement();
        bMoveRequested = false;
        SetState(EBHEnemyAIState::Patrol);
        UpdateFollowMovement();
    }
}

void ABHEnemyAIController::ClearFollowTarget()
{
    FollowTarget.Reset();
    FollowLocalOffset = FVector::ZeroVector;
    NextFollowMoveTime = 0.0f;
}

bool ABHEnemyAIController::HasFollowTarget() const
{
    return FollowTarget.IsValid();
}

void ABHEnemyAIController::SetHoldPosition(
    const FVector& NewHoldLocation
)
{
    ClearFollowTarget();
    HoldLocation = NewHoldLocation;
    bHoldingPosition = true;
    bHasHoldFacingYaw = false;
    ConsecutiveHoldMoveFailures = 0;
    NextHoldMoveTime = 0.0f;

    if (CurrentState == EBHEnemyAIState::Patrol ||
        CurrentState == EBHEnemyAIState::ReturnToPatrol)
    {
        StopMovement();
        bMoveRequested = false;
        SetState(EBHEnemyAIState::Patrol);
        UpdateHoldMovement();
    }
}

void ABHEnemyAIController::SetMoveAndHoldPosition(
    const FVector& NewHoldLocation,
    float CommandYaw
)
{
    SetHoldPosition(NewHoldLocation);
    HoldFacingYaw = FRotator::NormalizeAxis(CommandYaw);
    bHasHoldFacingYaw = true;
}

void ABHEnemyAIController::ClearHoldPosition()
{
    bHoldingPosition = false;
    HoldLocation = FVector::ZeroVector;
    HoldFacingYaw = 0.0f;
    bHasHoldFacingYaw = false;
    ConsecutiveHoldMoveFailures = 0;
    NextHoldMoveTime = 0.0f;
}

bool ABHEnemyAIController::HasHoldPosition() const
{
    return bHoldingPosition;
}

void ABHEnemyAIController::HandleTargetPerceptionUpdated(
    AActor* Actor,
    FAIStimulus Stimulus
)
{
    if (CurrentState == EBHEnemyAIState::Dead)
    {
        return;
    }

    AActor* PlayerTarget = ResolveCombatTarget(Actor);

    if (!IsValid(PlayerTarget))
    {
        return;
    }

    if (CurrentState == EBHEnemyAIState::Retreat)
    {
        if (Stimulus.WasSuccessfullySensed())
        {
            LastKnownTargetLocation =
                PlayerTarget->GetActorLocation();
        }
        return;
    }

    if (CurrentState == EBHEnemyAIState::EvadeExplosive)
    {
        if (Stimulus.WasSuccessfullySensed())
        {
            CombatTarget = PlayerTarget;
            LastKnownTargetLocation =
                PlayerTarget->GetActorLocation();
        }
        return;
    }

    const FAISenseID SightSenseID =
        UAISense::GetSenseID<UAISense_Sight>();
    const FAISenseID HearingSenseID =
        UAISense::GetSenseID<UAISense_Hearing>();

    if (Stimulus.Type == SightSenseID)
    {
        if (Stimulus.WasSuccessfullySensed())
        {
            if (CurrentState != EBHEnemyAIState::Combat &&
                !CanAcquireVisualTarget(PlayerTarget))
            {
                return;
            }

            bHasSightOfTarget = true;
            LastKnownTargetLocation = PlayerTarget->GetActorLocation();
            LastConfirmedTargetTime =
                GetWorld()->GetTimeSeconds();
            EnterCombat(PlayerTarget);
        }
        else if (CombatTarget.Get() == PlayerTarget)
        {
            bHasSightOfTarget = false;
            LastKnownTargetLocation =
                Stimulus.StimulusLocation.IsNearlyZero()
                ? PlayerTarget->GetActorLocation()
                : Stimulus.StimulusLocation;
        }
    }
    else if (
        Stimulus.Type == HearingSenseID &&
        Stimulus.WasSuccessfullySensed() &&
        CurrentState != EBHEnemyAIState::Combat)
    {
        LastKnownTargetLocation = Stimulus.StimulusLocation;
        EnterInvestigate(LastKnownTargetLocation);
    }
}

void ABHEnemyAIController::NotifyPawnDamaged(AActor* DamageCauser)
{
    if (CurrentState == EBHEnemyAIState::Dead)
    {
        return;
    }

    SuppressionLevel = FMath::Clamp(
        SuppressionLevel + 0.75f,
        0.0f,
        1.0f
    );
    NextCoverEvaluationTime = 0.0f;

    if (CurrentState == EBHEnemyAIState::Retreat ||
        CurrentState == EBHEnemyAIState::EvadeExplosive)
    {
        if (IsValid(DamageCauser))
        {
            LastKnownTargetLocation =
                DamageCauser->GetActorLocation();
        }
        return;
    }

    if (AActor* PlayerTarget = ResolveCombatTarget(DamageCauser))
    {
        LastKnownTargetLocation = PlayerTarget->GetActorLocation();
        NextCombatRepositionTime = 0.0f;
        EnterCombat(PlayerTarget);
    }
    else if (IsValid(DamageCauser))
    {
        LastKnownTargetLocation = DamageCauser->GetActorLocation();
        EnterInvestigate(LastKnownTargetLocation);
    }
}

void ABHEnemyAIController::NotifySuppressed(
    AActor* SourceActor,
    float Amount
)
{
    if (CurrentState == EBHEnemyAIState::Dead || Amount <= 0.0f)
    {
        return;
    }

    SuppressionLevel = FMath::Clamp(
        SuppressionLevel + Amount,
        0.0f,
        1.0f
    );
    NextCoverEvaluationTime = 0.0f;

    if (CurrentState == EBHEnemyAIState::Retreat ||
        CurrentState == EBHEnemyAIState::EvadeExplosive)
    {
        if (IsValid(SourceActor))
        {
            LastKnownTargetLocation =
                SourceActor->GetActorLocation();
        }
        return;
    }

    if (AActor* PlayerTarget = ResolveCombatTarget(SourceActor))
    {
        LastKnownTargetLocation = PlayerTarget->GetActorLocation();
        EnterCombat(PlayerTarget);
    }
}

void ABHEnemyAIController::NotifyAllyAlert(AActor* TargetActor)
{
    AActor* ResolvedTarget = ResolveCombatTarget(TargetActor);

    if (CurrentState == EBHEnemyAIState::Dead ||
        CurrentState == EBHEnemyAIState::Retreat ||
        CurrentState == EBHEnemyAIState::EvadeExplosive ||
        !IsValid(ResolvedTarget))
    {
        return;
    }

    bReceivingSquadAlert = true;
    EnterCombat(ResolvedTarget);
    bReceivingSquadAlert = false;
}

bool ABHEnemyAIController::NotifyGrenadeThreat(
    const FVector& ThreatLocation,
    float DangerRadius,
    float TimeUntilDetonation
)
{
    if (CurrentState == EBHEnemyAIState::Dead)
    {
        return false;
    }

    const ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy) || Enemy->IsDead())
    {
        return false;
    }

    const float SafeDistance =
        FMath::Max(100.0f, DangerRadius + 200.0f);
    const float CurrentDistance = FVector::Dist2D(
        Enemy->GetActorLocation(),
        ThreatLocation
    );

    if (CurrentDistance >= SafeDistance)
    {
        return false;
    }

    return EnterExplosiveEvade(
        ThreatLocation,
        DangerRadius,
        TimeUntilDetonation
    );
}

void ABHEnemyAIController::HandlePawnDeath()
{
    ReleaseCover();
    SetState(EBHEnemyAIState::Dead);
    CombatTarget.Reset();
    ClearFollowTarget();
    ClearHoldPosition();
    bHasSightOfTarget = false;
    bMoveRequested = false;
    ResetCombatTactics();
    ClearFocus(EAIFocusPriority::Gameplay);
    StopMovement();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            PatrolWaitTimerHandle
        );
    }

    if (IsValid(AIPerception))
    {
        AIPerception->OnTargetPerceptionUpdated.RemoveDynamic(
            this,
            &ABHEnemyAIController::HandleTargetPerceptionUpdated
        );
        AIPerception->ForgetAll();
        AIPerception->SetActive(false);
    }

    if (BrainComponent)
    {
        BrainComponent->StopLogic(TEXT("Enemy died"));
    }

    SetActorTickEnabled(false);
}

void ABHEnemyAIController::OnMoveCompleted(
    FAIRequestID RequestID,
    const FPathFollowingResult& Result
)
{
    Super::OnMoveCompleted(RequestID, Result);
    bMoveRequested = false;

    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy) || Enemy->IsDead())
    {
        return;
    }

    if (HasHoldPosition() &&
        CurrentState == EBHEnemyAIState::Patrol)
    {
        if (!Result.IsSuccess())
        {
            HandleHoldMoveFailure(TEXT("path-following failure"));
            return;
        }

        ConsecutiveHoldMoveFailures = 0;

        if (bHasHoldFacingYaw)
        {
            Enemy->SetActorRotation(
                FRotator(0.0f, HoldFacingYaw, 0.0f)
            );
        }
    }

    if (!Result.IsSuccess())
    {
        HandleNavigationMoveFailure();
        return;
    }

    if (CurrentState == EBHEnemyAIState::Patrol)
    {
        if (!HasFollowTarget() && !HasHoldPosition())
        {
            SchedulePatrolAdvance();
        }
    }
    else if (CurrentState == EBHEnemyAIState::AlertInvestigate)
    {
        StateEndTime = GetWorld()->GetTimeSeconds() +
            Enemy->GetInvestigateDuration();
    }
    else if (CurrentState == EBHEnemyAIState::ReturnToPatrol)
    {
        SetState(EBHEnemyAIState::Patrol);
        SchedulePatrolAdvance();
    }
}

ABHEnemySoldier* ABHEnemyAIController::GetEnemySoldier() const
{
    return Cast<ABHEnemySoldier>(GetPawn());
}

AActor* ABHEnemyAIController::ResolveCombatTarget(
    AActor* Candidate
) const
{
    AActor* Current = Candidate;

    for (int32 Depth = 0; IsValid(Current) && Depth < 4; ++Depth)
    {
        if (BHPlayerResolver::Find(this) == Current)
        {
            APawn* CombatPawn =
                BHPlayerResolver::FindCombatPawn(this);

            if (IsValidCombatTarget(CombatPawn))
            {
                return CombatPawn;
            }
        }

        if (IsValidCombatTarget(Current))
        {
            return Current;
        }

        if (Current == GetPawn() ||
            Current->IsA<ABHEnemySoldier>())
        {
            return nullptr;
        }

        AActor* Next = Current->GetOwner();

        if (!IsValid(Next))
        {
            if (APawn* InstigatorPawn = Current->GetInstigator())
            {
                Next = InstigatorPawn;
            }
        }

        if (Next == Current)
        {
            break;
        }

        Current = Next;
    }

    return nullptr;
}

bool ABHEnemyAIController::IsValidCombatTarget(
    AActor* Candidate
) const
{
    const ABHEnemySoldier* ThisSoldier = GetEnemySoldier();

    if (!IsValid(Candidate) ||
        Candidate == GetPawn() ||
        !IsValid(ThisSoldier))
    {
        return false;
    }

    if (const ABHSupplyConvoyTarget* ConvoyTarget =
        Cast<ABHSupplyConvoyTarget>(Candidate))
    {
        return !ConvoyTarget->IsResolved() &&
            ThisSoldier->IsHostileTo(ConvoyTarget);
    }

    const APawn* CandidatePawn = Cast<APawn>(Candidate);

    if (!IsValid(CandidatePawn))
    {
        return false;
    }

    if (const ABHEnemySoldier* OtherSoldier =
        Cast<ABHEnemySoldier>(CandidatePawn))
    {
        return ThisSoldier->IsHostileTo(OtherSoldier);
    }

    if (!CandidatePawn->IsPlayerControlled() ||
        !ThisSoldier->IsHostileTo(CandidatePawn))
    {
        return false;
    }

    const ABHCharacter* PlayerCharacter =
        Cast<ABHCharacter>(CandidatePawn);
    return !IsValid(PlayerCharacter) ||
        !IsValid(PlayerCharacter->GetHealthComponent()) ||
        !PlayerCharacter->GetHealthComponent()->IsDead();
}

AActor* ABHEnemyAIController::FindVisibleCombatTarget()
{
    const ABHEnemySoldier* ThisSoldier = GetEnemySoldier();

    if (!IsValid(ThisSoldier))
    {
        return nullptr;
    }

    AActor* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(
        ThisSoldier->GetSightRadius()
    );

    const auto ConsiderTarget = [
        this,
        ThisSoldier,
        &BestTarget,
        &BestDistanceSquared
    ](AActor* Candidate)
    {
        if (!IsValidCombatTarget(Candidate) ||
            !CanAcquireVisualTarget(Candidate) ||
            !HasUnobscuredLineOfSight(Candidate))
        {
            return;
        }

        const float DistanceSquared = FVector::DistSquared(
            ThisSoldier->GetActorLocation(),
            Candidate->GetActorLocation()
        );

        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    };

    ConsiderTarget(UGameplayStatics::GetPlayerPawn(this, 0));

    for (TActorIterator<ABHEnemySoldier> It(GetWorld()); It; ++It)
    {
        ConsiderTarget(*It);
    }

    for (TActorIterator<ABHSupplyConvoyTarget> It(GetWorld());
        It;
        ++It)
    {
        ConsiderTarget(*It);
    }

    return BestTarget;
}

bool ABHEnemyAIController::HasUnobscuredLineOfSight(AActor* Target) const
{
    const ABHEnemySoldier* Soldier = GetEnemySoldier();
    if (!IsValid(Soldier) || !IsValid(Target) || !LineOfSightTo(Target))
    {
        return false;
    }

    const FVector SightOrigin =
        Soldier->GetPawnViewLocation();
    const FVector TargetLocation =
        Target->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
    return !ABHTacticalSupportZone::IsLineObscuredBySmoke(
        GetWorld(),
        SightOrigin,
        TargetLocation
    );
}

bool ABHEnemyAIController::CanAcquireVisualTarget(
    AActor* Candidate
) const
{
    if (CurrentState == EBHEnemyAIState::Combat &&
        CombatTarget.Get() == Candidate)
    {
        return true;
    }

    const ABHCharacter* PlayerCharacter =
        Cast<ABHCharacter>(Candidate);
    const ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(PlayerCharacter) || !IsValid(Enemy))
    {
        return true;
    }

    const float EffectiveSightRadius =
        Enemy->GetSightRadius() *
        PlayerCharacter->GetAISightRangeMultiplier() *
        UBHBattlefieldConditions::GetCurrentProfile(this).
            SightRangeMultiplier;

    return FVector::DistSquared(
        Enemy->GetActorLocation(),
        PlayerCharacter->GetActorLocation()
    ) <= FMath::Square(FMath::Max(0.0f, EffectiveSightRadius));
}

void ABHEnemyAIController::SetState(EBHEnemyAIState NewState)
{
    if (CurrentState == NewState)
    {
        return;
    }

    const EBHEnemyAIState OldState = CurrentState;
    CurrentState = NewState;

    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (IsValid(Enemy) &&
        IsValid(Enemy->GetCharacterMovement()))
    {
        Enemy->GetCharacterMovement()->MaxWalkSpeed =
            (NewState == EBHEnemyAIState::Retreat ||
             NewState == EBHEnemyAIState::EvadeExplosive)
            ? Enemy->GetRetreatMovementSpeed()
            : Enemy->GetNormalMovementSpeed();
    }

    if (IsValid(Enemy) && Enemy->IsDebugEnabled())
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT("%s AI state %d -> %d"),
            *Enemy->GetName(),
            static_cast<uint8>(OldState),
            static_cast<uint8>(NewState)
        );
    }

    if (IsValid(Enemy) && HasAuthority())
    {
        switch (NewState)
        {
            case EBHEnemyAIState::AlertInvestigate:
                Enemy->PlayBark(EBHEnemyBarkType::Alert);
                break;
            case EBHEnemyAIState::Combat:
                Enemy->PlayBark(EBHEnemyBarkType::Contact);
                break;
            case EBHEnemyAIState::Retreat:
                Enemy->PlayBark(EBHEnemyBarkType::Retreat);
                break;
            case EBHEnemyAIState::Search:
                Enemy->PlayBark(EBHEnemyBarkType::Search);
                break;
            case EBHEnemyAIState::EvadeExplosive:
                Enemy->PlayBark(EBHEnemyBarkType::Grenade);
                break;
            default:
                break;
        }
    }
}

void ABHEnemyAIController::EnterCombat(AActor* NewTarget)
{
    const UWorld* CurrentWorld = GetWorld();

    if ((CurrentState == EBHEnemyAIState::Retreat ||
         CurrentState == EBHEnemyAIState::EvadeExplosive) &&
        IsValid(CurrentWorld) &&
        CurrentWorld->GetTimeSeconds() < StateEndTime)
    {
        return;
    }

    if (!IsValidCombatTarget(NewTarget))
    {
        return;
    }

    const bool bStartingNewCombat =
        CurrentState != EBHEnemyAIState::Combat ||
        CombatTarget.Get() != NewTarget;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            PatrolWaitTimerHandle
        );

        if (bStartingNewCombat)
        {
            StopMovement();
            bMoveRequested = false;
            RemainingBurstShots = 0;
            NextAllowedBurstTime = World->GetTimeSeconds();
            NextCombatRepositionTime = World->GetTimeSeconds();
            CombatStrafeDirection =
                FMath::RandBool() ? 1.0f : -1.0f;
        }
    }

    CombatTarget = NewTarget;
    const bool bLineOfSight = HasUnobscuredLineOfSight(NewTarget);
    const bool bReceivingNewSquadAlert = bReceivingSquadAlert;

    if (bStartingNewCombat || bReceivingNewSquadAlert)
    {
        bHasSightOfTarget = bLineOfSight;
    }
    else
    {
        bHasSightOfTarget = bHasSightOfTarget || bLineOfSight;
    }

    if (bStartingNewCombat || bLineOfSight || bReceivingNewSquadAlert)
    {
        LastKnownTargetLocation =
            NewTarget->GetActorLocation();

        if (bLineOfSight)
        {
            LastConfirmedTargetTime =
                GetWorld()->GetTimeSeconds();
        }
    }

    SetState(EBHEnemyAIState::Combat);
    SetFocus(NewTarget, EAIFocusPriority::Gameplay);

    if (bStartingNewCombat && !bReceivingSquadAlert)
    {
        const ABHEnemySoldier* ThisEnemy = GetEnemySoldier();

        if (IsValid(ThisEnemy))
        {
            const float AlertRadius =
                ThisEnemy->GetSquadAlertRadius();

            for (TActorIterator<ABHEnemySoldier> It(GetWorld());
                It;
                ++It)
            {
                ABHEnemySoldier* Ally = *It;

                if (!IsValid(Ally) ||
                    Ally == ThisEnemy ||
                    Ally->IsDead() ||
                    Ally->GetCombatFaction() !=
                        ThisEnemy->GetCombatFaction() ||
                    FVector::DistSquared2D(
                        Ally->GetActorLocation(),
                        ThisEnemy->GetActorLocation()
                    ) > FMath::Square(AlertRadius))
                {
                    continue;
                }

                if (ABHEnemyAIController* AllyController =
                    Cast<ABHEnemyAIController>(
                        Ally->GetController()
                    ))
                {
                    AllyController->NotifyAllyAlert(NewTarget);
                }
            }
        }
    }

    if (!bLoggedFactionEngagement &&
        bStartingNewCombat &&
        (
            NewTarget->IsA<ABHEnemySoldier>() ||
            NewTarget->IsA<ABHSupplyConvoyTarget>()
        ))
    {
        bLoggedFactionEngagement = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_AI_FACTION_ENGAGEMENT attacker=%s "
                "attacker_faction=%d target=%s"
            ),
            *GetNameSafe(GetPawn()),
            static_cast<int32>(
                GetEnemySoldier()->GetCombatFaction()
            ),
            *GetNameSafe(NewTarget)
        );
    }

    UpdateCombat(0.0f);
}

bool ABHEnemyAIController::EnterExplosiveEvade(
    const FVector& ThreatLocation,
    float DangerRadius,
    float TimeUntilDetonation
)
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();
    UWorld* World = GetWorld();

    if (!IsValid(Enemy) || !IsValid(World) || Enemy->IsDead())
    {
        return false;
    }

    FVector AwayDirection = (
        Enemy->GetActorLocation() - ThreatLocation
    ).GetSafeNormal2D();

    if (AwayDirection.IsNearlyZero())
    {
        AwayDirection =
            -Enemy->GetActorForwardVector().GetSafeNormal2D();
    }

    const float CurrentDistance = FVector::Dist2D(
        Enemy->GetActorLocation(),
        ThreatLocation
    );
    const float RequiredTravelDistance = FMath::Max(
        500.0f,
        (FMath::Max(0.0f, DangerRadius) + 250.0f) -
            CurrentDistance
    );
    FVector EvadeLocation =
        Enemy->GetActorLocation() +
        AwayDirection * RequiredTravelDistance;

    if (UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            World
        ))
    {
        FNavLocation ProjectedLocation;

        if (NavigationSystem->ProjectPointToNavigation(
                EvadeLocation,
                ProjectedLocation,
                FVector(700.0f, 700.0f, 500.0f)
            ))
        {
            EvadeLocation = ProjectedLocation.Location;
        }
    }

    LastExplosiveThreatLocation = ThreatLocation;
    ResetCombatTactics();
    ClearFocus(EAIFocusPriority::Gameplay);
    StopMovement();
    SetState(EBHEnemyAIState::EvadeExplosive);
    StateEndTime =
        World->GetTimeSeconds() +
        FMath::Clamp(
            TimeUntilDetonation + 0.35f,
            0.75f,
            4.0f
        );

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            EvadeLocation,
            Enemy->GetPatrolAcceptanceRadius()
        );
    bMoveRequested =
        MoveResult ==
        EPathFollowingRequestResult::RequestSuccessful;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AI_GRENADE_EVADE soldier=%s danger=%.0f "
            "detonation=%.2f destination=%s"
        ),
        *Enemy->GetName(),
        DangerRadius,
        TimeUntilDetonation,
        *EvadeLocation.ToCompactString()
    );
    return true;
}

void ABHEnemyAIController::ResumeAfterExplosiveEvade()
{
    if (IsValidCombatTarget(CombatTarget.Get()))
    {
        EnterCombat(CombatTarget.Get());
        return;
    }

    EnterSearch(LastExplosiveThreatLocation);
}

bool ABHEnemyAIController::ShouldRetreat(
    const ABHEnemySoldier* Enemy
) const
{
    if (!IsValid(Enemy))
    {
        return false;
    }

    if (Enemy->IsOutOfAmmunition())
    {
        return true;
    }

    if (
        SuppressionLevel <
            Enemy->GetRetreatSuppressionThreshold())
    {
        return false;
    }

    const UBHHealthComponent* HealthComponent =
        Enemy->GetHealthComponent();

    const bool bHealthCritical =
        IsValid(HealthComponent) &&
        !HealthComponent->IsDead() &&
        HealthComponent->GetHealthPercentage() <=
            Enemy->GetRetreatHealthThreshold();
    const bool bReadinessCritical =
        Enemy->GetCombatReadiness() <=
            Enemy->GetRetreatReadinessThreshold();
    const bool bSustainedPressure =
        SuppressionLevel >=
            (Enemy->GetRetreatSuppressionThreshold() * 0.5f);

    return bHealthCritical ||
        (bReadinessCritical && bSustainedPressure);
}

void ABHEnemyAIController::EnterRetreat(
    AActor* ThreatActor
)
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();
    UWorld* World = GetWorld();

    if (!IsValid(Enemy) || !IsValid(World))
    {
        return;
    }

    const bool bOutOfAmmunition =
        Enemy->IsOutOfAmmunition();

    if (bOutOfAmmunition &&
        !bWithdrawingForAmmunition)
    {
        bWithdrawingForAmmunition = true;
        AmmunitionWithdrawalEndTime =
            World->GetTimeSeconds() +
            (Enemy->GetRetreatDuration() * 2.0f);
    }

    const FVector ThreatLocation = IsValid(ThreatActor)
        ? ThreatActor->GetActorLocation()
        : LastKnownTargetLocation;
    FVector AwayDirection = (
        Enemy->GetActorLocation() - ThreatLocation
    ).GetSafeNormal2D();

    if (AwayDirection.IsNearlyZero())
    {
        AwayDirection =
            -Enemy->GetActorForwardVector().GetSafeNormal2D();
    }

    const FVector LateralDirection = FVector::CrossProduct(
        FVector::UpVector,
        AwayDirection
    ).GetSafeNormal();
    const float RetreatDistance =
        Enemy->GetRetreatDistance();
    FVector RetreatLocation =
        Enemy->GetActorLocation() +
        (AwayDirection * RetreatDistance) +
        (
            LateralDirection *
            FMath::FRandRange(-0.3f, 0.3f) *
            RetreatDistance
        );

    if (UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            World
        ))
    {
        FNavLocation ProjectedLocation;

        if (NavigationSystem->ProjectPointToNavigation(
                RetreatLocation,
                ProjectedLocation,
                FVector(600.0f, 600.0f, 500.0f)
            ))
        {
            RetreatLocation = ProjectedLocation.Location;
        }
    }

    LastKnownTargetLocation = ThreatLocation;
    CombatTarget.Reset();
    bHasSightOfTarget = false;
    ResetCombatTactics();
    ClearFocus(EAIFocusPriority::Gameplay);
    StopMovement();
    SetState(EBHEnemyAIState::Retreat);
    StateEndTime =
        World->GetTimeSeconds() +
        Enemy->GetRetreatDuration();

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            RetreatLocation,
            Enemy->GetPatrolAcceptanceRadius()
        );
    bMoveRequested =
        MoveResult ==
        EPathFollowingRequestResult::RequestSuccessful;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AI_RETREAT soldier=%s reason=%s health=%.2f "
            "suppression=%.2f ammo=%d/%d destination=%s"
        ),
        *Enemy->GetName(),
        bWithdrawingForAmmunition
            ? TEXT("ammunition")
            : TEXT("morale"),
        Enemy->GetHealthComponent()
            ? Enemy->GetHealthComponent()
                ->GetHealthPercentage()
            : 0.0f,
        SuppressionLevel,
        Enemy->GetCurrentMagazineAmmo(),
        Enemy->GetCurrentReserveAmmo(),
        *RetreatLocation.ToCompactString()
    );
}

void ABHEnemyAIController::CompleteAmmunitionWithdrawal()
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy))
    {
        Destroy();
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AI_AMMO_WITHDRAWAL_COMPLETE soldier=%s "
            "faction=%d"
        ),
        *Enemy->GetName(),
        static_cast<int32>(Enemy->GetCombatFaction())
    );

    HandlePawnDeath();
    UnPossess();
    Enemy->Destroy();
    Destroy();
}

void ABHEnemyAIController::EnterInvestigate(
    const FVector& StimulusLocation
)
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy))
    {
        return;
    }

    CombatTarget.Reset();
    bHasSightOfTarget = false;
    ResetCombatTactics();
    LastKnownTargetLocation = StimulusLocation;
    ClearFocus(EAIFocusPriority::Gameplay);
    StopMovement();
    SetState(EBHEnemyAIState::AlertInvestigate);
    StateEndTime = TNumericLimits<float>::Max();
    bMoveRequested = true;
    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(StimulusLocation, 75.0f);

    if (MoveResult !=
        EPathFollowingRequestResult::RequestSuccessful)
    {
        bMoveRequested = false;
        StateEndTime = GetWorld()->GetTimeSeconds() +
            Enemy->GetInvestigateDuration();
    }
}

void ABHEnemyAIController::EnterSearch(
    const FVector& SearchLocation
)
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy))
    {
        return;
    }

    CombatTarget.Reset();
    bHasSightOfTarget = false;
    ResetCombatTactics();
    LastKnownTargetLocation = SearchLocation;
    ClearFocus(EAIFocusPriority::Gameplay);
    StopMovement();
    SetState(EBHEnemyAIState::Search);
    StateEndTime = GetWorld()->GetTimeSeconds() +
        Enemy->GetSearchDuration();
    bMoveRequested = true;
    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
        SearchLocation,
        Enemy->GetPatrolAcceptanceRadius()
    );
    bMoveRequested =
        MoveResult ==
        EPathFollowingRequestResult::RequestSuccessful;
}

void ABHEnemyAIController::EnterReturnToPatrol()
{
    CombatTarget.Reset();
    bHasSightOfTarget = false;
    ResetCombatTactics();
    ClearFocus(EAIFocusPriority::Gameplay);
    StopMovement();
    if (HasFollowTarget())
    {
        SetState(EBHEnemyAIState::Patrol);
        UpdateFollowMovement();
    }
    else if (HasHoldPosition())
    {
        SetState(EBHEnemyAIState::Patrol);
        UpdateHoldMovement();
    }
    else
    {
        SetState(EBHEnemyAIState::ReturnToPatrol);
        MoveToNearestPatrolPoint();
    }
}

void ABHEnemyAIController::BeginPatrolMove()
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    if (IsNavigationBuilding(World))
    {
        bMoveRequested = false;
        NextPatrolRetryTime =
            World->GetTimeSeconds() + NavigationBuildRetrySeconds;
        return;
    }

    if (!IsValid(Enemy))
    {
        return;
    }

    if (HasFollowTarget())
    {
        UpdateFollowMovement();
        return;
    }

    if (HasHoldPosition())
    {
        UpdateHoldMovement();
        return;
    }

    const TArray<TObjectPtr<ABHPatrolPoint>>& Points =
        Enemy->GetPatrolPoints();

    if (Points.IsEmpty())
    {
        bMoveRequested = false;
        NextPatrolRetryTime = GetWorld()->GetTimeSeconds() +
            Enemy->GetPatrolRetryInterval();

        if (Enemy->IsDebugEnabled())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("%s has no assigned patrol points."),
                *Enemy->GetName()
            );
        }
        return;
    }

    CurrentPatrolIndex = FMath::Clamp(
        CurrentPatrolIndex,
        0,
        Points.Num() - 1
    );
    ABHPatrolPoint* PatrolPoint = Points[CurrentPatrolIndex];

    if (!IsValid(PatrolPoint))
    {
        AdvancePatrol();
        return;
    }

    bMoveRequested = true;
    const EPathFollowingRequestResult::Type MoveResult =
        MoveToActor(
        PatrolPoint,
        Enemy->GetPatrolAcceptanceRadius()
    );

    if (MoveResult !=
        EPathFollowingRequestResult::RequestSuccessful)
    {
        bMoveRequested = false;
        NextPatrolRetryTime = GetWorld()->GetTimeSeconds() +
            Enemy->GetPatrolRetryInterval();
        SchedulePatrolAdvance();
    }
}

void ABHEnemyAIController::AdvancePatrol()
{
    if (CurrentState != EBHEnemyAIState::Patrol)
    {
        return;
    }

    const ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy) || Enemy->GetPatrolPoints().IsEmpty())
    {
        return;
    }

    CurrentPatrolIndex =
        (CurrentPatrolIndex + 1) %
        Enemy->GetPatrolPoints().Num();
    BeginPatrolMove();
}

void ABHEnemyAIController::SchedulePatrolAdvance()
{
    const ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy) ||
        (HasFollowTarget() || HasHoldPosition()) ||
        CurrentState != EBHEnemyAIState::Patrol)
    {
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(
        PatrolWaitTimerHandle,
        this,
        &ABHEnemyAIController::AdvancePatrol,
        FMath::Max(0.01f, Enemy->GetPatrolWaitDuration()),
        false
    );
}

void ABHEnemyAIController::MoveToNearestPatrolPoint()
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    if (IsNavigationBuilding(World))
    {
        bMoveRequested = false;
        NextPatrolRetryTime =
            World->GetTimeSeconds() + NavigationBuildRetrySeconds;
        return;
    }

    if (HasFollowTarget())
    {
        SetState(EBHEnemyAIState::Patrol);
        UpdateFollowMovement();
        return;
    }

    if (HasHoldPosition())
    {
        SetState(EBHEnemyAIState::Patrol);
        UpdateHoldMovement();
        return;
    }

    if (!IsValid(Enemy) || Enemy->GetPatrolPoints().IsEmpty())
    {
        SetState(EBHEnemyAIState::Patrol);
        bMoveRequested = false;
        NextPatrolRetryTime = GetWorld()->GetTimeSeconds() +
            (IsValid(Enemy)
                ? Enemy->GetPatrolRetryInterval()
                : 1.0f);
        return;
    }

    float BestDistanceSquared = TNumericLimits<float>::Max();
    int32 BestIndex = INDEX_NONE;

    for (int32 Index = 0;
        Index < Enemy->GetPatrolPoints().Num();
        ++Index)
    {
        const ABHPatrolPoint* Point =
            Enemy->GetPatrolPoints()[Index];

        if (!IsValid(Point))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            Enemy->GetActorLocation(),
            Point->GetActorLocation()
        );

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestIndex = Index;
        }
    }

    if (BestIndex == INDEX_NONE)
    {
        SetState(EBHEnemyAIState::Patrol);
        bMoveRequested = false;
        NextPatrolRetryTime = GetWorld()->GetTimeSeconds() +
            Enemy->GetPatrolRetryInterval();
        return;
    }

    CurrentPatrolIndex = BestIndex;
    bMoveRequested = true;
    const EPathFollowingRequestResult::Type MoveResult =
        MoveToActor(
        Enemy->GetPatrolPoints()[BestIndex],
        Enemy->GetPatrolAcceptanceRadius()
    );

    if (MoveResult !=
        EPathFollowingRequestResult::RequestSuccessful)
    {
        bMoveRequested = false;
        NextPatrolRetryTime = GetWorld()->GetTimeSeconds() +
            Enemy->GetPatrolRetryInterval();
    }
}

void ABHEnemyAIController::UpdateFollowMovement()
{
    ABHEnemySoldier* Soldier = GetEnemySoldier();
    AActor* Leader = FollowTarget.Get();
    UWorld* World = GetWorld();

    if (!IsValid(Soldier) ||
        !IsValid(Leader) ||
        !IsValid(World) ||
        CurrentState != EBHEnemyAIState::Patrol)
    {
        return;
    }

    constexpr float FollowRepathInterval = 0.35f;
    constexpr float FollowRepositionDistance = 300.0f;
    constexpr float FollowAcceptanceRadius = 140.0f;
    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextFollowMoveTime)
    {
        return;
    }

    NextFollowMoveTime =
        CurrentTime + FollowRepathInterval;
    const FVector LeaderForward =
        Leader->GetActorForwardVector().GetSafeNormal2D();
    const FVector LeaderRight =
        Leader->GetActorRightVector().GetSafeNormal2D();
    const FVector DesiredLocation =
        Leader->GetActorLocation() +
        (LeaderForward * FollowLocalOffset.X) +
        (LeaderRight * FollowLocalOffset.Y);
    const float DistanceToFormation =
        FVector::Dist2D(
            Soldier->GetActorLocation(),
            DesiredLocation
        );

    if (DistanceToFormation <= FollowRepositionDistance)
    {
        if (bMoveRequested)
        {
            StopMovement();
            bMoveRequested = false;
        }

        return;
    }

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            DesiredLocation,
            FollowAcceptanceRadius,
            true,
            true,
            true,
            false
        );
    bMoveRequested =
        MoveResult ==
            EPathFollowingRequestResult::RequestSuccessful;
}

void ABHEnemyAIController::UpdateHoldMovement()
{
    ABHEnemySoldier* Soldier = GetEnemySoldier();
    UWorld* World = GetWorld();

    if (!IsValid(Soldier) ||
        !IsValid(World) ||
        !HasHoldPosition() ||
        CurrentState != EBHEnemyAIState::Patrol)
    {
        return;
    }

    constexpr float HoldRepathInterval = 0.5f;
    constexpr float HoldRepositionDistance = 220.0f;
    constexpr float HoldAcceptanceRadius = 100.0f;
    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextHoldMoveTime)
    {
        return;
    }

    NextHoldMoveTime = CurrentTime + HoldRepathInterval;
    const float DistanceToHold =
        FVector::Dist2D(Soldier->GetActorLocation(), HoldLocation);

    if (DistanceToHold <= HoldRepositionDistance)
    {
        if (bMoveRequested)
        {
            StopMovement();
            bMoveRequested = false;
        }

        ConsecutiveHoldMoveFailures = 0;

        if (bHasHoldFacingYaw)
        {
            Soldier->SetActorRotation(
                FRotator(0.0f, HoldFacingYaw, 0.0f)
            );
        }

        return;
    }

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            HoldLocation,
            HoldAcceptanceRadius,
            true,
            true,
            true,
            false
        );
    bMoveRequested =
        MoveResult ==
            EPathFollowingRequestResult::RequestSuccessful;

    if (MoveResult == EPathFollowingRequestResult::Failed)
    {
        HandleHoldMoveFailure(TEXT("move request rejected"));
    }

}

EBHEnemyAIState ABHEnemyAIController::ResolveNavigationFailureFallback(
    EBHEnemyAIState FailedState
)
{
    switch (FailedState)
    {
        case EBHEnemyAIState::AlertInvestigate:
        case EBHEnemyAIState::EvadeExplosive:
            return EBHEnemyAIState::Search;

        case EBHEnemyAIState::ReturnToPatrol:
            return EBHEnemyAIState::Patrol;

        case EBHEnemyAIState::Patrol:
        case EBHEnemyAIState::Combat:
        case EBHEnemyAIState::Retreat:
        case EBHEnemyAIState::Search:
        case EBHEnemyAIState::Dead:
        default:
            return FailedState;
    }
}

void ABHEnemyAIController::HandleNavigationMoveFailure()
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy) || Enemy->IsDead())
    {
        return;
    }

    const EBHEnemyAIState FailedState = CurrentState;
    const EBHEnemyAIState FallbackState =
        ResolveNavigationFailureFallback(FailedState);
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    StopMovement();
    bMoveRequested = false;

    const auto TryEnterLocalSearch =
        [this, Enemy, CurrentTime](FVector& OutLocation)
        {
            if (!TryFindLocalSearchLocation(
                    GetWorld(),
                    Enemy->GetActorLocation(),
                    OutLocation
                ))
            {
                return false;
            }

            CombatTarget.Reset();
            bHasSightOfTarget = false;
            ResetCombatTactics();
            LastKnownTargetLocation = OutLocation;
            ClearFocus(EAIFocusPriority::Gameplay);
            SetState(EBHEnemyAIState::Search);
            StateEndTime = CurrentTime + Enemy->GetSearchDuration();
            bMoveRequested = false;
            return true;
        };

    if (FailedState == EBHEnemyAIState::Combat)
    {
        ReleaseCover();
        FVector LocalSearchLocation;
        if (TryEnterLocalSearch(LocalSearchLocation))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_AI_NAVIGATION_LOCAL_SEARCH soldier=%s "
                    "failed_state=%d destination=%s"
                ),
                *Enemy->GetName(),
                static_cast<int32>(FailedState),
                *LocalSearchLocation.ToCompactString()
            );
        }
        else
        {
            NextCombatPursuitMoveTime = CurrentTime + 0.75f;
            NextCombatRepositionTime = CurrentTime + 0.75f;
        }
    }
    else if (FallbackState == EBHEnemyAIState::Search &&
             FailedState != EBHEnemyAIState::Search)
    {
        FVector LocalSearchLocation;
        if (TryEnterLocalSearch(LocalSearchLocation))
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_AI_NAVIGATION_LOCAL_SEARCH soldier=%s "
                    "failed_state=%d destination=%s"
                ),
                *Enemy->GetName(),
                static_cast<int32>(FailedState),
                *LocalSearchLocation.ToCompactString()
            );
        }
        else
        {
            SetState(EBHEnemyAIState::Search);
            StateEndTime = CurrentTime + Enemy->GetSearchDuration();
        }
    }
    else if (FallbackState == EBHEnemyAIState::Search)
    {
        SetState(EBHEnemyAIState::Search);
        StateEndTime = CurrentTime + Enemy->GetSearchDuration();
    }
    else if (FallbackState == EBHEnemyAIState::Patrol)
    {
        SetState(EBHEnemyAIState::Patrol);
        NextPatrolRetryTime = CurrentTime + 1.0f;
        SchedulePatrolAdvance();
    }
    else if (FallbackState == EBHEnemyAIState::Retreat)
    {
        // Preserve ammunition-withdrawal and morale-retreat deadlines while
        // holding safely at the last reachable location.
        StateEndTime = FMath::Min(
            StateEndTime,
            CurrentTime + Enemy->GetRetreatDuration()
        );
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "BH_AI_NAVIGATION_FALLBACK soldier=%s failed_state=%d "
            "fallback_state=%d location=%s"
        ),
        *Enemy->GetName(),
        static_cast<int32>(FailedState),
        static_cast<int32>(FallbackState),
        *Enemy->GetActorLocation().ToCompactString()
    );
}

float ABHEnemyAIController::CalculateCoverSelectionScore(
    float DistanceToCover,
    float DistanceToTarget,
    float CoverQuality
)
{
    const float QualityBonus = FMath::Max(0.0f, CoverQuality) * 400.0f;
    const float TargetPressure = DistanceToTarget * 0.1f;
    return DistanceToCover - TargetPressure - QualityBonus;
}

bool ABHEnemyAIController::CanAcceptSuppressiveFireOrder(
    bool bIsAlive,
    bool bIncapacitated,
    int32 RoundsAvailable,
    bool bTargetHostile
)
{
    return bIsAlive &&
        !bIncapacitated &&
        RoundsAvailable > 0 &&
        bTargetHostile;
}

float ABHEnemyAIController::CalculateSuppressiveFireSpread(
    float IncomingSpread,
    bool bSuppressiveOrder
)
{
    const float SafeSpread = FMath::Max(0.0f, IncomingSpread);
    return bSuppressiveOrder ? SafeSpread * 3.5f : SafeSpread;
}

void ABHEnemyAIController::HandleHoldMoveFailure(
    const TCHAR* FailureReason
)
{
    ABHEnemySoldier* Soldier = GetEnemySoldier();

    if (!IsValid(Soldier) || !HasHoldPosition())
    {
        return;
    }

    ++ConsecutiveHoldMoveFailures;

    if (ConsecutiveHoldMoveFailures < 3)
    {
        NextHoldMoveTime = 0.0f;
        return;
    }

    const FVector FailedDestination = HoldLocation;
    HoldLocation = Soldier->GetActorLocation();
    bMoveRequested = false;
    StopMovement();

    if (bHasHoldFacingYaw)
    {
        Soldier->SetActorRotation(
            FRotator(0.0f, HoldFacingYaw, 0.0f)
        );
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "BH_SQUAD_MOVE_HOLD_FAILED soldier=%s reason=%s "
            "destination=%s fallback=%s"
        ),
        *GetNameSafe(Soldier),
        FailureReason,
        *FailedDestination.ToCompactString(),
        *HoldLocation.ToCompactString()
    );

    OnHoldMoveFailed.Broadcast(
        Soldier,
        FailedDestination,
        HoldLocation
    );
}

void ABHEnemyAIController::UpdateCombat(float DeltaSeconds)
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();
    AActor* Target = CombatTarget.Get();

    if (!IsValid(Enemy) || !IsValidCombatTarget(Target))
    {
        EnterSearch(LastKnownTargetLocation);
        return;
    }

    const float Distance = FVector::Dist(
        Enemy->GetActorLocation(),
        Target->GetActorLocation()
    );
    const bool bLineOfSight = HasUnobscuredLineOfSight(Target);
    bHasSightOfTarget = bLineOfSight;

    if (bLineOfSight)
    {
        LastKnownTargetLocation =
            Target->GetActorLocation();
        LastConfirmedTargetTime =
            GetWorld()->GetTimeSeconds();

        if (bMoveRequested)
        {
            StopMovement();
            bMoveRequested = false;
        }
    }

    const float TimeWithoutSight =
        GetWorld()->GetTimeSeconds() -
        LastConfirmedTargetTime;
    const FVector GrenadeTargetLocation = bLineOfSight
        ? Target->GetActorLocation()
        : LastKnownTargetLocation;

    if (TimeWithoutSight <= Enemy->GetSightMemoryDuration() &&
        TryThrowGrenade(Enemy, GrenadeTargetLocation))
    {
        RemainingBurstShots = 0;
        NextAllowedBurstTime =
            GetWorld()->GetTimeSeconds() + 0.75f;
        return;
    }

    bool bCanFireFromCover = true;
    const bool bCoverControlsMovement = UpdateCoverTactics(
        Enemy,
        Target,
        DeltaSeconds,
        bCanFireFromCover
    );

    if (!bLineOfSight && !bCoverControlsMovement)
    {
        if (TimeWithoutSight <=
            Enemy->GetSightMemoryDuration())
        {
            PursueLastKnownTarget(Enemy, DeltaSeconds);
        }
        else
        {
            EnterSearch(LastKnownTargetLocation);
        }

        return;
    }

    if (bLineOfSight)
    {
        SetFocus(Target, EAIFocusPriority::Gameplay);
        FaceLocation(Target->GetActorLocation(), DeltaSeconds);
    }
    else
    {
        ClearFocus(EAIFocusPriority::Gameplay);
        SetFocalPoint(
            LastKnownTargetLocation,
            EAIFocusPriority::Gameplay
        );
        FaceLocation(LastKnownTargetLocation, DeltaSeconds);
    }

    if (!bCoverControlsMovement)
    {
        UpdateCombatMovement(Enemy, Target, Distance);
    }

    if (bLineOfSight && bCanFireFromCover)
    {
        UpdateCombatFire(Enemy, Target, Distance);
    }
    else
    {
        RemainingBurstShots = 0;
    }
}

void ABHEnemyAIController::PursueLastKnownTarget(
    ABHEnemySoldier* Enemy,
    float DeltaSeconds
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || !IsValid(Enemy))
    {
        return;
    }

    RemainingBurstShots = 0;
    ClearFocus(EAIFocusPriority::Gameplay);
    SetFocalPoint(
        LastKnownTargetLocation,
        EAIFocusPriority::Gameplay
    );
    FaceLocation(LastKnownTargetLocation, DeltaSeconds);

    const float AcceptanceRadius = FMath::Max(
        75.0f,
        Enemy->GetPatrolAcceptanceRadius()
    );

    if (FVector::DistSquared2D(
            Enemy->GetActorLocation(),
            LastKnownTargetLocation
        ) <= FMath::Square(AcceptanceRadius))
    {
        if (bMoveRequested)
        {
            StopMovement();
            bMoveRequested = false;
        }

        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (bMoveRequested &&
        CurrentTime < NextCombatPursuitMoveTime)
    {
        return;
    }

    if (bMoveRequested)
    {
        StopMovement();
        bMoveRequested = false;
    }

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            LastKnownTargetLocation,
            AcceptanceRadius
        );
    bMoveRequested =
        MoveResult ==
        EPathFollowingRequestResult::RequestSuccessful;
    NextCombatPursuitMoveTime = CurrentTime + 0.75f;
}

void ABHEnemyAIController::UpdateCombatMovement(
    ABHEnemySoldier* Enemy,
    AActor* Target,
    float Distance
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(Enemy) ||
        !IsValid(Target))
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();
    const float MinimumDistance =
        Enemy->GetMinimumEngagementDistance();
    const float DesiredDistance =
        Enemy->GetDesiredEngagementDistance();
    const float RepositionRadius =
        Enemy->GetCombatRepositionRadius();
    const FVector ToTarget = (
        Target->GetActorLocation() -
        Enemy->GetActorLocation()
    ).GetSafeNormal2D();
    const FVector LateralDirection = FVector::CrossProduct(
        FVector::UpVector,
        ToTarget
    ).GetSafeNormal();
    FVector CombatMoveDirection = FVector::ZeroVector;

    if (Distance > DesiredDistance * 1.15f)
    {
        CombatMoveDirection = ToTarget;
    }
    else if (Distance < MinimumDistance)
    {
        if (CurrentTime >= NextCombatRepositionTime)
        {
            CombatStrafeDirection *= -1.0f;
            NextCombatRepositionTime =
                CurrentTime +
                Enemy->GetCombatRepositionInterval();
        }

        CombatMoveDirection = (
            -ToTarget +
            (LateralDirection *
                CombatStrafeDirection *
                0.35f)
        ).GetSafeNormal();
    }
    else
    {
        if (RepositionRadius > 0.0f &&
            CurrentTime >= NextCombatRepositionTime)
        {
            CombatStrafeDirection *= -1.0f;
            CombatStrafeEndTime =
                CurrentTime +
                FMath::Min(
                    RepositionRadius / 300.0f,
                    Enemy->GetCombatRepositionInterval() * 0.5f
                );
            NextCombatRepositionTime =
                CurrentTime +
                Enemy->GetCombatRepositionInterval();
        }

        if (CurrentTime < CombatStrafeEndTime)
        {
            CombatMoveDirection =
                LateralDirection * CombatStrafeDirection;
        }
    }

    if (CombatMoveDirection.IsNearlyZero())
    {
        return;
    }

    if (bMoveRequested)
    {
        StopMovement();
        bMoveRequested = false;
    }

    Enemy->AddMovementInput(CombatMoveDirection, 1.0f);
}

void ABHEnemyAIController::UpdateCombatFire(
    ABHEnemySoldier* Enemy,
    AActor* Target,
    float Distance
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(Enemy) ||
        !IsValid(Target) ||
        Distance > Enemy->GetMaximumEngagementDistance())
    {
        RemainingBurstShots = 0;
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (RemainingBurstShots <= 0)
    {
        if (CurrentTime < NextAllowedBurstTime)
        {
            return;
        }

        RemainingBurstShots = FMath::RandRange(
            Enemy->GetMinimumBurstShots(),
            Enemy->GetMaximumBurstShots()
        );
    }

    const float SuppressionSpread =
        SuppressionLevel *
        Enemy->GetSuppressionSpreadPenalty();

    if (!Enemy->FireAt(Target, SuppressionSpread))
    {
        return;
    }

    --RemainingBurstShots;

    if (RemainingBurstShots <= 0)
    {
        NextAllowedBurstTime =
            CurrentTime +
            FMath::FRandRange(
                Enemy->GetMinimumBurstRecovery(),
                Enemy->GetMaximumBurstRecovery()
            );
    }
}

bool ABHEnemyAIController::TryThrowGrenade(
    ABHEnemySoldier* Enemy,
    const FVector& TargetLocation
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(Enemy) ||
        Enemy->IsDead() ||
        Enemy->IsReloading() ||
        Enemy->GetCurrentFragGrenades() <= 0 ||
        SuppressionLevel >= 0.6f ||
        TargetLocation.IsNearlyZero())
    {
        return false;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextGrenadeDecisionTime)
    {
        return false;
    }

    const float Distance = FVector::Dist2D(
        Enemy->GetActorLocation(),
        TargetLocation
    );

    if (Distance < Enemy->GetMinimumGrenadeRange() ||
        Distance > Enemy->GetMaximumGrenadeRange())
    {
        return false;
    }

    NextGrenadeDecisionTime =
        CurrentTime + Enemy->GetGrenadeDecisionInterval();

    if (FMath::FRand() > Enemy->GetGrenadeUseChance() ||
        !IsGrenadeTargetSafe(Enemy, TargetLocation))
    {
        return false;
    }

    if (!Enemy->ThrowFragGrenadeAt(TargetLocation))
    {
        NextGrenadeDecisionTime = CurrentTime + 1.0f;
        return false;
    }

    StopMovement();
    bMoveRequested = false;
    return true;
}

bool ABHEnemyAIController::IsGrenadeTargetSafe(
    const ABHEnemySoldier* Enemy,
    const FVector& TargetLocation
) const
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || !IsValid(Enemy))
    {
        return false;
    }

    const float SafetyRadius =
        Enemy->GetGrenadeFriendlySafetyRadius();
    const float SafetyRadiusSquared =
        FMath::Square(SafetyRadius);

    for (TActorIterator<ABHEnemySoldier> It(World);
        It;
        ++It)
    {
        const ABHEnemySoldier* Ally = *It;

        if (!IsValid(Ally) ||
            Ally == Enemy ||
            Ally->IsDead() ||
            Ally->GetCombatFaction() !=
                Enemy->GetCombatFaction())
        {
            continue;
        }

        if (FVector::DistSquared2D(
                Ally->GetActorLocation(),
                TargetLocation
            ) <= SafetyRadiusSquared)
        {
            UE_LOG(
                LogTemp,
                Verbose,
                TEXT(
                    "BH_AI_GRENADE_WITHHELD soldier=%s ally=%s"
                ),
                *Enemy->GetName(),
                *Ally->GetName()
            );
            return false;
        }
    }

    return true;
}

bool ABHEnemyAIController::UpdateCoverTactics(
    ABHEnemySoldier* Enemy,
    AActor* Target,
    float DeltaSeconds,
    bool& bOutCanFire
)
{
    bOutCanFire = true;
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(Enemy) ||
        !IsValid(Target))
    {
        return false;
    }

    const float CurrentTime = World->GetTimeSeconds();
    const bool bReloading = Enemy->IsReloading();

    if (!HasActiveCover())
    {
        if ((!bReloading &&
                SuppressionLevel <
                    Enemy->GetSuppressionCoverThreshold()) ||
            CurrentTime < NextCoverEvaluationTime ||
            !TryClaimCover(Enemy, Target))
        {
            return false;
        }
    }

    ABHCoverPoint* CoverPoint = ClaimedCoverPoint.Get();

    if (!IsValid(CoverPoint))
    {
        ReleaseCover();
        return false;
    }

    if (CurrentTime >= NextCoverEvaluationTime)
    {
        NextCoverEvaluationTime =
            CurrentTime +
            Enemy->GetCoverReevaluationInterval();

        if (!IsCoverProtective(CoverPoint, Target))
        {
            ReleaseCover();

            if (!TryClaimCover(Enemy, Target))
            {
                return false;
            }

            CoverPoint = ClaimedCoverPoint.Get();
        }
    }

    if (CurrentTime >= CoverHoldEndTime &&
        !bReloading &&
        SuppressionLevel <
            Enemy->GetSuppressionCoverThreshold() * 0.75f)
    {
        ReleaseCover();
        return false;
    }

    if (CurrentTime >= CoverHoldEndTime)
    {
        CoverHoldEndTime =
            CurrentTime + Enemy->GetCoverHoldDuration();
    }

    const auto MoveDirectlyTo = [
        this,
        Enemy
    ](const FVector& Destination)
    {
        if (bMoveRequested)
        {
            StopMovement();
            bMoveRequested = false;
        }

        const FVector MoveDirection = (
            Destination - Enemy->GetActorLocation()
        ).GetSafeNormal2D();

        if (!MoveDirection.IsNearlyZero())
        {
            Enemy->AddMovementInput(MoveDirection, 1.0f);
        }
    };

    const auto HasReached = [
        Enemy
    ](const FVector& Destination, float AcceptanceRadius)
    {
        return FVector::DistSquared2D(
            Enemy->GetActorLocation(),
            Destination
        ) <= FMath::Square(AcceptanceRadius);
    };

    const float AcceptanceRadius =
        Enemy->GetCoverAcceptanceRadius();
    const FVector AnchorLocation =
        CoverPoint->GetAnchorLocation();

    if (bReloading &&
        (bMovingToPeek || bPeekingFromCover))
    {
        bMovingToPeek = false;
        bPeekingFromCover = false;
        bReturningToCover = true;
        RemainingBurstShots = 0;
    }

    if (bMovingToCoverAnchor)
    {
        bOutCanFire = false;

        if (!HasReached(AnchorLocation, AcceptanceRadius))
        {
            MoveDirectlyTo(AnchorLocation);
            return true;
        }

        bMovingToCoverAnchor = false;
        bHiddenInCover = true;
        CoverPhaseEndTime =
            CurrentTime + Enemy->GetCoverHideDuration();
    }

    if (bReturningToCover)
    {
        bOutCanFire = false;

        if (!HasReached(AnchorLocation, AcceptanceRadius))
        {
            MoveDirectlyTo(AnchorLocation);
            return true;
        }

        bReturningToCover = false;
        bHiddenInCover = true;
        CoverPhaseEndTime =
            CurrentTime + Enemy->GetCoverHideDuration();
    }

    if (bHiddenInCover)
    {
        bOutCanFire = false;

        if (bReloading ||
            CurrentTime < CoverPhaseEndTime)
        {
            return true;
        }

        const FVector RightPeek =
            CoverPoint->GetPeekLocation(true);
        const FVector LeftPeek =
            CoverPoint->GetPeekLocation(false);
        FVector TargetEyeLocation;
        FRotator TargetEyeRotation;
        Target->GetActorEyesViewPoint(
            TargetEyeLocation,
            TargetEyeRotation
        );

        const auto HasClearPeek = [
            this,
            Enemy,
            Target,
            TargetEyeLocation
        ](const FVector& PeekLocation)
        {
            FCollisionQueryParams QueryParams(
                SCENE_QUERY_STAT(BHCoverPeek),
                true,
                Enemy
            );
            QueryParams.AddIgnoredActor(Enemy);
            FHitResult HitResult;
            const bool bHit = GetWorld()->LineTraceSingleByChannel(
                HitResult,
                PeekLocation + FVector(0.0f, 0.0f, 65.0f),
                TargetEyeLocation,
                ECC_Visibility,
                QueryParams
            );
            return !bHit || HitResult.GetActor() == Target;
        };

        const bool bRightClear = HasClearPeek(RightPeek);
        const bool bLeftClear = HasClearPeek(LeftPeek);
        bPeekingRight = bRightClear || !bLeftClear;
        bHiddenInCover = false;
        bMovingToPeek = true;
    }

    const FVector PeekLocation =
        CoverPoint->GetPeekLocation(bPeekingRight);

    if (bMovingToPeek)
    {
        bOutCanFire = false;

        if (!HasReached(PeekLocation, AcceptanceRadius))
        {
            MoveDirectlyTo(PeekLocation);
            return true;
        }

        bMovingToPeek = false;
        bPeekingFromCover = true;
        CoverPhaseEndTime =
            CurrentTime + Enemy->GetCoverPeekDuration();
        RemainingBurstShots = 0;
        NextAllowedBurstTime = CurrentTime;
    }

    if (bPeekingFromCover)
    {
        bOutCanFire = true;

        if (CurrentTime >= CoverPhaseEndTime)
        {
            bPeekingFromCover = false;
            bReturningToCover = true;
            bOutCanFire = false;
            RemainingBurstShots = 0;
        }

        return true;
    }

    return true;
}

bool ABHEnemyAIController::TryClaimCover(
    ABHEnemySoldier* Enemy,
    AActor* Target
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(Enemy) ||
        !IsValid(Target))
    {
        return false;
    }

    ABHCoverPoint* BestCover = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    const float SearchRadius = Enemy->GetCoverSearchRadius();

    for (TActorIterator<ABHCoverPoint> It(World); It; ++It)
    {
        ABHCoverPoint* Candidate = *It;

        if (!IsValid(Candidate) ||
            !Candidate->IsAvailableFor(Enemy) ||
            !IsCoverProtective(Candidate, Target))
        {
            continue;
        }

        const float Distance = FVector::Dist2D(
            Enemy->GetActorLocation(),
            Candidate->GetAnchorLocation()
        );

        if (Distance > SearchRadius)
        {
            continue;
        }

        const float ThreatDistance = FVector::Dist2D(
            Target->GetActorLocation(),
            Candidate->GetAnchorLocation()
        );
        float CoverQuality = 0.0f;
        if (const ABHFieldFortification* Fortification =
            Cast<ABHFieldFortification>(Candidate))
        {
            if (Fortification->GetHealthFraction() > 0.0f &&
                Fortification->GetSectorID() != NAME_None)
            {
                CoverQuality =
                    Fortification->GetAICoverQuality();
            }
        }

        const float Score = CalculateCoverSelectionScore(
            Distance,
            ThreatDistance,
            CoverQuality
        );

        if (Score < BestScore)
        {
            BestScore = Score;
            BestCover = Candidate;
        }
    }

    if (!IsValid(BestCover) || !BestCover->TryClaim(Enemy))
    {
        NextCoverEvaluationTime =
            World->GetTimeSeconds() +
            Enemy->GetCoverReevaluationInterval();
        return false;
    }

    ReleaseCover();
    ClaimedCoverPoint = BestCover;
    bMovingToCoverAnchor = true;
    bHiddenInCover = false;
    bMovingToPeek = false;
    bPeekingFromCover = false;
    bReturningToCover = false;
    CoverHoldEndTime =
        World->GetTimeSeconds() + Enemy->GetCoverHoldDuration();
    NextCoverEvaluationTime =
        World->GetTimeSeconds() +
        Enemy->GetCoverReevaluationInterval();
    RemainingBurstShots = 0;
    return true;
}

bool ABHEnemyAIController::IsCoverProtective(
    const ABHCoverPoint* CoverPoint,
    AActor* Target
) const
{
    const ABHEnemySoldier* Enemy = GetEnemySoldier();
    UWorld* World = GetWorld();

    if (!IsValid(CoverPoint) ||
        !IsValid(Target) ||
        !IsValid(Enemy) ||
        !IsValid(World))
    {
        return false;
    }

    FVector ThreatEyeLocation;
    FRotator ThreatEyeRotation;
    Target->GetActorEyesViewPoint(
        ThreatEyeLocation,
        ThreatEyeRotation
    );

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BHCoverProtection),
        true,
        Target
    );
    QueryParams.AddIgnoredActor(Target);
    QueryParams.AddIgnoredActor(Enemy);
    // Do not ignore the cover actor itself. Marker-only cover points have no
    // blocking geometry, while constructed field fortifications do and must
    // be allowed to prove that they actually protect the claimant.

    FHitResult HitResult;
    return World->LineTraceSingleByChannel(
        HitResult,
        ThreatEyeLocation,
        CoverPoint->GetAnchorLocation() +
            FVector(0.0f, 0.0f, 65.0f),
        ECC_Visibility,
        QueryParams
    );
}

bool ABHEnemyAIController::HasActiveCover() const
{
    return ClaimedCoverPoint.IsValid();
}

void ABHEnemyAIController::ReleaseCover()
{
    if (ABHCoverPoint* CoverPoint = ClaimedCoverPoint.Get())
    {
        CoverPoint->Release(GetPawn());
    }

    ClaimedCoverPoint.Reset();
    bMovingToCoverAnchor = false;
    bHiddenInCover = false;
    bMovingToPeek = false;
    bPeekingFromCover = false;
    bReturningToCover = false;
    CoverHoldEndTime = 0.0f;
    CoverPhaseEndTime = 0.0f;
}

void ABHEnemyAIController::UpdateSuppression(
    const ABHEnemySoldier* Enemy,
    float DeltaSeconds
)
{
    if (!IsValid(Enemy) || SuppressionLevel <= 0.0f)
    {
        return;
    }

    SuppressionLevel = FMath::Max(
        0.0f,
        SuppressionLevel -
            (Enemy->GetSuppressionDecayRate() * DeltaSeconds)
    );
}

void ABHEnemyAIController::ResetCombatTactics()
{
    ReleaseCover();
    RemainingBurstShots = 0;
    NextAllowedBurstTime = 0.0f;
    NextCombatRepositionTime = 0.0f;
    NextCombatPursuitMoveTime = 0.0f;
    CombatStrafeEndTime = 0.0f;
    NextCoverEvaluationTime = 0.0f;
}

void ABHEnemyAIController::UpdateMovementFacing(float DeltaSeconds)
{
    const ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy))
    {
        return;
    }

    const FVector MovementDirection =
        Enemy->GetVelocity().GetSafeNormal2D();

    if (!MovementDirection.IsNearlyZero())
    {
        FaceLocation(
            Enemy->GetActorLocation() + MovementDirection,
            DeltaSeconds
        );
    }
}

void ABHEnemyAIController::FaceLocation(
    const FVector& TargetLocation,
    float DeltaSeconds
)
{
    ABHEnemySoldier* Enemy = GetEnemySoldier();

    if (!IsValid(Enemy))
    {
        return;
    }

    FVector LookDirection =
        TargetLocation - Enemy->GetActorLocation();
    LookDirection.Z = 0.0f;

    if (LookDirection.IsNearlyZero())
    {
        return;
    }

    const FRotator DesiredRotation(
        0.0f,
        LookDirection.Rotation().Yaw,
        0.0f
    );
    const FRotator NewRotation = DeltaSeconds <= 0.0f
        ? DesiredRotation
        : FMath::RInterpTo(
            Enemy->GetActorRotation(),
            DesiredRotation,
            DeltaSeconds,
            Enemy->GetRotationInterpSpeed()
        );

    Enemy->SetActorRotation(NewRotation);
    SetControlRotation(NewRotation);
}

void ABHEnemyAIController::DrawDebugState() const
{
    const ABHEnemySoldier* Enemy = GetEnemySoldier();
    const UWorld* World = GetWorld();

    if (!IsValid(Enemy) ||
        !Enemy->IsDebugEnabled() ||
        !IsValid(World))
    {
        return;
    }

    FColor StateColor = FColor::Cyan;

    if (CurrentState == EBHEnemyAIState::Combat)
    {
        StateColor = FColor::Red;
    }
    else if (CurrentState == EBHEnemyAIState::Retreat)
    {
        StateColor = FColor::Magenta;
    }
    else if (CurrentState == EBHEnemyAIState::EvadeExplosive)
    {
        StateColor = FColor::Orange;
    }
    else if (
        CurrentState == EBHEnemyAIState::AlertInvestigate ||
        CurrentState == EBHEnemyAIState::Search)
    {
        StateColor = FColor::Yellow;
    }
    else if (CurrentState == EBHEnemyAIState::ReturnToPatrol)
    {
        StateColor = FColor::Green;
    }

    DrawDebugSphere(
        World,
        LastKnownTargetLocation,
        35.0f,
        12,
        StateColor,
        false,
        0.0f,
        0,
        1.5f
    );
}
