#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "BHWarSubsystem.generated.h"

class FSubsystemCollectionBase;
struct FBHWarStateSnapshot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FBHOnWarStateChanged,
    int32,
    TurnNumber,
    FName,
    PrioritySectorID,
    EBHWarPriorityType,
    PriorityType
);

UCLASS()
class BROKENHORIZON_API UBHWarSubsystem
    : public UGameInstanceSubsystem,
      public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection
    ) override;

    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableWhenPaused() const override;

    FBHWarStateSnapshot CaptureReplicatedSnapshot(
        int32 Revision
    ) const;

    bool ApplyReplicatedSnapshot(
        const FBHWarStateSnapshot& Snapshot
    );

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    void ResetCampaign();

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    void AdvanceWarTurn();

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    bool ResolvePriorityMission(bool bFriendlySucceeded);

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Operation"
    )
    bool SetCommittedOperation(
        FName SectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION(BlueprintPure, Category = "Persistent War|Difficulty")
    FBHCampaignDifficultyProfile GetCampaignDifficulty() const;

    UFUNCTION(BlueprintCallable, Category = "Persistent War|Difficulty")
    bool SetCampaignDifficultyPreset(
        EBHCampaignDifficultyPreset Preset
    );

    UFUNCTION(BlueprintCallable, Category = "Persistent War|Difficulty")
    bool SetCustomCampaignDifficulty(
        const FBHCampaignDifficultyProfile& CustomProfile
    );

    bool RestoreCampaignDifficulty(
        const FBHCampaignDifficultyProfile& SavedProfile,
        int32 SavedSchemaVersion
    );

    static FBHOperationAfterActionRecord BuildAfterActionRecord(
        FName OperationID,
        FName SectorID,
        EBHWarPriorityType OperationType,
        bool bSucceeded,
        int32 FriendlyCasualties,
        int32 EnemyCasualties,
        int32 EnemyRouted,
        float StrategicSupplyDelta,
        float RecoveredMateriel,
        EBHOperationTacticalOption TacticalOption =
            EBHOperationTacticalOption::None,
        float TacticalSupplyCost = 0.0f
    );

    UFUNCTION(BlueprintCallable, Category = "Persistent War|Progression")
    bool RecordOperationAfterAction(
        const FBHOperationAfterActionRecord& Record
    );

    UFUNCTION(BlueprintPure, Category = "Persistent War|Progression")
    FBHCampaignProgressionState GetCampaignProgression() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Progression")
    bool HasCampaignCapability(
        EBHCampaignCapability Capability
    ) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Progression")
    EBHOperationTacticalOption GetActiveTacticalOption() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Progression")
    bool IsTacticalOptionUnlocked(
        EBHOperationTacticalOption Option
    ) const;

    UFUNCTION(BlueprintCallable, Category = "Persistent War|Progression")
    bool SetActiveTacticalOption(
        EBHOperationTacticalOption Option
    );

    UFUNCTION(BlueprintPure, Category = "Persistent War|Authority")
    bool CanIssueStrategicCommands() const;

    bool RestoreCampaignProgression(
        const FBHCampaignProgressionState& SavedProgression,
        int32 SavedSchemaVersion
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Operation"
    )
    bool SetCommittedRescueOperation(
        FName DestinationSectorID,
        FName CasualtyID
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Operation"
    )
    void ClearCommittedOperation();

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    bool HasCommittedOperation() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    FName GetCommittedOperationSectorID() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    FName GetCommittedOperationID() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    FName GetCommittedOperationTargetID() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    FName GetCommittedOperationSupplySourceSectorID() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    FName GetCommittedOperationEnemySourceSectorID() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    bool DoesConvoyMatchCommittedEscort(FName ConvoyID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    bool IsOperationSectorLocked(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    EBHWarPriorityType GetCommittedOperationType() const;

    UFUNCTION(BlueprintCallable, Category = "Persistent War")
    bool ApplyMissionResult(
        FName SectorID,
        EBHWarPriorityType MissionType,
        bool bFriendlySucceeded
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Operation"
    )
    bool ApplyOperationCasualtyResult(
        FName SectorID,
        FName FriendlySourceSectorID,
        FName EnemySourceSectorID,
        int32 FriendlyCasualties,
        int32 EnemyCasualties
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Operation"
    )
    EBHRaidOperationalSignature ApplyRaidOperationalSignature(
        FName SectorID,
        int32 EnemyCasualties,
        int32 FriendlySupportCasualties,
        bool bRaidSucceeded = true,
        bool bDetectedBeforeSabotage = false
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Ambient Battle"
    )
    bool ApplyAmbientBattleResult(
        FName SectorID,
        int32 FriendlyCasualties,
        int32 EnemyCasualties
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Ambient Battle"
    )
    bool ApplyAmbientRoutResult(
        FName SectorID,
        int32 FriendlyRouted,
        int32 EnemyRouted
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Operation"
    )
    bool ApplyOperationRoutResult(
        FName SectorID,
        int32 EnemyRouted
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Logistics"
    )
    float RecoverBattlefieldMateriel(
        FName SectorID,
        int32 EnemyCasualties,
        int32 FriendlyCasualties,
        bool bMajorOperation
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Logistics"
    )
    bool ConsumeSectorSupply(
        FName SectorID,
        float SupplyAmount
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Logistics"
    )
    float WithdrawFieldLogisticsSupply(
        FName SourceSectorID,
        float RequestedSupply
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Logistics"
    )
    float DeliverFieldLogisticsSupply(
        FName SourceSectorID,
        FName DestinationSectorID,
        float AvailableSupply
    );

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operations"
    )
    bool DoesFieldLogisticsDeliveryCompleteOperation(
        FName SourceSectorID,
        FName DestinationSectorID,
        float DeliveredSupply
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    float GetFieldLogisticsDeliveryCapacity(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    FName GetRecommendedFieldLogisticsDestination(
        FName SourceSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    FName GetRecommendedFieldCivilianAidDestination(
        FName SourceSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    FName GetRecommendedInTransitCivilianAidDestination(
        FName SourceSectorID,
        FName PreferredDestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Population"
    )
    float WithdrawFieldCivilianAidSupply(
        FName SourceSectorID,
        FName DestinationSectorID
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Population"
    )
    bool DeliverFieldCivilianAidSupply(
        FName SourceSectorID,
        FName DestinationSectorID,
        float AvailableAidSupply
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Ambient Battle"
    )
    float CommitAmbientPatrolSupply(
        FName SectorID,
        EBHWarFaction ForceFaction,
        float RequestedSupply
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Logistics"
    )
    bool ConsumePriorityOperationSupply();

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Logistics"
    )
    bool ConsumeOperationSupply(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    bool CanFundPriorityOperation() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    bool CanFundOperation(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    FName GetPriorityOperationSupplySource() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    FName GetOperationSupplySource(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    FName GetOperationEnemySource(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    TArray<FName> GetPriorityOperationSupplyRoute() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    TArray<FName> GetOperationSupplyRoute(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    float GetPriorityOperationSupplyCost() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    float GetOperationSupplyCost(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Progression"
    )
    float GetActiveTacticalOptionSupplyCost() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operation"
    )
    bool IsViableOperation(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    TArray<FBHWarSectorState> GetSectorStates() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    TArray<FBHWarSupplyConvoyState> GetSupplyConvoys() const;

    UFUNCTION(BlueprintCallable, Category = "Persistent War|Logistics")
    bool SetConvoySelectedWorldRoute(
        FName ConvoyID,
        FName WorldRouteID
    );

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    TArray<FBHWarGarrisonTransferState>
        GetGarrisonTransfers() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    int32 GetFactionManpowerReserve(
        EBHWarFaction ForceFaction
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    bool CanRecruitFieldOperative(FName SectorID) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Manpower"
    )
    bool RecruitFieldOperative(FName SectorID);

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    float GetFieldOperativeSupplyCost() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    float GetFactionRecruitmentProgress(
        EBHWarFaction ForceFaction
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    float GetFactionRecruitmentPerTurn(
        EBHWarFaction ForceFaction
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|History"
    )
    TArray<FBHWarEventRecord> GetRecentWarEvents() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|History"
    )
    int32 GetRecentConvoyInterdictionCount(
        FName DestinationSectorID,
        int32 TurnWindow = 3
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    bool HasSupplyConvoy(FName ConvoyID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    FBHWarSupplyConvoyState GetSupplyConvoyState(
        FName ConvoyID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Operations"
    )
    FName GetEscortOperationTargetID(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Logistics"
    )
    bool InterdictSupplyConvoy(FName ConvoyID);

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    float GetIncomingConvoySupply(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    float GetOutgoingConvoySupply(FName SectorID) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    FBHWarSectorState GetSectorState(FName SectorID) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    bool HasSector(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Garrison"
    )
    int32 GetSectorGarrisonCount(
        FName SectorID,
        EBHWarFaction ForceFaction
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Garrison"
    )
    int32 GetSectorGarrisonCapacity(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Intelligence"
    )
    float GetSectorIntelConfidence(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    float GetSectorCivilianSupport(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    bool CanDeliverCivilianAid(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Population"
    )
    bool DeliverCivilianAid(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    );

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    float GetCivilianAidSupplyCost() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    float GetCivilianAidSupportGain() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Intelligence"
    )
    FText GetSectorEnemyIntelSummary(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Response"
    )
    float GetSectorEnemyResponsePressure(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Response"
    )
    FText GetSectorEnemyResponseSummary(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Adaptation"
    )
    FText GetSectorEnemyAdaptationSummary(FName SectorID) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Intelligence"
    )
    bool ReportSectorRecon(
        FName SectorID,
        float ConfidenceGain = 45.0f
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Population"
    )
    bool ReportCivilianSecurityOutcome(
        FName SectorID,
        bool bFriendlySucceeded,
        float SupportShift = 4.0f
    );

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    bool CanMobilizeSectorMilitia(FName SectorID) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Population"
    )
    bool MobilizeSectorMilitia(FName SectorID);

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    int32 GetSectorMilitiaMobilizationCount(
        FName SectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Population"
    )
    float GetSectorMilitiaMobilizationSupplyCost(
        FName SectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    FName GetSectorGarrisonRedeploymentSource(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    int32 GetSectorGarrisonRedeploymentCount(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    float GetSectorGarrisonRedeploymentSupplyCost(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    int32 GetSectorGarrisonRedeploymentTurns(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    bool CanRedeploySectorGarrison(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Manpower"
    )
    bool RedeploySectorGarrison(
        FName DestinationSectorID
    );

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    int32 GetIncomingGarrisonTransferCount(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Manpower"
    )
    int32 GetIncomingGarrisonTransferTurns(
        FName DestinationSectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    bool IsLogisticsHubSector(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    bool IsSectorConnectedToFactionLogistics(
        FName SectorID
    ) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    int32 GetTurnNumber() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    FName GetPrioritySectorID() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    EBHWarPriorityType GetPriorityType() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    FText GetPriorityText() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    FText GetPriorityReasonText() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Mission")
    FText GetPriorityOperationTitle() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Mission")
    FText GetOperationTitle(
        FName SectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Mission")
    FText GetPriorityMissionBriefing() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Mission")
    FText GetOperationMissionBriefing(
        FName SectorID,
        EBHWarPriorityType OperationType
    ) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Mission")
    FText GetPriorityObjectiveText(FName ObjectiveID) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Mission")
    FText GetOperationObjectiveText(
        FName SectorID,
        EBHWarPriorityType OperationType,
        FName ObjectiveID
    ) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    float GetFriendlyControlPercentage() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Campaign"
    )
    EBHWarCampaignOutcome GetCampaignOutcome() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Campaign"
    )
    bool IsCampaignResolved() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Campaign"
    )
    FText GetCampaignOutcomeText() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    float GetSectorSupplyChangePerTurn(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    float GetSectorIsolationAttritionPerTurn(
        FName SectorID
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    float GetSectorCombatSupplyFactor(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    float GetSectorEffectiveStrength(
        FName SectorID,
        EBHWarFaction ForceFaction
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    int32 GetSectorConstructedFortificationCount(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    float GetSectorFortificationDefenseMultiplier(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    int32 GetSectorFortificationCapacity(FName SectorID) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    bool CanPlaceAdditionalFortification(
        FName SectorID,
        int32 AdditionalCount = 1
    ) const;

    void SynchronizeFortificationStateWithWorld();

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    float GetSectorReinforcementPerTurn(FName SectorID) const;

    UFUNCTION(BlueprintPure, Category = "Persistent War")
    float GetSimulationAccumulator() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    float GetCurrentSimulationIntervalSeconds() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Simulation"
    )
    float GetSecondsUntilNextWarTurn() const;

    bool RestoreWarState(
        const TArray<FBHWarSectorState>& SavedSectorStates,
        const TArray<FBHWarSupplyConvoyState>&
            SavedSupplyConvoys,
        const TArray<FBHWarEventRecord>& SavedWarEvents,
        int32 SavedTurnNumber,
        float SavedSimulationAccumulator,
        int32 SavedSchemaVersion
    );

    bool RestoreGarrisonTransfers(
        const TArray<FBHWarGarrisonTransferState>&
            SavedGarrisonTransfers
    );

    bool RestoreManpowerState(
        int32 SavedFriendlyManpowerReserve,
        int32 SavedEnemyManpowerReserve,
        float SavedFriendlyRecruitmentProgress,
        float SavedEnemyRecruitmentProgress
    );

    bool RestoreCommittedOperation(
        FName SavedSectorID,
        EBHWarPriorityType SavedOperationType,
        FName SavedOperationID = NAME_None,
        FName SavedOperationTargetID = NAME_None
    );

    UPROPERTY(BlueprintAssignable, Category = "Persistent War")
    FBHOnWarStateChanged OnWarStateChanged;

protected:
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Simulation",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float SimulationIntervalSeconds = 120.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Persistent War|Simulation",
        meta = (ClampMin = "1.0", Units = "s")
    )
    float CommittedOperationSimulationIntervalSeconds = 180.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War"
    )
    TArray<FBHWarSectorState> SectorStates;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Logistics"
    )
    TArray<FBHWarSupplyConvoyState> SupplyConvoys;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Manpower"
    )
    TArray<FBHWarGarrisonTransferState> GarrisonTransfers;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Manpower"
    )
    int32 FriendlyManpowerReserve = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Manpower"
    )
    int32 EnemyManpowerReserve = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Manpower"
    )
    float FriendlyRecruitmentProgress = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Manpower"
    )
    float EnemyRecruitmentProgress = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|History"
    )
    TArray<FBHWarEventRecord> RecentWarEvents;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War"
    )
    int32 TurnNumber = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War"
    )
    FName PrioritySectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War"
    )
    EBHWarPriorityType PriorityType =
        EBHWarPriorityType::None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War"
    )
    FText PriorityReasonText;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation"
    )
    FName CommittedOperationSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation"
    )
    FName CommittedOperationID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation"
    )
    FName CommittedOperationTargetID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation"
    )
    FName CommittedOperationSupplySourceSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation"
    )
    FName CommittedOperationEnemySourceSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Operation"
    )
    EBHWarPriorityType CommittedOperationType =
        EBHWarPriorityType::None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Persistent War|Campaign"
    )
    EBHWarCampaignOutcome CampaignOutcome =
        EBHWarCampaignOutcome::Ongoing;

private:
    bool CanMutateWarState() const;
    void BuildDefaultCampaign();
    void SimulateFrontlines();
    void AdvanceSupplyConvoys();
    void UpdateCommittedConvoyOperationDeadline(float DeltaTime);
    void AdvanceGarrisonTransfers();
    void RedistributeSupplies();
    void RecruitFactionManpower();
    void ReinforceGarrisons();
    void UpdateCivilianSupport();
    void DecaySectorIntelligence();
    void ResolveContestedSectors(FRandomStream& RandomStream);
    void EnsureContinuousConflict();
    void EvaluateCampaignOutcome();
    void RecalculatePriority();
    void BroadcastWarState();
    void ClampSectorState(FBHWarSectorState& Sector) const;
    bool ValidateSectorGraph(
        const TArray<FBHWarSectorState>& CandidateStates
    ) const;
    bool ValidateSupplyConvoy(
        const FBHWarSupplyConvoyState& Convoy
    ) const;
    bool ValidateGarrisonTransfer(
        const FBHWarGarrisonTransferState& Transfer
    ) const;
    int32 FindSectorIndex(FName SectorID) const;
    int32 FindGarrisonRedeploymentSourceIndex(
        FName DestinationSectorID
    ) const;
    int32 GetFriendlyRouteDistance(
        FName SourceSectorID,
        FName DestinationSectorID
    ) const;
    int32 FindPriorityOperationSupplySourceIndex() const;
    TArray<int32> BuildOperationSupplyRouteIndices(
        FName TargetSectorID,
        EBHWarPriorityType OperationType
    ) const;
    bool IsFrontlineSector(int32 SectorIndex) const;
    bool IsCommittedOperationSector(FName SectorID) const;
    void RecordWarEvent(
        FName EventType,
        FName SectorID,
        const FString& Summary
    );
    void RecordEnemyResponseEscalation(
        const FBHWarSectorState& Sector,
        float PreviousPressure
    );

    bool bApplyingReplicatedSnapshot = false;
    FBHCampaignDifficultyProfile CampaignDifficulty;
    FBHCampaignProgressionState CampaignProgression;
    float SimulationAccumulator = 0.0f;
    float RouteOperationReplicationAccumulator = 0.0f;
};
