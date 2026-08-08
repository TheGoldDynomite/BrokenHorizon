#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.h"

class UBHWarSubsystem;

struct BROKENHORIZON_API FBHWarOperationForceTuning
{
    int32 AttackEnemyCount = 3;
    int32 AttackReinforcementWaveCount = 1;
    int32 AttackReinforcementsPerWave = 1;
    int32 DefenseWaveCount = 3;
    int32 DefenseEnemiesPerWave = 2;
    int32 MaximumFriendlySupport = 2;
};

struct BROKENHORIZON_API FBHWarOperationForcePackage
{
    int32 AttackEnemyCount = 3;
    int32 AttackReinforcementWaveCount = 1;
    int32 AttackReinforcementsPerWave = 1;
    int32 DefenseWaveCount = 3;
    int32 DefenseEnemiesPerWave = 2;
    int32 FriendlySupportCount = 0;
    FName SupplySourceSectorID = NAME_None;
    FName EnemySourceSectorID = NAME_None;
    float EnemyStrength = 0.0f;
    float TargetSupply = 50.0f;
    float StagingFriendlyStrength = 0.0f;
    float StagingSupply = 0.0f;
    float IntelConfidence = 0.0f;
    float CivilianSupport = 50.0f;
    float EnemyResponsePressure = 0.0f;
    int32 EnemyPatternPreparationLevel = 0;
    int32 EnemyGarrisonCount = 0;
    int32 FriendlyGarrisonCount = 0;
    int32 OccupationGarrisonCount = 0;
    int32 OccupationTransferCount = 0;
    int32 RemainingStagingGarrisonCount = 0;
    int32 DesiredOccupationGarrisonCount = 0;
    bool bOccupationGarrisonShortfall = false;
    EBHOperationTacticalOption TacticalOption =
        EBHOperationTacticalOption::None;
    bool bReconPlanningApplied = false;
    bool bReinforcementPriorityApplied = false;
    bool bMedicalPreparationApplied = false;
};

namespace BHWarOperationRules
{
    BROKENHORIZON_API void GetTacticalMedicalSupplyGrant(
        EBHOperationTacticalOption TacticalOption,
        int32& OutMedkits,
        int32& OutFieldDressings
    );

    BROKENHORIZON_API EBHRaidOperationalSignature
    ClassifyRaidOperationalSignature(
        int32 EnemyCasualties,
        int32 FriendlySupportCasualties,
        bool bDetectedBeforeSabotage = false
    );

    BROKENHORIZON_API int32 CalculateRaidReactionForceCount(
        int32 ReinforcementWaveCount,
        int32 ReinforcementsPerWave,
        bool bDetectedBeforeSabotage
    );

    BROKENHORIZON_API bool IsEnemyRoutedFromOperation(
        bool bIsRetreating,
        float DistanceFromObjective,
        float RequiredWithdrawalDistance
    );

    BROKENHORIZON_API bool IsOperationCombatantReady(
        bool bIsAlive,
        bool bHasController
    );

    BROKENHORIZON_API bool
    IsFieldSquadMemberTransportEligible(
        bool bIsDead,
        bool bIsIncapacitated,
        bool bSquadHolding,
        bool bRequiresMedicalEvacuation = false
    );

    BROKENHORIZON_API bool CanAssignFieldSquadCasualtyAid(
        bool bHasOwnedResponder,
        bool bTargetIsFriendlyCasualty,
        bool bSquadEmbarked,
        int32 FieldDressingCount
    );

    BROKENHORIZON_API bool CanAssignFieldSquadSabotage(
        bool bHasOwnedResponder,
        bool bTargetIsActiveRaidObjective,
        bool bSquadEmbarked
    );

    BROKENHORIZON_API bool CanAssignFieldSquadObjectivePresence(
        bool bHasOwnedResponder,
        bool bHasActiveOperation,
        EBHWarPriorityType OperationType,
        bool bTargetMatchesOperationSector,
        bool bSquadEmbarked
    );

    BROKENHORIZON_API float
    CalculateFieldOperativeReadinessSpread(
        float CombatReadiness,
        float MaximumSpreadPenalty = 4.0f
    );

    BROKENHORIZON_API float
    CalculateFieldOperativeFireInterval(
        float BaseFireInterval,
        float CombatReadiness,
        float MaximumIntervalMultiplier = 1.35f
    );

    BROKENHORIZON_API bool IsRaidExfiltrationComplete(
        float DistanceFromTarget,
        float RequiredDistance
    );

    BROKENHORIZON_API bool IsReconReportComplete(
        bool bHasCommittedOperation,
        EBHWarPriorityType OperationType,
        FName AssignedSectorID,
        FName ReportedSectorID,
        float IntelConfidence,
        float RequiredConfidence = 100.0f
    );

    BROKENHORIZON_API FVector
    CalculateFriendlyFormationOffset(
        int32 FriendlyIndex,
        float LateralSpacing = 250.0f,
        float TrailingDistance = 350.0f
    );

    BROKENHORIZON_API FName GetMobilizationSectorID(
        const UBHWarSubsystem* WarSubsystem,
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    );

    BROKENHORIZON_API FBHWarOperationForcePackage
    BuildForcePackage(
        const UBHWarSubsystem* WarSubsystem,
        FName TargetSectorID,
        EBHWarPriorityType OperationType,
        FName RequestedSupplySourceSectorID,
        const FBHWarOperationForceTuning& Tuning =
            FBHWarOperationForceTuning()
    );
}
