#include "BHOpenWorldOperationDirector.h"

#include "BHCharacter.h"
#include "BHEnemyAIController.h"
#include "BHEnemySoldier.h"
#include "BHHealthComponent.h"
#include "BHMissionData.h"
#include "BHPatrolPoint.h"
#include "BHRaidSabotageTarget.h"
#include "BHSaveSubsystem.h"
#include "BHSectorAnchor.h"
#include "BHWarGameState.h"
#include "BHWarSubsystem.h"
#include "BHWarOperationRules.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
constexpr int32 MaximumOperationSpawnAttempts = 6;
constexpr float AlternateSpawnAngleStep = 22.5f;
constexpr float AlternateSpawnRadiusStep = 250.0f;
}

ABHOpenWorldOperationDirector::ABHOpenWorldOperationDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    SetReplicates(true);
    bAlwaysRelevant = true;
}

bool ABHOpenWorldOperationDirector::StartOperation(
    ABHCharacter* InPlayerCharacter,
    FName InSectorID,
    EBHWarPriorityType InOperationType,
    FName InSupplySourceSectorID,
    bool bSuppressInitialCheckpoint
)
{
    if (!HasAuthority() ||
        !IsValid(InPlayerCharacter) ||
        InSectorID.IsNone() ||
        InOperationType == EBHWarPriorityType::None)
    {
        return false;
    }

    PlayerCharacter = InPlayerCharacter;
    SectorID = InSectorID;
    SupplySourceSectorID = InSupplySourceSectorID;
    OperationType = InOperationType;
    bSuppressProgressCheckpoint =
        bSuppressInitialCheckpoint;
    ResolveSectorAnchor();
    ResolveEnemyClass();
    ConfigureForcePackage();

    if (!EnemyClass)
    {
        return false;
    }

    OperationCenter = IsValid(SectorAnchor)
        ? SectorAnchor->GetOperationCenter()
        : PlayerCharacter->GetActorLocation();
    SetActorLocation(OperationCenter);

    const float ActivationRadius = IsValid(SectorAnchor)
        ? SectorAnchor->GetOperationActivationRadius()
        : 0.0f;
    const float PlayerDistance = FVector::Dist2D(
        PlayerCharacter->GetActorLocation(),
        OperationCenter
    );

    if (!IsValid(SectorAnchor) ||
        PlayerDistance <= ActivationRadius)
    {
        ActivateOperation();
        return true;
    }

    const UWorld* World = GetWorld();
    const float ApproachWindowSeconds =
        CalculateApproachWindowSeconds(PlayerDistance);
    ApproachDeadlineTime = IsValid(World)
        ? World->GetTimeSeconds() + ApproachWindowSeconds
        : ApproachWindowSeconds;

    PlayerCharacter->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldOperationAssigned",
                "OPERATION ASSIGNED // {0}\n\n"
                "Travel to the marked sector. Distance: {1} km.\n"
                "Mobilization window: {2} minutes."
            ),
            FText::FromName(SectorID),
            FText::AsNumber(
                FMath::RoundToFloat(
                    PlayerDistance / 10000.0f
                ) / 10.0f
            ),
            FText::AsNumber(
                FMath::Max(
                    1,
                    FMath::CeilToInt(
                        ApproachWindowSeconds / 60.0f
                    )
                )
            )
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_APPROACH_STARTED sector=%s "
            "distance=%.1f deadline=%.1f"
        ),
        *SectorID.ToString(),
        PlayerDistance,
        ApproachWindowSeconds
    );
    PublishCurrentOperationSnapshot();
    return true;
}

bool ABHOpenWorldOperationDirector::IsOperationInProgress() const
{
    return !bOperationComplete;
}

bool ABHOpenWorldOperationDirector::IsOperationActivated() const
{
    return bOperationActivated;
}

FVector ABHOpenWorldOperationDirector::GetOperationCenter() const
{
    return OperationCenter;
}

FText ABHOpenWorldOperationDirector::GetSectorDisplayName() const
{
    return IsValid(SectorAnchor)
        ? SectorAnchor->GetSectorDisplayName()
        : FText::FromName(SectorID);
}

FText ABHOpenWorldOperationDirector::GetOperationStatusText() const
{
    if (bOperationComplete)
    {
        return FText::GetEmpty();
    }

    const int32 TotalWaveCount =
        OperationType == EBHWarPriorityType::Defend
            ? EffectiveDefenseWaveCount
            : 1 + EffectiveAttackReinforcementWaveCount;
    const int32 ExpectedHostileCount =
        OperationType == EBHWarPriorityType::Defend
            ? EffectiveDefenseWaveCount *
                EffectiveDefenseEnemiesPerWave
            : EffectiveAttackEnemyCount +
                (
                    EffectiveAttackReinforcementWaveCount *
                    EffectiveAttackReinforcementsPerWave
                );
    const FText OperationLabel =
        OperationType == EBHWarPriorityType::Defend
            ? NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldOperationDefendLabel",
                "DEFEND"
            )
            : OperationType == EBHWarPriorityType::Raid
            ? NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldOperationRaidLabel",
                "RAID"
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldOperationAttackLabel",
                "ATTACK"
            );
    const FText ThreatLabel =
        ExpectedHostileCount >= 9
            ? NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldOperationHeavyThreatLabel",
                "HEAVY"
            )
            : ExpectedHostileCount >= 5
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldOperationModerateThreatLabel",
                    "MODERATE"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldOperationLightThreatLabel",
                    "LIGHT"
                );
    const EBHRaidOperationalSignature RaidSignature =
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            EnemyCasualties,
            FriendlySupportCasualties,
            bRaidDetectedBeforeSabotage
        );
    const FText RaidSignatureLabel =
        RaidSignature == EBHRaidOperationalSignature::Clean
            ? NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldRaidCleanSignatureLabel",
                "CLEAN"
            )
            : RaidSignature ==
                EBHRaidOperationalSignature::Loud
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldRaidLoudSignatureLabel",
                    "LOUD"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldRaidContestedSignatureLabel",
                    "CONTESTED"
                );

    if (!bOperationActivated)
    {
        const int32 RemainingSeconds = FMath::Max(
            0,
            FMath::CeilToInt(
                GetApproachSecondsRemaining()
            )
        );
        const int32 RemainingMinutes = RemainingSeconds / 60;
        const int32 RemainingMinuteSeconds =
            RemainingSeconds % 60;
        const FText ApproachTime = FText::FromString(
            FString::Printf(
                TEXT("%02d:%02d"),
                RemainingMinutes,
                RemainingMinuteSeconds
            )
        );

        if (OperationType == EBHWarPriorityType::Defend)
        {
            return FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldDefenseApproachHUDStatus",
                    "{0} // MOBILIZE {5} // {1} THREAT\n"
                    "FORCE {2} WAVES x {3} // SUPPORT {4}"
                ),
                OperationLabel,
                ThreatLabel,
                FText::AsNumber(EffectiveDefenseWaveCount),
                FText::AsNumber(EffectiveDefenseEnemiesPerWave),
                FText::AsNumber(EffectiveFriendlySupportCount),
                ApproachTime
            );
        }

        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldAttackApproachHUDStatus",
                "{0} // MOBILIZE {6} // {1} THREAT\n"
                "FORCE {2} + {3}x{4} REACTION // SUPPORT {5}"
            ),
            OperationLabel,
            ThreatLabel,
            FText::AsNumber(EffectiveAttackEnemyCount),
            FText::AsNumber(
                EffectiveAttackReinforcementWaveCount
            ),
            FText::AsNumber(
                EffectiveAttackReinforcementsPerWave
            ),
            FText::AsNumber(EffectiveFriendlySupportCount),
            ApproachTime
        );
    }

    FText TacticalStatus;

    if (OperationType == EBHWarPriorityType::Raid &&
        bRaidTargetSabotaged)
    {
        const float DistanceFromTarget =
            GetClosestParticipantDistanceToOperation();
        const int32 RemainingMeters = FMath::Max(
            0,
            FMath::CeilToInt(
                (RaidExfiltrationRadius - DistanceFromTarget) /
                100.0f
            )
        );
        TacticalStatus = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldRaidExfilHUDStatus",
                "{0} // CHARGES ARMED // BREAK CONTACT\n"
                "SIGNATURE {1} // EXFIL {2} M // "
                "HOSTILES {3} // SUPPORT {4}/{5}"
            ),
            OperationLabel,
            RaidSignatureLabel,
            FText::AsNumber(RemainingMeters),
            FText::AsNumber(GetLivingEnemyCount()),
            FText::AsNumber(GetLivingAllyCount()),
            FText::AsNumber(EffectiveFriendlySupportCount)
        );
    }
    else if (OperationType == EBHWarPriorityType::Raid)
    {
        TacticalStatus = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldRaidSabotageHUDStatus",
                "{0} // SABOTAGE LOGISTICS CACHE\n"
                "SIGNATURE {1} // HOSTILES {2} // SUPPORT {3}/{4}"
            ),
            OperationLabel,
            RaidSignatureLabel,
            FText::AsNumber(GetLivingEnemyCount()),
            FText::AsNumber(GetLivingAllyCount()),
            FText::AsNumber(EffectiveFriendlySupportCount)
        );
    }
    else if (OperationType == EBHWarPriorityType::Defend &&
        DefenseBreachProgress > KINDA_SMALL_NUMBER)
    {
        const int32 SecondsRemaining = FMath::Max(
            0,
            FMath::CeilToInt(
                FMath::Max(1.0f, DefenseBreachDuration) -
                DefenseBreachProgress
            )
        );
        const bool bObjectiveCurrentlyOverrun =
            IsSecureAreaContested() &&
            !HasLivingDefenderInSecureArea();
        TacticalStatus = FText::Format(
            bObjectiveCurrentlyOverrun
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldDefenseBreachHUDStatus",
                    "{0} // OBJECTIVE OVERRUN // {1}s\n"
                    "RETAKE DEFENSIVE LINE // SUPPORT {2}/{3}"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldDefenseBreachRecoveringHUDStatus",
                    "{0} // LINE STABILIZING // {1}s\n"
                    "HOLD DEFENSIVE LINE // SUPPORT {2}/{3}"
                ),
            OperationLabel,
            FText::AsNumber(SecondsRemaining),
            FText::AsNumber(GetLivingAllyCount()),
            FText::AsNumber(EffectiveFriendlySupportCount)
        );
    }
    else if (bSecuringObjective &&
        OperationType == EBHWarPriorityType::Defend)
    {
        const float SafeSecureDuration = FMath::Max(
            0.1f,
            ObjectiveSecureDuration
        );
        const int32 SecurePercentage = FMath::Clamp(
            FMath::RoundToInt(
                (ObjectiveSecureProgress / SafeSecureDuration) *
                    100.0f
            ),
            0,
            100
        );
        const bool bSecureAreaContested =
            IsSecureAreaContested();
        TacticalStatus = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldDefenseHoldHUDStatus",
                "{0} // CONFIRM HOLD // {1}%\n"
                "{2} // RADIUS {3} M // SUPPORT {4}/{5}"
            ),
            OperationLabel,
            FText::AsNumber(SecurePercentage),
            bSecureAreaContested
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldDefenseHoldAreaContested",
                    "CONTESTED // CLEAR HOSTILES"
                )
                : (
                    HasPlayerOrFieldOperativeInSecureArea()
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "OpenWorldDefenseInsideHoldArea",
                            "HOLDING DEFENSIVE LINE"
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "OpenWorldDefenseOutsideHoldArea",
                            "RETURN TO DEFENSIVE LINE"
                        )
                ),
            FText::AsNumber(
                FMath::RoundToInt(
                    FMath::Max(0.0f, ObjectiveSecureRadius) /
                        100.0f
                )
            ),
            FText::AsNumber(GetLivingAllyCount()),
            FText::AsNumber(EffectiveFriendlySupportCount)
        );
    }
    else if (bSecuringObjective &&
        OperationType != EBHWarPriorityType::Defend)
    {
        const float SafeSecureDuration = FMath::Max(
            0.1f,
            ObjectiveSecureDuration
        );
        const int32 SecurePercentage = FMath::Clamp(
            FMath::RoundToInt(
                (ObjectiveSecureProgress / SafeSecureDuration) *
                    100.0f
            ),
            0,
            100
        );
        const bool bSecureAreaContested =
            IsSecureAreaContested();
        TacticalStatus = FText::Format(
            OperationType == EBHWarPriorityType::Raid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldRaidDisruptHUDStatus",
                    "{0} // DISRUPT LOGISTICS // {1}%\n"
                    "{2} // RADIUS {3} M // SUPPORT {4}/{5}"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldAttackSecureHUDStatus",
                    "{0} // SECURE OBJECTIVE // {1}%\n"
                    "{2} // RADIUS {3} M // SUPPORT {4}/{5}"
                ),
            OperationLabel,
            FText::AsNumber(SecurePercentage),
            bSecureAreaContested
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldAttackSecureAreaContested",
                    "CONTESTED // CLEAR HOSTILES"
                )
                : (
                    HasPlayerOrFieldOperativeInSecureArea()
                        ? NSLOCTEXT(
                            "BrokenHorizon",
                            "OpenWorldAttackInsideSecureArea",
                            "SECURING"
                        )
                        : NSLOCTEXT(
                            "BrokenHorizon",
                            "OpenWorldAttackOutsideSecureArea",
                            "ENTER OBJECTIVE AREA"
                        )
                ),
            FText::AsNumber(
                FMath::RoundToInt(
                    FMath::Max(0.0f, ObjectiveSecureRadius) /
                        100.0f
                )
            ),
            FText::AsNumber(GetLivingAllyCount()),
            FText::AsNumber(EffectiveFriendlySupportCount)
        );
    }
    else if (bWaitingForWave)
    {
        TacticalStatus = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldOperationWaveCountdownHUDStatus",
                "{0} // {1} THREAT // WAVE {2}/{3} CLEAR\n"
                "REINFORCEMENTS {4}s // SUPPORT {5}/{6}"
            ),
            OperationLabel,
            ThreatLabel,
            FText::AsNumber(CurrentWave),
            FText::AsNumber(TotalWaveCount),
            FText::AsNumber(GetSecondsUntilNextWave()),
            FText::AsNumber(GetLivingAllyCount()),
            FText::AsNumber(EffectiveFriendlySupportCount)
        );
    }
    else
    {
        TacticalStatus = FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldOperationHUDStatus",
                "{0} // {1} THREAT // WAVE {2}/{3}\n"
                "HOSTILES {4} // ROUTED {5} // SUPPORT {6}/{7}"
            ),
            OperationLabel,
            ThreatLabel,
            FText::AsNumber(CurrentWave),
            FText::AsNumber(TotalWaveCount),
            FText::AsNumber(GetLivingEnemyCount()),
            FText::AsNumber(EnemyRoutedCount),
            FText::AsNumber(GetLivingAllyCount()),
            FText::AsNumber(EffectiveFriendlySupportCount)
        );
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const FBHWarSectorState StagingSector =
        IsValid(WarSubsystem)
            ? WarSubsystem->GetSectorState(SupplySourceSectorID)
            : FBHWarSectorState();

    if (StagingSector.SectorID.IsNone())
    {
        return TacticalStatus;
    }

    return FText::Format(
        NSLOCTEXT(
            "BrokenHorizon",
            "OpenWorldOperationLogisticsHUDStatus",
            "{0}\nSTAGING {1} // SUPPLY {2}%\n"
            "LOSSES // SUPPORT {3} // HOSTILES {4}"
        ),
        TacticalStatus,
        StagingSector.DisplayName,
        FText::AsNumber(
            FMath::RoundToInt(StagingSector.Supply)
        ),
        FText::AsNumber(FriendlySupportCasualties),
        FText::AsNumber(EnemyCasualties)
    );
}

int32 ABHOpenWorldOperationDirector::
    GetFriendlySupportCasualties() const
{
    return FriendlySupportCasualties;
}

int32 ABHOpenWorldOperationDirector::GetEnemyCasualties() const
{
    return EnemyCasualties;
}

int32 ABHOpenWorldOperationDirector::GetEnemyRoutedCount() const
{
    return EnemyRoutedCount;
}

bool ABHOpenWorldOperationDirector::
    WasRaidDetectedBeforeSabotage() const
{
    return bRaidDetectedBeforeSabotage;
}

bool ABHOpenWorldOperationDirector::
    ToggleFriendlySupportHoldOrder()
{
    if (!HasAuthority() ||
        !bOperationActivated ||
        bOperationComplete ||
        GetLivingAllyCount() <= 0)
    {
        return false;
    }

    bFriendlySupportHolding = !bFriendlySupportHolding;
    bFriendlySupportHasCommandLocation = false;
    FriendlySupportCommandLocation = FVector::ZeroVector;
    FriendlySupportCommandYaw = 0.0f;
    ApplyFriendlySupportOrder();
    RequestOperationCheckpoint();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_SQUAD_ORDER sector=%s order=%s allies=%d"),
        *SectorID.ToString(),
        bFriendlySupportHolding ? TEXT("hold") : TEXT("follow"),
        GetLivingAllyCount()
    );

    return true;
}

bool ABHOpenWorldOperationDirector::
    SetFriendlySupportMoveAndHoldOrder(
        const FVector& CommandLocation,
        float CommandYaw
    )
{
    if (!HasAuthority() ||
        !bOperationActivated ||
        bOperationComplete ||
        GetLivingAllyCount() <= 0 ||
        CommandLocation.ContainsNaN())
    {
        return false;
    }

    bFriendlySupportHolding = true;
    bFriendlySupportHasCommandLocation = true;
    FriendlySupportCommandLocation = CommandLocation;
    FriendlySupportCommandYaw =
        FRotator::NormalizeAxis(CommandYaw);
    ApplyFriendlySupportOrder();
    RequestOperationCheckpoint();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SQUAD_ORDER sector=%s order=hold "
            "mode=designated location=%s yaw=%.1f allies=%d"
        ),
        *SectorID.ToString(),
        *FriendlySupportCommandLocation.ToCompactString(),
        FriendlySupportCommandYaw,
        GetLivingAllyCount()
    );

    return true;
}

bool ABHOpenWorldOperationDirector::
    SetFriendlySupportFollowOrder()
{
    if (!HasAuthority() ||
        !bOperationActivated ||
        bOperationComplete ||
        GetLivingAllyCount() <= 0)
    {
        return false;
    }

    bFriendlySupportHolding = false;
    bFriendlySupportHasCommandLocation = false;
    FriendlySupportCommandLocation = FVector::ZeroVector;
    FriendlySupportCommandYaw = 0.0f;
    ApplyFriendlySupportOrder();
    RequestOperationCheckpoint();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SQUAD_ORDER sector=%s order=follow allies=%d"
        ),
        *SectorID.ToString(),
        GetLivingAllyCount()
    );

    return true;
}

bool ABHOpenWorldOperationDirector::
    IsFriendlySupportHolding() const
{
    return bFriendlySupportHolding;
}

bool ABHOpenWorldOperationDirector::
    HasFriendlySupportCommandLocation() const
{
    return
        bOperationActivated &&
        !bOperationComplete &&
        bFriendlySupportHolding &&
        bFriendlySupportHasCommandLocation &&
        GetLivingAllyCount() > 0;
}

FVector ABHOpenWorldOperationDirector::
    GetFriendlySupportCommandLocation() const
{
    return HasFriendlySupportCommandLocation()
        ? FriendlySupportCommandLocation
        : FVector::ZeroVector;
}

int32 ABHOpenWorldOperationDirector::
    GetLivingFriendlySupportCount() const
{
    return GetLivingAllyCount();
}

FBHOpenWorldOperationState
ABHOpenWorldOperationDirector::CaptureSaveState() const
{
    FBHOpenWorldOperationState State;

    if (bOperationComplete)
    {
        return State;
    }

    State.bHasSnapshot = true;
    State.bOperationActivated = bOperationActivated;
    State.bWaitingForWave = bWaitingForWave;
    State.bSecuringObjective = bSecuringObjective;
    State.bRaidTargetSabotaged = bRaidTargetSabotaged;
    State.bRaidDetectedBeforeSabotage =
        bRaidDetectedBeforeSabotage;
    State.bFriendlySupportHolding =
        bFriendlySupportHolding;
    State.bFriendlySupportHasCommandLocation =
        bFriendlySupportHasCommandLocation;
    State.FriendlySupportCommandLocation =
        FriendlySupportCommandLocation;
    State.FriendlySupportCommandYaw =
        FriendlySupportCommandYaw;
    State.ObjectiveSecureProgress =
        ObjectiveSecureProgress;
    State.DefenseBreachProgress =
        DefenseBreachProgress;
    State.CurrentWave = CurrentWave;
    State.LivingEnemyCount = GetLivingEnemyCount();
    State.LivingAllyCount = GetLivingAllyCount();
    State.FriendlySupportCasualties =
        FriendlySupportCasualties;
    State.EnemyCasualties = EnemyCasualties;
    State.EnemyRoutedCount = EnemyRoutedCount;
    State.AttackEnemyCount = EffectiveAttackEnemyCount;
    State.AttackReinforcementWaveCount =
        EffectiveAttackReinforcementWaveCount;
    State.AttackReinforcementsPerWave =
        EffectiveAttackReinforcementsPerWave;
    State.DefenseWaveCount = EffectiveDefenseWaveCount;
    State.DefenseEnemiesPerWave =
        EffectiveDefenseEnemiesPerWave;
    State.FriendlySupportCount =
        EffectiveFriendlySupportCount;
    State.EnemySourceSectorID = EnemySourceSectorID;

    const UWorld* World = GetWorld();

    if (bWaitingForWave && IsValid(World))
    {
        State.SecondsUntilNextWave = FMath::Max(
            0.0f,
            NextWaveTime - World->GetTimeSeconds()
        );
    }

    if (!bOperationActivated && IsValid(World))
    {
        State.SecondsUntilApproachDeadline =
            GetApproachSecondsRemaining();
    }

    return State;
}

EBHActiveOperationPhase
ABHOpenWorldOperationDirector::ResolveOperationPhase() const
{
    if (bOperationComplete)
    {
        return EBHActiveOperationPhase::None;
    }

    if (!bOperationActivated)
    {
        return EBHActiveOperationPhase::Approach;
    }

    if (OperationType == EBHWarPriorityType::Raid &&
        bRaidTargetSabotaged)
    {
        return EBHActiveOperationPhase::RaidExfiltration;
    }

    if (bWaitingForWave)
    {
        return EBHActiveOperationPhase::AwaitingWave;
    }

    if (bSecuringObjective)
    {
        return EBHActiveOperationPhase::Securing;
    }

    return EBHActiveOperationPhase::Combat;
}

void ABHOpenWorldOperationDirector::PublishOperationSnapshot(
    EBHActiveOperationPhase PhaseOverride
)
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    ABHWarGameState* WarGameState = IsValid(World)
        ? World->GetGameState<ABHWarGameState>()
        : nullptr;

    if (!IsValid(WarGameState))
    {
        return;
    }

    FBHActiveOperationSnapshot Snapshot;
    Snapshot.Revision = ++OperationSnapshotRevision;
    Snapshot.Phase = PhaseOverride;
    Snapshot.SectorID = SectorID;
    const UGameInstance* GameInstance = World
        ? World->GetGameInstance()
        : nullptr;
    const UBHWarSubsystem* WarSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    Snapshot.OperationID = IsValid(WarSubsystem)
        ? WarSubsystem->GetCommittedOperationID()
        : NAME_None;
    Snapshot.SupplySourceSectorID = SupplySourceSectorID;
    Snapshot.EnemySourceSectorID = EnemySourceSectorID;
    Snapshot.OperationType = OperationType;
    Snapshot.OperationCenter = OperationCenter;
    Snapshot.OperationState = CaptureSaveState();

    if (IsValid(World))
    {
        if (PhaseOverride == EBHActiveOperationPhase::Approach)
        {
            Snapshot.PhaseEndServerWorldTimeSeconds =
                ApproachDeadlineTime;
        }
        else if (
            PhaseOverride ==
            EBHActiveOperationPhase::AwaitingWave)
        {
            Snapshot.PhaseEndServerWorldTimeSeconds =
                NextWaveTime;
        }
    }

    WarGameState->PublishActiveOperationSnapshot(Snapshot);
}

void ABHOpenWorldOperationDirector::
    PublishCurrentOperationSnapshot()
{
    PublishOperationSnapshot(ResolveOperationPhase());
}

bool ABHOpenWorldOperationDirector::RestoreOperationState(
    const FBHOpenWorldOperationState& SavedState
)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!SavedState.bHasSnapshot)
    {
        bSuppressProgressCheckpoint = false;
        return true;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(PlayerCharacter) ||
        bOperationComplete)
    {
        return false;
    }

    DestroyTrackedUnits();
    DestroyOperationPatrolPoints();
    DestroyRaidSabotageTarget();

    EffectiveAttackEnemyCount = FMath::Max(
        1,
        SavedState.AttackEnemyCount
    );
    EffectiveAttackReinforcementWaveCount = FMath::Max(
        0,
        SavedState.AttackReinforcementWaveCount
    );
    EffectiveAttackReinforcementsPerWave = FMath::Max(
        1,
        SavedState.AttackReinforcementsPerWave
    );
    EffectiveDefenseWaveCount = FMath::Max(
        1,
        SavedState.DefenseWaveCount
    );
    EffectiveDefenseEnemiesPerWave = FMath::Max(
        1,
        SavedState.DefenseEnemiesPerWave
    );
    EffectiveFriendlySupportCount = FMath::Max(
        0,
        SavedState.FriendlySupportCount
    );
    FriendlySupportCasualties = FMath::Max(
        0,
        SavedState.FriendlySupportCasualties
    );
    EnemyCasualties = FMath::Max(
        0,
        SavedState.EnemyCasualties
    );
    EnemyRoutedCount = FMath::Max(
        0,
        SavedState.EnemyRoutedCount
    );
    if (!SavedState.EnemySourceSectorID.IsNone())
    {
        EnemySourceSectorID =
            SavedState.EnemySourceSectorID;
    }
    bOperationActivated = SavedState.bOperationActivated;
    bWaitingForWave = false;
    bSecuringObjective = false;
    bRaidTargetSabotaged = false;
    bRaidDetectedBeforeSabotage = false;
    ObjectiveSecureProgress = 0.0f;
    DefenseBreachProgress = 0.0f;
    LastCheckpointedDefenseBreachProgress = 0.0f;
    NextWaveTime = 0.0f;
    ApproachDeadlineTime = -1.0f;
    CurrentWave = 0;

    if (!bOperationActivated)
    {
        const float PlayerDistance = FVector::Dist2D(
            PlayerCharacter->GetActorLocation(),
            OperationCenter
        );
        const float RestoredApproachSeconds =
            SavedState.SecondsUntilApproachDeadline > 0.0f
                ? SavedState.SecondsUntilApproachDeadline
                : CalculateApproachWindowSeconds(
                    PlayerDistance
                );
        ApproachDeadlineTime =
            World->GetTimeSeconds() +
            RestoredApproachSeconds;
        bSuppressProgressCheckpoint = false;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_OPERATION_RESTORED sector=%s "
                "phase=approach deadline=%.1f"
            ),
            *SectorID.ToString(),
            RestoredApproachSeconds
        );
        PublishCurrentOperationSnapshot();
        return true;
    }

    const int32 TotalWaveCount =
        OperationType == EBHWarPriorityType::Defend
            ? EffectiveDefenseWaveCount
            : 1 + EffectiveAttackReinforcementWaveCount;
    CurrentWave = FMath::Clamp(
        SavedState.CurrentWave,
        1,
        TotalWaveCount
    );
    bWaitingForWave =
        SavedState.bWaitingForWave &&
        CurrentWave < TotalWaveCount;
    bSecuringObjective =
        SavedState.bSecuringObjective &&
        CurrentWave >= TotalWaveCount;
    bRaidTargetSabotaged =
        OperationType == EBHWarPriorityType::Raid &&
        SavedState.bRaidTargetSabotaged;
    bRaidDetectedBeforeSabotage =
        OperationType == EBHWarPriorityType::Raid &&
        SavedState.bRaidDetectedBeforeSabotage;
    bFriendlySupportHolding =
        SavedState.bFriendlySupportHolding;
    bFriendlySupportHasCommandLocation =
        bFriendlySupportHolding &&
        SavedState.bFriendlySupportHasCommandLocation;
    FriendlySupportCommandLocation =
        bFriendlySupportHasCommandLocation
            ? SavedState.FriendlySupportCommandLocation
            : FVector::ZeroVector;
    FriendlySupportCommandYaw =
        bFriendlySupportHasCommandLocation
            ? FRotator::NormalizeAxis(
                SavedState.FriendlySupportCommandYaw
            )
            : 0.0f;
    ObjectiveSecureProgress = bSecuringObjective
        ? FMath::Clamp(
            SavedState.ObjectiveSecureProgress,
            0.0f,
            FMath::Max(0.1f, ObjectiveSecureDuration)
        )
        : 0.0f;
    DefenseBreachProgress =
        OperationType == EBHWarPriorityType::Defend
            ? FMath::Clamp(
                SavedState.DefenseBreachProgress,
                0.0f,
                FMath::Max(1.0f, DefenseBreachDuration)
            )
            : 0.0f;
    LastCheckpointedDefenseBreachProgress =
        DefenseBreachProgress;

    BuildOperationPatrolPoints();
    if (OperationType == EBHWarPriorityType::Raid &&
        !bRaidTargetSabotaged)
    {
        SpawnRaidSabotageTarget();
    }

    const int32 LivingAllyCount = FMath::Clamp(
        SavedState.LivingAllyCount,
        0,
        EffectiveFriendlySupportCount
    );
    SpawnFriendlySupport(LivingAllyCount);
    ApplyFriendlySupportOrder();

    const int32 MaximumWaveEnemyCount =
        OperationType == EBHWarPriorityType::Raid &&
            bRaidTargetSabotaged
            ? EffectiveAttackEnemyCount +
                (
                    EffectiveAttackReinforcementWaveCount *
                    EffectiveAttackReinforcementsPerWave
                )
            : OperationType == EBHWarPriorityType::Defend
            ? EffectiveDefenseEnemiesPerWave
            : CurrentWave <= 1
                ? EffectiveAttackEnemyCount
                : EffectiveAttackReinforcementsPerWave;
    const int32 LivingEnemyCount = FMath::Clamp(
        SavedState.LivingEnemyCount,
        0,
        MaximumWaveEnemyCount
    );

    if (!bWaitingForWave)
    {
        SpawnEnemies(LivingEnemyCount);
        if (bRaidTargetSabotaged)
        {
            AlertRaidReactionForce();
        }
    }
    else
    {
        NextWaveTime =
            World->GetTimeSeconds() +
            FMath::Max(
                0.0f,
                SavedState.SecondsUntilNextWave
            );
    }

    bSuppressProgressCheckpoint = false;
    SetActorTickEnabled(true);
    PlayerCharacter->ShowStatusNotification(
        FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "OpenWorldOperationResumed",
                "OPERATION RESUMED // {0}\n\n"
                "Wave {1}/{2} // Hostiles {3} // Support {4}."
            ),
            GetSectorDisplayName(),
            FText::AsNumber(CurrentWave),
            FText::AsNumber(TotalWaveCount),
            FText::AsNumber(LivingEnemyCount),
            FText::AsNumber(LivingAllyCount)
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_RESTORED sector=%s phase=active "
            "wave=%d/%d waiting=%d securing=%d raid_exfil=%d "
            "raid_detected=%d "
            "secure_progress=%.1f breach_progress=%.1f "
            "hostiles=%d support=%d "
            "enemy_losses=%d enemy_routed=%d support_losses=%d"
        ),
        *SectorID.ToString(),
        CurrentWave,
        TotalWaveCount,
        bWaitingForWave ? 1 : 0,
        bSecuringObjective ? 1 : 0,
        bRaidTargetSabotaged ? 1 : 0,
        bRaidDetectedBeforeSabotage ? 1 : 0,
        ObjectiveSecureProgress,
        DefenseBreachProgress,
        LivingEnemyCount,
        LivingAllyCount,
        EnemyCasualties,
        EnemyRoutedCount,
        FriendlySupportCasualties
    );
    PublishCurrentOperationSnapshot();
    return true;
}

void ABHOpenWorldOperationDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority())
    {
        SetActorTickEnabled(false);
        return;
    }

    if (bOperationComplete)
    {
        return;
    }

    if (!IsValid(PlayerCharacter))
    {
        PlayerCharacter = FindLivingPlayerParticipant();

        if (!IsValid(PlayerCharacter))
        {
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_OPERATION_COMMAND_PLAYER_REASSIGNED "
                "sector=%s player=%s"
            ),
            *SectorID.ToString(),
            *PlayerCharacter->GetName()
        );

        if (!PlayerCharacter->AdoptSharedWarOperationAuthority(
                this,
                SectorID,
                SupplySourceSectorID,
                OperationType
            ))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_OPERATION_COMMAND_HANDOFF_REJECTED "
                    "sector=%s player=%s"
                ),
                *SectorID.ToString(),
                *PlayerCharacter->GetName()
            );
            PlayerCharacter = nullptr;
            return;
        }
    }

    for (ABHCharacter* Participant : GetLivingPlayerParticipants())
    {
        if (IsValid(Participant))
        {
            Participant->AdoptSharedWarOperationAuthority(
                this,
                SectorID,
                SupplySourceSectorID,
                OperationType
            );
        }
    }

    PublishCurrentOperationSnapshot();

    if (!bOperationActivated)
    {
        if (!IsValid(SectorAnchor) ||
            FMath::Square(
                GetClosestParticipantDistanceToOperation()
            ) <= FMath::Square(
                SectorAnchor->GetOperationActivationRadius()
            ))
        {
            ActivateOperation();
        }
        else if (GetApproachSecondsRemaining() <= 0.0f)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_OPERATION_APPROACH_EXPIRED sector=%s "
                    "type=%d"
                ),
                *SectorID.ToString(),
                static_cast<int32>(OperationType)
            );
            FailOperation(
                OperationType == EBHWarPriorityType::Defend
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "OpenWorldDefenseApproachExpired",
                        "Friendly defenses collapsed before "
                        "you reached the sector."
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "OpenWorldOffensiveApproachExpired",
                        "The operation was abandoned after the "
                        "mobilization window expired."
                    )
            );
        }

        return;
    }

    UpdateEnemyRouts();

    if (OperationType == EBHWarPriorityType::Defend)
    {
        UpdateDefenseBreach(DeltaSeconds);

        if (bOperationComplete)
        {
            return;
        }
    }

    if (OperationType == EBHWarPriorityType::Raid)
    {
        if (!bRaidTargetSabotaged)
        {
            UpdateRaidDetectionState();
        }

        if (bRaidTargetSabotaged)
        {
            const float ClosestParticipantDistance =
                GetClosestParticipantDistanceToOperation();
            if (AreAllLivingParticipantsOutsideRadius(
                    RaidExfiltrationRadius
                ))
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_RAID_EXFILTRATION_COMPLETE sector=%s "
                        "distance=%.1f"
                    ),
                    *SectorID.ToString(),
                    ClosestParticipantDistance
                );
                CompleteOperation();
            }

            return;
        }

        if (!IsValid(RaidSabotageTarget))
        {
            SpawnRaidSabotageTarget();
        }

        return;
    }

    if (HasLivingEnemies())
    {
        return;
    }

    const int32 TotalWaveCount =
        OperationType == EBHWarPriorityType::Defend
            ? EffectiveDefenseWaveCount
            : 1 + EffectiveAttackReinforcementWaveCount;

    if (CurrentWave >= TotalWaveCount)
    {
        if (OperationType != EBHWarPriorityType::Defend)
        {
            UpdateAttackObjectiveSecuring(DeltaSeconds);
        }
        else
        {
            UpdateDefenseObjectiveHolding(DeltaSeconds);
        }
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    if (!bWaitingForWave)
    {
        bWaitingForWave = true;
        const float InterWaveDelay =
            OperationType == EBHWarPriorityType::Defend
                ? DefenseInterWaveDelay
                : AttackInterWaveDelay;
        NextWaveTime =
            World->GetTimeSeconds() + InterWaveDelay;
        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                OperationType == EBHWarPriorityType::Defend
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "OpenWorldDefenseWaveCleared",
                        "DEFENSE LINE HOLDING\n\n"
                        "Wave {0}/{2} defeated. "
                        "Next assault in {1} seconds."
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "OpenWorldAttackReactionDetected",
                        "REACTION FORCE DETECTED\n\n"
                        "Initial force broken; {3} hostiles routed. "
                        "Enemy response arrives in {1} seconds."
                    ),
                FText::AsNumber(CurrentWave),
                FText::AsNumber(
                    FMath::CeilToInt(InterWaveDelay)
                ),
                FText::AsNumber(TotalWaveCount),
                FText::AsNumber(EnemyRoutedCount)
            )
        );
        RequestOperationCheckpoint();
        return;
    }

    if (World->GetTimeSeconds() >= NextWaveTime)
    {
        bWaitingForWave = false;
        ++CurrentWave;
        const int32 IncomingEnemyCount =
            OperationType == EBHWarPriorityType::Defend
                ? EffectiveDefenseEnemiesPerWave
                : EffectiveAttackReinforcementsPerWave;
        SpawnEnemies(IncomingEnemyCount);
        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                OperationType == EBHWarPriorityType::Defend
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "OpenWorldDefenseWaveArrived",
                        "ENEMY ASSAULT // WAVE {0}/{1}\n\n"
                        "{2} hostile contacts entering the sector."
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "OpenWorldAttackReactionArrived",
                        "REACTION FORCE // WAVE {0}/{1}\n\n"
                        "{2} hostile contacts entering the objective."
                    ),
                FText::AsNumber(CurrentWave),
                FText::AsNumber(TotalWaveCount),
                FText::AsNumber(IncomingEnemyCount)
            )
        );
        RequestOperationCheckpoint();
    }
}

void ABHOpenWorldOperationDirector::ResolveSectorAnchor()
{
    SectorAnchor = nullptr;

    for (TActorIterator<ABHSectorAnchor> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && It->MatchesSector(SectorID))
        {
            SectorAnchor = *It;
            break;
        }
    }

    if (!IsValid(SectorAnchor))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "No sector anchor exists for %s; "
                "the operation will use the player location."
            ),
            *SectorID.ToString()
        );
    }
}

void ABHOpenWorldOperationDirector::ResolveEnemyClass()
{
    EnemyClass = nullptr;

    for (TActorIterator<ABHEnemySoldier> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            EnemyClass = It->GetClass();
            break;
        }
    }

    if (!EnemyClass)
    {
        EnemyClass = ABHEnemySoldier::StaticClass();
    }
}

void ABHOpenWorldOperationDirector::ConfigureForcePackage()
{
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = IsValid(World)
        ? World->GetGameInstance()
        : nullptr;
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    FBHWarOperationForceTuning Tuning;
    Tuning.AttackEnemyCount = AttackEnemyCount;
    Tuning.AttackReinforcementWaveCount =
        AttackReinforcementWaveCount;
    Tuning.AttackReinforcementsPerWave =
        AttackReinforcementsPerWave;
    Tuning.DefenseWaveCount = DefenseWaveCount;
    Tuning.DefenseEnemiesPerWave = DefenseEnemiesPerWave;
    Tuning.MaximumFriendlySupport = MaximumFriendlySupport;
    const FBHWarOperationForcePackage ForcePackage =
        BHWarOperationRules::BuildForcePackage(
            WarSubsystem,
            SectorID,
            OperationType,
            SupplySourceSectorID,
            Tuning
        );
    EffectiveAttackEnemyCount =
        ForcePackage.AttackEnemyCount;
    EffectiveAttackReinforcementWaveCount =
        ForcePackage.AttackReinforcementWaveCount;
    EffectiveAttackReinforcementsPerWave =
        ForcePackage.AttackReinforcementsPerWave;
    EffectiveDefenseWaveCount =
        ForcePackage.DefenseWaveCount;
    EffectiveDefenseEnemiesPerWave =
        ForcePackage.DefenseEnemiesPerWave;
    EffectiveFriendlySupportCount =
        ForcePackage.FriendlySupportCount;
    SupplySourceSectorID =
        ForcePackage.SupplySourceSectorID;
    EnemySourceSectorID =
        ForcePackage.EnemySourceSectorID;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_FORCE_PACKAGE sector=%s type=%d "
            "attack_count=%d attack_reinforcement_waves=%d "
            "attack_reinforcement_size=%d defense_waves=%d "
            "defense_per_wave=%d friendly_support=%d "
            "staging=%s enemy_source=%s staging_strength=%.1f "
            "staging_supply=%.1f enemy_strength=%.1f "
            "target_supply=%.1f intel=%.1f enemy_garrison=%d "
            "friendly_garrison=%d"
        ),
        *SectorID.ToString(),
        static_cast<int32>(OperationType),
        EffectiveAttackEnemyCount,
        EffectiveAttackReinforcementWaveCount,
        EffectiveAttackReinforcementsPerWave,
        EffectiveDefenseWaveCount,
        EffectiveDefenseEnemiesPerWave,
        EffectiveFriendlySupportCount,
        *SupplySourceSectorID.ToString(),
        *EnemySourceSectorID.ToString(),
        ForcePackage.StagingFriendlyStrength,
        ForcePackage.StagingSupply,
        ForcePackage.EnemyStrength,
        ForcePackage.TargetSupply,
        ForcePackage.IntelConfidence,
        ForcePackage.EnemyGarrisonCount,
        ForcePackage.FriendlyGarrisonCount
    );
}

float ABHOpenWorldOperationDirector::
    CalculateApproachWindowSeconds(float PlayerDistance) const
{
    return CalculateApproachWindowSecondsForOperation(
        OperationType,
        PlayerDistance
    );
}

float ABHOpenWorldOperationDirector::
    CalculateApproachWindowSecondsForOperation(
        EBHWarPriorityType InOperationType,
        float PlayerDistance
    ) const
{
    const float BaseSeconds =
        InOperationType == EBHWarPriorityType::Defend
            ? DefenseApproachBaseSeconds
            : OffensiveApproachBaseSeconds;
    const float SafeTravelSpeed = FMath::Max(
        100.0f,
        ExpectedApproachTravelSpeed
    );
    const float EstimatedTravelSeconds =
        FMath::Max(0.0f, PlayerDistance) / SafeTravelSpeed;
    const float RequestedWindow =
        FMath::Max(30.0f, BaseSeconds) +
        (
            EstimatedTravelSeconds *
            FMath::Max(1.0f, ApproachTravelTimeMultiplier)
        );

    return FMath::Clamp(
        RequestedWindow,
        FMath::Max(30.0f, BaseSeconds),
        FMath::Max(
            FMath::Max(30.0f, BaseSeconds),
            MaximumApproachSeconds
        )
    );
}

float ABHOpenWorldOperationDirector::
    GetApproachSecondsRemaining() const
{
    const UWorld* World = GetWorld();

    if (bOperationActivated ||
        bOperationComplete ||
        !IsValid(World) ||
        ApproachDeadlineTime < 0.0f)
    {
        return 0.0f;
    }

    return FMath::Max(
        0.0f,
        ApproachDeadlineTime - World->GetTimeSeconds()
    );
}

void ABHOpenWorldOperationDirector::ActivateOperation()
{
    if (bOperationActivated || bOperationComplete)
    {
        return;
    }

    bOperationActivated = true;
    ApproachDeadlineTime = -1.0f;
    CurrentWave = 1;
    bSecuringObjective = false;
    bRaidTargetSabotaged = false;
    bRaidDetectedBeforeSabotage = false;
    bFriendlySupportHolding = false;
    bFriendlySupportHasCommandLocation = false;
    FriendlySupportCommandLocation = FVector::ZeroVector;
    FriendlySupportCommandYaw = 0.0f;
    ObjectiveSecureProgress = 0.0f;
    DefenseBreachProgress = 0.0f;
    LastCheckpointedDefenseBreachProgress = 0.0f;
    FriendlySupportCasualties = 0;
    EnemyCasualties = 0;
    EnemyRoutedCount = 0;
    BuildOperationPatrolPoints();
    if (OperationType == EBHWarPriorityType::Raid)
    {
        SpawnRaidSabotageTarget();
    }
    SpawnFriendlySupport(EffectiveFriendlySupportCount);
    SpawnEnemies(
        OperationType == EBHWarPriorityType::Defend
            ? EffectiveDefenseEnemiesPerWave
            : EffectiveAttackEnemyCount
    );

    PlayerCharacter->ShowStatusNotification(
        FText::Format(
            OperationType == EBHWarPriorityType::Defend
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldDefenseStarted",
                    "OPEN-WORLD OPERATION ACTIVE\n\n"
                    "Hold sector {0}. Enemy wave 1/{2} inbound.\n"
                    "Friendly support: {1}."
                )
                : OperationType == EBHWarPriorityType::Raid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldRaidStarted",
                    "OPEN-WORLD RAID ACTIVE\n\n"
                    "Disrupt enemy logistics in sector {0}.\n"
                    "Avoid detection and unnecessary casualties "
                    "to preserve a clean signature.\n"
                    "Expected enemy waves: {2}.\n"
                    "Friendly support: {1}."
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldAttackStarted",
                    "OPEN-WORLD OPERATION ACTIVE\n\n"
                    "Clear hostile forces from sector {0}.\n"
                    "Expected enemy waves: {2}.\n"
                    "Friendly support: {1}."
            ),
            FText::FromName(SectorID),
            FText::AsNumber(EffectiveFriendlySupportCount),
            FText::AsNumber(
                OperationType == EBHWarPriorityType::Defend
                    ? EffectiveDefenseWaveCount
                    : 1 + EffectiveAttackReinforcementWaveCount
            )
        )
    );
    RequestOperationCheckpoint();
    PublishCurrentOperationSnapshot();
}

void ABHOpenWorldOperationDirector::BuildOperationPatrolPoints()
{
    DestroyOperationPatrolPoints();

    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    if (!IsValid(World) || !IsValid(NavigationSystem))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_OPERATION_PATROL_UNAVAILABLE sector=%s "
                "reason=navigation"
            ),
            *SectorID.ToString()
        );
        return;
    }

    const int32 SafePointCount =
        FMath::Max(1, ObjectivePatrolPointCount);
    const float SectorAngleOffset =
        static_cast<float>(GetTypeHash(SectorID) % 360);

    for (int32 Index = 0; Index < SafePointCount; ++Index)
    {
        FVector Candidate = OperationCenter;

        if (Index > 0)
        {
            const int32 OuterPointCount =
                FMath::Max(1, SafePointCount - 1);
            const float Angle =
                SectorAngleOffset +
                ((360.0f / OuterPointCount) * (Index - 1));
            const FVector Direction(
                FMath::Cos(FMath::DegreesToRadians(Angle)),
                FMath::Sin(FMath::DegreesToRadians(Angle)),
                0.0f
            );
            Candidate += Direction * ObjectivePatrolRadius;
        }

        FNavLocation PatrolLocation;

        if (!NavigationSystem->ProjectPointToNavigation(
                Candidate,
                PatrolLocation,
                FVector(2500.0f, 2500.0f, 12000.0f)))
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParameters.ObjectFlags |= RF_Transient;
        SpawnParameters.Owner = this;

        ABHPatrolPoint* PatrolPoint =
            World->SpawnActor<ABHPatrolPoint>(
                ABHPatrolPoint::StaticClass(),
                PatrolLocation.Location,
                FRotator::ZeroRotator,
                SpawnParameters
            );

        if (IsValid(PatrolPoint))
        {
            OperationPatrolPoints.Add(PatrolPoint);
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_PATROL_READY sector=%s "
            "points=%d center=%s"
        ),
        *SectorID.ToString(),
        OperationPatrolPoints.Num(),
        *OperationCenter.ToCompactString()
    );
}

void ABHOpenWorldOperationDirector::DestroyOperationPatrolPoints()
{
    for (ABHPatrolPoint* PatrolPoint : OperationPatrolPoints)
    {
        if (IsValid(PatrolPoint))
        {
            PatrolPoint->Destroy();
        }
    }

    OperationPatrolPoints.Reset();
}

void ABHOpenWorldOperationDirector::DestroyTrackedUnits()
{
    for (ABHEnemySoldier* Enemy : TrackedEnemies)
    {
        if (IsValid(Enemy) && !Enemy->IsDead())
        {
            Enemy->Destroy();
        }
    }

    for (ABHEnemySoldier* Ally : TrackedAllies)
    {
        if (IsValid(Ally) && !Ally->IsDead())
        {
            Ally->Destroy();
        }
    }

    TrackedEnemies.Reset();
    TrackedAllies.Reset();
}

void ABHOpenWorldOperationDirector::RequestOperationCheckpoint(
    float DelaySeconds
)
{
    UWorld* World = GetWorld();

    if (bSuppressProgressCheckpoint ||
        !IsValid(World))
    {
        return;
    }

    const float SafeDelay = FMath::Max(0.0f, DelaySeconds);

    if (bCheckpointPending)
    {
        return;
    }

    bCheckpointPending = true;
    FTimerDelegate CheckpointDelegate;
    CheckpointDelegate.BindUObject(
        this,
        &ABHOpenWorldOperationDirector::
            SaveOperationProgressNow
    );

    if (SafeDelay <= 0.0f)
    {
        World->GetTimerManager().SetTimerForNextTick(
            CheckpointDelegate
        );
    }
    else
    {
        World->GetTimerManager().SetTimer(
            OperationCheckpointTimerHandle,
            CheckpointDelegate,
            SafeDelay,
            false
        );
    }
}

void ABHOpenWorldOperationDirector::SaveOperationProgressNow()
{
    bCheckpointPending = false;

    if (bSuppressProgressCheckpoint ||
        bOperationComplete)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UBHSaveSubsystem* SaveSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHSaveSubsystem>()
        : nullptr;

    if (IsValid(SaveSubsystem) &&
        SaveSubsystem->SaveProgress())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_OPERATION_CHECKPOINT sector=%s wave=%d "
                "waiting=%d hostiles=%d support=%d"
            ),
            *SectorID.ToString(),
            CurrentWave,
            bWaitingForWave ? 1 : 0,
            GetLivingEnemyCount(),
            GetLivingAllyCount()
        );
    }
}

void ABHOpenWorldOperationDirector::SpawnEnemies(
    int32 EnemyCount
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || !EnemyClass)
    {
        return;
    }

    TrackedEnemies.RemoveAll(
        [](const TObjectPtr<ABHEnemySoldier>& Enemy)
        {
            return !IsValid(Enemy) || Enemy->IsDead();
        }
    );

    int32 SpawnedCount = 0;

    for (int32 Index = 0; Index < EnemyCount; ++Index)
    {
        bool bSpawned = false;

        for (int32 AttemptIndex = 0;
             AttemptIndex < MaximumOperationSpawnAttempts;
             ++AttemptIndex)
        {
            const FTransform SpawnTransform =
                BuildSpawnTransform(Index, AttemptIndex);
            ABHEnemySoldier* Enemy =
                World->SpawnActorDeferred<ABHEnemySoldier>(
                    EnemyClass,
                    SpawnTransform,
                    this,
                    nullptr,
                    ESpawnActorCollisionHandlingMethod::
                        AdjustIfPossibleButDontSpawnIfColliding
                );

            if (!IsValid(Enemy))
            {
                continue;
            }

            Enemy->SetFlags(RF_Transient);
            Enemy->SetCombatFaction(EBHCombatFaction::Hostile);
            Enemy->SetCombatantArchetype(
                ABHEnemySoldier::ChooseFormationArchetype(
                    Index,
                    EnemyCount
                )
            );
            Enemy->SetObjectiveIdToCompleteOnDeath(NAME_None);
            Enemy->SetPatrolPoints(
                BuildPatrolPointAssignment(Index + CurrentWave)
            );
            UGameplayStatics::FinishSpawningActor(
                Enemy,
                SpawnTransform
            );

            if (!IsValid(Enemy))
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "BH_OPERATION_UNIT_REJECTED sector=%s "
                        "side=hostile reason=blocked_spawn"
                    ),
                    *SectorID.ToString()
                );
                continue;
            }

            ABHEnemyAIController* EnemyController =
                Cast<ABHEnemyAIController>(
                    Enemy->GetController()
                );

            if (!BHWarOperationRules::
                    IsOperationCombatantReady(
                        !Enemy->IsDead(),
                        IsValid(EnemyController)
                    ))
            {
                Enemy->SpawnDefaultController();
                EnemyController = Cast<ABHEnemyAIController>(
                    Enemy->GetController()
                );
            }

            if (!BHWarOperationRules::
                    IsOperationCombatantReady(
                        !Enemy->IsDead(),
                        IsValid(EnemyController)
                    ))
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "BH_OPERATION_UNIT_REJECTED sector=%s "
                        "side=hostile reason=no_controller"
                    ),
                    *SectorID.ToString()
                );
                Enemy->Destroy();
                break;
            }

            if (UBHHealthComponent* HealthComponent =
                Enemy->GetHealthComponent())
            {
                HealthComponent->OnDeath.AddDynamic(
                    this,
                    &ABHOpenWorldOperationDirector::HandleEnemyDeath
                );
            }

            TrackedEnemies.Add(Enemy);
            ++SpawnedCount;
            bSpawned = true;

            if (AttemptIndex > 0)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_OPERATION_UNIT_REPOSITIONED sector=%s "
                        "side=hostile unit=%d attempt=%d"
                    ),
                    *SectorID.ToString(),
                    Index,
                    AttemptIndex + 1
                );
            }

            break;
        }

        if (!bSpawned)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_OPERATION_UNIT_SPAWN_EXHAUSTED sector=%s "
                    "side=hostile unit=%d attempts=%d"
                ),
                *SectorID.ToString(),
                Index,
                MaximumOperationSpawnAttempts
            );
        }
    }

    if (SpawnedCount < EnemyCount)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_OPERATION_FORCE_SHORTFALL sector=%s "
                "side=hostile requested=%d spawned=%d"
            ),
            *SectorID.ToString(),
            EnemyCount,
            SpawnedCount
        );
    }
}

void ABHOpenWorldOperationDirector::SpawnFriendlySupport(
    int32 FriendlyCount
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !EnemyClass ||
        FriendlyCount <= 0)
    {
        return;
    }

    int32 SpawnedCount = 0;

    for (int32 Index = 0;
        Index < FriendlyCount;
        ++Index)
    {
        bool bSpawned = false;

        for (int32 AttemptIndex = 0;
             AttemptIndex < MaximumOperationSpawnAttempts;
             ++AttemptIndex)
        {
            const FTransform SpawnTransform =
                BuildFriendlySpawnTransform(
                    Index,
                    AttemptIndex
                );
            ABHEnemySoldier* FriendlySoldier =
                World->SpawnActorDeferred<ABHEnemySoldier>(
                    EnemyClass,
                    SpawnTransform,
                    this,
                    nullptr,
                    ESpawnActorCollisionHandlingMethod::
                        AdjustIfPossibleButDontSpawnIfColliding
                );

            if (!IsValid(FriendlySoldier))
            {
                continue;
            }

            FriendlySoldier->SetFlags(RF_Transient);
            FriendlySoldier->SetCombatFaction(
                EBHCombatFaction::Friendly
            );
            FriendlySoldier->SetCombatantArchetype(
                ABHEnemySoldier::ChooseFormationArchetype(
                    Index,
                    FriendlyCount
                )
            );
            FriendlySoldier->SetObjectiveIdToCompleteOnDeath(
                NAME_None
            );
            FriendlySoldier->SetPatrolPoints(
                BuildPatrolPointAssignment(Index)
            );
            UGameplayStatics::FinishSpawningActor(
                FriendlySoldier,
                SpawnTransform
            );

            if (!IsValid(FriendlySoldier))
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "BH_OPERATION_UNIT_REJECTED sector=%s "
                        "side=friendly reason=blocked_spawn"
                    ),
                    *SectorID.ToString()
                );
                continue;
            }

            ABHEnemyAIController* FriendlyController =
                Cast<ABHEnemyAIController>(
                    FriendlySoldier->GetController()
                );

            if (!BHWarOperationRules::
                    IsOperationCombatantReady(
                        !FriendlySoldier->IsDead(),
                        IsValid(FriendlyController)
                    ))
            {
                FriendlySoldier->SpawnDefaultController();
                FriendlyController = Cast<ABHEnemyAIController>(
                    FriendlySoldier->GetController()
                );
            }

            if (!BHWarOperationRules::
                    IsOperationCombatantReady(
                        !FriendlySoldier->IsDead(),
                        IsValid(FriendlyController)
                    ))
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT(
                        "BH_OPERATION_UNIT_REJECTED sector=%s "
                        "side=friendly reason=no_controller"
                    ),
                    *SectorID.ToString()
                );
                FriendlySoldier->Destroy();
                break;
            }

            FriendlyController->SetFollowTarget(
                PlayerCharacter,
                BHWarOperationRules::
                    CalculateFriendlyFormationOffset(Index)
            );

            if (UBHHealthComponent* HealthComponent =
                FriendlySoldier->GetHealthComponent())
            {
                HealthComponent->OnDeath.AddDynamic(
                    this,
                    &ABHOpenWorldOperationDirector::
                        HandleFriendlySupportDeath
                );
            }

            TrackedAllies.Add(FriendlySoldier);
            ++SpawnedCount;
            bSpawned = true;

            if (AttemptIndex > 0)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_OPERATION_UNIT_REPOSITIONED sector=%s "
                        "side=friendly unit=%d attempt=%d"
                    ),
                    *SectorID.ToString(),
                    Index,
                    AttemptIndex + 1
                );
            }

            break;
        }

        if (!bSpawned)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "BH_OPERATION_UNIT_SPAWN_EXHAUSTED sector=%s "
                    "side=friendly unit=%d attempts=%d"
                ),
                *SectorID.ToString(),
                Index,
                MaximumOperationSpawnAttempts
            );
        }
    }

    if (SpawnedCount < FriendlyCount)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_OPERATION_FORCE_SHORTFALL sector=%s "
                "side=friendly requested=%d spawned=%d"
            ),
            *SectorID.ToString(),
            FriendlyCount,
            SpawnedCount
        );
    }
}

void ABHOpenWorldOperationDirector::ApplyFriendlySupportOrder()
{
    int32 FormationIndex = 0;

    for (ABHEnemySoldier* Ally : TrackedAllies)
    {
        if (!IsValid(Ally) || Ally->IsDead())
        {
            continue;
        }

        ABHEnemyAIController* Controller =
            Cast<ABHEnemyAIController>(Ally->GetController());

        if (!IsValid(Controller))
        {
            continue;
        }

        if (IsValid(PlayerCharacter))
        {
            Controller->OnHoldMoveFailed.RemoveAll(PlayerCharacter);
            Controller->OnHoldMoveFailed.AddUObject(
                PlayerCharacter,
                &ABHCharacter::NotifySquadMoveAndHoldFailure
            );
        }

        if (bFriendlySupportHolding)
        {
            FVector HoldLocation = Ally->GetActorLocation();

            if (bFriendlySupportHasCommandLocation)
            {
                const FVector FormationOffset =
                    BHWarOperationRules::
                        CalculateFriendlyFormationOffset(
                            FormationIndex
                        );
                const FRotator CommandRotation(
                    0.0f,
                    FriendlySupportCommandYaw,
                    0.0f
                );
                HoldLocation =
                    FriendlySupportCommandLocation +
                    (CommandRotation.Vector() *
                     FormationOffset.X) +
                    (
                        FRotationMatrix(CommandRotation)
                            .GetUnitAxis(EAxis::Y) *
                        FormationOffset.Y
                    );
            }

            if (bFriendlySupportHasCommandLocation)
            {
                Controller->SetMoveAndHoldPosition(
                    HoldLocation,
                    FriendlySupportCommandYaw
                );
            }
            else
            {
                Controller->SetHoldPosition(HoldLocation);
            }
        }
        else
        {
            Controller->SetFollowTarget(
                PlayerCharacter,
                BHWarOperationRules::
                    CalculateFriendlyFormationOffset(
                        FormationIndex
                    )
            );
        }

        ++FormationIndex;
    }
}

void ABHOpenWorldOperationDirector::
    HandleFriendlySupportDeath(AActor* DamageCauser)
{
    ++FriendlySupportCasualties;

    const FName AttritionSectorID =
        SupplySourceSectorID.IsNone()
            ? SectorID
            : SupplySourceSectorID;
    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const bool bAttritionApplied =
        IsValid(WarSubsystem) &&
        WarSubsystem->ApplyAmbientBattleResult(
            AttritionSectorID,
            1,
            0
        );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_SUPPORT_CASUALTY target=%s "
            "staging=%s casualties=%d applied=%d causer=%s"
        ),
        *SectorID.ToString(),
        *AttritionSectorID.ToString(),
        FriendlySupportCasualties,
        bAttritionApplied ? 1 : 0,
        *GetNameSafe(DamageCauser)
    );
    RequestOperationCheckpoint(CasualtyCheckpointDelay);
}

void ABHOpenWorldOperationDirector::HandleEnemyDeath(
    AActor* DamageCauser
)
{
    ++EnemyCasualties;

    UGameInstance* GameInstance = GetGameInstance();
    UBHWarSubsystem* WarSubsystem = IsValid(GameInstance)
        ? GameInstance->GetSubsystem<UBHWarSubsystem>()
        : nullptr;
    const bool bAttritionApplied =
        IsValid(WarSubsystem) &&
        WarSubsystem->ApplyAmbientBattleResult(
            EnemySourceSectorID.IsNone()
                ? SectorID
                : EnemySourceSectorID,
            0,
            1
        );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_HOSTILE_CASUALTY target=%s source=%s "
            "casualties=%d applied=%d causer=%s"
        ),
        *SectorID.ToString(),
        *(EnemySourceSectorID.IsNone()
            ? SectorID
            : EnemySourceSectorID).ToString(),
        EnemyCasualties,
        bAttritionApplied ? 1 : 0,
        *GetNameSafe(DamageCauser)
    );
    RequestOperationCheckpoint(CasualtyCheckpointDelay);
}

void ABHOpenWorldOperationDirector::
    ReportFriendlySupportCasualties()
{
    if (FriendlySupportCasualties <= 0)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_SUPPORT_SUMMARY target=%s "
            "staging=%s casualties=%d"
        ),
        *SectorID.ToString(),
        *SupplySourceSectorID.ToString(),
        FriendlySupportCasualties
    );
}

void ABHOpenWorldOperationDirector::CompleteOperation()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!IsValid(PlayerCharacter))
    {
        PlayerCharacter = FindLivingPlayerParticipant();
    }

    if (!IsValid(PlayerCharacter) ||
        !PlayerCharacter->AdoptSharedWarOperationAuthority(
            this,
            SectorID,
            SupplySourceSectorID,
            OperationType
        ))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_OPERATION_COMPLETION_REJECTED_NO_COMMAND_PLAYER "
                "sector=%s"
            ),
            *SectorID.ToString()
        );
        return;
    }

    bOperationComplete = true;
    SetActorTickEnabled(false);
    ReportFriendlySupportCasualties();
    PublishOperationSnapshot(
        EBHActiveOperationPhase::DebriefSuccess
    );

    if (OperationType == EBHWarPriorityType::Raid)
    {
        DestroyTrackedUnits();
    }

    if (!PlayerCharacter->CompleteObjective(
            BHObjectiveIds::EliminateGuard
        ))
    {
        bOperationComplete = false;
        SetActorTickEnabled(true);
        PublishCurrentOperationSnapshot();
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_OPERATION_COMPLETION_OBJECTIVE_REJECTED "
                "sector=%s player=%s"
            ),
            *SectorID.ToString(),
            *PlayerCharacter->GetName()
        );
        return;
    }

    const FText DebriefMessage =
        PlayerCharacter->GetMissionCompleteMessage();

    for (ABHCharacter* Participant : GetPlayerParticipants())
    {
        if (IsValid(Participant))
        {
            Participant->PresentSharedOperationDebrief(
                DebriefMessage
            );
        }
    }
}

#if !UE_BUILD_SHIPPING
void ABHOpenWorldOperationDirector::CompleteOperationForTesting()
{
    CompleteOperation();
}
#endif

void ABHOpenWorldOperationDirector::FailOperation(
    const FText& FailureReason
)
{
    if (!HasAuthority() || bOperationComplete)
    {
        return;
    }

    if (!IsValid(PlayerCharacter))
    {
        PlayerCharacter = FindLivingPlayerParticipant();
    }

    if (!IsValid(PlayerCharacter) ||
        !PlayerCharacter->AdoptSharedWarOperationAuthority(
            this,
            SectorID,
            SupplySourceSectorID,
            OperationType
        ))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_OPERATION_FAILURE_REJECTED_NO_COMMAND_PLAYER "
                "sector=%s"
            ),
            *SectorID.ToString()
        );
        return;
    }

    bOperationComplete = true;
    SetActorTickEnabled(false);
    ReportFriendlySupportCasualties();
    PublishOperationSnapshot(
        EBHActiveOperationPhase::DebriefFailure
    );

    if (!PlayerCharacter->FailCurrentWarOperation(FailureReason))
    {
        bOperationComplete = false;
        SetActorTickEnabled(true);
        PublishCurrentOperationSnapshot();

        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "BH_OPERATION_FAILURE_REJECTED sector=%s type=%d"
            ),
            *SectorID.ToString(),
            static_cast<int32>(OperationType)
        );
        return;
    }

    const FText DebriefMessage =
        PlayerCharacter->GetMissionCompleteMessage();

    for (ABHCharacter* Participant : GetPlayerParticipants())
    {
        if (IsValid(Participant))
        {
            Participant->PresentSharedOperationDebrief(
                DebriefMessage
            );
        }
    }

    DestroyTrackedUnits();
    DestroyRaidSabotageTarget();
}

void ABHOpenWorldOperationDirector::HandleRaidTargetSabotaged(
    ABHRaidSabotageTarget* SabotagedTarget
)
{
    if (!HasAuthority() ||
        bOperationComplete ||
        !bOperationActivated ||
        OperationType != EBHWarPriorityType::Raid ||
        !IsValid(SabotagedTarget) ||
        SabotagedTarget != RaidSabotageTarget ||
        !SabotagedTarget->IsSabotaged())
    {
        return;
    }

    UpdateRaidDetectionState();
    bRaidTargetSabotaged = true;
    const int32 ReactionForceCount =
        BHWarOperationRules::CalculateRaidReactionForceCount(
            EffectiveAttackReinforcementWaveCount,
            EffectiveAttackReinforcementsPerWave,
            bRaidDetectedBeforeSabotage
        );
    CurrentWave =
        1 + EffectiveAttackReinforcementWaveCount;

    if (ReactionForceCount > 0)
    {
        SpawnEnemies(ReactionForceCount);
    }

    AlertRaidReactionForce();
    RequestOperationCheckpoint();

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldRaidExfiltrationStarted",
                    "CHARGES ARMED // {0}\n\n"
                    "Enemy reaction force: {2}. Break contact and "
                    "move {1} meters away from the cache."
                ),
                GetSectorDisplayName(),
                FText::AsNumber(
                    FMath::RoundToInt(
                        RaidExfiltrationRadius / 100.0f
                    )
                ),
                FText::AsNumber(ReactionForceCount)
            )
        );
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_RAID_EXFILTRATION_STARTED sector=%s "
            "reaction_force=%d detected_before_sabotage=%d "
            "hostiles_remaining=%d"
        ),
        *SectorID.ToString(),
        ReactionForceCount,
        bRaidDetectedBeforeSabotage ? 1 : 0,
        GetLivingEnemyCount()
    );

}

void ABHOpenWorldOperationDirector::SpawnRaidSabotageTarget()
{
    if (OperationType != EBHWarPriorityType::Raid ||
        !bOperationActivated ||
        bOperationComplete ||
        IsValid(RaidSabotageTarget))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    RaidSabotageTarget =
        World->SpawnActor<ABHRaidSabotageTarget>(
            ABHRaidSabotageTarget::StaticClass(),
            FTransform(
                FRotator::ZeroRotator,
                OperationCenter + FVector(0.0f, 0.0f, 10.0f)
            ),
            SpawnParameters
        );

    if (!IsValid(RaidSabotageTarget))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("BH_RAID_TARGET_SPAWN_FAILED sector=%s"),
            *SectorID.ToString()
        );
        return;
    }

    RaidSabotageTarget->ConfigureTarget(this, SectorID);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_RAID_TARGET_SPAWNED sector=%s location=%s"),
        *SectorID.ToString(),
        *RaidSabotageTarget->GetActorLocation().ToCompactString()
    );
}

void ABHOpenWorldOperationDirector::DestroyRaidSabotageTarget()
{
    if (IsValid(RaidSabotageTarget))
    {
        RaidSabotageTarget->Destroy();
    }

    RaidSabotageTarget = nullptr;
}

void ABHOpenWorldOperationDirector::UpdateRaidDetectionState()
{
    if (bRaidDetectedBeforeSabotage ||
        bRaidTargetSabotaged ||
        OperationType != EBHWarPriorityType::Raid)
    {
        return;
    }

    for (ABHEnemySoldier* Enemy : TrackedEnemies)
    {
        if (!IsValid(Enemy) || Enemy->IsDead())
        {
            continue;
        }

        const ABHEnemyAIController* EnemyController =
            Cast<ABHEnemyAIController>(Enemy->GetController());
        if (!IsValid(EnemyController))
        {
            continue;
        }

        const EBHEnemyAIState EnemyState =
            EnemyController->GetCurrentState();
        const bool bEnemyAlerted =
            EnemyState != EBHEnemyAIState::Patrol &&
            EnemyState != EBHEnemyAIState::Dead;
        if (!bEnemyAlerted)
        {
            continue;
        }

        bRaidDetectedBeforeSabotage = true;
        RequestOperationCheckpoint();

        if (IsValid(PlayerCharacter))
        {
            PlayerCharacter->ShowStatusNotification(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldRaidCompromised",
                    "RAID COMPROMISED // ENEMY ALERTED\n\n"
                    "Clean signature lost. Complete sabotage "
                    "and exfiltrate."
                )
            );
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_RAID_DETECTED_BEFORE_SABOTAGE "
                "sector=%s enemy=%s state=%d"
            ),
            *SectorID.ToString(),
            *Enemy->GetName(),
            static_cast<int32>(EnemyState)
        );
        return;
    }
}

void ABHOpenWorldOperationDirector::UpdateEnemyRouts()
{
    if (OperationType == EBHWarPriorityType::Raid ||
        bOperationComplete)
    {
        return;
    }

    int32 NewlyRoutedCount = 0;

    for (int32 Index = TrackedEnemies.Num() - 1;
         Index >= 0;
         --Index)
    {
        ABHEnemySoldier* Enemy = TrackedEnemies[Index];
        if (!IsValid(Enemy) || Enemy->IsDead())
        {
            continue;
        }

        ABHEnemyAIController* EnemyController =
            Cast<ABHEnemyAIController>(Enemy->GetController());
        bool bControllerRecoveryFailed = false;

        if (!BHWarOperationRules::IsOperationCombatantReady(
                true,
                IsValid(EnemyController)))
        {
            Enemy->SpawnDefaultController();
            EnemyController = Cast<ABHEnemyAIController>(
                Enemy->GetController()
            );

            if (BHWarOperationRules::IsOperationCombatantReady(
                    true,
                    IsValid(EnemyController)))
            {
                EnemyController->NotifyAllyAlert(PlayerCharacter);
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "BH_OPERATION_UNIT_CONTROLLER_RECOVERED "
                        "sector=%s enemy=%s"
                    ),
                    *SectorID.ToString(),
                    *Enemy->GetName()
                );
            }
            else
            {
                bControllerRecoveryFailed = true;
            }
        }

        const bool bIsRetreating =
            IsValid(EnemyController) &&
            EnemyController->GetCurrentState() ==
                EBHEnemyAIState::Retreat;
        const float DistanceFromObjective = FVector::Dist2D(
            Enemy->GetActorLocation(),
            OperationCenter
        );

        if (!bControllerRecoveryFailed &&
            !BHWarOperationRules::IsEnemyRoutedFromOperation(
                bIsRetreating,
                DistanceFromObjective,
                EnemyRoutWithdrawalDistance
            ))
        {
            continue;
        }

        if (UBHHealthComponent* HealthComponent =
                Enemy->GetHealthComponent())
        {
            HealthComponent->OnDeath.RemoveDynamic(
                this,
                &ABHOpenWorldOperationDirector::HandleEnemyDeath
            );
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_OPERATION_ENEMY_ROUTED sector=%s enemy=%s "
                "distance=%.1f reason=%s"
            ),
            *SectorID.ToString(),
            *Enemy->GetName(),
            DistanceFromObjective,
            bControllerRecoveryFailed
                ? TEXT("controller_unavailable")
                : TEXT("withdrawn")
        );

        TrackedEnemies.RemoveAtSwap(Index, 1, EAllowShrinking::No);
        ++EnemyRoutedCount;
        ++NewlyRoutedCount;

        if (IsValid(EnemyController))
        {
            EnemyController->UnPossess();
        }

        Enemy->Destroy();

        if (IsValid(EnemyController))
        {
            EnemyController->Destroy();
        }
    }

    if (NewlyRoutedCount <= 0)
    {
        return;
    }

    RequestOperationCheckpoint();

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldEnemyRouted",
                    "ENEMY WITHDRAWAL // {0} ROUTED\n\n"
                    "Broken hostiles have abandoned the engagement."
                ),
                FText::AsNumber(NewlyRoutedCount)
            )
        );
    }
}

void ABHOpenWorldOperationDirector::AlertRaidReactionForce()
{
    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    int32 AlertedEnemyCount = 0;

    for (ABHEnemySoldier* Enemy : TrackedEnemies)
    {
        if (!IsValid(Enemy) || Enemy->IsDead())
        {
            continue;
        }

        ABHEnemyAIController* EnemyController =
            Cast<ABHEnemyAIController>(Enemy->GetController());
        if (!IsValid(EnemyController))
        {
            continue;
        }

        EnemyController->NotifyAllyAlert(PlayerCharacter);
        ++AlertedEnemyCount;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_RAID_REACTION_FORCE_ALERTED sector=%s count=%d"
        ),
        *SectorID.ToString(),
        AlertedEnemyCount
    );
}

void ABHOpenWorldOperationDirector::
    UpdateAttackObjectiveSecuring(float DeltaSeconds)
{
    if (OperationType == EBHWarPriorityType::Defend ||
        !IsValid(PlayerCharacter))
    {
        return;
    }

    if (!bSecuringObjective)
    {
        bSecuringObjective = true;
        ObjectiveSecureProgress = 0.0f;

        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                OperationType == EBHWarPriorityType::Raid
                    ? NSLOCTEXT(
                        "BrokenHorizon",
                        "OpenWorldRaidDisruptObjective",
                        "REACTION FORCE DEFEATED\n\n"
                        "Enter the objective and finish disrupting "
                        "enemy logistics at {0}."
                    )
                    : NSLOCTEXT(
                        "BrokenHorizon",
                        "OpenWorldAttackSecureObjective",
                        "HOSTILE FORCE BROKEN\n\n"
                        "{1} hostiles routed. Enter and secure "
                        "the objective at {0}."
                    ),
                GetSectorDisplayName(),
                FText::AsNumber(EnemyRoutedCount)
            )
        );
        RequestOperationCheckpoint();

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_OPERATION_SECURING_STARTED sector=%s "
                "radius=%.0f duration=%.1f"
            ),
            *SectorID.ToString(),
            ObjectiveSecureRadius,
            ObjectiveSecureDuration
        );
    }

    const float SafeDeltaSeconds =
        FMath::Max(0.0f, DeltaSeconds);
    const float SafeSecureDuration =
        FMath::Max(0.1f, ObjectiveSecureDuration);

    const bool bPlayerInsideSecureArea =
        HasPlayerOrFieldOperativeInSecureArea();
    const bool bSecureAreaContested =
        IsSecureAreaContested();

    if (bPlayerInsideSecureArea && !bSecureAreaContested)
    {
        ObjectiveSecureProgress = FMath::Min(
            SafeSecureDuration,
            ObjectiveSecureProgress + SafeDeltaSeconds
        );
    }
    else if (!bPlayerInsideSecureArea)
    {
        ObjectiveSecureProgress = FMath::Max(
            0.0f,
            ObjectiveSecureProgress -
                (
                    SafeDeltaSeconds *
                    FMath::Max(
                        0.0f,
                        ObjectiveSecureDecayMultiplier
                    )
                )
        );
    }

    if (ObjectiveSecureProgress < SafeSecureDuration)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_OBJECTIVE_SECURED sector=%s "
            "duration=%.1f"
        ),
        *SectorID.ToString(),
        ObjectiveSecureProgress
    );
    CompleteOperation();
}

void ABHOpenWorldOperationDirector::
    UpdateDefenseObjectiveHolding(float DeltaSeconds)
{
    if (OperationType != EBHWarPriorityType::Defend ||
        !IsValid(PlayerCharacter))
    {
        return;
    }

    if (!bSecuringObjective)
    {
        bSecuringObjective = true;
        ObjectiveSecureProgress = 0.0f;

        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldDefenseConfirmHold",
                    "FINAL ASSAULT REPELLED\n\n"
                    "Return to the defensive line at {0} and "
                    "hold the sector."
                ),
                GetSectorDisplayName()
            )
        );
        RequestOperationCheckpoint();

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_OPERATION_DEFENSE_HOLD_STARTED sector=%s "
                "radius=%.0f duration=%.1f"
            ),
            *SectorID.ToString(),
            ObjectiveSecureRadius,
            ObjectiveSecureDuration
        );
    }

    const float SafeDeltaSeconds =
        FMath::Max(0.0f, DeltaSeconds);
    const float SafeSecureDuration =
        FMath::Max(0.1f, ObjectiveSecureDuration);
    const bool bPlayerInsideSecureArea =
        HasPlayerOrFieldOperativeInSecureArea();
    const bool bSecureAreaContested =
        IsSecureAreaContested();

    if (bPlayerInsideSecureArea && !bSecureAreaContested)
    {
        ObjectiveSecureProgress = FMath::Min(
            SafeSecureDuration,
            ObjectiveSecureProgress + SafeDeltaSeconds
        );
    }
    else if (!bPlayerInsideSecureArea)
    {
        ObjectiveSecureProgress = FMath::Max(
            0.0f,
            ObjectiveSecureProgress -
                (
                    SafeDeltaSeconds *
                    FMath::Max(
                        0.0f,
                        ObjectiveSecureDecayMultiplier
                    )
                )
        );
    }

    if (ObjectiveSecureProgress < SafeSecureDuration)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_DEFENSE_HOLD_CONFIRMED sector=%s "
            "duration=%.1f"
        ),
        *SectorID.ToString(),
        ObjectiveSecureProgress
    );
    CompleteOperation();
}

void ABHOpenWorldOperationDirector::UpdateDefenseBreach(
    float DeltaSeconds
)
{
    if (OperationType != EBHWarPriorityType::Defend ||
        !bOperationActivated ||
        bOperationComplete)
    {
        return;
    }

    const float PreviousProgress = DefenseBreachProgress;
    const float SafeDeltaSeconds =
        FMath::Max(0.0f, DeltaSeconds);
    const float SafeBreachDuration =
        FMath::Max(1.0f, DefenseBreachDuration);
    const bool bObjectiveOverrun =
        IsSecureAreaContested() &&
        !HasLivingDefenderInSecureArea();

    if (bObjectiveOverrun)
    {
        DefenseBreachProgress = FMath::Min(
            SafeBreachDuration,
            DefenseBreachProgress + SafeDeltaSeconds
        );
    }
    else
    {
        DefenseBreachProgress = FMath::Max(
            0.0f,
            DefenseBreachProgress -
                (
                    SafeDeltaSeconds *
                    FMath::Max(
                        0.0f,
                        DefenseBreachRecoveryMultiplier
                    )
                )
        );
    }

    const float CheckpointInterval = FMath::Max(
        1.0f,
        DefenseBreachCheckpointInterval
    );

    if (FMath::Abs(
            DefenseBreachProgress -
            LastCheckpointedDefenseBreachProgress
        ) >= CheckpointInterval)
    {
        LastCheckpointedDefenseBreachProgress =
            DefenseBreachProgress;
        RequestOperationCheckpoint();
    }

    if (PreviousProgress <= KINDA_SMALL_NUMBER &&
        DefenseBreachProgress > KINDA_SMALL_NUMBER)
    {
        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldDefenseBreachWarning",
                    "DEFENSIVE LINE BREACHED // {0}\n\n"
                    "Hostiles control the objective. Retake it "
                    "within {1} seconds or the sector will fall."
                ),
                GetSectorDisplayName(),
                FText::AsNumber(
                    FMath::CeilToInt(SafeBreachDuration)
                )
            )
        );
        LastCheckpointedDefenseBreachProgress =
            DefenseBreachProgress;
        RequestOperationCheckpoint();

        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_OPERATION_DEFENSE_BREACH_STARTED "
                "sector=%s duration=%.1f"
            ),
            *SectorID.ToString(),
            SafeBreachDuration
        );
    }
    else if (PreviousProgress > KINDA_SMALL_NUMBER &&
        DefenseBreachProgress <= KINDA_SMALL_NUMBER)
    {
        PlayerCharacter->ShowStatusNotification(
            FText::Format(
                NSLOCTEXT(
                    "BrokenHorizon",
                    "OpenWorldDefenseBreachRecovered",
                    "DEFENSIVE LINE STABILIZED // {0}\n\n"
                    "Friendly forces have regained the objective."
                ),
                GetSectorDisplayName()
            )
        );
        LastCheckpointedDefenseBreachProgress = 0.0f;
        RequestOperationCheckpoint();

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_OPERATION_DEFENSE_BREACH_RECOVERED "
                "sector=%s"
            ),
            *SectorID.ToString()
        );
    }

    if (DefenseBreachProgress < SafeBreachDuration)
    {
        return;
    }

    FailOperation(
        NSLOCTEXT(
            "BrokenHorizon",
            "OpenWorldDefenseBreachFailureReason",
            "Hostile forces overran the defensive objective."
        )
    );
}

bool ABHOpenWorldOperationDirector::
    IsPlayerInsideSecureArea() const
{
    const float SecureRadiusSquared = FMath::Square(
        FMath::Max(100.0f, ObjectiveSecureRadius)
    );

    for (const ABHCharacter* Participant :
        GetLivingPlayerParticipants())
    {
        if (IsValid(Participant) &&
            FVector::DistSquared2D(
                Participant->GetActorLocation(),
                OperationCenter
            ) <= SecureRadiusSquared)
        {
            return true;
        }
    }

    return false;
}

TArray<ABHCharacter*>
ABHOpenWorldOperationDirector::
    GetPlayerParticipants() const
{
    TArray<ABHCharacter*> Participants;
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return Participants;
    }

    for (TActorIterator<ABHCharacter> It(World); It; ++It)
    {
        ABHCharacter* Candidate = *It;

        if (!IsValid(Candidate) ||
            (
                Candidate != PlayerCharacter &&
                !Candidate->IsPlayerControlled() &&
                Candidate->GetPlayerState() == nullptr
            ))
        {
            continue;
        }

        Participants.Add(Candidate);
    }

    return Participants;
}

TArray<ABHCharacter*>
ABHOpenWorldOperationDirector::
    GetLivingPlayerParticipants() const
{
    TArray<ABHCharacter*> LivingParticipants;

    for (ABHCharacter* Candidate : GetPlayerParticipants())
    {
        const UBHHealthComponent* Health =
            IsValid(Candidate)
                ? Candidate->GetHealthComponent()
                : nullptr;

        if (IsValid(Candidate) &&
            (!IsValid(Health) || !Health->IsDead()))
        {
            LivingParticipants.Add(Candidate);
        }
    }

    return LivingParticipants;
}

ABHCharacter*
ABHOpenWorldOperationDirector::
    FindLivingPlayerParticipant() const
{
    const TArray<ABHCharacter*> Participants =
        GetLivingPlayerParticipants();
    return Participants.IsEmpty() ? nullptr : Participants[0];
}

float ABHOpenWorldOperationDirector::
    GetClosestParticipantDistanceToOperation() const
{
    float ClosestDistance = TNumericLimits<float>::Max();

    for (const ABHCharacter* Participant :
        GetLivingPlayerParticipants())
    {
        if (IsValid(Participant))
        {
            ClosestDistance = FMath::Min(
                ClosestDistance,
                FVector::Dist2D(
                    Participant->GetActorLocation(),
                    OperationCenter
                )
            );
        }
    }

    return ClosestDistance;
}

bool ABHOpenWorldOperationDirector::
    AreAllLivingParticipantsOutsideRadius(float Radius) const
{
    const TArray<ABHCharacter*> Participants =
        GetLivingPlayerParticipants();

    if (Participants.IsEmpty())
    {
        return false;
    }

    const float RadiusSquared = FMath::Square(
        FMath::Max(0.0f, Radius)
    );

    for (const ABHCharacter* Participant : Participants)
    {
        if (IsValid(Participant) &&
            FVector::DistSquared2D(
                Participant->GetActorLocation(),
                OperationCenter
            ) < RadiusSquared)
        {
            return false;
        }
    }

    return true;
}

bool ABHOpenWorldOperationDirector::
    IsSecureAreaContested() const
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    const float SecureRadiusSquared = FMath::Square(
        FMath::Max(100.0f, ObjectiveSecureRadius)
    );

    for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
    {
        const ABHEnemySoldier* Soldier = *It;

        if (!IsValid(Soldier) ||
            Soldier->IsDead() ||
            Soldier->IsOutOfAmmunition() ||
            Soldier->GetCombatFaction() !=
                EBHCombatFaction::Hostile)
        {
            continue;
        }

        if (FVector::DistSquared2D(
                Soldier->GetActorLocation(),
                OperationCenter
            ) <= SecureRadiusSquared)
        {
            return true;
        }
    }

    return false;
}

bool ABHOpenWorldOperationDirector::
    HasPlayerOrFieldOperativeInSecureArea() const
{
    return IsPlayerInsideSecureArea() ||
        HasLivingFieldOperativeInSecureArea();
}

bool ABHOpenWorldOperationDirector::
    HasLivingFieldOperativeInSecureArea() const
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    const float SecureRadiusSquared = FMath::Square(
        FMath::Max(100.0f, ObjectiveSecureRadius)
    );

    for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
    {
        const ABHEnemySoldier* Soldier = *It;
        const ABHCharacter* FieldCommander =
            IsValid(Soldier)
                ? Cast<ABHCharacter>(Soldier->GetOwner())
                : nullptr;

        if (!IsValid(Soldier) ||
            !IsValid(FieldCommander) ||
            Soldier->IsDead() ||
            Soldier->IsIncapacitated() ||
            Soldier->GetCombatFaction() !=
                EBHCombatFaction::Friendly)
        {
            continue;
        }

        if (FVector::DistSquared2D(
                Soldier->GetActorLocation(),
                OperationCenter
            ) <= SecureRadiusSquared)
        {
            return true;
        }
    }

    return false;
}

bool ABHOpenWorldOperationDirector::
    HasLivingDefenderInSecureArea() const
{
    if (IsPlayerInsideSecureArea())
    {
        return true;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    const float SecureRadiusSquared = FMath::Square(
        FMath::Max(100.0f, ObjectiveSecureRadius)
    );

    for (TActorIterator<ABHEnemySoldier> It(World); It; ++It)
    {
        const ABHEnemySoldier* Soldier = *It;

        if (!IsValid(Soldier) ||
            Soldier->IsDead() ||
            Soldier->IsOutOfAmmunition() ||
            Soldier->GetCombatFaction() !=
                EBHCombatFaction::Friendly)
        {
            continue;
        }

        if (FVector::DistSquared2D(
                Soldier->GetActorLocation(),
                OperationCenter
            ) <= SecureRadiusSquared)
        {
            return true;
        }
    }

    return false;
}

void ABHOpenWorldOperationDirector::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (HasAuthority())
    {
        DestroyTrackedUnits();
        DestroyOperationPatrolPoints();
        DestroyRaidSabotageTarget();
    }
    Super::EndPlay(EndPlayReason);
}

bool ABHOpenWorldOperationDirector::HasLivingEnemies() const
{
    return GetLivingEnemyCount() > 0;
}

int32 ABHOpenWorldOperationDirector::GetLivingEnemyCount() const
{
    int32 LivingEnemyCount = 0;

    for (const ABHEnemySoldier* Enemy : TrackedEnemies)
    {
        if (IsValid(Enemy) &&
            !Enemy->IsDead() &&
            Enemy->HasCombatAmmunition())
        {
            ++LivingEnemyCount;
        }
    }

    return LivingEnemyCount;
}

int32 ABHOpenWorldOperationDirector::GetLivingAllyCount() const
{
    int32 LivingAllyCount = 0;

    for (const ABHEnemySoldier* Ally : TrackedAllies)
    {
        if (IsValid(Ally) &&
            !Ally->IsDead() &&
            Ally->HasCombatAmmunition())
        {
            ++LivingAllyCount;
        }
    }

    return LivingAllyCount;
}

int32 ABHOpenWorldOperationDirector::
    GetSecondsUntilNextWave() const
{
    const UWorld* World = GetWorld();

    if (!bWaitingForWave || !IsValid(World))
    {
        return 0;
    }

    return FMath::Max(
        0,
        FMath::CeilToInt(
            NextWaveTime - World->GetTimeSeconds()
        )
    );
}

TArray<ABHPatrolPoint*>
ABHOpenWorldOperationDirector::BuildPatrolPointAssignment(
    int32 UnitIndex
) const
{
    TArray<ABHPatrolPoint*> Assignment;
    const int32 PointCount = OperationPatrolPoints.Num();

    if (PointCount <= 0)
    {
        return Assignment;
    }

    Assignment.Reserve(PointCount);
    const int32 StartIndex =
        FMath::Abs(UnitIndex) % PointCount;

    for (int32 Offset = 0; Offset < PointCount; ++Offset)
    {
        ABHPatrolPoint* PatrolPoint =
            OperationPatrolPoints[
                (StartIndex + Offset) % PointCount
            ];

        if (IsValid(PatrolPoint))
        {
            Assignment.Add(PatrolPoint);
        }
    }

    return Assignment;
}

FTransform ABHOpenWorldOperationDirector::BuildSpawnTransform(
    int32 EnemyIndex,
    int32 AttemptIndex
) const
{
    const int32 Count =
        OperationType == EBHWarPriorityType::Defend
            ? EffectiveDefenseEnemiesPerWave
            : CurrentWave <= 1
                ? EffectiveAttackEnemyCount
                : EffectiveAttackReinforcementsPerWave;
    FTransform SpawnTransform;

    if (IsValid(SectorAnchor))
    {
        SpawnTransform = SectorAnchor->BuildEnemySpawnTransform(
            EnemyIndex,
            Count,
            CurrentWave
        );
    }
    else
    {
        const float Angle =
            (360.0f / FMath::Max(1, Count)) * EnemyIndex +
            (CurrentWave * 47.0f);
        const FVector Direction(
            FMath::Cos(FMath::DegreesToRadians(Angle)),
            FMath::Sin(FMath::DegreesToRadians(Angle)),
            0.0f
        );

        SpawnTransform = FTransform(
            (-Direction).Rotation(),
            OperationCenter + (Direction * SpawnRadius)
        );
    }

    const int32 SafeAttemptIndex =
        FMath::Max(0, AttemptIndex);

    if (SafeAttemptIndex > 0)
    {
        FVector SpawnOffset =
            SpawnTransform.GetLocation() - OperationCenter;
        float SpawnDistance = SpawnOffset.Size2D();

        if (SpawnDistance <= KINDA_SMALL_NUMBER)
        {
            SpawnOffset = FVector::ForwardVector;
            SpawnDistance = FMath::Max(500.0f, SpawnRadius);
        }

        const int32 AttemptPair =
            (SafeAttemptIndex + 1) / 2;
        const float AttemptSign =
            (SafeAttemptIndex % 2) == 1
                ? 1.0f
                : -1.0f;
        const FVector AlternateDirection =
            SpawnOffset.GetSafeNormal2D().RotateAngleAxis(
                AttemptSign *
                    AlternateSpawnAngleStep *
                    AttemptPair,
                FVector::UpVector
            );
        const float AlternateDistance = FMath::Max(
            500.0f,
            SpawnDistance +
                (
                    AttemptSign *
                    AlternateSpawnRadiusStep *
                    AttemptPair
                )
        );
        const FVector AlternateLocation =
            OperationCenter +
            (AlternateDirection * AlternateDistance);
        SpawnTransform.SetLocation(AlternateLocation);
        SpawnTransform.SetRotation(
            (OperationCenter - AlternateLocation)
                .Rotation()
                .Quaternion()
        );
    }

    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );
    FNavLocation ProjectedLocation;

    if (IsValid(NavigationSystem) &&
        NavigationSystem->ProjectPointToNavigation(
            SpawnTransform.GetLocation(),
            ProjectedLocation,
            FVector(3000.0f, 3000.0f, 20000.0f)))
    {
        SpawnTransform.SetLocation(ProjectedLocation.Location);
        const FVector FacingDirection =
            (OperationCenter - ProjectedLocation.Location)
                .GetSafeNormal2D();

        if (!FacingDirection.IsNearlyZero())
        {
            SpawnTransform.SetRotation(
                FacingDirection.Rotation().Quaternion()
            );
        }
    }

    return SpawnTransform;
}

FTransform
ABHOpenWorldOperationDirector::BuildFriendlySpawnTransform(
    int32 FriendlyIndex,
    int32 AttemptIndex
) const
{
    FVector InsertionDirection = IsValid(PlayerCharacter)
        ? (PlayerCharacter->GetActorLocation() - OperationCenter)
            .GetSafeNormal2D()
        : FVector::ForwardVector;

    if (InsertionDirection.IsNearlyZero())
    {
        InsertionDirection = FVector::ForwardVector;
    }

    const FVector SideDirection(
        -InsertionDirection.Y,
        InsertionDirection.X,
        0.0f
    );
    const float SideOffset =
        (FriendlyIndex -
            ((EffectiveFriendlySupportCount - 1) * 0.5f)) *
        350.0f;
    FVector SpawnLocation =
        OperationCenter +
        (InsertionDirection * 8000.0f) +
        (SideDirection * SideOffset);
    const int32 SafeAttemptIndex =
        FMath::Max(0, AttemptIndex);

    if (SafeAttemptIndex > 0)
    {
        const int32 AttemptPair =
            (SafeAttemptIndex + 1) / 2;
        const float AttemptSign =
            (SafeAttemptIndex % 2) == 1
                ? 1.0f
                : -1.0f;
        SpawnLocation +=
            SideDirection *
            AttemptSign *
            AlternateSpawnRadiusStep *
            AttemptPair;
        SpawnLocation -=
            InsertionDirection *
            AlternateSpawnRadiusStep *
            (SafeAttemptIndex % 3);
    }

    UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );
    FNavLocation ProjectedLocation;

    if (IsValid(NavigationSystem) &&
        NavigationSystem->ProjectPointToNavigation(
            SpawnLocation,
            ProjectedLocation,
            FVector(3000.0f, 3000.0f, 20000.0f)))
    {
        SpawnLocation = ProjectedLocation.Location;
    }

    return FTransform(
        (OperationCenter - SpawnLocation).Rotation(),
        SpawnLocation
    );
}
