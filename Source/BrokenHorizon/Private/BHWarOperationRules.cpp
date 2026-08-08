#include "BHWarOperationRules.h"

#include "BHWarSubsystem.h"

namespace
{
constexpr int32 MinimumAttackForce = 2;
constexpr int32 MaximumAttackForce = 6;
constexpr int32 MaximumAttackReinforcementWaves = 2;
constexpr int32 MaximumAttackReinforcementWaveSize = 3;
constexpr int32 MaximumRaidForce = 4;
constexpr int32 MaximumRaidReinforcementWaves = 1;
constexpr int32 MaximumRaidSupport = 1;
constexpr int32 MinimumDefenseWaves = 2;
constexpr int32 MaximumDefenseWaves = 4;
constexpr int32 MinimumDefenseWaveSize = 1;
constexpr int32 MaximumDefenseWaveSize = 3;
constexpr int32 DesiredOccupationGarrison = 2;
constexpr float CounterinsurgencyWavePressureThreshold = 40.0f;
constexpr int32 CounterinsurgencyWaveAdjustment = 2;
constexpr int32 MaximumPatternPreparationLevel = 2;
}

EBHRaidOperationalSignature
BHWarOperationRules::ClassifyRaidOperationalSignature(
    int32 EnemyCasualties,
    int32 FriendlySupportCasualties,
    bool bDetectedBeforeSabotage
)
{
    const int32 SafeEnemyCasualties =
        FMath::Max(0, EnemyCasualties);
    const int32 SafeFriendlyCasualties =
        FMath::Max(0, FriendlySupportCasualties);

    if (SafeFriendlyCasualties > 0 ||
        SafeEnemyCasualties >= 4)
    {
        return EBHRaidOperationalSignature::Loud;
    }

    if (!bDetectedBeforeSabotage &&
        SafeEnemyCasualties <= 1)
    {
        return EBHRaidOperationalSignature::Clean;
    }

    return EBHRaidOperationalSignature::Contested;
}

int32 BHWarOperationRules::CalculateRaidReactionForceCount(
    int32 ReinforcementWaveCount,
    int32 ReinforcementsPerWave,
    bool bDetectedBeforeSabotage
)
{
    const int32 FullReactionForce =
        FMath::Max(0, ReinforcementWaveCount) *
        FMath::Max(0, ReinforcementsPerWave);

    return bDetectedBeforeSabotage
        ? FullReactionForce
        : FMath::Max(0, FullReactionForce - 1);
}

bool BHWarOperationRules::IsEnemyRoutedFromOperation(
    bool bIsRetreating,
    float DistanceFromObjective,
    float RequiredWithdrawalDistance
)
{
    return bIsRetreating &&
        FMath::Max(0.0f, DistanceFromObjective) >=
            FMath::Max(100.0f, RequiredWithdrawalDistance);
}

bool BHWarOperationRules::IsOperationCombatantReady(
    bool bIsAlive,
    bool bHasController
)
{
    return bIsAlive && bHasController;
}

bool BHWarOperationRules::
    IsFieldSquadMemberTransportEligible(
    bool bIsDead,
    bool bIsIncapacitated,
    bool bSquadHolding,
    bool bRequiresMedicalEvacuation
)
{
    if (bIsDead && !bIsIncapacitated)
    {
        return false;
    }

    return bIsIncapacitated ||
        bRequiresMedicalEvacuation ||
        !bSquadHolding;
}

bool BHWarOperationRules::CanAssignFieldSquadCasualtyAid(
    bool bHasOwnedResponder,
    bool bTargetIsFriendlyCasualty,
    bool bSquadEmbarked,
    int32 FieldDressingCount
)
{
    return bHasOwnedResponder &&
        bTargetIsFriendlyCasualty &&
        !bSquadEmbarked &&
        FieldDressingCount > 0;
}

bool BHWarOperationRules::CanAssignFieldSquadSabotage(
    bool bHasOwnedResponder,
    bool bTargetIsActiveRaidObjective,
    bool bSquadEmbarked
)
{
    return bHasOwnedResponder &&
        bTargetIsActiveRaidObjective &&
        !bSquadEmbarked;
}

bool BHWarOperationRules::CanAssignFieldSquadObjectivePresence(
    bool bHasOwnedResponder,
    bool bHasActiveOperation,
    EBHWarPriorityType OperationType,
    bool bTargetMatchesOperationSector,
    bool bSquadEmbarked
)
{
    return bHasOwnedResponder &&
        bHasActiveOperation &&
        (OperationType == EBHWarPriorityType::Attack ||
         OperationType == EBHWarPriorityType::Defend) &&
        bTargetMatchesOperationSector &&
        !bSquadEmbarked;
}

float BHWarOperationRules::
CalculateFieldOperativeReadinessSpread(
    float CombatReadiness,
    float MaximumSpreadPenalty
)
{
    const float Readiness = FMath::Clamp(
        CombatReadiness,
        0.0f,
        1.0f
    );
    return (1.0f - Readiness) *
        FMath::Max(0.0f, MaximumSpreadPenalty);
}

float BHWarOperationRules::
CalculateFieldOperativeFireInterval(
    float BaseFireInterval,
    float CombatReadiness,
    float MaximumIntervalMultiplier
)
{
    const float Readiness = FMath::Clamp(
        CombatReadiness,
        0.0f,
        1.0f
    );
    const float IntervalMultiplier = FMath::Lerp(
        FMath::Max(1.0f, MaximumIntervalMultiplier),
        1.0f,
        Readiness
    );
    return FMath::Max(0.05f, BaseFireInterval) *
        IntervalMultiplier;
}

bool BHWarOperationRules::IsRaidExfiltrationComplete(
    float DistanceFromTarget,
    float RequiredDistance
)
{
    return FMath::Max(0.0f, DistanceFromTarget) >=
            FMath::Max(0.0f, RequiredDistance);
}

bool BHWarOperationRules::IsReconReportComplete(
    bool bHasCommittedOperation,
    EBHWarPriorityType OperationType,
    FName AssignedSectorID,
    FName ReportedSectorID,
    float IntelConfidence,
    float RequiredConfidence
)
{
    return bHasCommittedOperation &&
        OperationType == EBHWarPriorityType::Recon &&
        !AssignedSectorID.IsNone() &&
        AssignedSectorID == ReportedSectorID &&
        FMath::Clamp(IntelConfidence, 0.0f, 100.0f) +
                KINDA_SMALL_NUMBER >=
            FMath::Clamp(RequiredConfidence, 1.0f, 100.0f);
}

FVector BHWarOperationRules::CalculateFriendlyFormationOffset(
    int32 FriendlyIndex,
    float LateralSpacing,
    float TrailingDistance
)
{
    const int32 SafeIndex = FMath::Max(0, FriendlyIndex);
    const int32 RowIndex = SafeIndex / 2;
    const float Side = SafeIndex % 2 == 0 ? -1.0f : 1.0f;
    const float SafeLateralSpacing =
        FMath::Max(100.0f, LateralSpacing);
    const float SafeTrailingDistance =
        FMath::Max(150.0f, TrailingDistance);

    return FVector(
        -(SafeTrailingDistance +
            (RowIndex * SafeLateralSpacing)),
        Side * SafeLateralSpacing,
        0.0f
    );
}

FName BHWarOperationRules::GetMobilizationSectorID(
    const UBHWarSubsystem* WarSubsystem,
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    if (!IsValid(WarSubsystem) ||
        !WarSubsystem->HasSector(TargetSectorID) ||
        OperationType == EBHWarPriorityType::None ||
        OperationType == EBHWarPriorityType::Resupply ||
        OperationType == EBHWarPriorityType::EscortRescue ||
        OperationType == EBHWarPriorityType::Rescue ||
        OperationType == EBHWarPriorityType::Recon)
    {
        return NAME_None;
    }

    return OperationType == EBHWarPriorityType::Defend
        ? TargetSectorID
        : WarSubsystem->GetOperationSupplySource(
            TargetSectorID,
            OperationType
        );
}

FBHWarOperationForcePackage BHWarOperationRules::BuildForcePackage(
    const UBHWarSubsystem* WarSubsystem,
    FName TargetSectorID,
    EBHWarPriorityType OperationType,
    FName RequestedSupplySourceSectorID,
    const FBHWarOperationForceTuning& Tuning
)
{
    FBHWarOperationForcePackage Package;
    Package.AttackEnemyCount =
        FMath::Max(1, Tuning.AttackEnemyCount);
    Package.AttackReinforcementWaveCount =
        FMath::Max(0, Tuning.AttackReinforcementWaveCount);
    Package.AttackReinforcementsPerWave =
        FMath::Max(1, Tuning.AttackReinforcementsPerWave);
    Package.DefenseWaveCount =
        FMath::Max(1, Tuning.DefenseWaveCount);
    Package.DefenseEnemiesPerWave =
        FMath::Max(1, Tuning.DefenseEnemiesPerWave);
    Package.SupplySourceSectorID =
        RequestedSupplySourceSectorID;

    if (!IsValid(WarSubsystem) ||
        !WarSubsystem->HasSector(TargetSectorID) ||
        OperationType == EBHWarPriorityType::None)
    {
        return Package;
    }

    const FBHWarSectorState TargetSector =
        WarSubsystem->GetSectorState(TargetSectorID);
    Package.TargetSupply = TargetSector.Supply;
    Package.IntelConfidence = TargetSector.IntelConfidence;
    Package.TacticalOption =
        WarSubsystem->GetActiveTacticalOption();
    const bool bSupportsTacticalPlanning =
        OperationType == EBHWarPriorityType::Attack ||
        OperationType == EBHWarPriorityType::Defend ||
        OperationType == EBHWarPriorityType::Raid;
    Package.bReconPlanningApplied =
        bSupportsTacticalPlanning &&
        Package.TacticalOption ==
            EBHOperationTacticalOption::ReconPlanning &&
        WarSubsystem->IsTacticalOptionUnlocked(
            EBHOperationTacticalOption::ReconPlanning);
    Package.bReinforcementPriorityApplied =
        bSupportsTacticalPlanning &&
        Package.TacticalOption ==
            EBHOperationTacticalOption::ReinforcementPriority &&
        WarSubsystem->IsTacticalOptionUnlocked(
            EBHOperationTacticalOption::ReinforcementPriority);
    Package.bMedicalPreparationApplied =
        bSupportsTacticalPlanning &&
        Package.TacticalOption ==
            EBHOperationTacticalOption::MedicalPreparation &&
        WarSubsystem->IsTacticalOptionUnlocked(
            EBHOperationTacticalOption::MedicalPreparation);
    if (Package.bReconPlanningApplied)
    {
        Package.IntelConfidence = FMath::Max(
            Package.IntelConfidence,
            80.0f
        );
    }
    Package.CivilianSupport = TargetSector.CivilianSupport;
    Package.EnemyResponsePressure =
        TargetSector.EnemyResponsePressure;
    Package.EnemySourceSectorID =
        WarSubsystem->GetOperationEnemySource(
            TargetSectorID,
            OperationType
        );
    const FBHWarSectorState EnemySourceSector =
        WarSubsystem->GetSectorState(
            Package.EnemySourceSectorID
        );
    Package.EnemyStrength =
        EnemySourceSector.SectorID.IsNone()
            ? TargetSector.EnemyStrength
            : EnemySourceSector.EnemyStrength;
    Package.EnemyGarrisonCount =
        EnemySourceSector.SectorID.IsNone()
            ? TargetSector.EnemyGarrison
            : EnemySourceSector.EnemyGarrison;
    const float EnemyLogistics =
        EnemySourceSector.SectorID.IsNone()
            ? TargetSector.Owner == EBHWarFaction::Enemy
                ? Package.TargetSupply
                : 50.0f
            : EnemySourceSector.Supply;
    FBHWarSectorState StagingSector =
        WarSubsystem->GetSectorState(
            RequestedSupplySourceSectorID
        );

    if (StagingSector.SectorID.IsNone() ||
        StagingSector.Owner != EBHWarFaction::Friendly)
    {
        if (TargetSector.Owner == EBHWarFaction::Friendly)
        {
            StagingSector = TargetSector;
            Package.SupplySourceSectorID =
                TargetSector.SectorID;
        }
    }

    if (!StagingSector.SectorID.IsNone() &&
        StagingSector.Owner == EBHWarFaction::Friendly)
    {
        Package.StagingFriendlyStrength =
            StagingSector.FriendlyStrength;
        Package.StagingSupply = StagingSector.Supply;
        Package.FriendlyGarrisonCount =
            StagingSector.FriendlyGarrison;
    }
    Package.RemainingStagingGarrisonCount =
        Package.FriendlyGarrisonCount;

    if (OperationType == EBHWarPriorityType::Attack)
    {
        Package.DesiredOccupationGarrisonCount =
            FMath::Min(
                DesiredOccupationGarrison,
                TargetSector.GarrisonCapacity
            );
        Package.OccupationTransferCount =
            FMath::Min(
                FMath::Max(
                    0,
                    Package.DesiredOccupationGarrisonCount -
                        TargetSector.FriendlyGarrison
                ),
                Package.FriendlyGarrisonCount
            );
        Package.OccupationGarrisonCount =
            TargetSector.FriendlyGarrison +
            Package.OccupationTransferCount;
        Package.RemainingStagingGarrisonCount =
            Package.FriendlyGarrisonCount -
            Package.OccupationTransferCount;
        Package.bOccupationGarrisonShortfall =
            Package.OccupationGarrisonCount <
            Package.DesiredOccupationGarrisonCount;
    }

    const int32 StrengthAdjustment =
        Package.EnemyStrength >= 75.0f
            ? 2
            : Package.EnemyStrength >= 45.0f
                ? 1
                : Package.EnemyStrength <= 20.0f
                    ? -1
                    : 0;
    const int32 SupplyAdjustment =
        EnemyLogistics >= 75.0f
            ? 1
            : EnemyLogistics <= 25.0f
                ? -1
                : 0;
    const bool bLowConfidenceIntel =
        Package.IntelConfidence < 45.0f;
    const bool bConfirmedIntel =
        Package.IntelConfidence >= 80.0f;
    const int32 IntelligenceForceAdjustment =
        bLowConfidenceIntel
            ? 1
            : bConfirmedIntel
                ? -1
                : 0;
    const int32 IntelligenceWaveAdjustment =
        bLowConfidenceIntel
            ? 1
            : bConfirmedIntel
                ? -1
                : 0;
    const int32 ResponseForceAdjustment =
        Package.EnemyResponsePressure >= 75.0f
            ? 1
            : 0;
    const bool bCounterinsurgencySweep =
        OperationType == EBHWarPriorityType::Defend &&
        TargetSector.Owner == EBHWarFaction::Friendly &&
        Package.EnemyResponsePressure >=
            CounterinsurgencyWavePressureThreshold;
    const int32 ResponseWaveAdjustment =
        bCounterinsurgencySweep
            ? CounterinsurgencyWaveAdjustment
            : Package.EnemyResponsePressure >= 75.0f
            ? 1
            : Package.EnemyResponsePressure >= 50.0f
                ? 1
                : 0;
    const int32 PopulationForceAdjustment =
        Package.CivilianSupport <= 30.0f
            ? 1
            : Package.CivilianSupport >= 70.0f
                ? -1
                : 0;
    const int32 PopulationWaveAdjustment =
        Package.CivilianSupport <= 25.0f
            ? 1
            : Package.CivilianSupport >= 75.0f
                ? -1
                : 0;
    Package.EnemyPatternPreparationLevel =
        TargetSector.AnticipatedOperationType == OperationType
            ? FMath::Clamp(
                TargetSector.RepeatedOperationCount,
                0,
                MaximumPatternPreparationLevel
            )
            : 0;
    const float DefenseWaveStrength =
        bCounterinsurgencySweep
            ? TargetSector.EnemyStrength
            : Package.EnemyStrength;
    const float DefenseWaveLogistics =
        bCounterinsurgencySweep
            ? 50.0f
            : EnemyLogistics;

    Package.AttackEnemyCount = FMath::Clamp(
        Tuning.AttackEnemyCount +
            StrengthAdjustment +
            SupplyAdjustment +
            IntelligenceForceAdjustment +
            PopulationForceAdjustment +
            ResponseForceAdjustment +
            Package.EnemyPatternPreparationLevel,
        MinimumAttackForce,
        MaximumAttackForce
    );
    Package.AttackReinforcementWaveCount =
        FMath::Clamp(
            Tuning.AttackReinforcementWaveCount +
                (EnemyLogistics >= 75.0f ? 1 : 0) -
                (EnemyLogistics <= 25.0f ? 1 : 0) +
                IntelligenceWaveAdjustment +
                PopulationWaveAdjustment +
                ResponseWaveAdjustment +
                (Package.EnemyPatternPreparationLevel >= 2
                    ? 1
                    : 0),
            0,
            MaximumAttackReinforcementWaves
        );
    Package.AttackReinforcementsPerWave =
        FMath::Clamp(
            Tuning.AttackReinforcementsPerWave +
                (Package.EnemyStrength >= 75.0f ? 1 : 0),
            1,
            MaximumAttackReinforcementWaveSize
        );

    if (OperationType == EBHWarPriorityType::Raid)
    {
        Package.AttackEnemyCount = FMath::Min(
            Package.AttackEnemyCount,
            MaximumRaidForce
        );
        Package.AttackReinforcementWaveCount = FMath::Min(
            Package.AttackReinforcementWaveCount,
            MaximumRaidReinforcementWaves
        );
    }
    Package.DefenseWaveCount = FMath::Clamp(
        Tuning.DefenseWaveCount +
            (DefenseWaveStrength >= 75.0f ? 1 : 0) -
            (DefenseWaveStrength <= 25.0f ? 1 : 0) +
            IntelligenceWaveAdjustment +
            PopulationWaveAdjustment +
            ResponseWaveAdjustment +
            Package.EnemyPatternPreparationLevel,
        MinimumDefenseWaves,
        MaximumDefenseWaves
    );
    Package.DefenseEnemiesPerWave = FMath::Clamp(
        Tuning.DefenseEnemiesPerWave +
            (DefenseWaveLogistics >= 75.0f ? 1 : 0) -
            (DefenseWaveLogistics <= 25.0f ? 1 : 0) +
                (Package.EnemyPatternPreparationLevel >= 2
                    ? 1
                    : 0),
        MinimumDefenseWaveSize,
        MaximumDefenseWaveSize
    );
    Package.FriendlySupportCount = FMath::Clamp(
        (Package.StagingFriendlyStrength >= 70.0f
            ? 2
            : Package.StagingFriendlyStrength >= 35.0f
                ? 1
                : 0) -
            (Package.StagingSupply <= 20.0f ? 1 : 0) +
            (Package.StagingSupply >= 75.0f ? 1 : 0) +
            (bConfirmedIntel ? 1 : 0) +
            (Package.CivilianSupport >= 70.0f ? 1 : 0) -
            (Package.CivilianSupport <= 25.0f ? 1 : 0),
        0,
        FMath::Min(
            FMath::Max(0, Tuning.MaximumFriendlySupport),
            Package.FriendlyGarrisonCount
        )
    );

    if (Package.bReinforcementPriorityApplied)
    {
        Package.FriendlySupportCount = FMath::Min(
            Package.FriendlySupportCount + 1,
            FMath::Min(
                FMath::Max(0, Tuning.MaximumFriendlySupport),
                Package.FriendlyGarrisonCount
            )
        );
    }

    if (OperationType == EBHWarPriorityType::Raid)
    {
        Package.FriendlySupportCount = FMath::Min(
            Package.FriendlySupportCount,
            MaximumRaidSupport
        );
    }

    if (OperationType == EBHWarPriorityType::Attack ||
        OperationType == EBHWarPriorityType::Raid)
    {
        const int32 AvailableDefenders = FMath::Max(
            0,
            Package.EnemyGarrisonCount
        );
        Package.AttackEnemyCount = FMath::Min(
            Package.AttackEnemyCount,
            AvailableDefenders
        );
        const int32 RemainingDefenders = FMath::Max(
            0,
            AvailableDefenders - Package.AttackEnemyCount
        );
        Package.AttackReinforcementWaveCount = FMath::Min(
            Package.AttackReinforcementWaveCount,
            RemainingDefenders
        );

        if (Package.AttackReinforcementWaveCount > 0)
        {
            Package.AttackReinforcementsPerWave =
                FMath::Clamp(
                    RemainingDefenders /
                        Package.AttackReinforcementWaveCount,
                    1,
                    Package.AttackReinforcementsPerWave
                );
        }
        else
        {
            Package.AttackReinforcementsPerWave = 0;
        }
    }
    else if (OperationType == EBHWarPriorityType::Defend &&
        Package.EnemyGarrisonCount > 0)
    {
        const int32 AvailableAttackers =
            Package.EnemyGarrisonCount;
        Package.DefenseWaveCount = FMath::Min(
            Package.DefenseWaveCount,
            AvailableAttackers
        );
        Package.DefenseEnemiesPerWave = FMath::Min(
            Package.DefenseEnemiesPerWave,
            FMath::Max(
                1,
                AvailableAttackers /
                    Package.DefenseWaveCount
            )
        );
    }

    return Package;
}
void BHWarOperationRules::GetTacticalMedicalSupplyGrant(
    EBHOperationTacticalOption TacticalOption,
    int32& OutMedkits,
    int32& OutFieldDressings
)
{
    const bool bMedicalPreparation =
        TacticalOption ==
            EBHOperationTacticalOption::MedicalPreparation;
    OutMedkits = bMedicalPreparation ? 1 : 0;
    OutFieldDressings = bMedicalPreparation ? 2 : 0;
}
