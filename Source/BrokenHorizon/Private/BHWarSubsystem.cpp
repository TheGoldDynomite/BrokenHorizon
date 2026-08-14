#include "BHWarSubsystem.h"

#include "Algo/Unique.h"
#include "BHMissionData.h"
#include "BHFieldFortification.h"
#include "BHWarGameState.h"
#include "BHWarOperationRules.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/SubsystemCollection.h"

#include <initializer_list>

namespace
{
constexpr float MaximumSectorStrength = 100.0f;
constexpr float MaximumSectorSupply = 100.0f;
constexpr float CaptureMinimumStrength = 12.0f;
constexpr float CounteroffensiveStrength = 55.0f;
constexpr float ResistanceStrength = 35.0f;
constexpr float AmbientStrengthLossPerCasualty = 2.0f;
constexpr float AmbientSupplyCostPerCasualty = 0.5f;
constexpr float AmbientStrengthLossPerRoutedUnit = 0.75f;
constexpr float AmbientSupplyCostPerRoutedUnit = 0.25f;
constexpr float SalvageSupplyPerEnemyCasualty = 1.5f;
constexpr float SalvageLossPerFriendlyCasualty = 0.5f;
constexpr float MaximumAmbientSalvageSupply = 4.0f;
constexpr float MaximumOperationSalvageSupply = 10.0f;
constexpr float MinimumSupplyTransferDifference = 10.0f;
constexpr float MaximumSupplyTransferPerTurn = 3.0f;
constexpr float MinimumSupplyReserve = 25.0f;
constexpr float FrontlineSupplyConsumptionPerTurn = -2.0f;
constexpr float IsolatedSupplyAttritionPerTurn = -1.0f;
constexpr float IsolatedAttritionSupplyThreshold = 30.0f;
constexpr float MinimumIsolationStrengthAttrition = 0.5f;
constexpr float MaximumIsolationStrengthAttrition = 1.5f;
constexpr float StandardSupplyProductionPerTurn = 1.0f;
constexpr float LogisticsHubSupplyProductionPerTurn = 3.0f;
constexpr float PriorityOperationSupplyCost = 10.0f;
constexpr float ReconOperationSupplyCost = 4.0f;
constexpr float ReconPlanningSupplyCost = 6.0f;
constexpr float ReinforcementPrioritySupplyCost = 12.0f;
constexpr float MedicalPreparationSupplyCost = 8.0f;
constexpr float MinimumSuppliedReinforcementFactor = 0.25f;
constexpr float MaximumReinforcementSupplyFactor = 1.25f;
constexpr float MinimumCombatSupplyFactor = 0.35f;
constexpr float MaximumCombatSupplyFactor = 1.10f;
constexpr float FortificationSupplyCacheSupportPerCharge = 1.9f;
constexpr float FortificationRallyDeploymentHealthThreshold = 0.5f;
constexpr float ReconnectionAttackBonus = 40.0f;
constexpr float CriticalDefensePriorityScore = 35.0f;
constexpr float GarrisonSupplyCostPerUnit = 1.5f;
constexpr float MinimumGarrisonReinforcementSupply = 20.0f;
constexpr float GarrisonSupplyReserve = 10.0f;
constexpr int32 MaximumGarrisonCapacity = 64;
constexpr int32 MaximumGarrisonReinforcementsPerTurn = 2;
constexpr int32 MinimumOccupationGarrison = 2;
constexpr float GarrisonStrengthPerUnit = 4.0f;
constexpr int32 InitialFriendlyManpowerReserve = 16;
constexpr int32 InitialEnemyManpowerReserve = 28;
constexpr int32 MaximumFactionManpowerReserve = 200;
constexpr int32 FieldOperativeManpowerCost = 1;
constexpr float FieldOperativeSupplyCost = 8.0f;
constexpr float MaximumIntelConfidence = 100.0f;
constexpr float FriendlySectorIntelFloor = 85.0f;
constexpr float IntelligenceDecayPerTurn = 6.0f;
constexpr float EstimatedIntelThreshold = 45.0f;
constexpr float ConfirmedIntelThreshold = 80.0f;
constexpr int32 SectorIntelSaveSchemaVersion = 16;
constexpr int32 CivilianSupportSaveSchemaVersion = 20;
constexpr float MaximumCivilianSupport = 100.0f;
constexpr float NeutralCivilianSupport = 50.0f;
constexpr float CivilianSupportDriftPerTurn = 1.5f;
constexpr float CivilianIntelContribution = 0.5f;
constexpr float MinimumMilitiaMobilizationSupport = 45.0f;
constexpr float MilitiaSupportCostPerUnit = 4.0f;
constexpr float MilitiaSupplyCostPerUnit = 4.0f;
constexpr float MilitiaStrengthPerUnit = 2.0f;
constexpr float MilitiaResponsePressurePerUnit = 12.5f;
constexpr int32 MaximumMilitiaMobilizationCount = 3;
constexpr int32 MinimumRedeploymentSourceGarrison = 2;
constexpr int32 MaximumGarrisonRedeploymentCount = 2;
constexpr float GarrisonRedeploymentSupplyCostPerUnit = 3.0f;
constexpr float CounterinsurgencyResponseThreshold = 40.0f;
constexpr float AttackSuccessCivilianSupport = 15.0f;
constexpr float AttackFailureCivilianSupport = -8.0f;
constexpr float RaidSuccessCivilianSupport = 5.0f;
constexpr float RaidFailureCivilianSupport = -4.0f;
constexpr float RaidSuccessEnemyStrengthLoss = 18.0f;
constexpr float RaidSuccessSupplyLoss = 30.0f;
constexpr float RaidFailureEnemyStrengthGain = 5.0f;
constexpr float DefenseSuccessCivilianSupport = 8.0f;
constexpr float DefenseFailureCivilianSupport = -15.0f;
constexpr float CivilianAidSupplyCost = 10.0f;
constexpr float CivilianAidSupportGain = 12.0f;
constexpr float CivilianAidIntelGain = 18.0f;
constexpr float CivilianAidHostileExposurePressure = 10.0f;
constexpr float CivilianAidFriendlyResponseReduction = 5.0f;
constexpr float UndergroundRecruitmentFactor = 0.25f;
constexpr float NeutralRecruitmentFactor = 0.10f;
constexpr float MaximumEnemyResponsePressure = 100.0f;
constexpr float EnemyResponseDecayPerTurn = 8.0f;
constexpr float EnemyCasualtyResponsePressure = 6.0f;
constexpr float EnemyRoutResponsePressure = 4.0f;
constexpr float FriendlyCasualtyResponsePressure = 1.0f;
constexpr float AttackSuccessResponsePressure = 30.0f;
constexpr float AttackFailureResponsePressure = 20.0f;
constexpr float RaidSuccessResponsePressure = 15.0f;
constexpr float RaidFailureResponsePressure = 20.0f;
constexpr float CleanRaidResponseReduction = 8.0f;
constexpr float CleanRaidCivilianSupportBonus = 4.0f;
constexpr float LoudRaidResponseIncrease = 12.0f;
constexpr float LoudRaidCivilianSupportPenalty = 6.0f;
constexpr float DefenseSuccessResponsePressure = 15.0f;
constexpr float CounterinsurgencySuccessResponseReduction = 35.0f;
constexpr int32 MaximumAmbientCasualtiesPerReport = 16;
constexpr int32 MaximumWarEventHistory = 16;
constexpr int32 MaximumWarEventSummaryLength = 160;

float CalculateCombatSupplyFactor(float Supply)
{
    const float NormalizedSupply = FMath::Clamp(
        Supply / MaximumSectorSupply,
        0.0f,
        1.0f
    );

    return FMath::Lerp(
        MinimumCombatSupplyFactor,
        MaximumCombatSupplyFactor,
        NormalizedSupply
    );
}

float CalculateReinforcementAmount(
    const FBHWarSectorState& Sector,
    bool bConnectedToLogistics
)
{
    if (Sector.Owner == EBHWarFaction::Neutral ||
        !bConnectedToLogistics ||
        Sector.Supply <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const float NormalizedSupply = FMath::Clamp(
        Sector.Supply / MaximumSectorSupply,
        0.0f,
        1.0f
    );
    const float SupplyFactor = FMath::Lerp(
        MinimumSuppliedReinforcementFactor,
        MaximumReinforcementSupplyFactor,
        NormalizedSupply
    );

    return Sector.ReinforcementRate * SupplyFactor;
}

bool IsLogisticsHub(FName SectorID)
{
    return SectorID == TEXT("WesternFOB") ||
        SectorID == TEXT("EasternDepot");
}

float GetSiteRecruitmentWeight(EBHWarSiteType SiteType)
{
    switch (SiteType)
    {
    case EBHWarSiteType::Headquarters:
        return 0.30f;
    case EBHWarSiteType::Town:
        return 0.25f;
    case EBHWarSiteType::Village:
        return 0.18f;
    case EBHWarSiteType::LogisticsDepot:
        return 0.12f;
    case EBHWarSiteType::Checkpoint:
        return 0.06f;
    case EBHWarSiteType::Bridge:
        return 0.03f;
    default:
        return 0.0f;
    }
}

FBHWarSectorState MakeSector(
    FName SectorID,
    const TCHAR* DisplayName,
    EBHWarFaction Owner,
    EBHWarSiteType SiteType,
    int32 GarrisonCapacity,
    int32 FriendlyGarrison,
    int32 EnemyGarrison,
    float IntelConfidence,
    float CivilianSupport,
    float FriendlyStrength,
    float EnemyStrength,
    float Supply,
    float ReinforcementRate,
    std::initializer_list<FName> ConnectedSectorIDs
)
{
    FBHWarSectorState Sector;
    Sector.SectorID = SectorID;
    Sector.DisplayName = FText::FromString(DisplayName);
    Sector.Owner = Owner;
    Sector.SiteType = SiteType;
    Sector.GarrisonCapacity = GarrisonCapacity;
    Sector.FriendlyGarrison = FriendlyGarrison;
    Sector.EnemyGarrison = EnemyGarrison;
    Sector.IntelConfidence = IntelConfidence;
    Sector.CivilianSupport = CivilianSupport;
    Sector.FriendlyStrength = FriendlyStrength;
    Sector.EnemyStrength = EnemyStrength;
    Sector.Supply = Supply;
    Sector.ReinforcementRate = ReinforcementRate;

    for (const FName ConnectedSectorID : ConnectedSectorIDs)
    {
        Sector.ConnectedSectorIDs.Add(ConnectedSectorID);
    }

    return Sector;
}
}

void UBHWarSubsystem::Initialize(
    FSubsystemCollectionBase& Collection
)
{
    Super::Initialize(Collection);
    ResetCampaign();
}

void UBHWarSubsystem::Deinitialize()
{
    SectorStates.Reset();
    SupplyConvoys.Reset();
    GarrisonTransfers.Reset();
    FriendlyManpowerReserve = 0;
    EnemyManpowerReserve = 0;
    FriendlyRecruitmentProgress = 0.0f;
    EnemyRecruitmentProgress = 0.0f;
    RecentWarEvents.Reset();
    CampaignOutcome = EBHWarCampaignOutcome::Ongoing;
    SimulationAccumulator = 0.0f;
    CommittedOperationSectorID = NAME_None;
    CommittedOperationID = NAME_None;
    CommittedOperationTargetID = NAME_None;
    CommittedOperationSupplySourceSectorID = NAME_None;
    CommittedOperationEnemySourceSectorID = NAME_None;
    CommittedOperationType = EBHWarPriorityType::None;
    Super::Deinitialize();
}

void UBHWarSubsystem::Tick(float DeltaTime)
{
    const UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !World->IsGameWorld() ||
        UGameplayStatics::IsGamePaused(World))
    {
        return;
    }

    const float Interval =
        GetCurrentSimulationIntervalSeconds();
    const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);

    if (World->GetNetMode() == NM_Client)
    {
        SimulationAccumulator = FMath::Min(
            SimulationAccumulator + SafeDeltaTime,
            FMath::Max(0.0f, Interval - KINDA_SMALL_NUMBER)
        );
        return;
    }

    UpdateCommittedConvoyOperationDeadline(SafeDeltaTime);

    SimulationAccumulator += SafeDeltaTime;

    while (SimulationAccumulator >= Interval)
    {
        SimulationAccumulator -= Interval;
        AdvanceWarTurn();
    }
}

TStatId UBHWarSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(
        UBHWarSubsystem,
        STATGROUP_Tickables
    );
}

bool UBHWarSubsystem::IsTickable() const
{
    return !IsTemplate() && IsValid(GetGameInstance());
}

bool UBHWarSubsystem::IsTickableWhenPaused() const
{
    return false;
}

FBHWarStateSnapshot
UBHWarSubsystem::CaptureReplicatedSnapshot(
    int32 Revision
) const
{
    FBHWarStateSnapshot Snapshot;
    Snapshot.Revision = Revision;
    Snapshot.bInitialized = true;
    Snapshot.CampaignDifficulty = CampaignDifficulty;
    Snapshot.CampaignProgression = CampaignProgression;
    Snapshot.SectorStates = SectorStates;
    Snapshot.SupplyConvoys = SupplyConvoys;
    Snapshot.GarrisonTransfers = GarrisonTransfers;
    Snapshot.RecentWarEvents = RecentWarEvents;
    Snapshot.FriendlyManpowerReserve =
        FriendlyManpowerReserve;
    Snapshot.EnemyManpowerReserve = EnemyManpowerReserve;
    Snapshot.FriendlyRecruitmentProgress =
        FriendlyRecruitmentProgress;
    Snapshot.EnemyRecruitmentProgress =
        EnemyRecruitmentProgress;
    Snapshot.TurnNumber = TurnNumber;
    Snapshot.SimulationAccumulator = SimulationAccumulator;
    Snapshot.PrioritySectorID = PrioritySectorID;
    Snapshot.PriorityType = PriorityType;
    Snapshot.PriorityReasonText = PriorityReasonText;
    Snapshot.CommittedOperationSectorID =
        CommittedOperationSectorID;
    Snapshot.CommittedOperationID = CommittedOperationID;
    Snapshot.CommittedOperationTargetID =
        CommittedOperationTargetID;
    Snapshot.CommittedOperationSupplySourceSectorID =
        CommittedOperationSupplySourceSectorID;
    Snapshot.CommittedOperationEnemySourceSectorID =
        CommittedOperationEnemySourceSectorID;
    Snapshot.CommittedOperationType =
        CommittedOperationType;
    Snapshot.CampaignOutcome = CampaignOutcome;
    return Snapshot;
}

bool UBHWarSubsystem::ApplyReplicatedSnapshot(
    const FBHWarStateSnapshot& Snapshot
)
{
    const UWorld* World = GetWorld();

    if (!Snapshot.bInitialized ||
        Snapshot.Revision < 0 ||
        Snapshot.SectorStates.IsEmpty() ||
        (IsValid(World) && World->GetNetMode() != NM_Client))
    {
        return false;
    }

    if (LastAppliedReplicatedSnapshotRevision != INDEX_NONE &&
        Snapshot.Revision < LastAppliedReplicatedSnapshotRevision)
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT(
                "BH_REPLICATION_SNAPSHOT_REJECTED_STALE revision=%d "
                "last_applied=%d"
            ),
            Snapshot.Revision,
            LastAppliedReplicatedSnapshotRevision
        );
        return false;
    }

    SectorStates = Snapshot.SectorStates;
    CampaignDifficulty = BHDifficulty::Sanitize(
        Snapshot.CampaignDifficulty
    );
    CampaignProgression = Snapshot.CampaignProgression;
    SupplyConvoys = Snapshot.SupplyConvoys;
    GarrisonTransfers = Snapshot.GarrisonTransfers;
    RecentWarEvents = Snapshot.RecentWarEvents;
    FriendlyManpowerReserve =
        Snapshot.FriendlyManpowerReserve;
    EnemyManpowerReserve = Snapshot.EnemyManpowerReserve;
    FriendlyRecruitmentProgress =
        Snapshot.FriendlyRecruitmentProgress;
    EnemyRecruitmentProgress =
        Snapshot.EnemyRecruitmentProgress;
    TurnNumber = Snapshot.TurnNumber;
    SimulationAccumulator = Snapshot.SimulationAccumulator;
    PrioritySectorID = Snapshot.PrioritySectorID;
    PriorityType = Snapshot.PriorityType;
    PriorityReasonText = Snapshot.PriorityReasonText;
    CommittedOperationSectorID =
        Snapshot.CommittedOperationSectorID;
    CommittedOperationID = Snapshot.CommittedOperationID;
    CommittedOperationTargetID =
        Snapshot.CommittedOperationTargetID;
    CommittedOperationSupplySourceSectorID =
        Snapshot.CommittedOperationSupplySourceSectorID;
    CommittedOperationEnemySourceSectorID =
        Snapshot.CommittedOperationEnemySourceSectorID;
    CommittedOperationType =
        Snapshot.CommittedOperationType;
    CampaignOutcome = Snapshot.CampaignOutcome;
    LastAppliedReplicatedSnapshotRevision = Snapshot.Revision;
    TGuardValue<bool> ApplyingSnapshotGuard(
        bApplyingReplicatedSnapshot,
        true
    );
    BroadcastWarState();
    return true;
}

void UBHWarSubsystem::ResetCampaign()
{
    if (!CanMutateWarState())
    {
        return;
    }

    CommittedOperationSectorID = NAME_None;
    CommittedOperationID = NAME_None;
    CommittedOperationTargetID = NAME_None;
    CommittedOperationSupplySourceSectorID = NAME_None;
    CommittedOperationEnemySourceSectorID = NAME_None;
    CommittedOperationType = EBHWarPriorityType::None;
    CampaignOutcome = EBHWarCampaignOutcome::Ongoing;
    RecentWarEvents.Reset();
    BuildDefaultCampaign();
    TurnNumber = 0;
    SimulationAccumulator = 0.0f;
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Persistent War initialized with %d sectors."),
        SectorStates.Num()
    );
}

void UBHWarSubsystem::BuildDefaultCampaign()
{
    CampaignOutcome = EBHWarCampaignOutcome::Ongoing;
    CampaignProgression = FBHCampaignProgressionState();
    SectorStates.Reset();
    SupplyConvoys.Reset();
    GarrisonTransfers.Reset();
    FriendlyManpowerReserve = InitialFriendlyManpowerReserve;
    EnemyManpowerReserve = InitialEnemyManpowerReserve;
    FriendlyRecruitmentProgress = 0.0f;
    EnemyRecruitmentProgress = 0.0f;

    SectorStates.Add(MakeSector(
        TEXT("NorthPass"),
        TEXT("North Pass"),
        EBHWarFaction::Neutral,
        EBHWarSiteType::Checkpoint,
        8,
        2,
        4,
        25.0f,
        45.0f,
        15.0f,
        30.0f,
        35.0f,
        2.5f,
        { TEXT("DovrenVillage"), TEXT("KoronaCrossroads") }
    ));
    SectorStates.Add(MakeSector(
        TEXT("KoronaCrossroads"),
        TEXT("Korona Crossroads"),
        EBHWarFaction::Enemy,
        EBHWarSiteType::Town,
        12,
        1,
        9,
        20.0f,
        30.0f,
        8.0f,
        65.0f,
        60.0f,
        3.0f,
        {
            TEXT("NorthPass"),
            TEXT("DovrenVillage"),
            TEXT("SouthBridge")
        }
    ));
    SectorStates.Add(MakeSector(
        TEXT("EasternDepot"),
        TEXT("Eastern Logistics Depot"),
        EBHWarFaction::Enemy,
        EBHWarSiteType::LogisticsDepot,
        14,
        0,
        12,
        10.0f,
        20.0f,
        0.0f,
        90.0f,
        90.0f,
        4.0f,
        { TEXT("SouthBridge") }
    ));
    SectorStates.Add(MakeSector(
        TEXT("WesternFOB"),
        TEXT("Western Forward Base"),
        EBHWarFaction::Friendly,
        EBHWarSiteType::Headquarters,
        14,
        10,
        0,
        100.0f,
        80.0f,
        80.0f,
        0.0f,
        80.0f,
        4.0f,
        { TEXT("DovrenVillage") }
    ));
    SectorStates.Add(MakeSector(
        TEXT("DovrenVillage"),
        TEXT("Dovren Village"),
        EBHWarFaction::Friendly,
        EBHWarSiteType::Village,
        8,
        6,
        1,
        90.0f,
        75.0f,
        55.0f,
        10.0f,
        55.0f,
        3.0f,
        {
            TEXT("WesternFOB"),
            TEXT("NorthPass"),
            TEXT("KoronaCrossroads")
        }
    ));
    SectorStates.Add(MakeSector(
        TEXT("SouthBridge"),
        TEXT("South Bridge"),
        EBHWarFaction::Enemy,
        EBHWarSiteType::Bridge,
        8,
        1,
        7,
        30.0f,
        40.0f,
        5.0f,
        50.0f,
        50.0f,
        3.0f,
        { TEXT("KoronaCrossroads"), TEXT("EasternDepot") }
    ));
}

void UBHWarSubsystem::AdvanceWarTurn()
{
    if (!CanMutateWarState())
    {
        return;
    }

    if (SectorStates.IsEmpty())
    {
        ResetCampaign();
        return;
    }

    if (IsCampaignResolved())
    {
        return;
    }

    ++TurnNumber;
    SimulateFrontlines();
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Log,
        TEXT(
            "War turn %d complete. Priority: %s %s. "
            "Friendly control: %.0f%%."
        ),
        TurnNumber,
        PriorityType == EBHWarPriorityType::Defend
            ? TEXT("DEFEND")
            : (PriorityType == EBHWarPriorityType::Attack
                ? TEXT("ATTACK")
                : TEXT("NONE")),
        *PrioritySectorID.ToString(),
        GetFriendlyControlPercentage()
    );
}

void UBHWarSubsystem::SimulateFrontlines()
{
    FRandomStream RandomStream(7319 + (TurnNumber * 7919));
    AdvanceSupplyConvoys();
    AdvanceGarrisonTransfers();
    TArray<float> FriendlyIncrements;
    TArray<float> EnemyIncrements;
    FriendlyIncrements.Init(0.0f, SectorStates.Num());
    EnemyIncrements.Init(0.0f, SectorStates.Num());

    for (int32 SectorIndex = 0;
        SectorIndex < SectorStates.Num();
        ++SectorIndex)
    {
        FBHWarSectorState& Sector = SectorStates[SectorIndex];

        if (IsCommittedOperationSector(Sector.SectorID))
        {
            continue;
        }

        Sector.EnemyResponsePressure = FMath::Max(
            0.0f,
            Sector.EnemyResponsePressure -
                EnemyResponseDecayPerTurn
        );

        const float Reinforcement =
            GetSectorReinforcementPerTurn(Sector.SectorID);

        if (Sector.Owner == EBHWarFaction::Friendly)
        {
            Sector.FriendlyStrength += Reinforcement;
        }
        else if (Sector.Owner == EBHWarFaction::Enemy)
        {
            Sector.EnemyStrength += Reinforcement;
        }

        if (Sector.Owner != EBHWarFaction::Neutral &&
            Sector.ReinforcementRate > KINDA_SMALL_NUMBER &&
            Reinforcement <= KINDA_SMALL_NUMBER)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_WAR_REINFORCEMENT_STARVED sector=%s "
                    "owner=%d supply=%.1f"
                ),
                *Sector.SectorID.ToString(),
                static_cast<int32>(Sector.Owner),
                Sector.Supply
            );
        }

        const float SupplyChange =
            GetSectorSupplyChangePerTurn(Sector.SectorID);
        Sector.Supply += SupplyChange;
        const float IsolationAttrition =
            GetSectorIsolationAttritionPerTurn(
                Sector.SectorID
            );

        if (IsolationAttrition > KINDA_SMALL_NUMBER)
        {
            if (Sector.Owner == EBHWarFaction::Friendly)
            {
                Sector.FriendlyStrength -= IsolationAttrition;
            }
            else if (Sector.Owner == EBHWarFaction::Enemy)
            {
                Sector.EnemyStrength -= IsolationAttrition;
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_WAR_ISOLATION_ATTRITION sector=%s "
                    "owner=%d strength_loss=%.2f supply=%.1f"
                ),
                *Sector.SectorID.ToString(),
                static_cast<int32>(Sector.Owner),
                IsolationAttrition,
                Sector.Supply
            );
        }

        if (SupplyChange > KINDA_SMALL_NUMBER)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_WAR_SUPPLY_PRODUCTION sector=%s "
                    "amount=%.1f owner=%d"
                ),
                *Sector.SectorID.ToString(),
                SupplyChange,
                static_cast<int32>(Sector.Owner)
            );
        }
    }

    RedistributeSupplies();
    RecruitFactionManpower();
    ReinforceGarrisons();
    UpdateCivilianSupport();
    DecaySectorIntelligence();

    for (int32 SourceIndex = 0;
        SourceIndex < SectorStates.Num();
        ++SourceIndex)
    {
        const FBHWarSectorState& Source =
            SectorStates[SourceIndex];

        for (const FName ConnectedSectorID :
            Source.ConnectedSectorIDs)
        {
            const int32 TargetIndex =
                FindSectorIndex(ConnectedSectorID);

            if (TargetIndex <= SourceIndex ||
                !SectorStates.IsValidIndex(TargetIndex))
            {
                continue;
            }

            const FBHWarSectorState& Target =
                SectorStates[TargetIndex];

            if (IsCommittedOperationSector(Source.SectorID) ||
                IsCommittedOperationSector(Target.SectorID) ||
                Source.Owner == Target.Owner ||
                Source.Owner == EBHWarFaction::Neutral ||
                Target.Owner == EBHWarFaction::Neutral)
            {
                continue;
            }

            const int32 FriendlyIndex =
                Source.Owner == EBHWarFaction::Friendly
                    ? SourceIndex
                    : TargetIndex;
            const int32 EnemyIndex =
                Source.Owner == EBHWarFaction::Enemy
                    ? SourceIndex
                    : TargetIndex;
            const float FriendlyPressure =
                GetSectorEffectiveStrength(
                    SectorStates[FriendlyIndex].SectorID,
                    EBHWarFaction::Friendly
                ) *
                0.04f *
                RandomStream.FRandRange(0.85f, 1.15f);
            const float EnemyPressure =
                GetSectorEffectiveStrength(
                    SectorStates[EnemyIndex].SectorID,
                    EBHWarFaction::Enemy
                ) *
                0.04f *
                RandomStream.FRandRange(0.85f, 1.15f);

            FriendlyIncrements[EnemyIndex] += FriendlyPressure;
            EnemyIncrements[FriendlyIndex] += EnemyPressure;
            SectorStates[FriendlyIndex].LastBattleTurn = TurnNumber;
            SectorStates[EnemyIndex].LastBattleTurn = TurnNumber;
        }
    }

    for (int32 SectorIndex = 0;
        SectorIndex < SectorStates.Num();
        ++SectorIndex)
    {
        SectorStates[SectorIndex].FriendlyStrength +=
            FriendlyIncrements[SectorIndex];
        SectorStates[SectorIndex].EnemyStrength +=
            EnemyIncrements[SectorIndex];
    }

    ResolveContestedSectors(RandomStream);

    for (FBHWarSectorState& Sector : SectorStates)
    {
        ClampSectorState(Sector);
    }
}

float UBHWarSubsystem::GetSectorSupplyChangePerTurn(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return 0.0f;
    }

    const FBHWarSectorState& Sector = SectorStates[SectorIndex];

    if (Sector.Owner == EBHWarFaction::Neutral)
    {
        return 0.0f;
    }

    if (IsFrontlineSector(SectorIndex))
    {
        return FrontlineSupplyConsumptionPerTurn;
    }

    if (IsLogisticsHub(SectorID))
    {
        return LogisticsHubSupplyProductionPerTurn;
    }

    return IsSectorConnectedToFactionLogistics(SectorID)
        ? StandardSupplyProductionPerTurn
        : IsolatedSupplyAttritionPerTurn;
}

float UBHWarSubsystem::GetSectorIsolationAttritionPerTurn(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return 0.0f;
    }

    const FBHWarSectorState& Sector = SectorStates[SectorIndex];

    if (Sector.Owner == EBHWarFaction::Neutral ||
        IsSectorConnectedToFactionLogistics(SectorID) ||
        Sector.Supply >= IsolatedAttritionSupplyThreshold)
    {
        return 0.0f;
    }

    const float SupplyDeficit = 1.0f - FMath::Clamp(
        Sector.Supply / IsolatedAttritionSupplyThreshold,
        0.0f,
        1.0f
    );

    return FMath::Lerp(
        MinimumIsolationStrengthAttrition,
        MaximumIsolationStrengthAttrition,
        SupplyDeficit
    );
}

bool UBHWarSubsystem::IsLogisticsHubSector(
    FName SectorID
) const
{
    return IsLogisticsHub(SectorID);
}

bool UBHWarSubsystem::IsSectorConnectedToFactionLogistics(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return false;
    }

    const EBHWarFaction Faction =
        SectorStates[SectorIndex].Owner;

    if (Faction == EBHWarFaction::Neutral)
    {
        return false;
    }

    TArray<int32> SearchQueue;
    TSet<int32> VisitedIndices;
    SearchQueue.Add(SectorIndex);
    VisitedIndices.Add(SectorIndex);

    for (int32 QueueIndex = 0;
        QueueIndex < SearchQueue.Num();
        ++QueueIndex)
    {
        const int32 CurrentIndex = SearchQueue[QueueIndex];
        const FBHWarSectorState& Current =
            SectorStates[CurrentIndex];

        if (IsLogisticsHub(Current.SectorID))
        {
            return true;
        }

        for (const FName ConnectedSectorID :
            Current.ConnectedSectorIDs)
        {
            const int32 ConnectedIndex =
                FindSectorIndex(ConnectedSectorID);

            if (!SectorStates.IsValidIndex(ConnectedIndex) ||
                VisitedIndices.Contains(ConnectedIndex) ||
                SectorStates[ConnectedIndex].Owner != Faction)
            {
                continue;
            }

            VisitedIndices.Add(ConnectedIndex);
            SearchQueue.Add(ConnectedIndex);
        }
    }

    return false;
}

float UBHWarSubsystem::GetSectorCombatSupplyFactor(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return 0.0f;
    }

    const float SupplyIncludingCaches = FMath::Clamp(
        SectorStates[SectorIndex].Supply +
            GetSectorFortificationSupplyBonus(SectorID),
        0.0f,
        MaximumSectorSupply
    );

    return CalculateCombatSupplyFactor(SupplyIncludingCaches);
}

float UBHWarSubsystem::GetSectorEffectiveStrength(
    FName SectorID,
    EBHWarFaction ForceFaction
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        (ForceFaction != EBHWarFaction::Friendly &&
            ForceFaction != EBHWarFaction::Enemy))
    {
        return 0.0f;
    }

    const FBHWarSectorState& Sector =
        SectorStates[SectorIndex];
    const float RawStrength =
        ForceFaction == EBHWarFaction::Friendly
            ? Sector.FriendlyStrength
            : Sector.EnemyStrength;
    const int32 Garrison =
        ForceFaction == EBHWarFaction::Friendly
            ? Sector.FriendlyGarrison
            : Sector.EnemyGarrison;
    const float SupplyFactor =
        Sector.Owner == ForceFaction
            ? GetSectorCombatSupplyFactor(SectorID)
            : 1.0f;

    return (
        RawStrength +
        (Garrison * GarrisonStrengthPerUnit)
    ) * SupplyFactor;
}

float UBHWarSubsystem::GetSectorReinforcementPerTurn(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return 0.0f;
    }

    FBHWarSectorState SimulatedSector = SectorStates[SectorIndex];
    SimulatedSector.Supply = FMath::Clamp(
        SimulatedSector.Supply + GetSectorFortificationSupplyBonus(SectorID),
        0.0f,
        MaximumSectorSupply
    );

    return CalculateReinforcementAmount(
        SimulatedSector,
        IsSectorConnectedToFactionLogistics(SectorID)
    );
}

void UBHWarSubsystem::AdvanceSupplyConvoys()
{
    for (int32 ConvoyIndex = SupplyConvoys.Num() - 1;
        ConvoyIndex >= 0;
        --ConvoyIndex)
    {
        FBHWarSupplyConvoyState& Convoy =
            SupplyConvoys[ConvoyIndex];
        --Convoy.TurnsRemaining;

        if (Convoy.TurnsRemaining > 0)
        {
            continue;
        }

        const int32 SourceIndex =
            FindSectorIndex(Convoy.SourceSectorID);
        const int32 DestinationIndex =
            FindSectorIndex(Convoy.DestinationSectorID);
        const bool bIsCivilianAid =
            Convoy.CargoType ==
            EBHWarConvoyCargoType::CivilianAid;
        bool bRouteValid =
            SectorStates.IsValidIndex(SourceIndex) &&
            SectorStates.IsValidIndex(DestinationIndex) &&
            SectorStates[SourceIndex].Owner == Convoy.Owner;

        if (bRouteValid && bIsCivilianAid)
        {
            TArray<int32> SearchQueue;
            TSet<int32> VisitedIndices;
            SearchQueue.Add(SourceIndex);
            VisitedIndices.Add(SourceIndex);
            bRouteValid = SourceIndex == DestinationIndex;

            for (int32 QueueIndex = 0;
                QueueIndex < SearchQueue.Num() && !bRouteValid;
                ++QueueIndex)
            {
                const FBHWarSectorState& Current =
                    SectorStates[SearchQueue[QueueIndex]];

                for (const FName ConnectedSectorID :
                    Current.ConnectedSectorIDs)
                {
                    const int32 ConnectedIndex =
                        FindSectorIndex(ConnectedSectorID);

                    if (!SectorStates.IsValidIndex(ConnectedIndex) ||
                        VisitedIndices.Contains(ConnectedIndex))
                    {
                        continue;
                    }

                    if (ConnectedIndex == DestinationIndex)
                    {
                        bRouteValid = true;
                        break;
                    }

                    if (SectorStates[ConnectedIndex].Owner ==
                        Convoy.Owner)
                    {
                        VisitedIndices.Add(ConnectedIndex);
                        SearchQueue.Add(ConnectedIndex);
                    }
                }
            }
        }
        else if (bRouteValid)
        {
            bRouteValid =
                SectorStates[SourceIndex].ConnectedSectorIDs.Contains(
                    Convoy.DestinationSectorID
                ) &&
                SectorStates[DestinationIndex].Owner == Convoy.Owner;
        }

        if (bRouteValid)
        {
            FBHWarSectorState& Destination =
                SectorStates[DestinationIndex];

            if (bIsCivilianAid)
            {
                const float PreviousSupport =
                    Destination.CivilianSupport;
                const float PreviousIntel =
                    Destination.IntelConfidence;
                const float PreviousResponse =
                    Destination.EnemyResponsePressure;
                Destination.CivilianSupport +=
                    CivilianAidSupportGain;
                Destination.IntelConfidence +=
                    CivilianAidIntelGain;

                if (Destination.Owner ==
                    EBHWarFaction::Friendly)
                {
                    Destination.EnemyResponsePressure -=
                        CivilianAidFriendlyResponseReduction;
                }
                else
                {
                    Destination.EnemyResponsePressure +=
                        CivilianAidHostileExposurePressure;
                }

                ClampSectorState(Destination);
                RecordEnemyResponseEscalation(
                    Destination,
                    PreviousResponse
                );
                RecordWarEvent(
                    TEXT("CivilianAidDelivered"),
                    Convoy.DestinationSectorID,
                    FString::Printf(
                        TEXT(
                            "Aid reached %s; local support rose "
                            "to %.0f%% and intelligence confidence "
                            "to %.0f%%"
                        ),
                        *Destination.DisplayName.ToString(),
                        Destination.CivilianSupport,
                        Destination.IntelConfidence
                    )
                );

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_CIVILIAN_AID_DELIVERED id=%s "
                        "target=%s source=%s "
                        "support=%.1f->%.1f intel=%.1f->%.1f "
                        "response=%.1f->%.1f"
                    ),
                    *Convoy.ConvoyID.ToString(),
                    *Convoy.DestinationSectorID.ToString(),
                    *Convoy.SourceSectorID.ToString(),
                    PreviousSupport,
                    Destination.CivilianSupport,
                    PreviousIntel,
                    Destination.IntelConfidence,
                    PreviousResponse,
                    Destination.EnemyResponsePressure
                );
            }
            else
            {
                Destination.Supply += Convoy.SupplyPayload;
                ClampSectorState(Destination);
                const FString FactionLabel =
                    Convoy.Owner == EBHWarFaction::Friendly
                        ? TEXT("Friendly")
                        : TEXT("Enemy");
                RecordWarEvent(
                    TEXT("ConvoyArrived"),
                    Convoy.DestinationSectorID,
                    FString::Printf(
                        TEXT(
                            "%s convoy delivered +%.0f supply "
                            "to %s"
                        ),
                        *FactionLabel,
                        Convoy.SupplyPayload,
                        *Destination.DisplayName.ToString()
                    )
                );

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_WAR_CONVOY_ARRIVED id=%s from=%s "
                        "to=%s payload=%.1f owner=%d"
                    ),
                    *Convoy.ConvoyID.ToString(),
                    *Convoy.SourceSectorID.ToString(),
                    *Convoy.DestinationSectorID.ToString(),
                    Convoy.SupplyPayload,
                    static_cast<int32>(Convoy.Owner)
                );
            }
        }
        else
        {
            const FString FactionLabel =
                Convoy.Owner == EBHWarFaction::Friendly
                    ? TEXT("Friendly")
                    : TEXT("Enemy");
            const FBHWarSectorState Destination =
                GetSectorState(Convoy.DestinationSectorID);
            RecordWarEvent(
                TEXT("ConvoyLost"),
                Convoy.DestinationSectorID,
                bIsCivilianAid
                    ? FString::Printf(
                        TEXT(
                            "Civilian aid convoy lost en route "
                            "to %s"
                        ),
                        Destination.SectorID.IsNone()
                            ? *Convoy.DestinationSectorID.ToString()
                            : *Destination.DisplayName.ToString()
                    )
                    : FString::Printf(
                        TEXT("%s convoy lost en route to %s"),
                        *FactionLabel,
                        Destination.SectorID.IsNone()
                            ? *Convoy.DestinationSectorID.ToString()
                            : *Destination.DisplayName.ToString()
                    )
            );

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_WAR_CONVOY_LOST id=%s from=%s "
                    "to=%s payload=%.1f"
                ),
                *Convoy.ConvoyID.ToString(),
                *Convoy.SourceSectorID.ToString(),
                *Convoy.DestinationSectorID.ToString(),
                Convoy.SupplyPayload
            );
        }

        SupplyConvoys.RemoveAtSwap(ConvoyIndex);
    }
}

void UBHWarSubsystem::AdvanceGarrisonTransfers()
{
    for (int32 TransferIndex = GarrisonTransfers.Num() - 1;
        TransferIndex >= 0;
        --TransferIndex)
    {
        FBHWarGarrisonTransferState& Transfer =
            GarrisonTransfers[TransferIndex];
        --Transfer.TurnsRemaining;

        if (Transfer.TurnsRemaining > 0)
        {
            continue;
        }

        const int32 SourceIndex =
            FindSectorIndex(Transfer.SourceSectorID);
        const int32 DestinationIndex =
            FindSectorIndex(Transfer.DestinationSectorID);
        const bool bDestinationAvailable =
            SectorStates.IsValidIndex(DestinationIndex) &&
            SectorStates[DestinationIndex].Owner ==
                EBHWarFaction::Friendly &&
            GetFriendlyRouteDistance(
                Transfer.SourceSectorID,
                Transfer.DestinationSectorID
            ) > 0;

        if (bDestinationAvailable)
        {
            FBHWarSectorState& Destination =
                SectorStates[DestinationIndex];
            const int32 ArrivingCount = FMath::Min(
                Transfer.TroopCount,
                FMath::Max(
                    0,
                    Destination.GarrisonCapacity -
                        Destination.FriendlyGarrison
                )
            );

            Destination.FriendlyGarrison += ArrivingCount;
            ClampSectorState(Destination);

            if (ArrivingCount > 0)
            {
                RecordWarEvent(
                    TEXT("GarrisonTransferArrived"),
                    Destination.SectorID,
                    FString::Printf(
                        TEXT(
                            "%d reserve troops arrived at %s from %s"
                        ),
                        ArrivingCount,
                        *Destination.DisplayName.ToString(),
                        *GetSectorState(
                            Transfer.SourceSectorID
                        ).DisplayName.ToString()
                    )
                );

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "BH_GARRISON_TRANSFER_ARRIVED id=%s "
                        "source=%s destination=%s troops=%d"
                    ),
                    *Transfer.TransferID.ToString(),
                    *Transfer.SourceSectorID.ToString(),
                    *Transfer.DestinationSectorID.ToString(),
                    ArrivingCount
                );
            }

            const int32 OverflowCount =
                Transfer.TroopCount - ArrivingCount;

            if (OverflowCount > 0 &&
                SectorStates.IsValidIndex(SourceIndex) &&
                SectorStates[SourceIndex].Owner ==
                    EBHWarFaction::Friendly)
            {
                FBHWarSectorState& Source =
                    SectorStates[SourceIndex];
                Source.FriendlyGarrison += OverflowCount;
                ClampSectorState(Source);
            }
        }
        else
        {
            int32 ReturnedCount = 0;

            if (SectorStates.IsValidIndex(SourceIndex) &&
                SectorStates[SourceIndex].Owner ==
                    EBHWarFaction::Friendly)
            {
                FBHWarSectorState& Source =
                    SectorStates[SourceIndex];
                ReturnedCount = FMath::Min(
                    Transfer.TroopCount,
                    FMath::Max(
                        0,
                        Source.GarrisonCapacity -
                            Source.FriendlyGarrison
                    )
                );
                Source.FriendlyGarrison += ReturnedCount;
                ClampSectorState(Source);
            }

            RecordWarEvent(
                TEXT("GarrisonTransferAborted"),
                Transfer.DestinationSectorID,
                FString::Printf(
                    TEXT(
                        "Reserve movement to %s was aborted; "
                        "%d troops returned"
                    ),
                    *Transfer.DestinationSectorID.ToString(),
                    ReturnedCount
                )
            );

            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_GARRISON_TRANSFER_ABORTED id=%s "
                    "source=%s destination=%s returned=%d"
                ),
                *Transfer.TransferID.ToString(),
                *Transfer.SourceSectorID.ToString(),
                *Transfer.DestinationSectorID.ToString(),
                ReturnedCount
            );
        }

        GarrisonTransfers.RemoveAtSwap(TransferIndex);
    }
}

void UBHWarSubsystem::RedistributeSupplies()
{
    int32 ConvoySequence = 0;

    for (int32 FirstIndex = 0;
        FirstIndex < SectorStates.Num();
        ++FirstIndex)
    {
        const FBHWarSectorState& First =
            SectorStates[FirstIndex];

        for (const FName ConnectedSectorID :
            First.ConnectedSectorIDs)
        {
            const int32 SecondIndex =
                FindSectorIndex(ConnectedSectorID);

            if (SecondIndex <= FirstIndex ||
                !SectorStates.IsValidIndex(SecondIndex))
            {
                continue;
            }

            const FBHWarSectorState& Second =
                SectorStates[SecondIndex];

            if (IsCommittedOperationSector(First.SectorID) ||
                IsCommittedOperationSector(Second.SectorID) ||
                First.Owner == EBHWarFaction::Neutral ||
                First.Owner != Second.Owner)
            {
                continue;
            }

            const float FirstEffectiveSupply =
                First.Supply;
            const float SecondEffectiveSupply =
                Second.Supply;
            const int32 SourceIndex =
                FirstEffectiveSupply >= SecondEffectiveSupply
                    ? FirstIndex
                    : SecondIndex;
            const int32 TargetIndex =
                SourceIndex == FirstIndex
                    ? SecondIndex
                    : FirstIndex;
            const float SourceSupply =
                SectorStates[SourceIndex].Supply;
            const float TargetSupply =
                SectorStates[TargetIndex].Supply;
            const float SupplyDifference =
                SourceSupply - TargetSupply;
            const float AvailableSupply = FMath::Max(
                0.0f,
                SourceSupply - MinimumSupplyReserve
            );
            const float TransferAmount = FMath::Min3(
                MaximumSupplyTransferPerTurn,
                AvailableSupply,
                FMath::Max(
                    0.0f,
                    (SupplyDifference -
                        MinimumSupplyTransferDifference) *
                        0.5f
                )
            );

            if (TransferAmount <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            FBHWarSectorState& SourceSector =
                SectorStates[SourceIndex];
            const FBHWarSectorState& TargetSector =
                SectorStates[TargetIndex];
            SourceSector.Supply -= TransferAmount;
            ClampSectorState(SourceSector);

            FBHWarSupplyConvoyState Convoy;
            Convoy.ConvoyID = FName(*FString::Printf(
                TEXT("Convoy_%d_%d_%s_%s"),
                TurnNumber,
                ConvoySequence++,
                *SourceSector.SectorID.ToString(),
                *TargetSector.SectorID.ToString()
            ));
            Convoy.SourceSectorID = SourceSector.SectorID;
            Convoy.DestinationSectorID = TargetSector.SectorID;
            Convoy.Owner = SourceSector.Owner;
            Convoy.SupplyPayload = TransferAmount;
            Convoy.TurnsRemaining = 1;
            Convoy.DispatchTurn = TurnNumber;
            BHRouteOperations::Initialize(Convoy);
            SupplyConvoys.Add(Convoy);

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_WAR_CONVOY_DISPATCHED id=%s from=%s "
                    "to=%s payload=%.1f owner=%d eta_turns=%d"
                ),
                *Convoy.ConvoyID.ToString(),
                *Convoy.SourceSectorID.ToString(),
                *Convoy.DestinationSectorID.ToString(),
                Convoy.SupplyPayload,
                static_cast<int32>(Convoy.Owner),
                Convoy.TurnsRemaining
            );
        }
    }
}

void UBHWarSubsystem::ReinforceGarrisons()
{
    for (FBHWarSectorState& Sector : SectorStates)
    {
        if (Sector.Owner == EBHWarFaction::Neutral ||
            IsCommittedOperationSector(Sector.SectorID) ||
            Sector.GarrisonCapacity <= 0 ||
            Sector.Supply < MinimumGarrisonReinforcementSupply ||
            !IsSectorConnectedToFactionLogistics(
                Sector.SectorID
            ))
        {
            continue;
        }

        int32& Garrison =
            Sector.Owner == EBHWarFaction::Friendly
                ? Sector.FriendlyGarrison
                : Sector.EnemyGarrison;
        int32& ManpowerReserve =
            Sector.Owner == EBHWarFaction::Friendly
                ? FriendlyManpowerReserve
                : EnemyManpowerReserve;
        const int32 RallyDeploymentReserve =
            GetSectorRallyDeploymentReserve(Sector.SectorID);

        if (ManpowerReserve <= 0 && RallyDeploymentReserve <= 0)
        {
            continue;
        }

        const int32 MissingTroops =
            Sector.GarrisonCapacity - Garrison;
        if (MissingTroops <= 0)
        {
            continue;
        }
        const int32 AffordableTroops = FMath::FloorToInt(
            FMath::Max(
                0.0f,
                Sector.Supply - GarrisonSupplyReserve
            ) /
            GarrisonSupplyCostPerUnit
        );
        const int32 ReinforcementBudget = FMath::Clamp(
            FMath::CeilToInt(Sector.ReinforcementRate * 0.5f),
            1,
            MaximumGarrisonReinforcementsPerTurn
        );
        const int32 ReinforcementCapacity = FMath::Min(
            MissingTroops,
            ReinforcementBudget + FMath::Min(
                RallyDeploymentReserve,
                MaximumGarrisonReinforcementsPerTurn
            )
        );
        const int32 ReinforcementsByManpower = FMath::Min3(
            MissingTroops,
            AffordableTroops,
            FMath::Min(
                ReinforcementBudget,
                FMath::Max(0, ManpowerReserve)
            )
        );
        const int32 RemainingCapacity =
            ReinforcementCapacity - ReinforcementsByManpower;
        const int32 RallyReinforcementsRequested = FMath::Min(
            RemainingCapacity,
            RallyDeploymentReserve
        );
        const int32 RallyReinforcements =
            ConsumeSectorRallyDeployments(
                Sector.SectorID,
                RallyReinforcementsRequested
            );
        const int32 ReinforcedTroops = FMath::Clamp(
            ReinforcementsByManpower + RallyReinforcements,
            0,
            MissingTroops
        );

        if (ReinforcedTroops <= 0)
        {
            continue;
        }

        const float PreviousSupply = Sector.Supply;
        const int32 PreviousManpower = ManpowerReserve;
        Garrison += ReinforcedTroops;
        ManpowerReserve -= ReinforcementsByManpower;
        Sector.Supply -=
            ReinforcementsByManpower * GarrisonSupplyCostPerUnit;
        ClampSectorState(Sector);

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_GARRISON_REINFORCED sector=%s owner=%d "
                "troops=%d (manpower=%d, rally=%d) "
                "garrison=%d/%d supply=%.1f->%.1f "
                "manpower=%d->%d"
            ),
            *Sector.SectorID.ToString(),
            static_cast<int32>(Sector.Owner),
            ReinforcedTroops,
            ReinforcementsByManpower,
            RallyReinforcements,
            Garrison,
            Sector.GarrisonCapacity,
            PreviousSupply,
            Sector.Supply,
            PreviousManpower,
            ManpowerReserve
        );
    }
}

void UBHWarSubsystem::RecruitFactionManpower()
{
    const auto AdvanceRecruitment =
        [this](
            EBHWarFaction Faction,
            int32& Reserve,
            float& Progress)
        {
            if (Reserve >= MaximumFactionManpowerReserve)
            {
                Progress = 0.0f;
                return;
            }

            Progress += GetFactionRecruitmentPerTurn(Faction);
            const int32 Recruited = FMath::Min(
                FMath::FloorToInt(Progress),
                MaximumFactionManpowerReserve - Reserve
            );

            if (Recruited <= 0)
            {
                return;
            }

            Reserve += Recruited;
            Progress -= Recruited;

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_MANPOWER_RECRUITED faction=%d count=%d "
                    "reserve=%d progress=%.2f"
                ),
                static_cast<int32>(Faction),
                Recruited,
                Reserve,
                Progress
            );
        };

    AdvanceRecruitment(
        EBHWarFaction::Friendly,
        FriendlyManpowerReserve,
        FriendlyRecruitmentProgress
    );
    AdvanceRecruitment(
        EBHWarFaction::Enemy,
        EnemyManpowerReserve,
        EnemyRecruitmentProgress
    );
}

void UBHWarSubsystem::UpdateCivilianSupport()
{
    for (FBHWarSectorState& Sector : SectorStates)
    {
        const float SupportTarget =
            Sector.Owner == EBHWarFaction::Friendly
                ? MaximumCivilianSupport
                : Sector.Owner == EBHWarFaction::Enemy
                    ? 0.0f
                    : NeutralCivilianSupport;

        Sector.CivilianSupport = FMath::FInterpConstantTo(
            Sector.CivilianSupport,
            SupportTarget,
            1.0f,
            CivilianSupportDriftPerTurn
        );
        ClampSectorState(Sector);
    }
}

void UBHWarSubsystem::DecaySectorIntelligence()
{
    for (FBHWarSectorState& Sector : SectorStates)
    {
        if (Sector.Owner == EBHWarFaction::Friendly)
        {
            Sector.IntelConfidence = FMath::Max(
                Sector.IntelConfidence,
                FriendlySectorIntelFloor
            );
            continue;
        }

        const float CivilianIntelFloor =
            Sector.CivilianSupport * CivilianIntelContribution;
        Sector.IntelConfidence = FMath::Max(
            CivilianIntelFloor,
            Sector.IntelConfidence - IntelligenceDecayPerTurn
        );
    }
}

void UBHWarSubsystem::ResolveContestedSectors(
    FRandomStream& RandomStream
)
{
    for (FBHWarSectorState& Sector : SectorStates)
    {
        if (IsCommittedOperationSector(Sector.SectorID))
        {
            continue;
        }

        if (Sector.FriendlyStrength > KINDA_SMALL_NUMBER &&
            Sector.EnemyStrength > KINDA_SMALL_NUMBER)
        {
            float FriendlyLoss = FMath::Min(
                Sector.FriendlyStrength,
                Sector.EnemyStrength *
                    0.08f *
                    RandomStream.FRandRange(0.8f, 1.2f)
            );
            float EnemyLoss = FMath::Min(
                Sector.EnemyStrength,
                Sector.FriendlyStrength *
                    0.08f *
                    RandomStream.FRandRange(0.8f, 1.2f)
            );

            const float FortificationDefense =
                GetSectorFortificationDefense(Sector.SectorID);
            const float FortificationCasualtyMultiplier =
                CalculateFortificationCasualtyMultiplier(
                    FortificationDefense);
            if (Sector.Owner == EBHWarFaction::Friendly)
            {
                FriendlyLoss *= FortificationCasualtyMultiplier;
            }
            else if (Sector.Owner == EBHWarFaction::Enemy)
            {
                EnemyLoss *= FortificationCasualtyMultiplier;
            }

            Sector.FriendlyStrength -= FriendlyLoss;
            Sector.EnemyStrength -= EnemyLoss;
        }

        const bool bFriendlyCapture =
            Sector.Owner != EBHWarFaction::Friendly &&
            Sector.FriendlyStrength >= CaptureMinimumStrength &&
            Sector.FriendlyStrength >
                Sector.EnemyStrength * 1.5f;
        const bool bEnemyCapture =
            Sector.Owner != EBHWarFaction::Enemy &&
            Sector.EnemyStrength >= CaptureMinimumStrength &&
            Sector.EnemyStrength >
                Sector.FriendlyStrength * 1.5f;

        if (bFriendlyCapture)
        {
            Sector.Owner = EBHWarFaction::Friendly;
            Sector.EnemyStrength *= 0.25f;
            Sector.EnemyGarrison = 0;
            Sector.LastBattleTurn = TurnNumber;
            RecordWarEvent(
                TEXT("SectorCaptured"),
                Sector.SectorID,
                FString::Printf(
                    TEXT("Friendly forces captured %s"),
                    *Sector.DisplayName.ToString()
                )
            );
        }
        else if (bEnemyCapture)
        {
            Sector.Owner = EBHWarFaction::Enemy;
            Sector.FriendlyStrength *= 0.25f;
            Sector.FriendlyGarrison = 0;
            Sector.LastBattleTurn = TurnNumber;
            RecordWarEvent(
                TEXT("SectorLost"),
                Sector.SectorID,
                FString::Printf(
                    TEXT("%s fell to enemy forces"),
                    *Sector.DisplayName.ToString()
                )
            );
        }
        else if (Sector.Owner == EBHWarFaction::Neutral)
        {
            Sector.Owner =
                Sector.FriendlyStrength >= Sector.EnemyStrength
                    ? EBHWarFaction::Friendly
                    : EBHWarFaction::Enemy;
            if (Sector.Owner == EBHWarFaction::Friendly)
            {
                Sector.EnemyGarrison = 0;
            }
            else
            {
                Sector.FriendlyGarrison = 0;
            }
            RecordWarEvent(
                Sector.Owner == EBHWarFaction::Friendly
                    ? FName(TEXT("SectorCaptured"))
                    : FName(TEXT("SectorLost")),
                Sector.SectorID,
                FString::Printf(
                    TEXT("%s established control of %s"),
                    Sector.Owner == EBHWarFaction::Friendly
                        ? TEXT("Friendly forces")
                        : TEXT("Enemy forces"),
                    *Sector.DisplayName.ToString()
                )
            );
        }
    }
}

bool UBHWarSubsystem::ResolvePriorityMission(
    bool bFriendlySucceeded
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    return ApplyMissionResult(
        PrioritySectorID,
        PriorityType,
        bFriendlySucceeded
    );
}

bool UBHWarSubsystem::SetCommittedOperation(
    FName SectorID,
    EBHWarPriorityType OperationType
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    if (IsCampaignResolved() ||
        !HasSector(SectorID) ||
        OperationType == EBHWarPriorityType::None ||
        (OperationType == EBHWarPriorityType::EscortRescue &&
         GetEscortOperationTargetID(SectorID).IsNone()))
    {
        return false;
    }

    if (HasCommittedOperation())
    {
        const bool bMatchesCurrentOperation =
            SectorID == CommittedOperationSectorID &&
            OperationType == CommittedOperationType;

        if (!bMatchesCurrentOperation)
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_WAR_OPERATION_COMMIT_REJECTED requested=%s "
                    "requested_type=%d active=%s active_type=%d"
                ),
                *SectorID.ToString(),
                static_cast<int32>(OperationType),
                *CommittedOperationSectorID.ToString(),
                static_cast<int32>(CommittedOperationType)
            );
        }

        return bMatchesCurrentOperation;
    }

    CommittedOperationSectorID = SectorID;
    CommittedOperationID = FName(*FString::Printf(
        TEXT("Operation_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)
    ));
    CommittedOperationTargetID =
        OperationType == EBHWarPriorityType::EscortRescue
            ? GetEscortOperationTargetID(SectorID)
            : NAME_None;
    CommittedOperationSupplySourceSectorID =
        GetOperationSupplySource(SectorID, OperationType);
    CommittedOperationEnemySourceSectorID =
        GetOperationEnemySource(SectorID, OperationType);
    CommittedOperationType = OperationType;
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_WAR_OPERATION_COMMITTED id=%s target_id=%s "
            "sector=%s type=%d staging=%s enemy_source=%s"
        ),
        *CommittedOperationID.ToString(),
        *CommittedOperationTargetID.ToString(),
        *CommittedOperationSectorID.ToString(),
        static_cast<int32>(CommittedOperationType),
        *CommittedOperationSupplySourceSectorID.ToString(),
        *CommittedOperationEnemySourceSectorID.ToString()
    );
    return true;
}

FBHCampaignDifficultyProfile
UBHWarSubsystem::GetCampaignDifficulty() const
{
    return CampaignDifficulty;
}

bool UBHWarSubsystem::SetCampaignDifficultyPreset(
    EBHCampaignDifficultyPreset Preset
)
{
    if (!CanMutateWarState() ||
        Preset == EBHCampaignDifficultyPreset::Custom)
    {
        return false;
    }

    CampaignDifficulty = BHDifficulty::BuildPreset(Preset);
    BroadcastWarState();
    return true;
}

bool UBHWarSubsystem::SetCustomCampaignDifficulty(
    const FBHCampaignDifficultyProfile& CustomProfile
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    CampaignDifficulty = BHDifficulty::Sanitize(CustomProfile);
    CampaignDifficulty.Preset = EBHCampaignDifficultyPreset::Custom;
    BroadcastWarState();
    return true;
}

bool UBHWarSubsystem::RestoreCampaignDifficulty(
    const FBHCampaignDifficultyProfile& SavedProfile,
    int32 SavedSchemaVersion
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    CampaignDifficulty = SavedSchemaVersion >= 42
        ? BHDifficulty::Sanitize(SavedProfile)
        : BHDifficulty::BuildPreset(
            EBHCampaignDifficultyPreset::Operator
        );
    BroadcastWarState();
    return true;
}

FBHOperationAfterActionRecord UBHWarSubsystem::BuildAfterActionRecord(
    FName OperationID,
    FName SectorID,
    EBHWarPriorityType OperationType,
    bool bSucceeded,
    int32 FriendlyCasualties,
    int32 EnemyCasualties,
    int32 EnemyRouted,
    float StrategicSupplyDelta,
    float RecoveredMateriel,
    EBHOperationTacticalOption TacticalOption,
    float TacticalSupplyCost
)
{
    FBHOperationAfterActionRecord Record;
    Record.OperationID = OperationID;
    Record.SectorID = SectorID;
    Record.OperationType = OperationType;
    Record.bSucceeded = bSucceeded;
    Record.FriendlyCasualties = FMath::Max(0, FriendlyCasualties);
    Record.EnemyCasualties = FMath::Max(0, EnemyCasualties);
    Record.EnemyRouted = FMath::Max(0, EnemyRouted);
    Record.StrategicSupplyDelta = StrategicSupplyDelta;
    Record.RecoveredMateriel = FMath::Max(0.0f, RecoveredMateriel);
    const bool bSupportsTacticalPlanning =
        OperationType == EBHWarPriorityType::Attack ||
        OperationType == EBHWarPriorityType::Defend ||
        OperationType == EBHWarPriorityType::Raid;
    Record.TacticalOption = bSupportsTacticalPlanning
        ? TacticalOption
        : EBHOperationTacticalOption::None;
    Record.TacticalSupplyCost =
        Record.TacticalOption != EBHOperationTacticalOption::None
            ? FMath::Max(0.0f, TacticalSupplyCost)
            : 0.0f;
    Record.MissionResultScore = bSucceeded ? 40 : 8;
    Record.ForcePreservationScore = FMath::Clamp(
        25 - (Record.FriendlyCasualties * 8), 0, 25);
    Record.EnemyOutcomeScore = FMath::Clamp(
        (Record.EnemyCasualties * 3) + (Record.EnemyRouted * 2),
        0, 15);
    Record.ResourceEfficiencyScore = FMath::Clamp(
        FMath::RoundToInt(
            5.0f + Record.RecoveredMateriel +
            FMath::Max(0.0f, Record.StrategicSupplyDelta) * 0.25f -
            Record.TacticalSupplyCost / 3.0f),
        0, 10);
    Record.OperationalEffectScore = bSucceeded ? 10 : 2;
    if (Record.TacticalOption ==
            EBHOperationTacticalOption::ReconPlanning)
    {
        Record.TacticalExecutionScore = bSucceeded ? 5 : 1;
    }
    else if (Record.TacticalOption ==
        EBHOperationTacticalOption::ReinforcementPriority)
    {
        Record.TacticalExecutionScore = bSucceeded
            ? FMath::Clamp(5 - Record.FriendlyCasualties * 2, 0, 5)
            : 0;
    }
    else if (Record.TacticalOption ==
        EBHOperationTacticalOption::MedicalPreparation)
    {
        Record.TacticalExecutionScore = bSucceeded
            ? FMath::Clamp(
                5 - Record.FriendlyCasualties * 2,
                0,
                5
            )
            : 0;
    }
    Record.TotalScore = Record.MissionResultScore +
        Record.ForcePreservationScore + Record.EnemyOutcomeScore +
        Record.ResourceEfficiencyScore + Record.OperationalEffectScore +
        Record.TacticalExecutionScore;
    Record.Grade = Record.TotalScore >= 85
        ? EBHAfterActionGrade::Exceptional
        : Record.TotalScore >= 70
            ? EBHAfterActionGrade::Strong
            : Record.TotalScore >= 50
                ? EBHAfterActionGrade::Effective
                : EBHAfterActionGrade::Diminished;
    return Record;
}

bool UBHWarSubsystem::RecordOperationAfterAction(
    const FBHOperationAfterActionRecord& Record
)
{
    if (!CanMutateWarState() || Record.OperationID.IsNone() ||
        Record.SectorID.IsNone() ||
        Record.OperationType == EBHWarPriorityType::None)
    {
        return false;
    }

    CampaignProgression.LastAfterAction = Record;
    CampaignProgression.CampaignMerit += FMath::Max(0, Record.TotalScore);
    ++CampaignProgression.CompletedOperations;
    CampaignProgression.SuccessfulOperations += Record.bSucceeded ? 1 : 0;

    const auto UnlockAt = [this](int32 Threshold,
        EBHCampaignCapability Capability)
    {
        if (CampaignProgression.CampaignMerit >= Threshold)
        {
            CampaignProgression.UnlockedCapabilities.AddUnique(Capability);
        }
    };
    UnlockAt(100, EBHCampaignCapability::IntelligenceNetwork);
    UnlockAt(250, EBHCampaignCapability::CasualtyRecoveryNetwork);
    UnlockAt(450, EBHCampaignCapability::TransportSupportNetwork);
    if (HasCampaignCapability(
            EBHCampaignCapability::IntelligenceNetwork))
    {
        for (FBHWarSectorState& Sector : SectorStates)
        {
            Sector.IntelConfidence = FMath::Max(
                Sector.IntelConfidence,
                35.0f
            );

        }
    }
    BroadcastWarState();
    return true;
}

float UBHWarSubsystem::GetSectorFortificationDefense(
    FName SectorID
) const
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || SectorID.IsNone())
    {
        return 0.0f;
    }

    float Defense = 0.0f;
    for (TActorIterator<ABHFieldFortification> It(World); It; ++It)
    {
        const ABHFieldFortification* Fortification = *It;
        if (IsValid(Fortification) &&
            Fortification->GetSectorID() == SectorID)
        {
            if (Fortification->IsConstructed())
            {
                Defense += Fortification->GetStrategicDefenseValue();
            }
        }
    }
    return FMath::Clamp(Defense, 0.0f, 18.0f);
}

void UBHWarSubsystem::GetSectorFortificationSummary(
    FName SectorID,
    int32& OutConstructedCount,
    int32& OutUnfinishedCount,
    float& OutDefenseValue
) const
{
    OutConstructedCount = 0;
    OutUnfinishedCount = 0;
    OutDefenseValue = 0.0f;

    UWorld* World = GetWorld();
    if (!IsValid(World) || SectorID.IsNone())
    {
        return;
    }

    for (TActorIterator<ABHFieldFortification> It(World); It; ++It)
    {
        const ABHFieldFortification* Fortification = *It;
        if (IsValid(Fortification) &&
            Fortification->GetSectorID() == SectorID)
        {
            OutDefenseValue += Fortification->GetStrategicDefenseValue();
            if (Fortification->IsConstructed())
            {
                ++OutConstructedCount;
            }
            else if (!Fortification->IsDismantling())
            {
                ++OutUnfinishedCount;
            }
        }
    }

    OutDefenseValue = FMath::Clamp(OutDefenseValue, 0.0f, 18.0f);
}

float UBHWarSubsystem::CalculateFortificationCasualtyMultiplier(
    float FortificationDefense
)
{
    const float Reduction = FMath::Min(
        FMath::Max(0.0f, FortificationDefense) / 50.0f,
        0.35f
    );
    return 1.0f - Reduction;
}

float UBHWarSubsystem::GetSectorFortificationSupplyBonus(
    FName SectorID
) const
{
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return 0.0f;
    }

    float SupplyBonus = 0.0f;
    for (TActorIterator<ABHFieldFortification> It(World); It; ++It)
    {
        const ABHFieldFortification* Fortification = *It;
        if (IsValid(Fortification) &&
            Fortification->GetSectorID() == SectorID &&
            Fortification->GetSelectedPlan() ==
                EBHFortificationPlan::FieldSupplyCache &&
            Fortification->IsConstructed())
        {
            SupplyBonus +=
                static_cast<float>(
                    Fortification->GetSupplyCacheChargesRemaining()
                ) * FortificationSupplyCacheSupportPerCharge;
        }
    }

    return FMath::Min(SupplyBonus, MaximumSectorSupply);
}

int32 UBHWarSubsystem::GetSectorRallyDeploymentReserve(
    FName SectorID
) const
{
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return 0;
    }

    int32 AvailableDeployments = 0;
    for (TActorIterator<ABHFieldFortification> It(World); It; ++It)
    {
        const ABHFieldFortification* Fortification = *It;
        if (IsValid(Fortification) &&
            Fortification->GetSectorID() == SectorID &&
            Fortification->GetSelectedPlan() ==
                EBHFortificationPlan::FieldRallyPoint &&
            Fortification->IsConstructed() &&
            Fortification->GetHealthFraction() >=
                FortificationRallyDeploymentHealthThreshold)
        {
            AvailableDeployments +=
                FMath::Max(0, Fortification->GetRallyDeploymentsRemaining());
        }
    }

    return AvailableDeployments;
}

int32 UBHWarSubsystem::ConsumeSectorRallyDeployments(
    FName SectorID,
    int32 DeploymentCount
)
{
    if (!CanMutateWarState() || DeploymentCount <= 0)
    {
        return 0;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return 0;
    }

    int32 RemainingToConsume = DeploymentCount;
    int32 Consumed = 0;
    for (TActorIterator<ABHFieldFortification> It(World); It; ++It)
    {
        if (RemainingToConsume <= 0)
        {
            break;
        }

        ABHFieldFortification* Fortification = *It;
        if (!IsValid(Fortification) ||
            Fortification->GetSectorID() != SectorID ||
            Fortification->GetSelectedPlan() !=
                EBHFortificationPlan::FieldRallyPoint ||
            !Fortification->IsConstructed() ||
            Fortification->GetHealthFraction() <
                FortificationRallyDeploymentHealthThreshold)
        {
            continue;
        }

        const int32 NewlyConsumed = Fortification->ConsumeRallyDeployments(
            RemainingToConsume
        );
        RemainingToConsume -= NewlyConsumed;
        Consumed += NewlyConsumed;
    }

    return Consumed;
}

FBHCampaignProgressionState UBHWarSubsystem::GetCampaignProgression() const
{
    return CampaignProgression;
}

bool UBHWarSubsystem::HasCampaignCapability(
    EBHCampaignCapability Capability
) const
{
    return CampaignProgression.UnlockedCapabilities.Contains(Capability);
}

EBHOperationTacticalOption UBHWarSubsystem::GetActiveTacticalOption() const
{
    return CampaignProgression.ActiveTacticalOption;
}

bool UBHWarSubsystem::IsTacticalOptionUnlocked(
    EBHOperationTacticalOption Option
) const
{
    switch (Option)
    {
        case EBHOperationTacticalOption::None:
            return true;
        case EBHOperationTacticalOption::ReconPlanning:
            return HasCampaignCapability(
                EBHCampaignCapability::IntelligenceNetwork);
        case EBHOperationTacticalOption::ReinforcementPriority:
            return HasCampaignCapability(
                EBHCampaignCapability::TransportSupportNetwork);
        case EBHOperationTacticalOption::MedicalPreparation:
            return HasCampaignCapability(
                EBHCampaignCapability::CasualtyRecoveryNetwork);
        default:
            return false;
    }
}

bool UBHWarSubsystem::SetActiveTacticalOption(
    EBHOperationTacticalOption Option
)
{
    if (!CanMutateWarState() || HasCommittedOperation() ||
        !IsTacticalOptionUnlocked(Option))
    {
        return false;
    }

    CampaignProgression.ActiveTacticalOption = Option;
    BroadcastWarState();
    return true;
}

bool UBHWarSubsystem::CanIssueStrategicCommands() const
{
    return CanMutateWarState();
}

bool UBHWarSubsystem::RestoreCampaignProgression(
    const FBHCampaignProgressionState& SavedProgression,
    int32 SavedSchemaVersion
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    CampaignProgression = SavedSchemaVersion >= 43
        ? SavedProgression
        : FBHCampaignProgressionState();
    CampaignProgression.CampaignMerit = FMath::Max(
        0, CampaignProgression.CampaignMerit);
    CampaignProgression.CompletedOperations = FMath::Max(
        0, CampaignProgression.CompletedOperations);
    CampaignProgression.SuccessfulOperations = FMath::Clamp(
        CampaignProgression.SuccessfulOperations,
        0, CampaignProgression.CompletedOperations);
    if (SavedSchemaVersion < 44 ||
        !IsTacticalOptionUnlocked(
            CampaignProgression.ActiveTacticalOption))
    {
        CampaignProgression.ActiveTacticalOption =
            EBHOperationTacticalOption::None;
    }
    BroadcastWarState();
    return true;
}

void UBHWarSubsystem::UpdateCommittedConvoyOperationDeadline(
    float DeltaTime
)
{
    if (!HasCommittedOperation() ||
        CommittedOperationType != EBHWarPriorityType::EscortRescue ||
        CommittedOperationTargetID.IsNone())
    {
        RouteOperationReplicationAccumulator = 0.0f;
        return;
    }

    FBHWarSupplyConvoyState* Convoy = SupplyConvoys.FindByPredicate(
        [this](const FBHWarSupplyConvoyState& Candidate)
        {
            return Candidate.ConvoyID == CommittedOperationTargetID;
        }
    );
    if (!Convoy ||
        Convoy->RouteOperationProfile.Variation !=
            EBHRouteOperationVariation::TimeCritical ||
        Convoy->OperationDeadlineSecondsRemaining <= 0.0f)
    {
        return;
    }

    Convoy->OperationDeadlineSecondsRemaining = FMath::Max(
        0.0f,
        Convoy->OperationDeadlineSecondsRemaining -
            FMath::Max(0.0f, DeltaTime)
    );
    RouteOperationReplicationAccumulator += FMath::Max(0.0f, DeltaTime);

    if (Convoy->OperationDeadlineSecondsRemaining <= KINDA_SMALL_NUMBER ||
        RouteOperationReplicationAccumulator >= 0.5f)
    {
        RouteOperationReplicationAccumulator = 0.0f;
        BroadcastWarState();
    }
}

bool UBHWarSubsystem::SetCommittedRescueOperation(
    FName DestinationSectorID,
    FName CasualtyID
)
{
    if (CasualtyID.IsNone() ||
        !IsViableOperation(
            DestinationSectorID,
            EBHWarPriorityType::Rescue
        ) ||
        !SetCommittedOperation(
            DestinationSectorID,
            EBHWarPriorityType::Rescue
        ))
    {
        return false;
    }

    CommittedOperationTargetID = CasualtyID;
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_RESCUE_OPERATION_TARGETED id=%s casualty=%s "
            "destination=%s"
        ),
        *CommittedOperationID.ToString(),
        *CommittedOperationTargetID.ToString(),
        *CommittedOperationSectorID.ToString()
    );
    return true;
}

void UBHWarSubsystem::ClearCommittedOperation()
{
    if (!CanMutateWarState())
    {
        return;
    }

    if (!HasCommittedOperation())
    {
        return;
    }

    const FName PreviousSectorID =
        CommittedOperationSectorID;
    CommittedOperationSectorID = NAME_None;
    CommittedOperationID = NAME_None;
    CommittedOperationTargetID = NAME_None;
    CommittedOperationSupplySourceSectorID = NAME_None;
    CommittedOperationEnemySourceSectorID = NAME_None;
    CommittedOperationType = EBHWarPriorityType::None;
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_WAR_OPERATION_RELEASED sector=%s"),
        *PreviousSectorID.ToString()
    );
}

bool UBHWarSubsystem::HasCommittedOperation() const
{
    return !CommittedOperationSectorID.IsNone() &&
        CommittedOperationType != EBHWarPriorityType::None &&
        HasSector(CommittedOperationSectorID);
}

FName UBHWarSubsystem::GetCommittedOperationSectorID() const
{
    return HasCommittedOperation()
        ? CommittedOperationSectorID
        : NAME_None;
}

FName UBHWarSubsystem::GetCommittedOperationTargetID() const
{
    return HasCommittedOperation()
        ? CommittedOperationTargetID
        : NAME_None;
}

FName UBHWarSubsystem::
GetCommittedOperationSupplySourceSectorID() const
{
    return HasCommittedOperation()
        ? CommittedOperationSupplySourceSectorID
        : NAME_None;
}

FName UBHWarSubsystem::
GetCommittedOperationEnemySourceSectorID() const
{
    return HasCommittedOperation()
        ? CommittedOperationEnemySourceSectorID
        : NAME_None;
}

bool UBHWarSubsystem::DoesConvoyMatchCommittedEscort(
    FName ConvoyID
) const
{
    return HasCommittedOperation() &&
        CommittedOperationType ==
            EBHWarPriorityType::EscortRescue &&
        !ConvoyID.IsNone() &&
        ConvoyID == CommittedOperationTargetID;
}

bool UBHWarSubsystem::IsOperationSectorLocked(
    FName SectorID
) const
{
    return IsCommittedOperationSector(SectorID);
}

EBHWarPriorityType
UBHWarSubsystem::GetCommittedOperationType() const
{
    return HasCommittedOperation()
        ? CommittedOperationType
        : EBHWarPriorityType::None;
}

bool UBHWarSubsystem::ApplyMissionResult(
    FName SectorID,
    EBHWarPriorityType MissionType,
    bool bFriendlySucceeded
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (IsCampaignResolved() ||
        !SectorStates.IsValidIndex(SectorIndex) ||
        MissionType == EBHWarPriorityType::None)
    {
        return false;
    }

    const bool bMatchesCommittedOperation =
        SectorID == CommittedOperationSectorID &&
        MissionType == CommittedOperationType;
    const FName OperationSupplySourceSectorID =
        bMatchesCommittedOperation
            ? CommittedOperationSupplySourceSectorID
            : GetOperationSupplySource(SectorID, MissionType);
    const FName OperationEnemySourceSectorID =
        bMatchesCommittedOperation
            ? CommittedOperationEnemySourceSectorID
            : GetOperationEnemySource(SectorID, MissionType);
    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousResponsePressure =
        Sector.EnemyResponsePressure;
    const float PreviousCivilianSupport =
        Sector.CivilianSupport;
    const float PreviousIntelConfidence = Sector.IntelConfidence;
    const bool bCounterinsurgencyDefense =
        MissionType == EBHWarPriorityType::Defend &&
        PreviousResponsePressure >=
            CounterinsurgencyResponseThreshold;
    ++TurnNumber;
    Sector.LastBattleTurn = TurnNumber;
    Sector.IntelConfidence = MaximumIntelConfidence;

    if (MissionType == EBHWarPriorityType::Recon)
    {
        Sector.IntelConfidence = bFriendlySucceeded
            ? MaximumIntelConfidence
            : PreviousIntelConfidence;
        RecordWarEvent(
            bFriendlySucceeded
                ? FName(TEXT("ReconOperationSucceeded"))
                : FName(TEXT("ReconOperationFailed")),
            Sector.SectorID,
            bFriendlySucceeded
                ? FString::Printf(
                    TEXT("Field reconnaissance confirmed enemy activity at %s"),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT("Reconnaissance at %s ended before a confirmed report"),
                    *Sector.DisplayName.ToString()
                )
        );
    }
    else if (MissionType == EBHWarPriorityType::EscortRescue ||
        MissionType == EBHWarPriorityType::Rescue)
    {
        RecordWarEvent(
            bFriendlySucceeded
                ? MissionType == EBHWarPriorityType::Rescue
                    ? FName(TEXT("RescueOperationSucceeded"))
                    : FName(TEXT("EscortOperationSucceeded"))
                : MissionType == EBHWarPriorityType::Rescue
                    ? FName(TEXT("RescueOperationFailed"))
                    : FName(TEXT("EscortOperationFailed")),
            Sector.SectorID,
            MissionType == EBHWarPriorityType::Rescue
                ? bFriendlySucceeded
                    ? FString::Printf(
                        TEXT("Casualty evacuated to %s"),
                        *Sector.DisplayName.ToString()
                    )
                    : FString::Printf(
                        TEXT("Casualty was lost en route to %s"),
                        *Sector.DisplayName.ToString()
                    )
                : bFriendlySucceeded
                ? FString::Printf(
                    TEXT("Protected convoy reached the route exit for %s"),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT("Protected convoy bound for %s was lost"),
                    *Sector.DisplayName.ToString()
                )
        );
    }
    else if (MissionType == EBHWarPriorityType::Resupply)
    {
        RecordWarEvent(
            bFriendlySucceeded
                ? FName(TEXT("ResupplyOperationSucceeded"))
                : FName(TEXT("ResupplyOperationFailed")),
            Sector.SectorID,
            bFriendlySucceeded
                ? FString::Printf(
                    TEXT(
                        "Field logistics restored the supply line to %s"
                    ),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT(
                        "The field logistics run to %s was abandoned"
                    ),
                    *Sector.DisplayName.ToString()
                )
        );
    }
    else if (MissionType == EBHWarPriorityType::Raid)
    {
        if (bFriendlySucceeded)
        {
            Sector.EnemyStrength -= RaidSuccessEnemyStrengthLoss;
            Sector.Supply -= RaidSuccessSupplyLoss;
            Sector.EnemyResponsePressure +=
                RaidSuccessResponsePressure;
            Sector.CivilianSupport +=
                RaidSuccessCivilianSupport;

            RecordWarEvent(
                FName(TEXT("LogisticsRaidSucceeded")),
                Sector.SectorID,
                FString::Printf(
                    TEXT(
                        "Resistance raiders disrupted %s; "
                        "enemy supply fell by %.0f"
                    ),
                    *Sector.DisplayName.ToString(),
                    RaidSuccessSupplyLoss
                )
            );

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_LOGISTICS_RAID_SUCCEEDED sector=%s "
                    "supply_loss=%.1f strength_loss=%.1f"
                ),
                *Sector.SectorID.ToString(),
                RaidSuccessSupplyLoss,
                RaidSuccessEnemyStrengthLoss
            );
        }
        else
        {
            Sector.EnemyStrength += RaidFailureEnemyStrengthGain;
            Sector.EnemyResponsePressure +=
                RaidFailureResponsePressure;
            Sector.CivilianSupport +=
                RaidFailureCivilianSupport;

            RecordWarEvent(
                FName(TEXT("LogisticsRaidFailed")),
                Sector.SectorID,
                FString::Printf(
                    TEXT(
                        "Raiders withdrew from %s before disrupting "
                        "enemy logistics"
                    ),
                    *Sector.DisplayName.ToString()
                )
            );
        }
    }
    else if (MissionType == EBHWarPriorityType::Attack)
    {
        if (bFriendlySucceeded)
        {
            Sector.Owner = EBHWarFaction::Friendly;
            Sector.FriendlyStrength = FMath::Max(
                45.0f,
                Sector.FriendlyStrength + 30.0f
            );
            Sector.EnemyStrength *= 0.2f;
            Sector.Supply -= 10.0f;
            Sector.EnemyGarrison = 0;

            const int32 SupplySourceIndex =
                FindSectorIndex(OperationSupplySourceSectorID);

            if (SectorStates.IsValidIndex(SupplySourceIndex) &&
                SupplySourceIndex != SectorIndex)
            {
                FBHWarSectorState& SupplySource =
                    SectorStates[SupplySourceIndex];
                const int32 RequiredOccupationGarrison =
                    FMath::Min(
                        MinimumOccupationGarrison,
                        Sector.GarrisonCapacity
                    );
                const int32 OccupationGarrison =
                    FMath::Min(
                        FMath::Max(
                            0,
                            RequiredOccupationGarrison -
                                Sector.FriendlyGarrison
                        ),
                        SupplySource.FriendlyGarrison
                    );

                SupplySource.FriendlyGarrison -=
                    OccupationGarrison;
                Sector.FriendlyGarrison +=
                    OccupationGarrison;
                ClampSectorState(SupplySource);

                if (OccupationGarrison > 0)
                {
                    RecordWarEvent(
                        FName(TEXT("OccupationForceDeployed")),
                        Sector.SectorID,
                        FString::Printf(
                            TEXT(
                                "%d troops transferred from %s "
                                "to secure %s"
                            ),
                            OccupationGarrison,
                            *SupplySource.DisplayName.ToString(),
                            *Sector.DisplayName.ToString()
                        )
                    );

                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT(
                            "BH_OCCUPATION_GARRISON_TRANSFERRED "
                            "from=%s to=%s count=%d "
                            "source_remaining=%d"
                        ),
                        *SupplySource.SectorID.ToString(),
                        *Sector.SectorID.ToString(),
                        OccupationGarrison,
                        SupplySource.FriendlyGarrison
                    );
                }
                else if (
                    Sector.FriendlyGarrison <
                        RequiredOccupationGarrison)
                {
                    RecordWarEvent(
                        FName(TEXT("OccupationForceUnavailable")),
                        Sector.SectorID,
                        FString::Printf(
                            TEXT(
                                "%s was captured without a permanent "
                                "garrison; %s had no troops to spare"
                            ),
                            *Sector.DisplayName.ToString(),
                            *SupplySource.DisplayName.ToString()
                        )
                    );

                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT(
                            "BH_CAPTURE_UNGARRISONED "
                            "from=%s to=%s"
                        ),
                        *SupplySource.SectorID.ToString(),
                        *Sector.SectorID.ToString()
                    );
                }
            }
            Sector.EnemyResponsePressure = FMath::Max(
                Sector.EnemyResponsePressure,
                AttackSuccessResponsePressure
            );
            Sector.CivilianSupport +=
                AttackSuccessCivilianSupport;
        }
        else
        {
            Sector.FriendlyStrength *= 0.5f;
            Sector.EnemyStrength += 15.0f;
            Sector.EnemyResponsePressure +=
                AttackFailureResponsePressure;
            Sector.CivilianSupport +=
                AttackFailureCivilianSupport;
        }
    }
    else
    {
        if (bFriendlySucceeded)
        {
            Sector.Owner = EBHWarFaction::Friendly;
            Sector.FriendlyStrength += 15.0f;
            Sector.EnemyStrength *= 0.2f;
            Sector.Supply -= 5.0f;
            Sector.EnemyGarrison = 0;
            Sector.EnemyResponsePressure =
                bCounterinsurgencyDefense
                    ? FMath::Max(
                        0.0f,
                        Sector.EnemyResponsePressure -
                            CounterinsurgencySuccessResponseReduction
                    )
                    : Sector.EnemyResponsePressure +
                        DefenseSuccessResponsePressure;
            Sector.CivilianSupport +=
                DefenseSuccessCivilianSupport;
        }
        else
        {
            Sector.Owner = EBHWarFaction::Enemy;
            Sector.EnemyStrength = FMath::Max(
                45.0f,
                Sector.EnemyStrength + 25.0f
            );
            Sector.FriendlyStrength *= 0.2f;
            Sector.FriendlyGarrison = 0;

            const int32 EnemySourceIndex =
                FindSectorIndex(OperationEnemySourceSectorID);

            if (SectorStates.IsValidIndex(EnemySourceIndex) &&
                EnemySourceIndex != SectorIndex)
            {
                FBHWarSectorState& EnemySource =
                    SectorStates[EnemySourceIndex];
                const int32 RequiredOccupationGarrison =
                    FMath::Min(
                        MinimumOccupationGarrison,
                        Sector.GarrisonCapacity
                    );
                const int32 OccupationGarrison =
                    FMath::Min(
                        FMath::Max(
                            0,
                            RequiredOccupationGarrison -
                                Sector.EnemyGarrison
                        ),
                        EnemySource.EnemyGarrison
                    );

                EnemySource.EnemyGarrison -=
                    OccupationGarrison;
                Sector.EnemyGarrison += OccupationGarrison;
                ClampSectorState(EnemySource);

                if (OccupationGarrison > 0)
                {
                    RecordWarEvent(
                        FName(TEXT("EnemyOccupationForceDeployed")),
                        Sector.SectorID,
                        FString::Printf(
                            TEXT(
                                "%d enemy troops transferred from "
                                "%s to occupy %s"
                            ),
                            OccupationGarrison,
                            *EnemySource.DisplayName.ToString(),
                            *Sector.DisplayName.ToString()
                        )
                    );

                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT(
                            "BH_ENEMY_OCCUPATION_GARRISON_TRANSFERRED "
                            "from=%s to=%s count=%d "
                            "source_remaining=%d"
                        ),
                        *EnemySource.SectorID.ToString(),
                        *Sector.SectorID.ToString(),
                        OccupationGarrison,
                        EnemySource.EnemyGarrison
                    );
                }
                else if (
                    Sector.EnemyGarrison <
                        RequiredOccupationGarrison)
                {
                    RecordWarEvent(
                        FName(TEXT("EnemyOccupationForceUnavailable")),
                        Sector.SectorID,
                        FString::Printf(
                            TEXT(
                                "%s fell without a permanent enemy "
                                "garrison; %s had no troops to spare"
                            ),
                            *Sector.DisplayName.ToString(),
                            *EnemySource.DisplayName.ToString()
                        )
                    );

                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT(
                            "BH_ENEMY_CAPTURE_UNGARRISONED "
                            "from=%s to=%s"
                        ),
                        *EnemySource.SectorID.ToString(),
                        *Sector.SectorID.ToString()
                    );
                }
            }
            Sector.EnemyResponsePressure = 0.0f;
            Sector.CivilianSupport +=
                DefenseFailureCivilianSupport;
        }
    }

    const EBHWarPriorityType PreviousAnticipatedOperation =
        Sector.AnticipatedOperationType;

    if (MissionType == EBHWarPriorityType::Recon)
    {
        // Observation does not teach the enemy a repeated combat pattern.
    }
    else if (PreviousAnticipatedOperation == MissionType)
    {
        Sector.RepeatedOperationCount = FMath::Min(
            Sector.RepeatedOperationCount + 1,
            99
        );
    }
    else
    {
        Sector.AnticipatedOperationType = MissionType;
        Sector.RepeatedOperationCount = 1;
    }

    if (MissionType != EBHWarPriorityType::Recon &&
        Sector.RepeatedOperationCount >= 2)
    {
        const TCHAR* PatternLabel =
            MissionType == EBHWarPriorityType::Attack
                ? TEXT("assaults")
                : MissionType == EBHWarPriorityType::Raid
                    ? TEXT("raids")
                : MissionType == EBHWarPriorityType::Resupply
                        ? TEXT("resupply runs")
                        : MissionType ==
                                EBHWarPriorityType::EscortRescue
                            ? TEXT("convoy escorts")
                        : MissionType == EBHWarPriorityType::Rescue
                            ? TEXT("casualty evacuations")
                        : TEXT("defensive stands");

        RecordWarEvent(
            FName(TEXT("EnemyPatternAdapted")),
            Sector.SectorID,
            FString::Printf(
                TEXT(
                    "Enemy intelligence at %s has adapted to "
                    "repeated %s; change tactics to break the read"
                ),
                *Sector.DisplayName.ToString(),
                PatternLabel
            )
        );
    }

    ClampSectorState(Sector);
    RecordEnemyResponseEscalation(
        Sector,
        PreviousResponsePressure
    );
    const FString OperationSummary =
        MissionType == EBHWarPriorityType::Recon
        ? (
            bFriendlySucceeded
                ? FString::Printf(
                    TEXT("Enemy activity confirmed at %s"),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT("Reconnaissance at %s was incomplete"),
                    *Sector.DisplayName.ToString()
                )
        )
        : MissionType == EBHWarPriorityType::Rescue
        ? (
            bFriendlySucceeded
                ? FString::Printf(
                    TEXT("Casualty evacuated to %s"),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT("Casualty evacuation to %s failed"),
                    *Sector.DisplayName.ToString()
                )
        )
        : MissionType == EBHWarPriorityType::EscortRescue
        ? (
            bFriendlySucceeded
                ? FString::Printf(
                    TEXT("Protected convoy secured for %s"),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT("Protected convoy to %s was lost"),
                    *Sector.DisplayName.ToString()
                )
        )
        : MissionType == EBHWarPriorityType::Resupply
        ? (
            bFriendlySucceeded
                ? FString::Printf(
                    TEXT("Supply line restored at %s"),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT("Resupply run to %s failed"),
                    *Sector.DisplayName.ToString()
                )
        )
        : MissionType == EBHWarPriorityType::Raid
        ? (
            bFriendlySucceeded
                ? FString::Printf(
                    TEXT("Logistics raid disrupted %s"),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT("Logistics raid on %s failed"),
                    *Sector.DisplayName.ToString()
                )
        )
        : MissionType == EBHWarPriorityType::Attack
        ? (
            bFriendlySucceeded
                ? FString::Printf(
                    TEXT("Operation succeeded: %s captured"),
                    *Sector.DisplayName.ToString()
                )
                : FString::Printf(
                    TEXT("Assault on %s failed"),
                    *Sector.DisplayName.ToString()
                )
        )
        : (
            bFriendlySucceeded
                ? (
                    bCounterinsurgencyDefense
                        ? FString::Printf(
                            TEXT("Enemy sweep broken at %s"),
                            *Sector.DisplayName.ToString()
                        )
                        : FString::Printf(
                            TEXT("Defense held at %s"),
                            *Sector.DisplayName.ToString()
                        )
                )
                : FString::Printf(
                    TEXT("%s lost after failed defense"),
                    *Sector.DisplayName.ToString()
                )
        );
    if (!FMath::IsNearlyEqual(
        PreviousCivilianSupport,
        Sector.CivilianSupport
    ))
    {
        RecordWarEvent(
            FName(TEXT("CivilianSupportShifted")),
            Sector.SectorID,
            FString::Printf(
                TEXT(
                    "Local support at %s shifted from %.0f%% "
                    "to %.0f%%"
                ),
                *Sector.DisplayName.ToString(),
                PreviousCivilianSupport,
                Sector.CivilianSupport
            )
        );
    }
    if (bFriendlySucceeded && bCounterinsurgencyDefense)
    {
        RecordWarEvent(
            FName(TEXT("CounterinsurgencyBroken")),
            Sector.SectorID,
            FString::Printf(
                TEXT(
                    "Resistance cells survived the enemy sweep at "
                    "%s; response fell from %.0f%% to %.0f%%"
                ),
                *Sector.DisplayName.ToString(),
                PreviousResponsePressure,
                Sector.EnemyResponsePressure
            )
        );

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_COUNTERINSURGENCY_BROKEN sector=%s "
                "response=%.1f->%.1f support=%.1f->%.1f"
            ),
            *Sector.SectorID.ToString(),
            PreviousResponsePressure,
            Sector.EnemyResponsePressure,
            PreviousCivilianSupport,
            Sector.CivilianSupport
        );
    }
    RecordWarEvent(
        bFriendlySucceeded
            ? FName(TEXT("OperationSucceeded"))
            : FName(TEXT("OperationFailed")),
        Sector.SectorID,
        OperationSummary
    );

    if (SectorID == CommittedOperationSectorID &&
        MissionType == CommittedOperationType)
    {
        CommittedOperationSectorID = NAME_None;
        CommittedOperationID = NAME_None;
        CommittedOperationTargetID = NAME_None;
        CommittedOperationSupplySourceSectorID = NAME_None;
        CommittedOperationEnemySourceSectorID = NAME_None;
        CommittedOperationType = EBHWarPriorityType::None;
    }

    RecalculatePriority();
    BroadcastWarState();
    return true;
}

bool UBHWarSubsystem::ApplyAmbientBattleResult(
    FName SectorID,
    int32 FriendlyCasualties,
    int32 EnemyCasualties
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    const int32 SafeFriendlyCasualties = FMath::Clamp(
        FriendlyCasualties,
        0,
        MaximumAmbientCasualtiesPerReport
    );
    const int32 SafeEnemyCasualties = FMath::Clamp(
        EnemyCasualties,
        0,
        MaximumAmbientCasualtiesPerReport
    );

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        (SafeFriendlyCasualties + SafeEnemyCasualties) <= 0)
    {
        return false;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousFriendlyStrength =
        Sector.FriendlyStrength;
    const float PreviousEnemyStrength = Sector.EnemyStrength;
    const float PreviousSupply = Sector.Supply;
    const float PreviousResponsePressure =
        Sector.EnemyResponsePressure;

    Sector.FriendlyStrength -=
        SafeFriendlyCasualties *
        AmbientStrengthLossPerCasualty;
    Sector.EnemyStrength -=
        SafeEnemyCasualties *
        AmbientStrengthLossPerCasualty;
    Sector.FriendlyGarrison -= SafeFriendlyCasualties;
    Sector.EnemyGarrison -= SafeEnemyCasualties;
    Sector.Supply -=
        (SafeFriendlyCasualties + SafeEnemyCasualties) *
        AmbientSupplyCostPerCasualty;
    Sector.LastBattleTurn = TurnNumber;
    Sector.IntelConfidence = FMath::Max(
        Sector.IntelConfidence,
        70.0f
    );
    Sector.EnemyResponsePressure +=
        (SafeEnemyCasualties *
            EnemyCasualtyResponsePressure) +
        (SafeFriendlyCasualties *
            FriendlyCasualtyResponsePressure);

    ClampSectorState(Sector);
    RecordEnemyResponseEscalation(
        Sector,
        PreviousResponsePressure
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_BATTLE_RESOLVED sector=%s "
            "friendly_casualties=%d enemy_casualties=%d "
            "friendly_strength=%.1f->%.1f "
            "enemy_strength=%.1f->%.1f supply=%.1f->%.1f "
            "garrison_f=%d garrison_e=%d response=%.1f"
        ),
        *Sector.SectorID.ToString(),
        SafeFriendlyCasualties,
        SafeEnemyCasualties,
        PreviousFriendlyStrength,
        Sector.FriendlyStrength,
        PreviousEnemyStrength,
        Sector.EnemyStrength,
        PreviousSupply,
        Sector.Supply,
        Sector.FriendlyGarrison,
        Sector.EnemyGarrison,
        Sector.EnemyResponsePressure
    );
    return true;
}

bool UBHWarSubsystem::ApplyOperationCasualtyResult(
    FName SectorID,
    FName FriendlySourceSectorID,
    FName EnemySourceSectorID,
    int32 FriendlyCasualties,
    int32 EnemyCasualties
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 SafeFriendlyCasualties = FMath::Clamp(
        FriendlyCasualties,
        0,
        MaximumAmbientCasualtiesPerReport
    );
    const int32 SafeEnemyCasualties = FMath::Clamp(
        EnemyCasualties,
        0,
        MaximumAmbientCasualtiesPerReport
    );

    if (!HasSector(SectorID) ||
        (SafeFriendlyCasualties + SafeEnemyCasualties) <= 0)
    {
        return false;
    }

    const FName ResolvedFriendlySourceID =
        HasSector(FriendlySourceSectorID)
            ? FriendlySourceSectorID
            : SectorID;
    const FName ResolvedEnemySourceID =
        HasSector(EnemySourceSectorID)
            ? EnemySourceSectorID
            : SectorID;
    bool bApplied = false;

    if (ResolvedFriendlySourceID == ResolvedEnemySourceID)
    {
        bApplied = ApplyAmbientBattleResult(
            ResolvedFriendlySourceID,
            SafeFriendlyCasualties,
            SafeEnemyCasualties
        );
    }
    else
    {
        if (SafeFriendlyCasualties > 0)
        {
            bApplied =
                ApplyAmbientBattleResult(
                    ResolvedFriendlySourceID,
                    SafeFriendlyCasualties,
                    0
                ) ||
                bApplied;
        }

        if (SafeEnemyCasualties > 0)
        {
            bApplied =
                ApplyAmbientBattleResult(
                    ResolvedEnemySourceID,
                    0,
                    SafeEnemyCasualties
                ) ||
                bApplied;
        }
    }

    if (!bApplied)
    {
        return false;
    }

    const FBHWarSectorState Sector = GetSectorState(SectorID);
    RecordWarEvent(
        FName(TEXT("OperationCasualtiesApplied")),
        SectorID,
        FString::Printf(
            TEXT(
                "Fighting around %s cost %d friendly and %d "
                "enemy troops"
            ),
            Sector.DisplayName.IsEmpty()
                ? *SectorID.ToString()
                : *Sector.DisplayName.ToString(),
            SafeFriendlyCasualties,
            SafeEnemyCasualties
        )
    );
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_CASUALTIES_APPLIED sector=%s "
            "friendly_source=%s enemy_source=%s "
            "friendly_casualties=%d enemy_casualties=%d"
        ),
        *SectorID.ToString(),
        *ResolvedFriendlySourceID.ToString(),
        *ResolvedEnemySourceID.ToString(),
        SafeFriendlyCasualties,
        SafeEnemyCasualties
    );
    return true;
}

EBHRaidOperationalSignature
UBHWarSubsystem::ApplyRaidOperationalSignature(
    FName SectorID,
    int32 EnemyCasualties,
    int32 FriendlySupportCasualties,
    bool bRaidSucceeded,
    bool bDetectedBeforeSabotage
)
{
    if (!CanMutateWarState())
    {
        return EBHRaidOperationalSignature::Contested;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return EBHRaidOperationalSignature::Contested;
    }

    const int32 SafeEnemyCasualties =
        FMath::Max(0, EnemyCasualties);
    const int32 SafeFriendlyCasualties =
        FMath::Max(0, FriendlySupportCasualties);
    const EBHRaidOperationalSignature Signature =
        BHWarOperationRules::ClassifyRaidOperationalSignature(
            SafeEnemyCasualties,
            SafeFriendlyCasualties,
            bDetectedBeforeSabotage
        );
    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousResponse =
        Sector.EnemyResponsePressure;
    const float PreviousSupport =
        Sector.CivilianSupport;
    FName EventType(TEXT("ContestedRaidSignature"));
    FString SignatureLabel(TEXT("contested"));

    if (Signature == EBHRaidOperationalSignature::Clean &&
        bRaidSucceeded)
    {
        Sector.EnemyResponsePressure -=
            CleanRaidResponseReduction;
        Sector.CivilianSupport +=
            CleanRaidCivilianSupportBonus;
        EventType = FName(TEXT("CleanRaidSignature"));
        SignatureLabel = TEXT("clean");
    }
    else if (Signature == EBHRaidOperationalSignature::Loud)
    {
        Sector.EnemyResponsePressure +=
            LoudRaidResponseIncrease;
        Sector.CivilianSupport -=
            LoudRaidCivilianSupportPenalty;
        EventType = FName(TEXT("LoudRaidSignature"));
        SignatureLabel = TEXT("loud");
    }

    ClampSectorState(Sector);
    RecordWarEvent(
        EventType,
        SectorID,
        FString::Printf(
            TEXT(
                "%s raid signature at %s: %d hostile losses, "
                "%d support losses, detected=%s (%s)"
            ),
            *SignatureLabel,
            *Sector.DisplayName.ToString(),
            SafeEnemyCasualties,
            SafeFriendlyCasualties,
            bDetectedBeforeSabotage
                ? TEXT("yes")
                : TEXT("no"),
            bRaidSucceeded ? TEXT("success") : TEXT("failure")
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_RAID_OPERATIONAL_SIGNATURE sector=%s "
            "signature=%s succeeded=%d "
            "detected_before_sabotage=%d "
            "hostile_losses=%d support_losses=%d "
            "response=%.1f->%.1f support=%.1f->%.1f"
        ),
        *SectorID.ToString(),
        *SignatureLabel,
        bRaidSucceeded ? 1 : 0,
        bDetectedBeforeSabotage ? 1 : 0,
        SafeEnemyCasualties,
        SafeFriendlyCasualties,
        PreviousResponse,
        Sector.EnemyResponsePressure,
        PreviousSupport,
        Sector.CivilianSupport
    );

    RecalculatePriority();
    BroadcastWarState();
    return Signature;
}

bool UBHWarSubsystem::ApplyAmbientRoutResult(
    FName SectorID,
    int32 FriendlyRouted,
    int32 EnemyRouted
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    const int32 SafeFriendlyRouted = FMath::Clamp(
        FriendlyRouted,
        0,
        MaximumAmbientCasualtiesPerReport
    );
    const int32 SafeEnemyRouted = FMath::Clamp(
        EnemyRouted,
        0,
        MaximumAmbientCasualtiesPerReport
    );

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        (SafeFriendlyRouted + SafeEnemyRouted) <= 0)
    {
        return false;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousFriendlyStrength =
        Sector.FriendlyStrength;
    const float PreviousEnemyStrength = Sector.EnemyStrength;
    const float PreviousSupply = Sector.Supply;
    const float PreviousResponsePressure =
        Sector.EnemyResponsePressure;

    Sector.FriendlyStrength -=
        SafeFriendlyRouted *
        AmbientStrengthLossPerRoutedUnit;
    Sector.EnemyStrength -=
        SafeEnemyRouted *
        AmbientStrengthLossPerRoutedUnit;
    Sector.Supply -=
        (SafeFriendlyRouted + SafeEnemyRouted) *
        AmbientSupplyCostPerRoutedUnit;
    Sector.LastBattleTurn = TurnNumber;
    Sector.EnemyResponsePressure +=
        (SafeEnemyRouted * EnemyRoutResponsePressure) +
        (SafeFriendlyRouted *
            FriendlyCasualtyResponsePressure);

    ClampSectorState(Sector);
    RecordEnemyResponseEscalation(
        Sector,
        PreviousResponsePressure
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_ROUT_RESOLVED sector=%s "
            "friendly_routed=%d enemy_routed=%d "
            "friendly_strength=%.1f->%.1f "
            "enemy_strength=%.1f->%.1f supply=%.1f->%.1f "
            "response=%.1f"
        ),
        *Sector.SectorID.ToString(),
        SafeFriendlyRouted,
        SafeEnemyRouted,
        PreviousFriendlyStrength,
        Sector.FriendlyStrength,
        PreviousEnemyStrength,
        Sector.EnemyStrength,
        PreviousSupply,
        Sector.Supply,
        Sector.EnemyResponsePressure
    );
    return true;
}

bool UBHWarSubsystem::ApplyOperationRoutResult(
    FName SectorID,
    int32 EnemyRouted
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 SafeEnemyRouted = FMath::Clamp(
        EnemyRouted,
        0,
        MaximumAmbientCasualtiesPerReport
    );

    if (SafeEnemyRouted <= 0 ||
        !ApplyAmbientRoutResult(
            SectorID,
            0,
            SafeEnemyRouted
        ))
    {
        return false;
    }

    const FBHWarSectorState Sector = GetSectorState(SectorID);
    RecordWarEvent(
        FName(TEXT("EnemyOperationForcesRouted")),
        SectorID,
        FString::Printf(
            TEXT(
                "%d enemy troops broke contact and withdrew "
                "from %s"
            ),
            SafeEnemyRouted,
            Sector.DisplayName.IsEmpty()
                ? *SectorID.ToString()
                : *Sector.DisplayName.ToString()
        )
    );
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_ROUT_APPLIED sector=%s "
            "enemy_routed=%d"
        ),
        *SectorID.ToString(),
        SafeEnemyRouted
    );
    return true;
}

float UBHWarSubsystem::RecoverBattlefieldMateriel(
    FName SectorID,
    int32 EnemyCasualties,
    int32 FriendlyCasualties,
    bool bMajorOperation
)
{
    if (!CanMutateWarState())
    {
        return 0.0f;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    const int32 SafeEnemyCasualties = FMath::Clamp(
        EnemyCasualties,
        0,
        MaximumAmbientCasualtiesPerReport
    );
    const int32 SafeFriendlyCasualties = FMath::Clamp(
        FriendlyCasualties,
        0,
        MaximumAmbientCasualtiesPerReport
    );

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        SafeEnemyCasualties <= 0)
    {
        return 0.0f;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];

    if (Sector.Owner != EBHWarFaction::Friendly ||
        Sector.Supply >= MaximumSectorSupply -
            KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const float RecoveryCap = bMajorOperation
        ? MaximumOperationSalvageSupply
        : MaximumAmbientSalvageSupply;
    const float RequestedRecovery = FMath::Clamp(
        (SafeEnemyCasualties *
            SalvageSupplyPerEnemyCasualty) -
        (SafeFriendlyCasualties *
            SalvageLossPerFriendlyCasualty),
        0.0f,
        RecoveryCap
    );
    const float PreviousSupply = Sector.Supply;
    Sector.Supply += RequestedRecovery;
    ClampSectorState(Sector);
    const float RecoveredSupply =
        Sector.Supply - PreviousSupply;

    if (RecoveredSupply <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    RecordWarEvent(
        TEXT("BattlefieldMaterielRecovered"),
        Sector.SectorID,
        FString::Printf(
            TEXT(
                "Resistance teams recovered %.0f supply from "
                "the battlefield at %s"
            ),
            RecoveredSupply,
            *Sector.DisplayName.ToString()
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_BATTLEFIELD_MATERIEL_RECOVERED sector=%s "
            "enemy_casualties=%d friendly_casualties=%d "
            "major_operation=%d supply=%.1f->%.1f recovered=%.1f"
        ),
        *Sector.SectorID.ToString(),
        SafeEnemyCasualties,
        SafeFriendlyCasualties,
        bMajorOperation ? 1 : 0,
        PreviousSupply,
        Sector.Supply,
        RecoveredSupply
    );
    return RecoveredSupply;
}

bool UBHWarSubsystem::ConsumeSectorSupply(
    FName SectorID,
    float SupplyAmount
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    const float SafeSupplyAmount = FMath::Max(
        0.0f,
        SupplyAmount
    );

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        SafeSupplyAmount <= 0.0f)
    {
        return false;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];

    if (Sector.Owner != EBHWarFaction::Friendly ||
        Sector.Supply + KINDA_SMALL_NUMBER < SafeSupplyAmount)
    {
        return false;
    }

    const float PreviousSupply = Sector.Supply;
    Sector.Supply -= SafeSupplyAmount;
    ClampSectorState(Sector);
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SECTOR_RESUPPLY_CONSUMED sector=%s "
            "amount=%.1f supply=%.1f->%.1f"
        ),
        *Sector.SectorID.ToString(),
        SafeSupplyAmount,
        PreviousSupply,
        Sector.Supply
    );
    return true;
}

float UBHWarSubsystem::RecoverFortificationSupply(
    FName SectorID,
    float SupplyAmount
)
{
    if (!CanMutateWarState())
    {
        return 0.0f;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return 0.0f;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    if (Sector.Owner != EBHWarFaction::Friendly)
    {
        return 0.0f;
    }

    const float PreviousSupply = Sector.Supply;
    Sector.Supply += FMath::Max(0.0f, SupplyAmount);
    ClampSectorState(Sector);
    const float Recovered = Sector.Supply - PreviousSupply;
    if (Recovered <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    RecordWarEvent(
        TEXT("FortificationMaterielRecovered"),
        Sector.SectorID,
        FString::Printf(
            TEXT("Engineers recovered %.0f supply from fieldworks at %s"),
            Recovered,
            *Sector.DisplayName.ToString()
        )
    );
    RecalculatePriority();
    BroadcastWarState();
    UE_LOG(LogTemp, Display, TEXT(
        "BH_FORTIFICATION_SUPPLY_RECOVERED sector=%s amount=%.1f supply=%.1f->%.1f"),
        *Sector.SectorID.ToString(),
        Recovered,
        PreviousSupply,
        Sector.Supply
    );
    return Recovered;
}

float UBHWarSubsystem::WithdrawFieldLogisticsSupply(
    FName SourceSectorID,
    float RequestedSupply
)
{
    if (!CanMutateWarState())
    {
        return 0.0f;
    }

    const int32 SectorIndex = FindSectorIndex(SourceSectorID);
    const float SafeRequest = FMath::Max(0.0f, RequestedSupply);

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        SafeRequest <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    FBHWarSectorState& Source = SectorStates[SectorIndex];

    if (Source.Owner != EBHWarFaction::Friendly)
    {
        return 0.0f;
    }

    const float AvailableSupply = FMath::Max(
        0.0f,
        Source.Supply - MinimumSupplyReserve
    );
    const float LoadedSupply = FMath::Min(
        SafeRequest,
        AvailableSupply
    );

    if (LoadedSupply <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const float PreviousSupply = Source.Supply;
    Source.Supply -= LoadedSupply;
    ClampSectorState(Source);
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_LOGISTICS_LOADED source=%s amount=%.1f "
            "supply=%.1f->%.1f"
        ),
        *SourceSectorID.ToString(),
        LoadedSupply,
        PreviousSupply,
        Source.Supply
    );
    return LoadedSupply;
}

float UBHWarSubsystem::DeliverFieldLogisticsSupply(
    FName SourceSectorID,
    FName DestinationSectorID,
    float AvailableSupply
)
{
    if (!CanMutateWarState())
    {
        return 0.0f;
    }

    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);
    const float SafeAvailableSupply =
        FMath::Max(0.0f, AvailableSupply);

    if (!SectorStates.IsValidIndex(DestinationIndex) ||
        SafeAvailableSupply <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    FBHWarSectorState& Destination =
        SectorStates[DestinationIndex];

    if (Destination.Owner != EBHWarFaction::Friendly)
    {
        return 0.0f;
    }

    const float PreviousSupply = Destination.Supply;
    Destination.Supply += SafeAvailableSupply;
    ClampSectorState(Destination);
    const float DeliveredSupply =
        Destination.Supply - PreviousSupply;

    if (DeliveredSupply <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const FBHWarSectorState Source =
        GetSectorState(SourceSectorID);
    RecordWarEvent(
        TEXT("FieldLogisticsDelivered"),
        DestinationSectorID,
        FString::Printf(
            TEXT(
                "Field transport delivered %.0f supply from %s "
                "to %s"
            ),
            DeliveredSupply,
            Source.SectorID.IsNone()
                ? *SourceSectorID.ToString()
                : *Source.DisplayName.ToString(),
            *Destination.DisplayName.ToString()
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_LOGISTICS_DELIVERED source=%s "
            "destination=%s amount=%.1f supply=%.1f->%.1f"
        ),
        *SourceSectorID.ToString(),
        *DestinationSectorID.ToString(),
        DeliveredSupply,
        PreviousSupply,
        Destination.Supply
    );
    return DeliveredSupply;
}

bool UBHWarSubsystem::
DoesFieldLogisticsDeliveryCompleteOperation(
    FName SourceSectorID,
    FName DestinationSectorID,
    float DeliveredSupply
) const
{
    return HasCommittedOperation() &&
        CommittedOperationType == EBHWarPriorityType::Resupply &&
        SourceSectorID == CommittedOperationSupplySourceSectorID &&
        DestinationSectorID == CommittedOperationSectorID &&
        DeliveredSupply + KINDA_SMALL_NUMBER >=
            PriorityOperationSupplyCost;
}

float UBHWarSubsystem::GetFieldLogisticsDeliveryCapacity(
    FName DestinationSectorID
) const
{
    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);

    if (!SectorStates.IsValidIndex(DestinationIndex))
    {
        return 0.0f;
    }

    const FBHWarSectorState& Destination =
        SectorStates[DestinationIndex];

    return Destination.Owner == EBHWarFaction::Friendly
        ? FMath::Max(
            0.0f,
            MaximumSectorSupply - Destination.Supply
        )
        : 0.0f;
}

FName UBHWarSubsystem::GetRecommendedFieldLogisticsDestination(
    FName SourceSectorID
) const
{
    if (HasCommittedOperation() &&
        CommittedOperationType == EBHWarPriorityType::Resupply &&
        SourceSectorID == CommittedOperationSupplySourceSectorID &&
        GetFieldLogisticsDeliveryCapacity(
            CommittedOperationSectorID
        ) > KINDA_SMALL_NUMBER)
    {
        return CommittedOperationSectorID;
    }

    FName RecommendedSectorID = NAME_None;
    float RecommendedSupply = TNumericLimits<float>::Max();
    bool bRecommendedSectorIsFrontline = false;

    for (int32 SectorIndex = 0;
        SectorIndex < SectorStates.Num();
        ++SectorIndex)
    {
        const FBHWarSectorState& Sector =
            SectorStates[SectorIndex];

        if (Sector.Owner != EBHWarFaction::Friendly ||
            Sector.SectorID == SourceSectorID ||
            GetFieldLogisticsDeliveryCapacity(Sector.SectorID) <=
                KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const bool bSectorIsFrontline =
            IsFrontlineSector(SectorIndex);
        const bool bIsBetterRecommendation =
            RecommendedSectorID.IsNone() ||
            (
                bSectorIsFrontline &&
                !bRecommendedSectorIsFrontline
            ) ||
            (
                bSectorIsFrontline ==
                    bRecommendedSectorIsFrontline &&
                Sector.Supply < RecommendedSupply
            );

        if (!bIsBetterRecommendation)
        {
            continue;
        }

        RecommendedSectorID = Sector.SectorID;
        RecommendedSupply = Sector.Supply;
        bRecommendedSectorIsFrontline = bSectorIsFrontline;
    }

    return RecommendedSectorID;
}

FName UBHWarSubsystem::
GetRecommendedFieldCivilianAidDestination(
    FName SourceSectorID
) const
{
    const int32 SourceIndex = FindSectorIndex(SourceSectorID);

    if (CampaignOutcome != EBHWarCampaignOutcome::Ongoing ||
        !SectorStates.IsValidIndex(SourceIndex))
    {
        return NAME_None;
    }

    const FBHWarSectorState& Source = SectorStates[SourceIndex];

    if (Source.Owner != EBHWarFaction::Friendly ||
        !IsSectorConnectedToFactionLogistics(SourceSectorID) ||
        Source.Supply + KINDA_SMALL_NUMBER <
            CivilianAidSupplyCost)
    {
        return NAME_None;
    }

    return GetRecommendedInTransitCivilianAidDestination(
        SourceSectorID,
        NAME_None
    );
}

FName UBHWarSubsystem::
GetRecommendedInTransitCivilianAidDestination(
    FName SourceSectorID,
    FName PreferredDestinationSectorID
) const
{
    if (CampaignOutcome != EBHWarCampaignOutcome::Ongoing)
    {
        return NAME_None;
    }

    const auto CanAidImproveSector =
        [this](const FBHWarSectorState& Sector)
        {
            return
                Sector.CivilianSupport <
                    MaximumCivilianSupport -
                        KINDA_SMALL_NUMBER ||
                Sector.IntelConfidence <
                    MaximumIntelConfidence -
                        KINDA_SMALL_NUMBER ||
                (
                    Sector.Owner ==
                        EBHWarFaction::Friendly &&
                    Sector.EnemyResponsePressure >
                        KINDA_SMALL_NUMBER
                );
        };

    const int32 PreferredIndex =
        FindSectorIndex(PreferredDestinationSectorID);

    if (SectorStates.IsValidIndex(PreferredIndex) &&
        PreferredDestinationSectorID != SourceSectorID &&
        CanAidImproveSector(SectorStates[PreferredIndex]))
    {
        return PreferredDestinationSectorID;
    }

    FName RecommendedSectorID = NAME_None;
    float HighestNeedScore = -BIG_NUMBER;

    for (const FBHWarSectorState& Sector : SectorStates)
    {
        if (Sector.SectorID == SourceSectorID)
        {
            continue;
        }

        if (!CanAidImproveSector(Sector))
        {
            continue;
        }

        const float NeedScore =
            (Sector.Owner == EBHWarFaction::Friendly
                ? 0.0f
                : 1000.0f) +
            ((MaximumCivilianSupport -
                Sector.CivilianSupport) * 10.0f) +
            (MaximumIntelConfidence -
                Sector.IntelConfidence) +
            (Sector.Owner == EBHWarFaction::Friendly
                ? Sector.EnemyResponsePressure
                : 0.0f);

        if (NeedScore > HighestNeedScore)
        {
            HighestNeedScore = NeedScore;
            RecommendedSectorID = Sector.SectorID;
        }
    }

    if (RecommendedSectorID.IsNone() &&
        SectorStates.IsValidIndex(PreferredIndex) &&
        PreferredDestinationSectorID != SourceSectorID)
    {
        return PreferredDestinationSectorID;
    }

    return RecommendedSectorID;
}

float UBHWarSubsystem::WithdrawFieldCivilianAidSupply(
    FName SourceSectorID,
    FName DestinationSectorID
)
{
    if (!CanMutateWarState())
    {
        return 0.0f;
    }

    if (DestinationSectorID.IsNone() ||
        GetRecommendedFieldCivilianAidDestination(
            SourceSectorID
        ).IsNone())
    {
        return 0.0f;
    }

    const int32 SourceIndex = FindSectorIndex(SourceSectorID);
    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);

    if (!SectorStates.IsValidIndex(SourceIndex) ||
        !SectorStates.IsValidIndex(DestinationIndex) ||
        SourceSectorID == DestinationSectorID)
    {
        return 0.0f;
    }

    FBHWarSectorState& Source = SectorStates[SourceIndex];
    const FBHWarSectorState& Destination =
        SectorStates[DestinationIndex];
    const bool bAidCanImproveNetwork =
        Destination.CivilianSupport <
            MaximumCivilianSupport - KINDA_SMALL_NUMBER ||
        Destination.IntelConfidence <
            MaximumIntelConfidence - KINDA_SMALL_NUMBER ||
        (Destination.Owner == EBHWarFaction::Friendly &&
            Destination.EnemyResponsePressure >
                KINDA_SMALL_NUMBER);

    if (Source.Owner != EBHWarFaction::Friendly ||
        !IsSectorConnectedToFactionLogistics(SourceSectorID) ||
        !bAidCanImproveNetwork ||
        Source.Supply + KINDA_SMALL_NUMBER <
            CivilianAidSupplyCost)
    {
        return 0.0f;
    }

    const float PreviousSupply = Source.Supply;
    Source.Supply -= CivilianAidSupplyCost;
    ClampSectorState(Source);
    RecordWarEvent(
        TEXT("FieldCivilianAidLoaded"),
        DestinationSectorID,
        FString::Printf(
            TEXT(
                "Player loaded civilian aid at %s for delivery "
                "to %s"
            ),
            *Source.DisplayName.ToString(),
            *SectorStates[DestinationIndex].DisplayName.ToString()
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_CIVILIAN_AID_LOADED source=%s "
            "destination=%s supply=%.1f->%.1f cargo=%.1f"
        ),
        *SourceSectorID.ToString(),
        *DestinationSectorID.ToString(),
        PreviousSupply,
        Source.Supply,
        CivilianAidSupplyCost
    );
    return CivilianAidSupplyCost;
}

bool UBHWarSubsystem::DeliverFieldCivilianAidSupply(
    FName SourceSectorID,
    FName DestinationSectorID,
    float AvailableAidSupply
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);

    if (CampaignOutcome != EBHWarCampaignOutcome::Ongoing ||
        !SectorStates.IsValidIndex(DestinationIndex) ||
        AvailableAidSupply + KINDA_SMALL_NUMBER <
            CivilianAidSupplyCost)
    {
        return false;
    }

    FBHWarSectorState& Destination =
        SectorStates[DestinationIndex];
    const float PreviousSupport =
        Destination.CivilianSupport;
    const float PreviousIntel =
        Destination.IntelConfidence;
    const float PreviousResponse =
        Destination.EnemyResponsePressure;
    Destination.CivilianSupport += CivilianAidSupportGain;
    Destination.IntelConfidence += CivilianAidIntelGain;

    if (Destination.Owner == EBHWarFaction::Friendly)
    {
        Destination.EnemyResponsePressure -=
            CivilianAidFriendlyResponseReduction;
    }
    else
    {
        Destination.EnemyResponsePressure +=
            CivilianAidHostileExposurePressure;
    }

    ClampSectorState(Destination);
    RecordEnemyResponseEscalation(
        Destination,
        PreviousResponse
    );
    RecordWarEvent(
        TEXT("FieldCivilianAidDelivered"),
        DestinationSectorID,
        FString::Printf(
            TEXT(
                "Player delivered civilian aid from %s to %s; "
                "support rose to %.0f%% and intelligence to %.0f%%"
            ),
            *SourceSectorID.ToString(),
            *Destination.DisplayName.ToString(),
            Destination.CivilianSupport,
            Destination.IntelConfidence
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_CIVILIAN_AID_DELIVERED source=%s "
            "destination=%s support=%.1f->%.1f "
            "intel=%.1f->%.1f response=%.1f->%.1f"
        ),
        *SourceSectorID.ToString(),
        *DestinationSectorID.ToString(),
        PreviousSupport,
        Destination.CivilianSupport,
        PreviousIntel,
        Destination.IntelConfidence,
        PreviousResponse,
        Destination.EnemyResponsePressure
    );
    return true;
}

float UBHWarSubsystem::CommitAmbientPatrolSupply(
    FName SectorID,
    EBHWarFaction ForceFaction,
    float RequestedSupply
)
{
    if (!CanMutateWarState())
    {
        return 0.0f;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    const float SafeRequestedSupply = FMath::Max(
        0.0f,
        RequestedSupply
    );

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        SafeRequestedSupply <= KINDA_SMALL_NUMBER ||
        (ForceFaction != EBHWarFaction::Friendly &&
            ForceFaction != EBHWarFaction::Enemy))
    {
        return 0.0f;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float ForceStrength =
        ForceFaction == EBHWarFaction::Friendly
            ? Sector.FriendlyStrength
            : Sector.EnemyStrength;

    if (ForceStrength <= KINDA_SMALL_NUMBER ||
        Sector.Supply <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const float PreviousSupply = Sector.Supply;
    const float CommittedSupply = FMath::Min(
        PreviousSupply,
        SafeRequestedSupply
    );
    Sector.Supply -= CommittedSupply;
    ClampSectorState(Sector);
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_AMBIENT_PATROL_SUPPLY_COMMITTED sector=%s "
            "faction=%d requested=%.2f committed=%.2f "
            "supply=%.1f->%.1f"
        ),
        *Sector.SectorID.ToString(),
        static_cast<int32>(ForceFaction),
        SafeRequestedSupply,
        CommittedSupply,
        PreviousSupply,
        Sector.Supply
    );
    return CommittedSupply;
}

bool UBHWarSubsystem::ConsumePriorityOperationSupply()
{
    if (!CanMutateWarState())
    {
        return false;
    }

    return ConsumeOperationSupply(
        PrioritySectorID,
        PriorityType
    );
}

bool UBHWarSubsystem::ConsumeOperationSupply(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    if (OperationType == EBHWarPriorityType::Resupply)
    {
        return CanFundOperation(TargetSectorID, OperationType);
    }

    const TArray<int32> RouteIndices =
        BuildOperationSupplyRouteIndices(
            TargetSectorID,
            OperationType
        );
    const int32 SourceIndex = RouteIndices.IsEmpty()
        ? INDEX_NONE
        : RouteIndices[0];

    const float TotalSupplyCost = GetOperationSupplyCost(
        TargetSectorID,
        OperationType
    );
    if (!SectorStates.IsValidIndex(SourceIndex) ||
        SectorStates[SourceIndex].Supply + KINDA_SMALL_NUMBER <
            TotalSupplyCost)
    {
        return false;
    }

    FBHWarSectorState& SourceSector = SectorStates[SourceIndex];
    SourceSector.Supply = FMath::Max(
        0.0f,
        SourceSector.Supply - TotalSupplyCost
    );
    ClampSectorState(SourceSector);
    BroadcastWarState();

    FString RouteLog;

    for (const int32 RouteIndex : RouteIndices)
    {
        if (!SectorStates.IsValidIndex(RouteIndex))
        {
            continue;
        }

        if (!RouteLog.IsEmpty())
        {
            RouteLog += TEXT(">");
        }

        RouteLog +=
            SectorStates[RouteIndex].SectorID.ToString();
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_OPERATION_SUPPLY_COMMITTED source=%s target=%s "
            "cost=%.1f tactical_cost=%.1f remaining=%.1f route=%s"
        ),
        *SourceSector.SectorID.ToString(),
        *TargetSectorID.ToString(),
        TotalSupplyCost,
        TotalSupplyCost -
            (OperationType == EBHWarPriorityType::Recon
                ? ReconOperationSupplyCost
                : PriorityOperationSupplyCost),
        SourceSector.Supply,
        *RouteLog
    );
    return true;
}

bool UBHWarSubsystem::CanFundPriorityOperation() const
{
    return CanFundOperation(
        PrioritySectorID,
        PriorityType
    );
}

bool UBHWarSubsystem::CanFundOperation(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
) const
{
    const TArray<int32> RouteIndices =
        BuildOperationSupplyRouteIndices(
            TargetSectorID,
            OperationType
        );
    const int32 SourceIndex = RouteIndices.IsEmpty()
        ? INDEX_NONE
        : RouteIndices[0];

    if (!SectorStates.IsValidIndex(SourceIndex))
    {
        return false;
    }

    const float OperationSupplyCost = GetOperationSupplyCost(
        TargetSectorID,
        OperationType
    );
    const float RequiredSourceSupply =
        OperationType == EBHWarPriorityType::Resupply
            ? MinimumSupplyReserve + OperationSupplyCost
            : OperationSupplyCost;
    return SectorStates[SourceIndex].Supply + KINDA_SMALL_NUMBER >=
            RequiredSourceSupply &&
        (OperationType != EBHWarPriorityType::Resupply ||
         GetFieldLogisticsDeliveryCapacity(TargetSectorID) +
                KINDA_SMALL_NUMBER >= OperationSupplyCost);
}

FName UBHWarSubsystem::GetPriorityOperationSupplySource() const
{
    return GetOperationSupplySource(
        PrioritySectorID,
        PriorityType
    );
}

FName UBHWarSubsystem::GetOperationSupplySource(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
) const
{
    const TArray<int32> RouteIndices =
        BuildOperationSupplyRouteIndices(
            TargetSectorID,
            OperationType
        );
    const int32 SourceIndex = RouteIndices.IsEmpty()
        ? INDEX_NONE
        : RouteIndices[0];

    return SectorStates.IsValidIndex(SourceIndex)
        ? SectorStates[SourceIndex].SectorID
        : NAME_None;
}

FName UBHWarSubsystem::GetOperationEnemySource(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
) const
{
    const int32 TargetIndex = FindSectorIndex(TargetSectorID);

    if (!SectorStates.IsValidIndex(TargetIndex) ||
        OperationType == EBHWarPriorityType::None)
    {
        return NAME_None;
    }

    if ((OperationType == EBHWarPriorityType::Attack ||
         OperationType == EBHWarPriorityType::Raid) &&
        SectorStates[TargetIndex].Owner == EBHWarFaction::Enemy)
    {
        return TargetSectorID;
    }

    TArray<int32> Distances;
    TArray<int32> SearchQueue;
    Distances.Init(INDEX_NONE, SectorStates.Num());
    Distances[TargetIndex] = 0;
    SearchQueue.Add(TargetIndex);

    int32 BestSourceIndex = INDEX_NONE;
    int32 BestDistance = MAX_int32;
    float BestStrength = -1.0f;
    int32 BestGarrison = -1;

    for (int32 QueueIndex = 0;
        QueueIndex < SearchQueue.Num();
        ++QueueIndex)
    {
        const int32 CurrentIndex = SearchQueue[QueueIndex];
        const int32 CurrentDistance = Distances[CurrentIndex];

        if (CurrentDistance > BestDistance)
        {
            break;
        }

        const FBHWarSectorState& CurrentSector =
            SectorStates[CurrentIndex];

        if (CurrentSector.Owner == EBHWarFaction::Enemy)
        {
            if (CurrentDistance < BestDistance ||
                CurrentSector.EnemyStrength > BestStrength ||
                (FMath::IsNearlyEqual(
                    CurrentSector.EnemyStrength,
                    BestStrength
                ) &&
                 CurrentSector.EnemyGarrison > BestGarrison))
            {
                BestSourceIndex = CurrentIndex;
                BestDistance = CurrentDistance;
                BestStrength = CurrentSector.EnemyStrength;
                BestGarrison = CurrentSector.EnemyGarrison;
            }

            continue;
        }

        for (const FName ConnectedSectorID :
            CurrentSector.ConnectedSectorIDs)
        {
            const int32 ConnectedIndex =
                FindSectorIndex(ConnectedSectorID);

            if (SectorStates.IsValidIndex(ConnectedIndex) &&
                Distances[ConnectedIndex] == INDEX_NONE)
            {
                Distances[ConnectedIndex] = CurrentDistance + 1;
                SearchQueue.Add(ConnectedIndex);
            }
        }
    }

    return SectorStates.IsValidIndex(BestSourceIndex)
        ? SectorStates[BestSourceIndex].SectorID
        : NAME_None;
}

TArray<FName>
UBHWarSubsystem::GetPriorityOperationSupplyRoute() const
{
    return GetOperationSupplyRoute(
        PrioritySectorID,
        PriorityType
    );
}

TArray<FName> UBHWarSubsystem::GetOperationSupplyRoute(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
) const
{
    TArray<FName> Route;

    for (const int32 SectorIndex :
        BuildOperationSupplyRouteIndices(
            TargetSectorID,
            OperationType
        ))
    {
        if (SectorStates.IsValidIndex(SectorIndex))
        {
            Route.Add(SectorStates[SectorIndex].SectorID);
        }
    }

    return Route;
}

float UBHWarSubsystem::GetPriorityOperationSupplyCost() const
{
    return GetOperationSupplyCost(PrioritySectorID, PriorityType);
}

float UBHWarSubsystem::GetOperationSupplyCost(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
) const
{
    const bool bSupportsTacticalPlanning =
        HasSector(TargetSectorID) &&
        (OperationType == EBHWarPriorityType::Attack ||
         OperationType == EBHWarPriorityType::Defend ||
         OperationType == EBHWarPriorityType::Raid);
    const float BaseOperationCost =
        OperationType == EBHWarPriorityType::Recon
            ? ReconOperationSupplyCost
            : PriorityOperationSupplyCost;
    return BaseOperationCost +
        (bSupportsTacticalPlanning
            ? GetActiveTacticalOptionSupplyCost()
            : 0.0f);
}

float UBHWarSubsystem::GetActiveTacticalOptionSupplyCost() const
{
    switch (CampaignProgression.ActiveTacticalOption)
    {
        case EBHOperationTacticalOption::ReconPlanning:
            return IsTacticalOptionUnlocked(
                EBHOperationTacticalOption::ReconPlanning)
                ? ReconPlanningSupplyCost
                : 0.0f;
        case EBHOperationTacticalOption::ReinforcementPriority:
            return IsTacticalOptionUnlocked(
                EBHOperationTacticalOption::ReinforcementPriority)
                ? ReinforcementPrioritySupplyCost
                : 0.0f;
        case EBHOperationTacticalOption::MedicalPreparation:
            return IsTacticalOptionUnlocked(
                EBHOperationTacticalOption::MedicalPreparation)
                ? MedicalPreparationSupplyCost
                : 0.0f;
        default:
            return 0.0f;
    }
}

bool UBHWarSubsystem::IsViableOperation(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
) const
{
    const int32 SectorIndex = FindSectorIndex(TargetSectorID);

    if (IsCampaignResolved() ||
        !SectorStates.IsValidIndex(SectorIndex) ||
        OperationType == EBHWarPriorityType::None)
    {
        return false;
    }

    if (HasCommittedOperation())
    {
        return TargetSectorID == CommittedOperationSectorID &&
            OperationType == CommittedOperationType;
    }

    const FBHWarSectorState& Sector = SectorStates[SectorIndex];

    if (OperationType == EBHWarPriorityType::Attack)
    {
        return Sector.Owner != EBHWarFaction::Friendly &&
            !GetOperationSupplySource(
                TargetSectorID,
                OperationType
            ).IsNone();
    }

    if (OperationType == EBHWarPriorityType::Raid)
    {
        return Sector.Owner == EBHWarFaction::Enemy &&
            Sector.Supply > KINDA_SMALL_NUMBER &&
            !GetOperationSupplySource(
                TargetSectorID,
                OperationType
            ).IsNone();
    }

    if (OperationType == EBHWarPriorityType::Resupply)
    {
        return Sector.Owner == EBHWarFaction::Friendly &&
            GetFieldLogisticsDeliveryCapacity(TargetSectorID) +
                    KINDA_SMALL_NUMBER >=
                PriorityOperationSupplyCost &&
            !GetOperationSupplySource(
                TargetSectorID,
                OperationType
            ).IsNone();
    }

    if (OperationType == EBHWarPriorityType::EscortRescue)
    {
        return Sector.Owner == EBHWarFaction::Friendly &&
            !GetEscortOperationTargetID(TargetSectorID).IsNone() &&
            !GetOperationSupplySource(
                TargetSectorID,
                OperationType
            ).IsNone();
    }

    if (OperationType == EBHWarPriorityType::Rescue)
    {
        return Sector.Owner == EBHWarFaction::Friendly &&
            !GetOperationSupplySource(
                TargetSectorID,
                OperationType
            ).IsNone();
    }

    if (OperationType == EBHWarPriorityType::Recon)
    {
        return Sector.Owner != EBHWarFaction::Friendly &&
            Sector.IntelConfidence <
                MaximumIntelConfidence - KINDA_SMALL_NUMBER &&
            !GetOperationSupplySource(
                TargetSectorID,
                OperationType
            ).IsNone();
    }

    return OperationType == EBHWarPriorityType::Defend &&
        Sector.Owner == EBHWarFaction::Friendly &&
        (IsFrontlineSector(SectorIndex) ||
            Sector.EnemyResponsePressure >=
                CounterinsurgencyResponseThreshold) &&
        !GetOperationSupplySource(
            TargetSectorID,
            OperationType
        ).IsNone();
}

TArray<FBHWarSectorState> UBHWarSubsystem::GetSectorStates() const
{
    return SectorStates;
}

TArray<FBHWarSupplyConvoyState>
UBHWarSubsystem::GetSupplyConvoys() const
{
    return SupplyConvoys;
}

TArray<FBHWarGarrisonTransferState>
UBHWarSubsystem::GetGarrisonTransfers() const
{
    return GarrisonTransfers;
}

int32 UBHWarSubsystem::GetFactionManpowerReserve(
    EBHWarFaction ForceFaction
) const
{
    if (ForceFaction == EBHWarFaction::Friendly)
    {
        return FriendlyManpowerReserve;
    }

    if (ForceFaction == EBHWarFaction::Enemy)
    {
        return EnemyManpowerReserve;
    }

    return 0;
}

bool UBHWarSubsystem::CanRecruitFieldOperative(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (CampaignOutcome != EBHWarCampaignOutcome::Ongoing ||
        !SectorStates.IsValidIndex(SectorIndex))
    {
        return false;
    }

    const FBHWarSectorState& Sector = SectorStates[SectorIndex];
    return Sector.Owner == EBHWarFaction::Friendly &&
        IsSectorConnectedToFactionLogistics(SectorID) &&
        FriendlyManpowerReserve >= FieldOperativeManpowerCost &&
        Sector.Supply + KINDA_SMALL_NUMBER >=
            FieldOperativeSupplyCost;
}

bool UBHWarSubsystem::RecruitFieldOperative(FName SectorID)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    if (!CanRecruitFieldOperative(SectorID))
    {
        return false;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const int32 PreviousManpower = FriendlyManpowerReserve;
    const float PreviousSupply = Sector.Supply;
    FriendlyManpowerReserve -= FieldOperativeManpowerCost;
    Sector.Supply -= FieldOperativeSupplyCost;
    ClampSectorState(Sector);

    RecordWarEvent(
        TEXT("FieldOperativeRecruited"),
        SectorID,
        FString::Printf(
            TEXT(
                "%s equipped a field operative for the player fireteam"
            ),
            *Sector.DisplayName.ToString()
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_FIELD_OPERATIVE_RECRUITED sector=%s "
            "manpower=%d->%d supply=%.1f->%.1f"
        ),
        *SectorID.ToString(),
        PreviousManpower,
        FriendlyManpowerReserve,
        PreviousSupply,
        Sector.Supply
    );
    return true;
}

float UBHWarSubsystem::GetFieldOperativeSupplyCost() const
{
    return FieldOperativeSupplyCost;
}

float UBHWarSubsystem::GetFactionRecruitmentProgress(
    EBHWarFaction ForceFaction
) const
{
    if (ForceFaction == EBHWarFaction::Friendly)
    {
        return FriendlyRecruitmentProgress;
    }

    if (ForceFaction == EBHWarFaction::Enemy)
    {
        return EnemyRecruitmentProgress;
    }

    return 0.0f;
}

float UBHWarSubsystem::GetFactionRecruitmentPerTurn(
    EBHWarFaction ForceFaction
) const
{
    if (ForceFaction == EBHWarFaction::Neutral)
    {
        return 0.0f;
    }

    float Recruitment = 0.0f;

    for (const FBHWarSectorState& Sector : SectorStates)
    {
        const float LocalSupport =
            ForceFaction == EBHWarFaction::Friendly
                ? Sector.CivilianSupport
                : MaximumCivilianSupport -
                    Sector.CivilianSupport;
        const float ControlFactor =
            Sector.Owner == ForceFaction
                ? 1.0f
                : Sector.Owner == EBHWarFaction::Neutral
                    ? NeutralRecruitmentFactor
                    : UndergroundRecruitmentFactor;
        Recruitment +=
            GetSiteRecruitmentWeight(Sector.SiteType) *
            FMath::Clamp(
                LocalSupport / MaximumCivilianSupport,
                0.0f,
                1.0f
            ) *
            ControlFactor;
    }

    return Recruitment;
}

TArray<FBHWarEventRecord>
UBHWarSubsystem::GetRecentWarEvents() const
{
    return RecentWarEvents;
}

int32 UBHWarSubsystem::GetRecentConvoyInterdictionCount(
    FName DestinationSectorID,
    int32 TurnWindow
) const
{
    if (DestinationSectorID.IsNone())
    {
        return 0;
    }

    const int32 EarliestIncludedTurn = FMath::Max(
        0,
        TurnNumber - FMath::Max(0, TurnWindow)
    );
    int32 InterdictionCount = 0;

    for (const FBHWarEventRecord& Event : RecentWarEvents)
    {
        if (Event.EventType == TEXT("ConvoyInterdicted") &&
            Event.SectorID == DestinationSectorID &&
            Event.TurnNumber >= EarliestIncludedTurn &&
            Event.TurnNumber <= TurnNumber)
        {
            ++InterdictionCount;
        }
    }

    return InterdictionCount;
}

bool UBHWarSubsystem::HasSupplyConvoy(
    FName ConvoyID
) const
{
    return SupplyConvoys.ContainsByPredicate(
        [ConvoyID](
            const FBHWarSupplyConvoyState& Convoy
        )
        {
            return Convoy.ConvoyID == ConvoyID;
        }
    );
}

FBHWarSupplyConvoyState
UBHWarSubsystem::GetSupplyConvoyState(
    FName ConvoyID
) const
{
    const FBHWarSupplyConvoyState* Convoy =
        SupplyConvoys.FindByPredicate(
            [ConvoyID](
                const FBHWarSupplyConvoyState& Candidate
            )
            {
                return Candidate.ConvoyID == ConvoyID;
            }
        );

    return Convoy
        ? *Convoy
        : FBHWarSupplyConvoyState();
}

bool UBHWarSubsystem::SetConvoySelectedWorldRoute(
    FName ConvoyID,
    FName WorldRouteID
)
{
    if (!CanMutateWarState() || ConvoyID.IsNone() ||
        WorldRouteID.IsNone() ||
        !DoesConvoyMatchCommittedEscort(ConvoyID))
    {
        return false;
    }

    FBHWarSupplyConvoyState* Convoy = SupplyConvoys.FindByPredicate(
        [ConvoyID](const FBHWarSupplyConvoyState& Candidate)
        {
            return Candidate.ConvoyID == ConvoyID;
        }
    );
    if (!Convoy)
    {
        return false;
    }

    Convoy->SelectedWorldRouteID = WorldRouteID;
    BroadcastWarState();
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_CONVOY_ROUTE_SELECTED id=%s route=%s"),
        *ConvoyID.ToString(),
        *WorldRouteID.ToString()
    );
    return true;
}

FName UBHWarSubsystem::GetEscortOperationTargetID(
    FName DestinationSectorID
) const
{
    const FBHWarSupplyConvoyState* BestConvoy = nullptr;

    for (const FBHWarSupplyConvoyState& Convoy : SupplyConvoys)
    {
        if (Convoy.Owner != EBHWarFaction::Friendly ||
            Convoy.DestinationSectorID != DestinationSectorID ||
            Convoy.ConvoyID.IsNone())
        {
            continue;
        }

        if (!BestConvoy ||
            Convoy.DispatchTurn < BestConvoy->DispatchTurn ||
            (Convoy.DispatchTurn == BestConvoy->DispatchTurn &&
             Convoy.ConvoyID.ToString() <
                BestConvoy->ConvoyID.ToString()))
        {
            BestConvoy = &Convoy;
        }
    }

    return BestConvoy ? BestConvoy->ConvoyID : NAME_None;
}

bool UBHWarSubsystem::InterdictSupplyConvoy(
    FName ConvoyID
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 ConvoyIndex =
        SupplyConvoys.IndexOfByPredicate(
            [ConvoyID](
                const FBHWarSupplyConvoyState& Convoy
            )
            {
                return Convoy.ConvoyID == ConvoyID;
            }
        );

    if (!SupplyConvoys.IsValidIndex(ConvoyIndex))
    {
        return false;
    }

    const FBHWarSupplyConvoyState InterdictedConvoy =
        SupplyConvoys[ConvoyIndex];
    SupplyConvoys.RemoveAtSwap(ConvoyIndex);
    const FBHWarSectorState Destination =
        GetSectorState(InterdictedConvoy.DestinationSectorID);
    RecordWarEvent(
        TEXT("ConvoyInterdicted"),
        InterdictedConvoy.DestinationSectorID,
        FString::Printf(
            TEXT("%s convoy interdicted before reaching %s"),
            InterdictedConvoy.CargoType ==
                EBHWarConvoyCargoType::CivilianAid
                    ? TEXT("Civilian aid")
                    : InterdictedConvoy.Owner ==
                        EBHWarFaction::Friendly
                        ? TEXT("Friendly")
                        : TEXT("Enemy"),
            Destination.SectorID.IsNone()
                ? *InterdictedConvoy.DestinationSectorID.ToString()
                : *Destination.DisplayName.ToString()
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_WAR_CONVOY_INTERDICTED id=%s from=%s "
            "to=%s payload=%.1f owner=%d"
        ),
        *InterdictedConvoy.ConvoyID.ToString(),
        *InterdictedConvoy.SourceSectorID.ToString(),
        *InterdictedConvoy.DestinationSectorID.ToString(),
        InterdictedConvoy.SupplyPayload,
        static_cast<int32>(InterdictedConvoy.Owner)
    );

    RecalculatePriority();
    BroadcastWarState();
    return true;
}

float UBHWarSubsystem::GetIncomingConvoySupply(
    FName SectorID
) const
{
    float IncomingSupply = 0.0f;

    for (const FBHWarSupplyConvoyState& Convoy :
        SupplyConvoys)
    {
        if (Convoy.DestinationSectorID == SectorID)
        {
            if (Convoy.CargoType ==
                EBHWarConvoyCargoType::MilitarySupply)
            {
                IncomingSupply += Convoy.SupplyPayload;
            }
        }
    }

    return IncomingSupply;
}

float UBHWarSubsystem::GetOutgoingConvoySupply(
    FName SectorID
) const
{
    float OutgoingSupply = 0.0f;

    for (const FBHWarSupplyConvoyState& Convoy :
        SupplyConvoys)
    {
        if (Convoy.SourceSectorID == SectorID)
        {
            if (Convoy.CargoType ==
                EBHWarConvoyCargoType::MilitarySupply)
            {
                OutgoingSupply += Convoy.SupplyPayload;
            }
        }
    }

    return OutgoingSupply;
}

FBHWarSectorState UBHWarSubsystem::GetSectorState(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    return SectorStates.IsValidIndex(SectorIndex)
        ? SectorStates[SectorIndex]
        : FBHWarSectorState();
}

#if !UE_BUILD_SHIPPING
void UBHWarSubsystem::SetSectorOwnerForTesting(
    FName SectorID,
    EBHWarFaction Owner
)
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (SectorStates.IsValidIndex(SectorIndex))
    {
        SectorStates[SectorIndex].Owner = Owner;
    }
}

void UBHWarSubsystem::SetSectorSupplyForTesting(
    FName SectorID,
    float Supply
)
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (SectorStates.IsValidIndex(SectorIndex))
    {
        SectorStates[SectorIndex].Supply = FMath::Clamp(
            Supply,
            0.0f,
            MaximumSectorSupply
        );
    }
}
#endif

bool UBHWarSubsystem::HasSector(FName SectorID) const
{
    return FindSectorIndex(SectorID) != INDEX_NONE;
}

int32 UBHWarSubsystem::GetSectorGarrisonCount(
    FName SectorID,
    EBHWarFaction ForceFaction
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return 0;
    }

    if (ForceFaction == EBHWarFaction::Friendly)
    {
        return SectorStates[SectorIndex].FriendlyGarrison;
    }

    if (ForceFaction == EBHWarFaction::Enemy)
    {
        return SectorStates[SectorIndex].EnemyGarrison;
    }

    return 0;
}

int32 UBHWarSubsystem::GetSectorGarrisonCapacity(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    return SectorStates.IsValidIndex(SectorIndex)
        ? SectorStates[SectorIndex].GarrisonCapacity
        : 0;
}

float UBHWarSubsystem::GetSectorIntelConfidence(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    return SectorStates.IsValidIndex(SectorIndex)
        ? SectorStates[SectorIndex].IntelConfidence
        : 0.0f;
}

float UBHWarSubsystem::GetSectorCivilianSupport(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    return SectorStates.IsValidIndex(SectorIndex)
        ? SectorStates[SectorIndex].CivilianSupport
        : NeutralCivilianSupport;
}

FText UBHWarSubsystem::GetSectorEnemyIntelSummary(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "MissingSectorIntel",
            "INTEL UNAVAILABLE"
        );
    }

    const FBHWarSectorState& Sector =
        SectorStates[SectorIndex];
    const int32 RoundedConfidence =
        FMath::RoundToInt(Sector.IntelConfidence);

    if (Sector.IntelConfidence <
        EstimatedIntelThreshold)
    {
        const TCHAR* Activity =
            Sector.EnemyStrength < 20.0f
                ? TEXT("LOW")
                : Sector.EnemyStrength < 60.0f
                    ? TEXT("MEDIUM")
                    : TEXT("HIGH");

        return FText::FromString(FString::Printf(
            TEXT(
                "INTEL %d%% // ENEMY ACTIVITY %s // "
                "STRENGTH UNKNOWN"
            ),
            RoundedConfidence,
            Activity
        ));
    }

    if (Sector.IntelConfidence <
        ConfirmedIntelThreshold)
    {
        const int32 EstimatedStrength =
            FMath::RoundToInt(
                Sector.EnemyStrength / 10.0f
            ) * 10;
        const int32 EstimatedGarrison =
            FMath::Clamp(
                FMath::RoundToInt(
                    Sector.EnemyGarrison / 2.0f
                ) * 2,
                0,
                Sector.GarrisonCapacity
            );

        return FText::FromString(FString::Printf(
            TEXT(
                "INTEL %d%% // EST ENEMY %d // "
                "GARRISON ~%d"
            ),
            RoundedConfidence,
            EstimatedStrength,
            EstimatedGarrison
        ));
    }

    return FText::FromString(FString::Printf(
        TEXT(
            "INTEL %d%% // CONFIRMED ENEMY %d // "
            "GARRISON %d"
        ),
        RoundedConfidence,
        FMath::RoundToInt(Sector.EnemyStrength),
        Sector.EnemyGarrison
    ));
}

float UBHWarSubsystem::GetSectorEnemyResponsePressure(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);
    return SectorStates.IsValidIndex(SectorIndex)
        ? SectorStates[SectorIndex].EnemyResponsePressure
        : 0.0f;
}

FText UBHWarSubsystem::GetSectorEnemyResponseSummary(
    FName SectorID
) const
{
    const float Pressure =
        GetSectorEnemyResponsePressure(SectorID);
    const TCHAR* Status = Pressure >= 75.0f
        ? TEXT("CRACKDOWN")
        : Pressure >= 50.0f
            ? TEXT("HUNTING")
            : Pressure >= 25.0f
                ? TEXT("WATCHFUL")
                : TEXT("DORMANT");

    return FText::FromString(FString::Printf(
        TEXT("RESPONSE %s %.0f%%"),
        Status,
        Pressure
    ));
}

FText UBHWarSubsystem::GetSectorEnemyAdaptationSummary(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return NSLOCTEXT(
            "BrokenHorizon", "WarMapPatternUnknown",
            "PATTERN UNKNOWN");
    }

    const FBHWarSectorState& Sector = SectorStates[SectorIndex];

    if (Sector.AnticipatedOperationType ==
            EBHWarPriorityType::None ||
        Sector.RepeatedOperationCount <= 0)
    {
        return FText::FromString(TEXT("PATTERN UNREAD"));
    }

    const TCHAR* OperationLabel =
        Sector.AnticipatedOperationType ==
            EBHWarPriorityType::Attack
            ? TEXT("ASSAULT")
            : Sector.AnticipatedOperationType ==
                EBHWarPriorityType::Raid
                ? TEXT("RAID")
                : TEXT("DEFENSE");
    const TCHAR* ReadLabel =
        Sector.RepeatedOperationCount >= 2
            ? TEXT("COUNTER READY")
            : TEXT("ENEMY WATCHING");

    return FText::FromString(FString::Printf(
        TEXT("%s %s x%d"),
        ReadLabel,
        OperationLabel,
        Sector.RepeatedOperationCount
    ));
}

bool UBHWarSubsystem::ReportSectorRecon(
    FName SectorID,
    float ConfidenceGain
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        ConfidenceGain <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousConfidence = Sector.IntelConfidence;
    Sector.IntelConfidence = FMath::Clamp(
        Sector.IntelConfidence + ConfidenceGain,
        0.0f,
        MaximumIntelConfidence
    );

    if (FMath::IsNearlyEqual(
            PreviousConfidence,
            Sector.IntelConfidence))
    {
        return false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_SECTOR_RECON_UPDATED sector=%s "
            "intel=%.0f->%.0f"
        ),
        *Sector.SectorID.ToString(),
        PreviousConfidence,
        Sector.IntelConfidence
    );

    BroadcastWarState();
    return true;
}

bool UBHWarSubsystem::ReportEnemyDetained(
    FName SectorID,
    float IntelligenceGain,
    float EnemyStrengthLoss
)
{
    if (!CanMutateWarState())
    {
        return false;
    }
    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return false;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousIntel = Sector.IntelConfidence;
    const float PreviousEnemyStrength = Sector.EnemyStrength;
    Sector.IntelConfidence = FMath::Clamp(
        Sector.IntelConfidence + FMath::Max(0.0f, IntelligenceGain),
        0.0f,
        MaximumIntelConfidence
    );
    Sector.EnemyStrength = FMath::Max(
        0.0f,
        Sector.EnemyStrength - FMath::Max(0.0f, EnemyStrengthLoss)
    );
    ClampSectorState(Sector);
    RecordWarEvent(
        TEXT("EnemyDetained"),
        SectorID,
        FString::Printf(
            TEXT("Resistance forces detained an enemy combatant at %s"),
            *Sector.DisplayName.ToString())
    );
    RecalculatePriority();
    BroadcastWarState();
    UE_LOG(LogTemp, Display, TEXT(
        "BH_ENEMY_DETENTION_REPORTED sector=%s intel=%.1f->%.1f enemy=%.1f->%.1f"),
        *SectorID.ToString(),
        PreviousIntel,
        Sector.IntelConfidence,
        PreviousEnemyStrength,
        Sector.EnemyStrength);
    return true;
}

bool UBHWarSubsystem::ReportSurrenderedEnemyKilled(
    FName SectorID,
    float CivilianSupportLoss,
    float EnemyResponseGain,
    int32 CampaignMeritLoss
)
{
    if (!CanMutateWarState())
    {
        return false;
    }
    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return false;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousSupport = Sector.CivilianSupport;
    const float PreviousResponse = Sector.EnemyResponsePressure;
    const int32 PreviousMerit = CampaignProgression.CampaignMerit;
    Sector.CivilianSupport -= FMath::Max(0.0f, CivilianSupportLoss);
    Sector.EnemyResponsePressure += FMath::Max(0.0f, EnemyResponseGain);
    CampaignProgression.CampaignMerit = FMath::Max(
        0,
        CampaignProgression.CampaignMerit - FMath::Max(0, CampaignMeritLoss)
    );
    ClampSectorState(Sector);
    RecordWarEvent(
        TEXT("SurrenderViolation"),
        SectorID,
        FString::Printf(
            TEXT("Civilian trust fell after a surrendered combatant was killed at %s"),
            *Sector.DisplayName.ToString())
    );
    RecalculatePriority();
    BroadcastWarState();
    UE_LOG(LogTemp, Warning, TEXT(
        "BH_SURRENDER_VIOLATION sector=%s support=%.1f->%.1f response=%.1f->%.1f merit=%d->%d"),
        *SectorID.ToString(),
        PreviousSupport,
        Sector.CivilianSupport,
        PreviousResponse,
        Sector.EnemyResponsePressure,
        PreviousMerit,
        CampaignProgression.CampaignMerit);
    return true;
}

bool UBHWarSubsystem::ReportSurrenderedEnemyEscaped(
    FName SectorID,
    float EnemyResponseGain,
    float IntelligenceLoss
)
{
    if (!CanMutateWarState())
    {
        return false;
    }
    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return false;
    }
    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousResponse = Sector.EnemyResponsePressure;
    const float PreviousIntel = Sector.IntelConfidence;
    Sector.EnemyResponsePressure += FMath::Max(0.0f, EnemyResponseGain);
    Sector.IntelConfidence -= FMath::Max(0.0f, IntelligenceLoss);
    ClampSectorState(Sector);
    RecordWarEvent(
        TEXT("SurrenderEscape"),
        SectorID,
        FString::Printf(
            TEXT("An unsecured enemy combatant escaped custody at %s"),
            *Sector.DisplayName.ToString())
    );
    RecalculatePriority();
    BroadcastWarState();
    UE_LOG(LogTemp, Warning, TEXT(
        "BH_SURRENDER_ESCAPE sector=%s response=%.1f->%.1f intel=%.1f->%.1f"),
        *SectorID.ToString(), PreviousResponse, Sector.EnemyResponsePressure,
        PreviousIntel, Sector.IntelConfidence);
    return true;
}

bool UBHWarSubsystem::ReportFriendlyFireEscalation(
    FName SectorID,
    float CivilianSupportLoss,
    float EnemyResponseGain,
    int32 CampaignMeritLoss
)
{
    if (!CanMutateWarState())
    {
        return false;
    }
    const int32 SectorIndex = FindSectorIndex(SectorID);
    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return false;
    }
    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousSupport = Sector.CivilianSupport;
    const float PreviousResponse = Sector.EnemyResponsePressure;
    const int32 PreviousMerit = CampaignProgression.CampaignMerit;
    Sector.CivilianSupport -= FMath::Max(0.0f, CivilianSupportLoss);
    Sector.EnemyResponsePressure += FMath::Max(0.0f, EnemyResponseGain);
    CampaignProgression.CampaignMerit = FMath::Max(
        0,
        CampaignProgression.CampaignMerit - FMath::Max(0, CampaignMeritLoss));
    ClampSectorState(Sector);
    RecordWarEvent(
        TEXT("FriendlyFireEscalation"),
        SectorID,
        FString::Printf(
            TEXT("Repeated friendly fire damaged resistance confidence at %s"),
            *Sector.DisplayName.ToString())
    );
    RecalculatePriority();
    BroadcastWarState();
    UE_LOG(LogTemp, Warning, TEXT(
        "BH_FRIENDLY_FIRE_ESCALATION sector=%s support=%.1f->%.1f response=%.1f->%.1f merit=%d->%d"),
        *SectorID.ToString(), PreviousSupport, Sector.CivilianSupport,
        PreviousResponse, Sector.EnemyResponsePressure,
        PreviousMerit, CampaignProgression.CampaignMerit);
    return true;
}

bool UBHWarSubsystem::ReportCivilianSecurityOutcome(
    FName SectorID,
    bool bFriendlySucceeded,
    float SupportShift
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    const float SafeShift = FMath::Max(0.0f, SupportShift);

    if (!SectorStates.IsValidIndex(SectorIndex) ||
        SafeShift <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const float PreviousSupport = Sector.CivilianSupport;
    Sector.CivilianSupport +=
        bFriendlySucceeded ? SafeShift : -SafeShift;
    ClampSectorState(Sector);

    if (bFriendlySucceeded)
    {
        Sector.IntelConfidence = FMath::Max(
            Sector.IntelConfidence,
            Sector.CivilianSupport * CivilianIntelContribution
        );
    }

    if (FMath::IsNearlyEqual(
        PreviousSupport,
        Sector.CivilianSupport
    ))
    {
        return false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CIVILIAN_SECURITY_OUTCOME sector=%s "
            "friendly_success=%d support=%.0f->%.0f"
        ),
        *Sector.SectorID.ToString(),
        bFriendlySucceeded ? 1 : 0,
        PreviousSupport,
        Sector.CivilianSupport
    );

    BroadcastWarState();
    return true;
}

bool UBHWarSubsystem::CanDeliverCivilianAid(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
) const
{
    const int32 TargetIndex = FindSectorIndex(TargetSectorID);
    const TArray<int32> RouteIndices =
        BuildOperationSupplyRouteIndices(
            TargetSectorID,
            OperationType
        );
    const int32 SourceIndex = RouteIndices.IsEmpty()
        ? INDEX_NONE
        : RouteIndices[0];

    if (CampaignOutcome != EBHWarCampaignOutcome::Ongoing ||
        !SectorStates.IsValidIndex(TargetIndex) ||
        !SectorStates.IsValidIndex(SourceIndex) ||
        OperationType == EBHWarPriorityType::None)
    {
        return false;
    }

    const FBHWarSectorState& Target = SectorStates[TargetIndex];
    const FBHWarSectorState& Source = SectorStates[SourceIndex];
    const bool bAidCanImproveNetwork =
        Target.CivilianSupport <
            MaximumCivilianSupport - KINDA_SMALL_NUMBER ||
        Target.IntelConfidence <
            MaximumIntelConfidence - KINDA_SMALL_NUMBER ||
        (Target.Owner == EBHWarFaction::Friendly &&
            Target.EnemyResponsePressure > KINDA_SMALL_NUMBER);
    const bool bAidAlreadyInTransit =
        SupplyConvoys.ContainsByPredicate(
            [TargetSectorID](
                const FBHWarSupplyConvoyState& Convoy)
            {
                return Convoy.CargoType ==
                        EBHWarConvoyCargoType::CivilianAid &&
                    Convoy.DestinationSectorID == TargetSectorID;
            }
        );

    return !bAidAlreadyInTransit &&
        bAidCanImproveNetwork &&
        Source.Owner == EBHWarFaction::Friendly &&
        Source.Supply + KINDA_SMALL_NUMBER >=
            CivilianAidSupplyCost;
}

bool UBHWarSubsystem::DeliverCivilianAid(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    if (!CanDeliverCivilianAid(
            TargetSectorID,
            OperationType))
    {
        return false;
    }

    const int32 TargetIndex = FindSectorIndex(TargetSectorID);
    const TArray<int32> RouteIndices =
        BuildOperationSupplyRouteIndices(
            TargetSectorID,
            OperationType
        );
    const int32 SourceIndex = RouteIndices[0];
    const FBHWarSectorState& Target = SectorStates[TargetIndex];
    FBHWarSectorState& Source = SectorStates[SourceIndex];
    const float PreviousSourceSupply = Source.Supply;

    Source.Supply -= CivilianAidSupplyCost;
    ClampSectorState(Source);

    FBHWarSupplyConvoyState Convoy;
    Convoy.ConvoyID = FName(*FString::Printf(
        TEXT("AidConvoy_%d_%s_%s"),
        TurnNumber,
        *Source.SectorID.ToString(),
        *Target.SectorID.ToString()
    ));
    Convoy.SourceSectorID = Source.SectorID;
    Convoy.DestinationSectorID = Target.SectorID;
    Convoy.Owner = EBHWarFaction::Friendly;
    Convoy.CargoType =
        EBHWarConvoyCargoType::CivilianAid;
    Convoy.SupplyPayload = CivilianAidSupplyCost;
    Convoy.TurnsRemaining = 1;
    Convoy.DispatchTurn = TurnNumber;
    BHRouteOperations::Initialize(Convoy);
    SupplyConvoys.Add(Convoy);

    RecordWarEvent(
        TEXT("CivilianAidDispatched"),
        Target.SectorID,
        FString::Printf(
            TEXT(
                "Civilian aid departed %s for %s; the shipment "
                "must survive the route before its network grows"
            ),
            *Source.DisplayName.ToString(),
            *Target.DisplayName.ToString()
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CIVILIAN_AID_DISPATCHED id=%s target=%s "
            "source=%s supply=%.1f->%.1f eta_turns=%d"
        ),
        *Convoy.ConvoyID.ToString(),
        *Target.SectorID.ToString(),
        *Source.SectorID.ToString(),
        PreviousSourceSupply,
        Source.Supply,
        Convoy.TurnsRemaining
    );
    return true;
}

float UBHWarSubsystem::GetCivilianAidSupplyCost() const
{
    return CivilianAidSupplyCost;
}

float UBHWarSubsystem::GetCivilianAidSupportGain() const
{
    return CivilianAidSupportGain;
}

int32 UBHWarSubsystem::GetSectorMilitiaMobilizationCount(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return 0;
    }

    const FBHWarSectorState& Sector = SectorStates[SectorIndex];

    if (Sector.Owner != EBHWarFaction::Friendly ||
        Sector.GarrisonCapacity <= Sector.FriendlyGarrison ||
        Sector.CivilianSupport <
            MinimumMilitiaMobilizationSupport)
    {
        return 0;
    }

    const int32 SupportDrivenCount = 1 + FMath::FloorToInt(
        (Sector.CivilianSupport -
            MinimumMilitiaMobilizationSupport) /
        20.0f
    );

    return FMath::Min3(
        Sector.GarrisonCapacity - Sector.FriendlyGarrison,
        SupportDrivenCount,
        MaximumMilitiaMobilizationCount
    );
}

float UBHWarSubsystem::GetSectorMilitiaMobilizationSupplyCost(
    FName SectorID
) const
{
    return GetSectorMilitiaMobilizationCount(SectorID) *
        MilitiaSupplyCostPerUnit;
}

bool UBHWarSubsystem::CanMobilizeSectorMilitia(
    FName SectorID
) const
{
    const int32 SectorIndex = FindSectorIndex(SectorID);

    if (CampaignOutcome != EBHWarCampaignOutcome::Ongoing ||
        !SectorStates.IsValidIndex(SectorIndex) ||
        IsCommittedOperationSector(SectorID))
    {
        return false;
    }

    const FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const int32 MobilizationCount =
        GetSectorMilitiaMobilizationCount(SectorID);
    const float SupplyCost =
        MobilizationCount * MilitiaSupplyCostPerUnit;

    return MobilizationCount > 0 &&
        Sector.Owner == EBHWarFaction::Friendly &&
        IsSectorConnectedToFactionLogistics(SectorID) &&
        Sector.Supply + KINDA_SMALL_NUMBER >= SupplyCost;
}

bool UBHWarSubsystem::MobilizeSectorMilitia(FName SectorID)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    if (!CanMobilizeSectorMilitia(SectorID))
    {
        return false;
    }

    const int32 SectorIndex = FindSectorIndex(SectorID);
    FBHWarSectorState& Sector = SectorStates[SectorIndex];
    const int32 MobilizationCount =
        GetSectorMilitiaMobilizationCount(SectorID);
    const float SupplyCost =
        MobilizationCount * MilitiaSupplyCostPerUnit;
    const float SupportCost =
        MobilizationCount * MilitiaSupportCostPerUnit;
    const int32 PreviousGarrison = Sector.FriendlyGarrison;
    const float PreviousSupply = Sector.Supply;
    const float PreviousSupport = Sector.CivilianSupport;
    const float PreviousResponsePressure =
        Sector.EnemyResponsePressure;

    Sector.FriendlyGarrison += MobilizationCount;
    Sector.FriendlyStrength +=
        MobilizationCount * MilitiaStrengthPerUnit;
    Sector.Supply -= SupplyCost;
    Sector.CivilianSupport -= SupportCost;
    Sector.EnemyResponsePressure +=
        MobilizationCount * MilitiaResponsePressurePerUnit;
    ClampSectorState(Sector);
    RecordEnemyResponseEscalation(
        Sector,
        PreviousResponsePressure
    );

    RecordWarEvent(
        TEXT("MilitiaMobilized"),
        Sector.SectorID,
        FString::Printf(
            TEXT(
                "%s rallied %d local militia; enemy attention rose to %.0f%%"
            ),
            *Sector.DisplayName.ToString(),
            MobilizationCount,
            Sector.EnemyResponsePressure
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_MILITIA_MOBILIZED sector=%s troops=%d "
            "garrison=%d->%d supply=%.1f->%.1f "
            "support=%.1f->%.1f response=%.1f->%.1f"
        ),
        *Sector.SectorID.ToString(),
        MobilizationCount,
        PreviousGarrison,
        Sector.FriendlyGarrison,
        PreviousSupply,
        Sector.Supply,
        PreviousSupport,
        Sector.CivilianSupport,
        PreviousResponsePressure,
        Sector.EnemyResponsePressure
    );
    return true;
}

FName UBHWarSubsystem::GetSectorGarrisonRedeploymentSource(
    FName DestinationSectorID
) const
{
    const int32 SourceIndex =
        FindGarrisonRedeploymentSourceIndex(
            DestinationSectorID
        );

    return SectorStates.IsValidIndex(SourceIndex)
        ? SectorStates[SourceIndex].SectorID
        : NAME_None;
}

int32 UBHWarSubsystem::GetSectorGarrisonRedeploymentCount(
    FName DestinationSectorID
) const
{
    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);
    const int32 SourceIndex =
        FindGarrisonRedeploymentSourceIndex(
            DestinationSectorID
        );

    if (!SectorStates.IsValidIndex(DestinationIndex) ||
        !SectorStates.IsValidIndex(SourceIndex))
    {
        return 0;
    }

    const FBHWarSectorState& Destination =
        SectorStates[DestinationIndex];
    const FBHWarSectorState& Source =
        SectorStates[SourceIndex];
    const int32 IncomingCount =
        GetIncomingGarrisonTransferCount(
            DestinationSectorID
        );

    return FMath::Min3(
        MaximumGarrisonRedeploymentCount,
        FMath::Max(
            0,
            Source.FriendlyGarrison -
                MinimumRedeploymentSourceGarrison
        ),
        FMath::Max(
            0,
            Destination.GarrisonCapacity -
                Destination.FriendlyGarrison -
                IncomingCount
        )
    );
}

float UBHWarSubsystem::GetSectorGarrisonRedeploymentSupplyCost(
    FName DestinationSectorID
) const
{
    const FName SourceSectorID =
        GetSectorGarrisonRedeploymentSource(
            DestinationSectorID
        );
    const int32 RouteDistance = GetFriendlyRouteDistance(
        SourceSectorID,
        DestinationSectorID
    );

    return GetSectorGarrisonRedeploymentCount(
        DestinationSectorID
    ) *
        GarrisonRedeploymentSupplyCostPerUnit *
        FMath::Max(1, RouteDistance);
}

int32 UBHWarSubsystem::GetSectorGarrisonRedeploymentTurns(
    FName DestinationSectorID
) const
{
    return FMath::Max(
        0,
        GetFriendlyRouteDistance(
            GetSectorGarrisonRedeploymentSource(
                DestinationSectorID
            ),
            DestinationSectorID
        )
    );
}

bool UBHWarSubsystem::CanRedeploySectorGarrison(
    FName DestinationSectorID
) const
{
    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);
    const int32 SourceIndex =
        FindGarrisonRedeploymentSourceIndex(
            DestinationSectorID
        );

    if (CampaignOutcome != EBHWarCampaignOutcome::Ongoing ||
        !SectorStates.IsValidIndex(DestinationIndex) ||
        !SectorStates.IsValidIndex(SourceIndex) ||
        IsCommittedOperationSector(DestinationSectorID))
    {
        return false;
    }

    const FBHWarSectorState& Destination =
        SectorStates[DestinationIndex];
    const FBHWarSectorState& Source =
        SectorStates[SourceIndex];
    const int32 RedeploymentCount =
        GetSectorGarrisonRedeploymentCount(
            DestinationSectorID
        );
    const float SupplyCost =
        GetSectorGarrisonRedeploymentSupplyCost(
            DestinationSectorID
        );

    return RedeploymentCount > 0 &&
        Destination.Owner == EBHWarFaction::Friendly &&
        Source.Owner == EBHWarFaction::Friendly &&
        !IsCommittedOperationSector(Source.SectorID) &&
        Source.Supply + KINDA_SMALL_NUMBER >= SupplyCost;
}

bool UBHWarSubsystem::RedeploySectorGarrison(
    FName DestinationSectorID
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    if (!CanRedeploySectorGarrison(DestinationSectorID))
    {
        return false;
    }

    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);
    const int32 SourceIndex =
        FindGarrisonRedeploymentSourceIndex(
            DestinationSectorID
        );

    if (!SectorStates.IsValidIndex(DestinationIndex) ||
        !SectorStates.IsValidIndex(SourceIndex))
    {
        return false;
    }

    FBHWarSectorState& Destination =
        SectorStates[DestinationIndex];
    FBHWarSectorState& Source = SectorStates[SourceIndex];
    const int32 RedeploymentCount =
        GetSectorGarrisonRedeploymentCount(
            DestinationSectorID
        );
    const int32 RouteDistance = GetFriendlyRouteDistance(
        Source.SectorID,
        Destination.SectorID
    );
    const float SupplyCost =
        GetSectorGarrisonRedeploymentSupplyCost(
            DestinationSectorID
        );
    const int32 PreviousSourceGarrison =
        Source.FriendlyGarrison;
    const int32 PreviousDestinationGarrison =
        Destination.FriendlyGarrison;
    const float PreviousSourceSupply = Source.Supply;

    Source.FriendlyGarrison -= RedeploymentCount;
    Source.Supply -= SupplyCost;
    ClampSectorState(Source);

    FBHWarGarrisonTransferState Transfer;
    Transfer.TransferID = FName(
        *FString::Printf(
            TEXT("Transfer_%d_%d_%s_%s"),
            TurnNumber,
            GarrisonTransfers.Num(),
            *Source.SectorID.ToString(),
            *Destination.SectorID.ToString()
        )
    );
    Transfer.SourceSectorID = Source.SectorID;
    Transfer.DestinationSectorID = Destination.SectorID;
    Transfer.TroopCount = RedeploymentCount;
    Transfer.TurnsRemaining = FMath::Max(1, RouteDistance);
    Transfer.DispatchTurn = TurnNumber;
    GarrisonTransfers.Add(Transfer);

    RecordWarEvent(
        TEXT("GarrisonTransferDispatched"),
        Destination.SectorID,
        FString::Printf(
            TEXT(
                "%d troops departed %s for %s; ETA %d turn%s"
            ),
            RedeploymentCount,
            *Source.DisplayName.ToString(),
            *Destination.DisplayName.ToString(),
            Transfer.TurnsRemaining,
            Transfer.TurnsRemaining == 1 ? TEXT("") : TEXT("s")
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_GARRISON_REDEPLOYED source=%s destination=%s "
            "troops=%d source_garrison=%d->%d "
            "destination_garrison=%d incoming=%d "
            "eta_turns=%d supply=%.1f->%.1f"
        ),
        *Source.SectorID.ToString(),
        *Destination.SectorID.ToString(),
        RedeploymentCount,
        PreviousSourceGarrison,
        Source.FriendlyGarrison,
        PreviousDestinationGarrison,
        RedeploymentCount,
        Transfer.TurnsRemaining,
        PreviousSourceSupply,
        Source.Supply
    );
    return true;
}

int32 UBHWarSubsystem::GetIncomingGarrisonTransferCount(
    FName DestinationSectorID
) const
{
    int32 IncomingCount = 0;

    for (const FBHWarGarrisonTransferState& Transfer :
        GarrisonTransfers)
    {
        if (Transfer.DestinationSectorID ==
            DestinationSectorID)
        {
            IncomingCount += FMath::Max(
                0,
                Transfer.TroopCount
            );
        }
    }

    return IncomingCount;
}

int32 UBHWarSubsystem::GetIncomingGarrisonTransferTurns(
    FName DestinationSectorID
) const
{
    int32 MinimumTurns = MAX_int32;

    for (const FBHWarGarrisonTransferState& Transfer :
        GarrisonTransfers)
    {
        if (Transfer.DestinationSectorID ==
            DestinationSectorID)
        {
            MinimumTurns = FMath::Min(
                MinimumTurns,
                Transfer.TurnsRemaining
            );
        }
    }

    return MinimumTurns == MAX_int32
        ? 0
        : FMath::Max(0, MinimumTurns);
}

int32 UBHWarSubsystem::GetTurnNumber() const
{
    return TurnNumber;
}

FName UBHWarSubsystem::GetPrioritySectorID() const
{
    return PrioritySectorID;
}

EBHWarPriorityType UBHWarSubsystem::GetPriorityType() const
{
    return PriorityType;
}

FText UBHWarSubsystem::GetPriorityText() const
{
    const FBHWarSectorState Sector =
        GetSectorState(PrioritySectorID);

    if (Sector.SectorID.IsNone() ||
        PriorityType == EBHWarPriorityType::None)
    {
        return FText::FromString(TEXT("No active frontline priority"));
    }

    return FText::Format(
        PriorityType == EBHWarPriorityType::Defend
            ? FText::FromString(TEXT("Defend {0}"))
            : PriorityType == EBHWarPriorityType::Raid
            ? FText::FromString(TEXT("Raid {0}"))
            : PriorityType == EBHWarPriorityType::Resupply
            ? FText::FromString(TEXT("Resupply {0}"))
            : PriorityType == EBHWarPriorityType::EscortRescue
            ? FText::FromString(TEXT("Escort convoy to {0}"))
            : PriorityType == EBHWarPriorityType::Rescue
            ? FText::FromString(TEXT("Evacuate casualty to {0}"))
            : PriorityType == EBHWarPriorityType::Recon
            ? FText::FromString(TEXT("Reconnoiter {0}"))
            : FText::FromString(TEXT("Attack {0}")),
        Sector.DisplayName
    );
}

FText UBHWarSubsystem::GetPriorityReasonText() const
{
    return PriorityReasonText;
}

FText UBHWarSubsystem::GetPriorityOperationTitle() const
{
    return GetOperationTitle(
        PrioritySectorID,
        PriorityType
    );
}

FText UBHWarSubsystem::GetOperationTitle(
    FName SectorID,
    EBHWarPriorityType OperationType
) const
{
    const FBHWarSectorState Sector =
        GetSectorState(SectorID);

    if (Sector.SectorID.IsNone() ||
        OperationType == EBHWarPriorityType::None)
    {
        return FText::FromString(TEXT("NO ACTIVE OPERATION"));
    }

    const bool bCounterinsurgencyDefense =
        OperationType == EBHWarPriorityType::Defend &&
        Sector.EnemyResponsePressure >=
            CounterinsurgencyResponseThreshold;

    return FText::Format(
        bCounterinsurgencyDefense
            ? NSLOCTEXT(
                "BrokenHorizon",
                "CounterinsurgencyDefenseOperationTitle",
                "OPERATION SAFEHOUSE // {0}"
            )
            : OperationType == EBHWarPriorityType::Defend
            ? NSLOCTEXT(
                "BrokenHorizon",
                "DefenseOperationTitle",
                "OPERATION HOLDFAST // {0}"
            )
            : OperationType == EBHWarPriorityType::Raid
            ? NSLOCTEXT(
                "BrokenHorizon",
                "RaidOperationTitle",
                "OPERATION BLACKOUT // {0}"
            )
            : OperationType == EBHWarPriorityType::Resupply
            ? NSLOCTEXT(
                "BrokenHorizon",
                "ResupplyOperationTitle",
                "OPERATION LIFELINE // {0}"
            )
            : OperationType == EBHWarPriorityType::EscortRescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "EscortOperationTitle",
                "OPERATION SHEPHERD // {0}"
            )
            : OperationType == EBHWarPriorityType::Rescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "RescueOperationTitle",
                "OPERATION GUARDIAN // {0}"
            )
            : OperationType == EBHWarPriorityType::Recon
            ? NSLOCTEXT(
                "BrokenHorizon",
                "ReconOperationTitle",
                "OPERATION WATCHTOWER // {0}"
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "AttackOperationTitle",
                "OPERATION BREAKTHROUGH // {0}"
            ),
        Sector.DisplayName
    );
}

FText UBHWarSubsystem::GetPriorityMissionBriefing() const
{
    return GetOperationMissionBriefing(
        PrioritySectorID,
        PriorityType
    );
}

FText UBHWarSubsystem::GetOperationMissionBriefing(
    FName SectorID,
    EBHWarPriorityType OperationType
) const
{
    const FBHWarSectorState Sector =
        GetSectorState(SectorID);

    if (Sector.SectorID.IsNone() ||
        OperationType == EBHWarPriorityType::None)
    {
        return FText::FromString(
            TEXT("Command has not assigned a frontline operation.")
        );
    }

    const bool bCounterinsurgencyDefense =
        OperationType == EBHWarPriorityType::Defend &&
        Sector.EnemyResponsePressure >=
            CounterinsurgencyResponseThreshold;

    return FText::Format(
        bCounterinsurgencyDefense
            ? NSLOCTEXT(
                "BrokenHorizon",
                "CounterinsurgencyDefenseMissionBriefing",
                "Enemy security forces are sweeping {0} for militia "
                "cells and informants. Reach the resistance "
                "stronghold, hold the defensive line, and break "
                "the sweep before the local network is dismantled."
            )
            : OperationType == EBHWarPriorityType::Defend
            ? NSLOCTEXT(
                "BrokenHorizon",
                "DefenseMissionBriefing",
                "Enemy forces are assaulting {0}. Recover local "
                "access, reach the defensive line, and repel the "
                "attack before the sector falls."
            )
            : OperationType == EBHWarPriorityType::Raid
            ? NSLOCTEXT(
                "BrokenHorizon",
                "RaidMissionBriefing",
                "Enemy logistics are sustaining operations around "
                "{0}. Infiltrate the sector and sabotage the supply "
                "cache without committing to an occupation. Avoid "
                "unnecessary casualties: a clean raid earns local "
                "support and reduces the enemy response; a loud "
                "raid triggers the opposite."
            )
            : OperationType == EBHWarPriorityType::Resupply
            ? NSLOCTEXT(
                "BrokenHorizon",
                "ResupplyMissionBriefing",
                "The friendly position at {0} is running short. "
                "Take a field transport to the assigned staging "
                "sector, load military supply with X, then deliver "
                "the full mission load at {0}. Expect the route to "
                "remain exposed until the delivery is complete."
            )
            : OperationType == EBHWarPriorityType::EscortRescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "EscortMissionBriefing",
                "A friendly convoy bound for {0} is exposed on "
                "the route. Locate the marked convoy, suppress "
                "the ambush force, and keep the vehicle intact "
                "until it clears the local route."
            )
            : OperationType == EBHWarPriorityType::Rescue
            ? NSLOCTEXT(
                "BrokenHorizon",
                "RescueMissionBriefing",
                "A field operative requires urgent evacuation. "
                "Stabilize the casualty if necessary, move them "
                "by foot or vehicle, and deliver them to the "
                "friendly treatment point at {0}. The casualty "
                "cannot return to combat until treated."
            )
            : OperationType == EBHWarPriorityType::Recon
            ? NSLOCTEXT(
                "BrokenHorizon",
                "ReconMissionBriefing",
                "Enemy activity around {0} is unconfirmed. Enter the "
                "sector, vary your observation position, and remain in "
                "the field long enough to file a confirmed intelligence "
                "report. Avoid major contact where possible."
            )
            : NSLOCTEXT(
                "BrokenHorizon",
                "AttackMissionBriefing",
                "Enemy forces control {0}. Infiltrate the sector, "
                "breach its security perimeter, eliminate the "
                "garrison, and secure the objective."
            ),
        Sector.DisplayName
    );
}

FText UBHWarSubsystem::GetPriorityObjectiveText(
    FName ObjectiveID
) const
{
    return GetOperationObjectiveText(
        PrioritySectorID,
        PriorityType,
        ObjectiveID
    );
}

FText UBHWarSubsystem::GetOperationObjectiveText(
    FName SectorID,
    EBHWarPriorityType OperationType,
    FName ObjectiveID
) const
{
    const FBHWarSectorState Sector =
        GetSectorState(SectorID);

    if (Sector.SectorID.IsNone() ||
        OperationType == EBHWarPriorityType::None ||
        ObjectiveID.IsNone())
    {
        return FText::GetEmpty();
    }

    const bool bDefense =
        OperationType == EBHWarPriorityType::Defend;
    const bool bRaid =
        OperationType == EBHWarPriorityType::Raid;

    if (ObjectiveID == BHObjectiveIds::DeliverResupply &&
        OperationType == EBHWarPriorityType::Resupply)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "DeliverResupplyObjective",
                "Deliver a full military supply load to {0}"
            ),
            Sector.DisplayName
        );
    }

    if (ObjectiveID == BHObjectiveIds::ProtectConvoy &&
        OperationType == EBHWarPriorityType::EscortRescue)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "ProtectConvoyObjective",
                "Protect the convoy until it clears the route to {0}"
            ),
            Sector.DisplayName
        );
    }

    if (ObjectiveID == BHObjectiveIds::EvacuateCasualty &&
        OperationType == EBHWarPriorityType::Rescue)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "EvacuateCasualtyObjective",
                "Evacuate the assigned casualty to {0}"
            ),
            Sector.DisplayName
        );
    }

    if (ObjectiveID == BHObjectiveIds::ObserveSector &&
        OperationType == EBHWarPriorityType::Recon)
    {
        return FText::Format(
            NSLOCTEXT(
                "BrokenHorizon",
                "ObserveSectorObjective",
                "Move through {0} and file a confirmed intelligence report"
            ),
            Sector.DisplayName
        );
    }

    if (ObjectiveID == BHObjectiveIds::FindRedKeycard)
    {
        return FText::Format(
            bDefense
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "DefenseFindKeycardObjective",
                    "Recover the emergency access card at {0}"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "AttackFindKeycardObjective",
                    "Locate the access card for {0}"
                ),
            Sector.DisplayName
        );
    }

    if (ObjectiveID == BHObjectiveIds::UnlockSecurityDoor)
    {
        return FText::Format(
            bDefense
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "DefenseUnlockDoorObjective",
                    "Open the defensive route into {0}"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "AttackUnlockDoorObjective",
                    "Breach the security perimeter at {0}"
                ),
            Sector.DisplayName
        );
    }

    if (ObjectiveID == BHObjectiveIds::ExploreBeyondSecurityDoor)
    {
        return FText::Format(
            bDefense
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "DefenseExploreObjective",
                    "Move to the defensive line at {0}"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "AttackExploreObjective",
                    "Advance into {0}"
                ),
            Sector.DisplayName
        );
    }

    if (ObjectiveID == BHObjectiveIds::EliminateGuard)
    {
        return FText::Format(
            bDefense
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "DefenseEliminateObjective",
                    "Repel the enemy assault on {0}"
                )
                : bRaid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "RaidEliminateObjective",
                    "Disrupt enemy logistics in {0}"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "AttackEliminateObjective",
                    "Neutralize hostile forces in {0}"
                ),
            Sector.DisplayName
        );
    }

    if (ObjectiveID == BHObjectiveIds::ReachExtraction)
    {
        return FText::Format(
            bDefense
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "DefenseExtractionObjective",
                    "Hold {0} and report to the rally point"
                )
                : bRaid
                ? NSLOCTEXT(
                    "BrokenHorizon",
                    "RaidExtractionObjective",
                    "Break contact and report the raid on {0}"
                )
                : NSLOCTEXT(
                    "BrokenHorizon",
                    "AttackExtractionObjective",
                    "Secure {0} and reach the rally point"
                ),
            Sector.DisplayName
        );
    }

    return FText::GetEmpty();
}

float UBHWarSubsystem::GetFriendlyControlPercentage() const
{
    if (SectorStates.IsEmpty())
    {
        return 0.0f;
    }

    int32 FriendlySectors = 0;

    for (const FBHWarSectorState& Sector : SectorStates)
    {
        FriendlySectors +=
            Sector.Owner == EBHWarFaction::Friendly ? 1 : 0;
    }

    return 100.0f *
        static_cast<float>(FriendlySectors) /
        static_cast<float>(SectorStates.Num());
}

EBHWarCampaignOutcome UBHWarSubsystem::GetCampaignOutcome() const
{
    return CampaignOutcome;
}

bool UBHWarSubsystem::IsCampaignResolved() const
{
    return CampaignOutcome != EBHWarCampaignOutcome::Ongoing;
}

FText UBHWarSubsystem::GetCampaignOutcomeText() const
{
    if (CampaignOutcome ==
        EBHWarCampaignOutcome::FriendlyVictory)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "FriendlyCampaignVictory",
            "CAMPAIGN VICTORY // KORONA LIBERATED"
        );
    }

    if (CampaignOutcome ==
        EBHWarCampaignOutcome::EnemyVictory)
    {
        return NSLOCTEXT(
            "BrokenHorizon",
            "EnemyCampaignVictory",
            "CAMPAIGN DEFEAT // WESTERN COMMAND LOST"
        );
    }

    return NSLOCTEXT(
        "BrokenHorizon",
        "CampaignOngoing",
        "CAMPAIGN ACTIVE"
    );
}

float UBHWarSubsystem::GetSimulationAccumulator() const
{
    return SimulationAccumulator;
}

float UBHWarSubsystem::GetCurrentSimulationIntervalSeconds() const
{
    const float PressureMultiplier = FMath::Clamp(
        CampaignDifficulty.StrategicPressureMultiplier,
        0.5f,
        2.0f
    );
    return FMath::Max(
        1.0f,
        (HasCommittedOperation()
            ? CommittedOperationSimulationIntervalSeconds
            : SimulationIntervalSeconds) / PressureMultiplier
    );
}

float UBHWarSubsystem::GetSecondsUntilNextWarTurn() const
{
    return FMath::Max(
        0.0f,
        GetCurrentSimulationIntervalSeconds() -
            SimulationAccumulator
    );
}

bool UBHWarSubsystem::RestoreWarState(
    const TArray<FBHWarSectorState>& SavedSectorStates,
    const TArray<FBHWarSupplyConvoyState>&
        SavedSupplyConvoys,
    const TArray<FBHWarEventRecord>& SavedWarEvents,
    int32 SavedTurnNumber,
    float SavedSimulationAccumulator,
    int32 SavedSchemaVersion
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    if (!ValidateSectorGraph(SavedSectorStates))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Saved persistent-war sector graph is invalid.")
        );
        return false;
    }

    const TArray<FBHWarSectorState> PreviousSectorStates =
        SectorStates;
    BuildDefaultCampaign();

    for (const FBHWarSectorState& SavedSector :
        SavedSectorStates)
    {
        const int32 SectorIndex =
            FindSectorIndex(SavedSector.SectorID);

        if (!SectorStates.IsValidIndex(SectorIndex))
        {
            SectorStates = PreviousSectorStates;

            UE_LOG(
                LogTemp,
                Error,
                TEXT(
                    "Saved persistent-war sector %s is not "
                    "part of the current campaign."
                ),
                *SavedSector.SectorID.ToString()
            );
            return false;
        }

        FBHWarSectorState& CurrentSector =
            SectorStates[SectorIndex];
        const EBHWarFaction DefaultOwner =
            CurrentSector.Owner;
        CurrentSector.Owner = SavedSector.Owner;

        if (SavedSector.GarrisonCapacity > 0)
        {
            CurrentSector.SiteType = SavedSector.SiteType;
            CurrentSector.GarrisonCapacity =
                SavedSector.GarrisonCapacity;
            CurrentSector.FriendlyGarrison =
                SavedSector.FriendlyGarrison;
            CurrentSector.EnemyGarrison =
                SavedSector.EnemyGarrison;
        }
        else
        {
            if (SavedSector.Owner != DefaultOwner)
            {
                CurrentSector.FriendlyGarrison = 0;
                CurrentSector.EnemyGarrison = 0;

                if (SavedSector.Owner ==
                    EBHWarFaction::Friendly)
                {
                    CurrentSector.FriendlyGarrison =
                        MinimumOccupationGarrison;
                }
                else if (SavedSector.Owner ==
                    EBHWarFaction::Enemy)
                {
                    CurrentSector.EnemyGarrison =
                        MinimumOccupationGarrison;
                }
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_GARRISON_SAVE_MIGRATED sector=%s "
                    "friendly=%d enemy=%d capacity=%d"
                ),
                *CurrentSector.SectorID.ToString(),
                CurrentSector.FriendlyGarrison,
                CurrentSector.EnemyGarrison,
                CurrentSector.GarrisonCapacity
            );
        }

        if (SavedSchemaVersion >=
            SectorIntelSaveSchemaVersion)
        {
            CurrentSector.IntelConfidence =
                SavedSector.IntelConfidence;
        }
        else
        {
            if (SavedSector.Owner ==
                EBHWarFaction::Friendly)
            {
                CurrentSector.IntelConfidence =
                    FriendlySectorIntelFloor;
            }
            else if (SavedSector.Owner != DefaultOwner)
            {
                CurrentSector.IntelConfidence =
                    EstimatedIntelThreshold;
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_INTEL_SAVE_MIGRATED sector=%s "
                    "intel=%.0f schema=%d"
                ),
                *CurrentSector.SectorID.ToString(),
                CurrentSector.IntelConfidence,
                SavedSchemaVersion
            );
        }

        if (SavedSchemaVersion >=
            CivilianSupportSaveSchemaVersion)
        {
            CurrentSector.CivilianSupport =
                SavedSector.CivilianSupport;
        }
        else
        {
            UE_LOG(
                LogTemp,
                Display,
                TEXT(
                    "BH_CIVILIAN_SUPPORT_SAVE_MIGRATED "
                    "sector=%s support=%.0f schema=%d"
                ),
                *CurrentSector.SectorID.ToString(),
                CurrentSector.CivilianSupport,
                SavedSchemaVersion
            );
        }

        CurrentSector.FriendlyStrength =
            SavedSector.FriendlyStrength;
        CurrentSector.EnemyStrength =
            SavedSector.EnemyStrength;
        CurrentSector.Supply = SavedSector.Supply;
        CurrentSector.ReinforcementRate =
            SavedSector.ReinforcementRate;
        CurrentSector.LastBattleTurn =
            SavedSector.LastBattleTurn;
        ClampSectorState(CurrentSector);
    }

    SupplyConvoys.Reset();

    for (const FBHWarSupplyConvoyState& SavedConvoy :
        SavedSupplyConvoys)
    {
        FBHWarSupplyConvoyState RestoredConvoy = SavedConvoy;
        if (SavedSchemaVersion < 41 ||
            !RestoredConvoy.bRouteOperationInitialized)
        {
            BHRouteOperations::Initialize(RestoredConvoy);
        }

        if (ValidateSupplyConvoy(RestoredConvoy))
        {
            SupplyConvoys.Add(RestoredConvoy);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_WAR_CONVOY_SAVE_DROPPED id=%s "
                    "from=%s to=%s"
                ),
                *SavedConvoy.ConvoyID.ToString(),
                *SavedConvoy.SourceSectorID.ToString(),
                *SavedConvoy.DestinationSectorID.ToString()
            );
        }
    }

    TurnNumber = FMath::Max(0, SavedTurnNumber);
    RecentWarEvents.Reset();

    for (const FBHWarEventRecord& SavedEvent : SavedWarEvents)
    {
        const FString SanitizedSummary =
            SavedEvent.Summary
                .TrimStartAndEnd()
                .Left(MaximumWarEventSummaryLength);

        if (SanitizedSummary.IsEmpty())
        {
            continue;
        }

        FBHWarEventRecord SanitizedEvent = SavedEvent;
        SanitizedEvent.TurnNumber =
            FMath::Max(0, SavedEvent.TurnNumber);
        SanitizedEvent.EventType =
            SavedEvent.EventType.IsNone()
                ? FName(TEXT("CampaignEvent"))
                : SavedEvent.EventType;
        SanitizedEvent.Summary = SanitizedSummary;
        RecentWarEvents.Add(MoveTemp(SanitizedEvent));
    }

    if (RecentWarEvents.Num() > MaximumWarEventHistory)
    {
        RecentWarEvents.RemoveAt(
            0,
            RecentWarEvents.Num() - MaximumWarEventHistory
        );
    }

    SimulationAccumulator = FMath::Clamp(
        SavedSimulationAccumulator,
        0.0f,
        FMath::Max(
            FMath::Max(
                SimulationIntervalSeconds,
                CommittedOperationSimulationIntervalSeconds
            ) -
                KINDA_SMALL_NUMBER,
            0.0f
        )
    );
    RecalculatePriority();
    BroadcastWarState();

    if (SavedSectorStates.Num() != SectorStates.Num())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "BH_WAR_SAVE_MIGRATED saved_sectors=%d "
                "campaign_sectors=%d"
            ),
            SavedSectorStates.Num(),
            SectorStates.Num()
        );
    }

    return true;
}

bool UBHWarSubsystem::RestoreGarrisonTransfers(
    const TArray<FBHWarGarrisonTransferState>&
        SavedGarrisonTransfers
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    GarrisonTransfers.Reset();

    for (const FBHWarGarrisonTransferState& SavedTransfer :
        SavedGarrisonTransfers)
    {
        if (ValidateGarrisonTransfer(SavedTransfer))
        {
            GarrisonTransfers.Add(SavedTransfer);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "BH_GARRISON_TRANSFER_SAVE_DROPPED id=%s "
                    "from=%s to=%s"
                ),
                *SavedTransfer.TransferID.ToString(),
                *SavedTransfer.SourceSectorID.ToString(),
                *SavedTransfer.DestinationSectorID.ToString()
            );
        }
    }

    BroadcastWarState();
    return GarrisonTransfers.Num() ==
        SavedGarrisonTransfers.Num();
}

bool UBHWarSubsystem::RestoreManpowerState(
    int32 SavedFriendlyManpowerReserve,
    int32 SavedEnemyManpowerReserve,
    float SavedFriendlyRecruitmentProgress,
    float SavedEnemyRecruitmentProgress
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    FriendlyManpowerReserve = FMath::Clamp(
        SavedFriendlyManpowerReserve,
        0,
        MaximumFactionManpowerReserve
    );
    EnemyManpowerReserve = FMath::Clamp(
        SavedEnemyManpowerReserve,
        0,
        MaximumFactionManpowerReserve
    );
    FriendlyRecruitmentProgress = FMath::Clamp(
        SavedFriendlyRecruitmentProgress,
        0.0f,
        0.999f
    );
    EnemyRecruitmentProgress = FMath::Clamp(
        SavedEnemyRecruitmentProgress,
        0.0f,
        0.999f
    );

    BroadcastWarState();
    return SavedFriendlyManpowerReserve ==
            FriendlyManpowerReserve &&
        SavedEnemyManpowerReserve ==
            EnemyManpowerReserve &&
        FMath::IsNearlyEqual(
            SavedFriendlyRecruitmentProgress,
            FriendlyRecruitmentProgress
        ) &&
        FMath::IsNearlyEqual(
            SavedEnemyRecruitmentProgress,
            EnemyRecruitmentProgress
        );
}

FName UBHWarSubsystem::GetCommittedOperationID() const
{
    return HasCommittedOperation()
        ? CommittedOperationID
        : NAME_None;
}

bool UBHWarSubsystem::RestoreCommittedOperation(
    FName SavedSectorID,
    EBHWarPriorityType SavedOperationType,
    FName SavedOperationID,
    FName SavedOperationTargetID
)
{
    if (!CanMutateWarState())
    {
        return false;
    }

    if (SavedSectorID.IsNone() ||
        SavedOperationType == EBHWarPriorityType::None)
    {
        ClearCommittedOperation();
        return true;
    }

    const bool bRestored = SetCommittedOperation(
        SavedSectorID,
        SavedOperationType
    );

    if (bRestored && !SavedOperationID.IsNone())
    {
        CommittedOperationID = SavedOperationID;
    }

    if (bRestored &&
        SavedOperationType == EBHWarPriorityType::EscortRescue &&
        !SavedOperationTargetID.IsNone())
    {
        const FBHWarSupplyConvoyState SavedTarget =
            GetSupplyConvoyState(SavedOperationTargetID);

        if (SavedTarget.Owner != EBHWarFaction::Friendly ||
            SavedTarget.DestinationSectorID != SavedSectorID)
        {
            ClearCommittedOperation();
            return false;
        }

        CommittedOperationTargetID = SavedOperationTargetID;
    }

    if (bRestored &&
        SavedOperationType == EBHWarPriorityType::Rescue)
    {
        if (SavedOperationTargetID.IsNone())
        {
            ClearCommittedOperation();
            return false;
        }

        CommittedOperationTargetID = SavedOperationTargetID;
    }

    if (bRestored)
    {
        BroadcastWarState();
    }

    if (!bRestored)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "BH_WAR_OPERATION_RESTORE_REJECTED sector=%s "
                "type=%d"
            ),
            *SavedSectorID.ToString(),
            static_cast<int32>(SavedOperationType)
        );
    }

    return bRestored;
}

void UBHWarSubsystem::RecalculatePriority()
{
    PrioritySectorID = NAME_None;
    PriorityType = EBHWarPriorityType::None;
    PriorityReasonText = NSLOCTEXT(
        "BrokenHorizon",
        "NoActiveFrontPriorityReason",
        "NO ACTIVE FRONT"
    );

    if (IsCampaignResolved())
    {
        PriorityReasonText = GetCampaignOutcomeText();
        CommittedOperationSectorID = NAME_None;
        CommittedOperationID = NAME_None;
        CommittedOperationTargetID = NAME_None;
        CommittedOperationSupplySourceSectorID = NAME_None;
        CommittedOperationEnemySourceSectorID = NAME_None;
        CommittedOperationType = EBHWarPriorityType::None;
        return;
    }

    EnsureContinuousConflict();

    if (HasCommittedOperation())
    {
        PrioritySectorID = CommittedOperationSectorID;
        PriorityType = CommittedOperationType;
        PriorityReasonText = NSLOCTEXT(
            "BrokenHorizon",
            "CommittedOperationPriorityReason",
            "OPERATION IN PROGRESS"
        );
        return;
    }

    CommittedOperationSectorID = NAME_None;
    CommittedOperationID = NAME_None;
    CommittedOperationTargetID = NAME_None;
    CommittedOperationSupplySourceSectorID = NAME_None;
    CommittedOperationEnemySourceSectorID = NAME_None;
    CommittedOperationType = EBHWarPriorityType::None;

    float BestDefenseScore = -BIG_NUMBER;
    float BestAttackScore = -BIG_NUMBER;
    float BestReconnectionAttackScore = -BIG_NUMBER;
    float HighestCounterinsurgencyPressure = -BIG_NUMBER;
    FName BestDefenseSector = NAME_None;
    FName BestAttackSector = NAME_None;
    FName BestReconnectionAttackSector = NAME_None;
    FName CounterinsurgencyDefenseSector = NAME_None;

    for (int32 SectorIndex = 0;
        SectorIndex < SectorStates.Num();
        ++SectorIndex)
    {
        const FBHWarSectorState& Sector =
            SectorStates[SectorIndex];
        float AdjacentFriendlyStrength = 0.0f;
        float AdjacentEnemyStrength = 0.0f;
        bool bTouchesFriendly = false;
        bool bTouchesEnemy = false;
        bool bTouchesConnectedFriendly = false;
        bool bTouchesIsolatedFriendly = false;

        for (const FName ConnectedSectorID :
            Sector.ConnectedSectorIDs)
        {
            const int32 ConnectedIndex =
                FindSectorIndex(ConnectedSectorID);

            if (!SectorStates.IsValidIndex(ConnectedIndex))
            {
                continue;
            }

            const FBHWarSectorState& Connected =
                SectorStates[ConnectedIndex];
            bTouchesFriendly |=
                Connected.Owner == EBHWarFaction::Friendly;
            bTouchesEnemy |=
                Connected.Owner == EBHWarFaction::Enemy;

            if (Connected.Owner == EBHWarFaction::Friendly)
            {
                const bool bConnectedToLogistics =
                    IsSectorConnectedToFactionLogistics(
                        Connected.SectorID
                    );
                bTouchesConnectedFriendly |=
                    bConnectedToLogistics;
                bTouchesIsolatedFriendly |=
                    !bConnectedToLogistics;
            }

            AdjacentFriendlyStrength +=
                GetSectorEffectiveStrength(
                    Connected.SectorID,
                    EBHWarFaction::Friendly
                );
            AdjacentEnemyStrength +=
                GetSectorEffectiveStrength(
                    Connected.SectorID,
                    EBHWarFaction::Enemy
                );
        }

        const bool bLocalEnemyThreat =
            Sector.EnemyStrength >= CaptureMinimumStrength;
        const bool bLocalFriendlyPresence =
            Sector.FriendlyStrength >= CaptureMinimumStrength;
        const float EffectiveFriendlyStrength =
            GetSectorEffectiveStrength(
                Sector.SectorID,
                EBHWarFaction::Friendly
            );
        const float EffectiveEnemyStrength =
            GetSectorEffectiveStrength(
                Sector.SectorID,
                EBHWarFaction::Enemy
            );

        const bool bCounterinsurgencyThreat =
            Sector.Owner == EBHWarFaction::Friendly &&
            Sector.EnemyResponsePressure >=
                CounterinsurgencyResponseThreshold;

        if (Sector.Owner == EBHWarFaction::Friendly &&
            (bTouchesEnemy ||
                bLocalEnemyThreat ||
                bCounterinsurgencyThreat))
        {
            if (bCounterinsurgencyThreat &&
                Sector.EnemyResponsePressure >
                    HighestCounterinsurgencyPressure)
            {
                HighestCounterinsurgencyPressure =
                    Sector.EnemyResponsePressure;
                CounterinsurgencyDefenseSector =
                    Sector.SectorID;
            }

            const float DefenseScore =
                Sector.EnemyStrength +
                (AdjacentEnemyStrength * 0.35f) -
                EffectiveFriendlyStrength +
                ((100.0f - Sector.Supply) * 0.1f);

            if (DefenseScore > BestDefenseScore)
            {
                BestDefenseScore = DefenseScore;
                BestDefenseSector = Sector.SectorID;
            }
        }
        else if (Sector.Owner == EBHWarFaction::Enemy &&
            (bTouchesFriendly || bLocalFriendlyPresence))
        {
            const bool bReconnectsIsolatedFriendlySector =
                bTouchesConnectedFriendly &&
                bTouchesIsolatedFriendly;
            const float AttackScore =
                Sector.FriendlyStrength +
                (AdjacentFriendlyStrength * 0.25f) -
                (EffectiveEnemyStrength * 0.5f) +
                ((100.0f - Sector.Supply) * 0.1f) +
                (bReconnectsIsolatedFriendlySector
                    ? ReconnectionAttackBonus
                    : 0.0f);

            if (AttackScore > BestAttackScore)
            {
                BestAttackScore = AttackScore;
                BestAttackSector = Sector.SectorID;
            }

            if (bReconnectsIsolatedFriendlySector &&
                AttackScore > BestReconnectionAttackScore)
            {
                BestReconnectionAttackScore = AttackScore;
                BestReconnectionAttackSector = Sector.SectorID;
            }
        }
    }

    if (!BestDefenseSector.IsNone() &&
        BestDefenseScore >= CriticalDefensePriorityScore)
    {
        PrioritySectorID = BestDefenseSector;
        PriorityType = EBHWarPriorityType::Defend;
        PriorityReasonText = NSLOCTEXT(
            "BrokenHorizon",
            "CriticalDefensePriorityReason",
            "BREAKTHROUGH THREAT"
        );
    }
    else if (!CounterinsurgencyDefenseSector.IsNone())
    {
        PrioritySectorID = CounterinsurgencyDefenseSector;
        PriorityType = EBHWarPriorityType::Defend;
        PriorityReasonText = NSLOCTEXT(
            "BrokenHorizon",
            "CounterinsurgencyDefensePriorityReason",
            "ENEMY SWEEP DETECTED"
        );
    }
    else if (!BestReconnectionAttackSector.IsNone())
    {
        PrioritySectorID = BestReconnectionAttackSector;
        PriorityType = EBHWarPriorityType::Attack;
        PriorityReasonText = NSLOCTEXT(
            "BrokenHorizon",
            "ReconnectionAttackPriorityReason",
            "RESTORE ISOLATED SECTORS"
        );
    }
    else if (!BestDefenseSector.IsNone() &&
        BestDefenseScore >= 15.0f)
    {
        PrioritySectorID = BestDefenseSector;
        PriorityType = EBHWarPriorityType::Defend;
        PriorityReasonText = NSLOCTEXT(
            "BrokenHorizon",
            "FrontlineDefensePriorityReason",
            "FRONTLINE UNDER PRESSURE"
        );
    }
    else if (!BestAttackSector.IsNone())
    {
        PrioritySectorID = BestAttackSector;
        PriorityType = EBHWarPriorityType::Attack;
        PriorityReasonText = NSLOCTEXT(
            "BrokenHorizon",
            "ExpansionAttackPriorityReason",
            "EXPAND FRIENDLY CONTROL"
        );
    }
    else if (!BestDefenseSector.IsNone())
    {
        PrioritySectorID = BestDefenseSector;
        PriorityType = EBHWarPriorityType::Defend;
        PriorityReasonText = NSLOCTEXT(
            "BrokenHorizon",
            "HoldFrontPriorityReason",
            "HOLD THE FRONT"
        );
    }
}

void UBHWarSubsystem::EnsureContinuousConflict()
{
    if (SectorStates.IsEmpty())
    {
        return;
    }

    bool bHasFriendlySector = false;
    bool bHasEnemySector = false;

    for (const FBHWarSectorState& Sector : SectorStates)
    {
        bHasFriendlySector |=
            Sector.Owner == EBHWarFaction::Friendly;
        bHasEnemySector |=
            Sector.Owner == EBHWarFaction::Enemy;
    }

    if (bHasFriendlySector && !bHasEnemySector)
    {
        int32 TargetIndex = INDEX_NONE;
        float BestTargetScore = -BIG_NUMBER;

        for (int32 Index = 0; Index < SectorStates.Num(); ++Index)
        {
            const FBHWarSectorState& Sector =
                SectorStates[Index];

            if (Sector.Owner != EBHWarFaction::Friendly)
            {
                continue;
            }

            const float TargetScore =
                (MaximumSectorStrength -
                    Sector.FriendlyStrength) +
                (Sector.Supply * 0.25f) +
                (Sector.ReinforcementRate * 2.0f);

            if (TargetScore > BestTargetScore)
            {
                BestTargetScore = TargetScore;
                TargetIndex = Index;
            }
        }

        if (SectorStates.IsValidIndex(TargetIndex))
        {
            FBHWarSectorState& Target =
                SectorStates[TargetIndex];
            const float PreviousEnemyStrength =
                Target.EnemyStrength;
            const float PreviousResponsePressure =
                Target.EnemyResponsePressure;
            Target.EnemyStrength = FMath::Max(
                Target.EnemyStrength,
                CounteroffensiveStrength
            );
            Target.EnemyGarrison = FMath::Max(
                Target.EnemyGarrison,
                FMath::Min(4, Target.GarrisonCapacity)
            );
            Target.Supply = FMath::Max(
                0.0f,
                Target.Supply - 10.0f
            );
            Target.LastBattleTurn = TurnNumber;
            Target.EnemyResponsePressure = FMath::Max(
                Target.EnemyResponsePressure,
                75.0f
            );
            ClampSectorState(Target);
            RecordEnemyResponseEscalation(
                Target,
                PreviousResponsePressure
            );

            if (Target.EnemyStrength >
                PreviousEnemyStrength + KINDA_SMALL_NUMBER)
            {
                RecordWarEvent(
                    TEXT("Counteroffensive"),
                    Target.SectorID,
                    FString::Printf(
                        TEXT("Enemy counteroffensive opened at %s"),
                        *Target.DisplayName.ToString()
                    )
                );
            }

            UE_LOG(
                LogTemp,
                Log,
                TEXT(
                    "Enemy counteroffensive detected in %s; "
                    "the persistent war remains active."
                ),
                *Target.SectorID.ToString()
            );
        }
    }
    else if (bHasEnemySector && !bHasFriendlySector)
    {
        int32 TargetIndex = INDEX_NONE;
        float LowestEnemyStrength = BIG_NUMBER;

        for (int32 Index = 0; Index < SectorStates.Num(); ++Index)
        {
            const FBHWarSectorState& Sector =
                SectorStates[Index];

            if (Sector.Owner == EBHWarFaction::Enemy &&
                Sector.EnemyStrength < LowestEnemyStrength)
            {
                LowestEnemyStrength = Sector.EnemyStrength;
                TargetIndex = Index;
            }
        }

        if (SectorStates.IsValidIndex(TargetIndex))
        {
            FBHWarSectorState& Target =
                SectorStates[TargetIndex];
            const float PreviousFriendlyStrength =
                Target.FriendlyStrength;
            const float PreviousResponsePressure =
                Target.EnemyResponsePressure;
            Target.FriendlyStrength = FMath::Max(
                Target.FriendlyStrength,
                ResistanceStrength
            );
            Target.FriendlyGarrison = FMath::Max(
                Target.FriendlyGarrison,
                FMath::Min(3, Target.GarrisonCapacity)
            );
            Target.LastBattleTurn = TurnNumber;
            Target.EnemyResponsePressure = FMath::Max(
                Target.EnemyResponsePressure,
                50.0f
            );
            ClampSectorState(Target);
            RecordEnemyResponseEscalation(
                Target,
                PreviousResponsePressure
            );

            if (Target.FriendlyStrength >
                PreviousFriendlyStrength + KINDA_SMALL_NUMBER)
            {
                RecordWarEvent(
                    TEXT("Resistance"),
                    Target.SectorID,
                    FString::Printf(
                        TEXT("Friendly resistance reorganized in %s"),
                        *Target.DisplayName.ToString()
                    )
                );
            }

            UE_LOG(
                LogTemp,
                Log,
                TEXT(
                    "Friendly resistance reorganized in %s; "
                    "the persistent war remains active."
                ),
                *Target.SectorID.ToString()
            );
        }
    }
}

void UBHWarSubsystem::RecordWarEvent(
    FName EventType,
    FName SectorID,
    const FString& Summary
)
{
    const FString SanitizedSummary =
        Summary.TrimStartAndEnd().Left(
            MaximumWarEventSummaryLength
        );

    if (SanitizedSummary.IsEmpty())
    {
        return;
    }

    FBHWarEventRecord Event;
    Event.TurnNumber = FMath::Max(0, TurnNumber);
    Event.EventType = EventType.IsNone()
        ? FName(TEXT("CampaignEvent"))
        : EventType;
    Event.SectorID = SectorID;
    Event.Summary = SanitizedSummary;
    RecentWarEvents.Add(MoveTemp(Event));

    if (RecentWarEvents.Num() > MaximumWarEventHistory)
    {
        RecentWarEvents.RemoveAt(
            0,
            RecentWarEvents.Num() - MaximumWarEventHistory
        );
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_WAR_EVENT turn=%d type=%s sector=%s summary=\"%s\""),
        TurnNumber,
        *EventType.ToString(),
        *SectorID.ToString(),
        *SanitizedSummary
    );
}

void UBHWarSubsystem::RecordEnemyResponseEscalation(
    const FBHWarSectorState& Sector,
    float PreviousPressure
)
{
    const auto GetResponseTier = [](float Pressure)
    {
        return Pressure >= 75.0f
            ? 3
            : Pressure >= 50.0f
                ? 2
                : Pressure >= 25.0f
                    ? 1
                    : 0;
    };
    const int32 PreviousTier =
        GetResponseTier(PreviousPressure);
    const int32 CurrentTier =
        GetResponseTier(Sector.EnemyResponsePressure);

    if (CurrentTier <= PreviousTier || CurrentTier <= 0)
    {
        return;
    }

    const TCHAR* ResponseLabel = CurrentTier >= 3
        ? TEXT("CRACKDOWN")
        : CurrentTier >= 2
            ? TEXT("HUNTING")
            : TEXT("WATCHFUL");
    RecordWarEvent(
        TEXT("EnemyResponse"),
        Sector.SectorID,
        FString::Printf(
            TEXT("Enemy response escalated to %s in %s"),
            ResponseLabel,
            *Sector.DisplayName.ToString()
        )
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_ENEMY_RESPONSE_ESCALATED sector=%s "
            "previous=%.1f current=%.1f tier=%s"
        ),
        *Sector.SectorID.ToString(),
        PreviousPressure,
        Sector.EnemyResponsePressure,
        ResponseLabel
    );
}

void UBHWarSubsystem::BroadcastWarState()
{
    if (!bApplyingReplicatedSnapshot &&
        CanMutateWarState())
    {
        EvaluateCampaignOutcome();
    }

    OnWarStateChanged.Broadcast(
        TurnNumber,
        PrioritySectorID,
        PriorityType
    );
}

bool UBHWarSubsystem::CanMutateWarState() const
{
    const UWorld* World = GetWorld();
    return !IsValid(World) || World->GetNetMode() != NM_Client;
}

void UBHWarSubsystem::EvaluateCampaignOutcome()
{
    if (SectorStates.IsEmpty() ||
        CampaignOutcome != EBHWarCampaignOutcome::Ongoing)
    {
        return;
    }

    // The strategic simulation must not end the campaign while the
    // player is still resolving a committed operation. Its target,
    // staging sector, and enemy source are already protected from
    // background simulation. Deferring the terminal outcome here keeps
    // that same contract intact until ApplyMissionResult records the
    // player's result and releases the commitment.
    if (HasCommittedOperation())
    {
        return;
    }

    bool bAnyFriendlySector = false;
    bool bAllSectorsFriendly = true;
    bool bFriendlyHeadquartersLost = false;
    FName LostHeadquartersSectorID = NAME_None;

    for (const FBHWarSectorState& Sector : SectorStates)
    {
        bAnyFriendlySector |=
            Sector.Owner == EBHWarFaction::Friendly;
        bAllSectorsFriendly &=
            Sector.Owner == EBHWarFaction::Friendly;

        if (Sector.SiteType == EBHWarSiteType::Headquarters &&
            Sector.Owner == EBHWarFaction::Enemy)
        {
            bFriendlyHeadquartersLost = true;
            LostHeadquartersSectorID = Sector.SectorID;
        }
    }

    if (bFriendlyHeadquartersLost || !bAnyFriendlySector)
    {
        CampaignOutcome = EBHWarCampaignOutcome::EnemyVictory;
        PrioritySectorID = NAME_None;
        PriorityType = EBHWarPriorityType::None;
        PriorityReasonText = FText::GetEmpty();
        CommittedOperationSectorID = NAME_None;
        CommittedOperationID = NAME_None;
        CommittedOperationTargetID = NAME_None;
        CommittedOperationSupplySourceSectorID = NAME_None;
        CommittedOperationEnemySourceSectorID = NAME_None;
        CommittedOperationType = EBHWarPriorityType::None;
        SupplyConvoys.Reset();
        GarrisonTransfers.Reset();
        RecordWarEvent(
            TEXT("CampaignDefeat"),
            LostHeadquartersSectorID,
            TEXT(
                "Western command collapsed. "
                "The Korona campaign was lost."
            )
        );
    }
    else if (bAllSectorsFriendly)
    {
        CampaignOutcome =
            EBHWarCampaignOutcome::FriendlyVictory;
        PrioritySectorID = NAME_None;
        PriorityType = EBHWarPriorityType::None;
        PriorityReasonText = FText::GetEmpty();
        CommittedOperationSectorID = NAME_None;
        CommittedOperationID = NAME_None;
        CommittedOperationTargetID = NAME_None;
        CommittedOperationSupplySourceSectorID = NAME_None;
        CommittedOperationEnemySourceSectorID = NAME_None;
        CommittedOperationType = EBHWarPriorityType::None;
        SupplyConvoys.Reset();
        GarrisonTransfers.Reset();
        RecordWarEvent(
            TEXT("CampaignVictory"),
            NAME_None,
            TEXT(
                "All Koronan sectors were secured. "
                "The occupation has ended."
            )
        );
    }
    else
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "BH_CAMPAIGN_RESOLVED outcome=%d turn=%d "
            "friendly_control=%.0f"
        ),
        static_cast<int32>(CampaignOutcome),
        TurnNumber,
        GetFriendlyControlPercentage()
    );
}

void UBHWarSubsystem::ClampSectorState(
    FBHWarSectorState& Sector
) const
{
    Sector.GarrisonCapacity = FMath::Clamp(
        Sector.GarrisonCapacity,
        0,
        MaximumGarrisonCapacity
    );
    Sector.FriendlyGarrison = FMath::Clamp(
        Sector.FriendlyGarrison,
        0,
        Sector.GarrisonCapacity
    );
    Sector.EnemyGarrison = FMath::Clamp(
        Sector.EnemyGarrison,
        0,
        Sector.GarrisonCapacity
    );
    Sector.IntelConfidence = FMath::Clamp(
        Sector.IntelConfidence,
        0.0f,
        MaximumIntelConfidence
    );
    Sector.CivilianSupport = FMath::Clamp(
        Sector.CivilianSupport,
        0.0f,
        MaximumCivilianSupport
    );
    if (Sector.Owner == EBHWarFaction::Friendly)
    {
        Sector.IntelConfidence = FMath::Max(
            Sector.IntelConfidence,
            FriendlySectorIntelFloor
        );
    }
    Sector.FriendlyStrength = FMath::Clamp(
        Sector.FriendlyStrength,
        0.0f,
        MaximumSectorStrength
    );
    Sector.EnemyStrength = FMath::Clamp(
        Sector.EnemyStrength,
        0.0f,
        MaximumSectorStrength
    );
    Sector.Supply = FMath::Clamp(
        Sector.Supply,
        0.0f,
        MaximumSectorSupply
    );
    Sector.ReinforcementRate = FMath::Max(
        0.0f,
        Sector.ReinforcementRate
    );
    Sector.EnemyResponsePressure = FMath::Clamp(
        Sector.EnemyResponsePressure,
        0.0f,
        MaximumEnemyResponsePressure
    );
    Sector.RepeatedOperationCount = FMath::Clamp(
        Sector.RepeatedOperationCount,
        0,
        99
    );

    if (Sector.RepeatedOperationCount <= 0 ||
        Sector.AnticipatedOperationType ==
            EBHWarPriorityType::None)
    {
        Sector.AnticipatedOperationType =
            EBHWarPriorityType::None;
        Sector.RepeatedOperationCount = 0;
    }
    Sector.ConnectedSectorIDs.Remove(Sector.SectorID);
    Sector.ConnectedSectorIDs.Remove(NAME_None);
    Sector.ConnectedSectorIDs.Sort(FNameLexicalLess());
    Sector.ConnectedSectorIDs.SetNum(
        Algo::Unique(Sector.ConnectedSectorIDs)
    );
}

bool UBHWarSubsystem::ValidateSectorGraph(
    const TArray<FBHWarSectorState>& CandidateStates
) const
{
    if (CandidateStates.IsEmpty())
    {
        return false;
    }

    TSet<FName> SectorIDs;

    for (const FBHWarSectorState& Sector : CandidateStates)
    {
        if (Sector.SectorID.IsNone() ||
            SectorIDs.Contains(Sector.SectorID) ||
            Sector.GarrisonCapacity < 0 ||
            Sector.FriendlyGarrison < 0 ||
            Sector.EnemyGarrison < 0 ||
            (Sector.GarrisonCapacity > 0 &&
                (
                    Sector.FriendlyGarrison >
                        Sector.GarrisonCapacity ||
                    Sector.EnemyGarrison >
                        Sector.GarrisonCapacity
                )))
        {
            return false;
        }

        SectorIDs.Add(Sector.SectorID);
    }

    for (const FBHWarSectorState& Sector : CandidateStates)
    {
        for (const FName ConnectedSectorID :
            Sector.ConnectedSectorIDs)
        {
            if (ConnectedSectorID.IsNone() ||
                ConnectedSectorID == Sector.SectorID ||
                !SectorIDs.Contains(ConnectedSectorID))
            {
                return false;
            }
        }
    }

    return true;
}

bool UBHWarSubsystem::ValidateSupplyConvoy(
    const FBHWarSupplyConvoyState& Convoy
) const
{
    const int32 SourceIndex =
        FindSectorIndex(Convoy.SourceSectorID);
    const int32 DestinationIndex =
        FindSectorIndex(Convoy.DestinationSectorID);

    return !Convoy.ConvoyID.IsNone() &&
        SectorStates.IsValidIndex(SourceIndex) &&
        SectorStates.IsValidIndex(DestinationIndex) &&
        SourceIndex != DestinationIndex &&
        SectorStates[SourceIndex].ConnectedSectorIDs.Contains(
            Convoy.DestinationSectorID
        ) &&
        Convoy.Owner != EBHWarFaction::Neutral &&
        Convoy.SupplyPayload > KINDA_SMALL_NUMBER &&
        Convoy.TurnsRemaining > 0;
}

bool UBHWarSubsystem::ValidateGarrisonTransfer(
    const FBHWarGarrisonTransferState& Transfer
) const
{
    const int32 SourceIndex =
        FindSectorIndex(Transfer.SourceSectorID);
    const int32 DestinationIndex =
        FindSectorIndex(Transfer.DestinationSectorID);

    return !Transfer.TransferID.IsNone() &&
        SectorStates.IsValidIndex(SourceIndex) &&
        SectorStates.IsValidIndex(DestinationIndex) &&
        SourceIndex != DestinationIndex &&
        Transfer.TroopCount > 0 &&
        Transfer.TurnsRemaining > 0 &&
        Transfer.DispatchTurn >= 0;
}

int32 UBHWarSubsystem::FindSectorIndex(FName SectorID) const
{
    return SectorStates.IndexOfByPredicate(
        [SectorID](const FBHWarSectorState& Sector)
        {
            return Sector.SectorID == SectorID;
        }
    );
}

int32 UBHWarSubsystem::FindGarrisonRedeploymentSourceIndex(
    FName DestinationSectorID
) const
{
    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);

    if (!SectorStates.IsValidIndex(DestinationIndex) ||
        CampaignOutcome != EBHWarCampaignOutcome::Ongoing)
    {
        return INDEX_NONE;
    }

    const FBHWarSectorState& Destination =
        SectorStates[DestinationIndex];

    if (Destination.Owner != EBHWarFaction::Friendly ||
        Destination.FriendlyGarrison >=
            Destination.GarrisonCapacity ||
        IsCommittedOperationSector(DestinationSectorID))
    {
        return INDEX_NONE;
    }

    TArray<int32> Distances;
    TArray<int32> SearchQueue;
    Distances.Init(INDEX_NONE, SectorStates.Num());
    Distances[DestinationIndex] = 0;
    SearchQueue.Add(DestinationIndex);

    int32 BestSourceIndex = INDEX_NONE;
    int32 BestExcessGarrison = 0;
    int32 BestDistance = MAX_int32;
    float BestSupply = -1.0f;

    for (int32 QueueIndex = 0;
        QueueIndex < SearchQueue.Num();
        ++QueueIndex)
    {
        const int32 CurrentIndex = SearchQueue[QueueIndex];
        const FBHWarSectorState& CurrentSector =
            SectorStates[CurrentIndex];
        const int32 CurrentDistance = Distances[CurrentIndex];

        if (CurrentIndex != DestinationIndex &&
            !IsCommittedOperationSector(CurrentSector.SectorID))
        {
            const int32 ExcessGarrison = FMath::Max(
                0,
                CurrentSector.FriendlyGarrison -
                    MinimumRedeploymentSourceGarrison
            );
            const int32 TransferCount = FMath::Min3(
                MaximumGarrisonRedeploymentCount,
                ExcessGarrison,
                Destination.GarrisonCapacity -
                    Destination.FriendlyGarrison
            );
            const float TransferCost =
                TransferCount *
                GarrisonRedeploymentSupplyCostPerUnit *
                FMath::Max(1, CurrentDistance);

            if (TransferCount > 0 &&
                CurrentSector.Supply + KINDA_SMALL_NUMBER >=
                    TransferCost &&
                (
                    ExcessGarrison > BestExcessGarrison ||
                    (
                        ExcessGarrison == BestExcessGarrison &&
                        CurrentDistance < BestDistance
                    ) ||
                    (
                        ExcessGarrison == BestExcessGarrison &&
                        CurrentDistance == BestDistance &&
                        CurrentSector.Supply > BestSupply
                    )
                ))
            {
                BestSourceIndex = CurrentIndex;
                BestExcessGarrison = ExcessGarrison;
                BestDistance = CurrentDistance;
                BestSupply = CurrentSector.Supply;
            }
        }

        for (const FName ConnectedSectorID :
            CurrentSector.ConnectedSectorIDs)
        {
            const int32 ConnectedIndex =
                FindSectorIndex(ConnectedSectorID);

            if (!SectorStates.IsValidIndex(ConnectedIndex) ||
                Distances[ConnectedIndex] != INDEX_NONE ||
                SectorStates[ConnectedIndex].Owner !=
                    EBHWarFaction::Friendly)
            {
                continue;
            }

            Distances[ConnectedIndex] = CurrentDistance + 1;
            SearchQueue.Add(ConnectedIndex);
        }
    }

    return BestSourceIndex;
}

int32 UBHWarSubsystem::GetFriendlyRouteDistance(
    FName SourceSectorID,
    FName DestinationSectorID
) const
{
    const int32 SourceIndex = FindSectorIndex(SourceSectorID);
    const int32 DestinationIndex =
        FindSectorIndex(DestinationSectorID);

    if (!SectorStates.IsValidIndex(SourceIndex) ||
        !SectorStates.IsValidIndex(DestinationIndex) ||
        SectorStates[SourceIndex].Owner !=
            EBHWarFaction::Friendly ||
        SectorStates[DestinationIndex].Owner !=
            EBHWarFaction::Friendly)
    {
        return INDEX_NONE;
    }

    TArray<int32> Distances;
    TArray<int32> SearchQueue;
    Distances.Init(INDEX_NONE, SectorStates.Num());
    Distances[SourceIndex] = 0;
    SearchQueue.Add(SourceIndex);

    for (int32 QueueIndex = 0;
        QueueIndex < SearchQueue.Num();
        ++QueueIndex)
    {
        const int32 CurrentIndex = SearchQueue[QueueIndex];

        if (CurrentIndex == DestinationIndex)
        {
            return Distances[CurrentIndex];
        }

        for (const FName ConnectedSectorID :
            SectorStates[CurrentIndex].ConnectedSectorIDs)
        {
            const int32 ConnectedIndex =
                FindSectorIndex(ConnectedSectorID);

            if (!SectorStates.IsValidIndex(ConnectedIndex) ||
                Distances[ConnectedIndex] != INDEX_NONE ||
                SectorStates[ConnectedIndex].Owner !=
                    EBHWarFaction::Friendly)
            {
                continue;
            }

            Distances[ConnectedIndex] =
                Distances[CurrentIndex] + 1;
            SearchQueue.Add(ConnectedIndex);
        }
    }

    return INDEX_NONE;
}

int32 UBHWarSubsystem::
FindPriorityOperationSupplySourceIndex() const
{
    const TArray<int32> RouteIndices =
        BuildOperationSupplyRouteIndices(
            PrioritySectorID,
            PriorityType
        );

    return RouteIndices.IsEmpty()
        ? INDEX_NONE
        : RouteIndices[0];
}

TArray<int32>
UBHWarSubsystem::BuildOperationSupplyRouteIndices(
    FName TargetSectorID,
    EBHWarPriorityType OperationType
) const
{
    const int32 TargetIndex = FindSectorIndex(TargetSectorID);

    if (!SectorStates.IsValidIndex(TargetIndex) ||
        OperationType == EBHWarPriorityType::None)
    {
        return {};
    }

    TArray<int32> DistanceFromTarget;
    TArray<int32> NextHopTowardTarget;
    TArray<int32> SearchQueue;
    DistanceFromTarget.Init(INDEX_NONE, SectorStates.Num());
    NextHopTowardTarget.Init(INDEX_NONE, SectorStates.Num());

    const FBHWarSectorState& TargetSector =
        SectorStates[TargetIndex];

    if (TargetSector.Owner == EBHWarFaction::Friendly)
    {
        DistanceFromTarget[TargetIndex] = 0;
        SearchQueue.Add(TargetIndex);
    }

    for (const FName ConnectedSectorID :
        TargetSector.ConnectedSectorIDs)
    {
        const int32 ConnectedIndex =
            FindSectorIndex(ConnectedSectorID);

        if (SectorStates.IsValidIndex(ConnectedIndex) &&
            SectorStates[ConnectedIndex].Owner ==
                EBHWarFaction::Friendly &&
            DistanceFromTarget[ConnectedIndex] == INDEX_NONE)
        {
            DistanceFromTarget[ConnectedIndex] = 1;
            NextHopTowardTarget[ConnectedIndex] = TargetIndex;
            SearchQueue.Add(ConnectedIndex);
        }
    }

    for (int32 QueueIndex = 0;
        QueueIndex < SearchQueue.Num();
        ++QueueIndex)
    {
        const int32 CurrentIndex = SearchQueue[QueueIndex];
        const FBHWarSectorState& CurrentSector =
            SectorStates[CurrentIndex];

        for (const FName ConnectedSectorID :
            CurrentSector.ConnectedSectorIDs)
        {
            const int32 ConnectedIndex =
                FindSectorIndex(ConnectedSectorID);

            if (!SectorStates.IsValidIndex(ConnectedIndex) ||
                SectorStates[ConnectedIndex].Owner !=
                    EBHWarFaction::Friendly ||
                DistanceFromTarget[ConnectedIndex] != INDEX_NONE)
            {
                continue;
            }

            DistanceFromTarget[ConnectedIndex] =
                DistanceFromTarget[CurrentIndex] + 1;
            NextHopTowardTarget[ConnectedIndex] = CurrentIndex;
            SearchQueue.Add(ConnectedIndex);
        }
    }

    int32 BestSourceIndex = INDEX_NONE;
    float BestSupply = -1.0f;
    int32 BestDistance = MAX_int32;

    if (OperationType == EBHWarPriorityType::EscortRescue)
    {
        const FName TargetConvoyID =
            HasCommittedOperation() &&
                CommittedOperationType ==
                    EBHWarPriorityType::EscortRescue &&
                CommittedOperationSectorID == TargetSectorID
                ? CommittedOperationTargetID
                : GetEscortOperationTargetID(TargetSectorID);
        const int32 ConvoySourceIndex = FindSectorIndex(
            GetSupplyConvoyState(TargetConvoyID).SourceSectorID
        );

        if (!SectorStates.IsValidIndex(ConvoySourceIndex) ||
            !SearchQueue.Contains(ConvoySourceIndex))
        {
            return {};
        }

        BestSourceIndex = ConvoySourceIndex;
    }

    for (const int32 CandidateIndex : SearchQueue)
    {
        if (OperationType == EBHWarPriorityType::EscortRescue)
        {
            break;
        }
        const float CandidateSupply =
            SectorStates[CandidateIndex].Supply;
        const int32 CandidateDistance =
            DistanceFromTarget[CandidateIndex];

        if (OperationType == EBHWarPriorityType::Resupply &&
            (CandidateIndex == TargetIndex ||
             CandidateSupply + KINDA_SMALL_NUMBER <
                MinimumSupplyReserve +
                    PriorityOperationSupplyCost))
        {
            continue;
        }

        if (CandidateSupply > BestSupply ||
            (FMath::IsNearlyEqual(CandidateSupply, BestSupply) &&
             CandidateDistance < BestDistance))
        {
            BestSupply = CandidateSupply;
            BestDistance = CandidateDistance;
            BestSourceIndex = CandidateIndex;
        }
    }

    if (!SectorStates.IsValidIndex(BestSourceIndex))
    {
        return {};
    }

    TArray<int32> RouteIndices;
    int32 CurrentIndex = BestSourceIndex;

    while (SectorStates.IsValidIndex(CurrentIndex))
    {
        RouteIndices.Add(CurrentIndex);

        if (CurrentIndex == TargetIndex)
        {
            break;
        }

        CurrentIndex = NextHopTowardTarget[CurrentIndex];

        if (RouteIndices.Num() > SectorStates.Num())
        {
            return {};
        }
    }

    return !RouteIndices.IsEmpty() &&
        RouteIndices.Last() == TargetIndex
        ? RouteIndices
        : TArray<int32>();
}

bool UBHWarSubsystem::IsFrontlineSector(int32 SectorIndex) const
{
    if (!SectorStates.IsValidIndex(SectorIndex))
    {
        return false;
    }

    const FBHWarSectorState& Sector = SectorStates[SectorIndex];

    for (const FName ConnectedSectorID :
        Sector.ConnectedSectorIDs)
    {
        const int32 ConnectedIndex =
            FindSectorIndex(ConnectedSectorID);

        if (SectorStates.IsValidIndex(ConnectedIndex) &&
            SectorStates[ConnectedIndex].Owner != Sector.Owner)
        {
            return true;
        }
    }

    return false;
}

bool UBHWarSubsystem::IsCommittedOperationSector(
    FName SectorID
) const
{
    return HasCommittedOperation() &&
        (
            SectorID == CommittedOperationSectorID ||
            SectorID ==
                CommittedOperationSupplySourceSectorID ||
            SectorID ==
                CommittedOperationEnemySourceSectorID
        );
}
