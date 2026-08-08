#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.generated.h"

UENUM(BlueprintType)
enum class EBHWarFaction : uint8
{
    Neutral UMETA(DisplayName = "Neutral"),
    Friendly UMETA(DisplayName = "Friendly"),
    Enemy UMETA(DisplayName = "Enemy")
};

UENUM(BlueprintType)
enum class EBHWarPriorityType : uint8
{
    None UMETA(DisplayName = "None"),
    Attack UMETA(DisplayName = "Attack"),
    Defend UMETA(DisplayName = "Defend"),
    Raid UMETA(DisplayName = "Raid"),
    Resupply UMETA(DisplayName = "Resupply"),
    EscortRescue UMETA(DisplayName = "Escort"),
    Rescue UMETA(DisplayName = "Rescue"),
    Recon UMETA(DisplayName = "Recon")
};

UENUM(BlueprintType)
enum class EBHRaidOperationalSignature : uint8
{
    Clean UMETA(DisplayName = "Clean"),
    Contested UMETA(DisplayName = "Contested"),
    Loud UMETA(DisplayName = "Loud")
};

UENUM(BlueprintType)
enum class EBHWarCampaignOutcome : uint8
{
    Ongoing UMETA(DisplayName = "Ongoing"),
    FriendlyVictory UMETA(DisplayName = "Friendly Victory"),
    EnemyVictory UMETA(DisplayName = "Enemy Victory")
};

UENUM(BlueprintType)
enum class EBHWarSiteType : uint8
{
    Headquarters UMETA(DisplayName = "Headquarters"),
    Village UMETA(DisplayName = "Village"),
    Checkpoint UMETA(DisplayName = "Checkpoint"),
    Town UMETA(DisplayName = "Town"),
    Bridge UMETA(DisplayName = "Bridge"),
    LogisticsDepot UMETA(DisplayName = "Logistics Depot")
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHFieldSquadMemberState
{
    GENERATED_BODY()

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    FName MemberID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    float Health = -1.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    int32 MagazineAmmo = -1;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    int32 ReserveAmmo = -1;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    int32 FragGrenades = -1;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float CombatReadiness = 1.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    bool bIncapacitated = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    bool bRequiresMedicalEvacuation = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    bool bEmbarked = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    bool bHasWorldTransform = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    FTransform WorldTransform = FTransform::Identity;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "Persistent War|Field Squad"
    )
    float IncapacitationSecondsRemaining = 0.0f;
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHOpenWorldOperationState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    bool bHasSnapshot = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    bool bOperationActivated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    bool bWaitingForWave = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    bool bSecuringObjective = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    bool bRaidTargetSabotaged = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    bool bRaidDetectedBeforeSabotage = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    bool bFriendlySupportHolding = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    bool bFriendlySupportHasCommandLocation = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    FVector FriendlySupportCommandLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float FriendlySupportCommandYaw = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float ObjectiveSecureProgress = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float DefenseBreachProgress = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 CurrentWave = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float SecondsUntilNextWave = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float SecondsUntilApproachDeadline = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 LivingEnemyCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 LivingAllyCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 FriendlySupportCasualties = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 EnemyCasualties = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 EnemyRoutedCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 AttackEnemyCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 AttackReinforcementWaveCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 AttackReinforcementsPerWave = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 DefenseWaveCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 DefenseEnemiesPerWave = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    int32 FriendlySupportCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    FName EnemySourceSectorID = NAME_None;
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHWarSectorState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    FName SectorID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    EBHWarFaction Owner = EBHWarFaction::Neutral;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Garrison"
    )
    EBHWarSiteType SiteType = EBHWarSiteType::Checkpoint;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Garrison"
    )
    int32 GarrisonCapacity = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Garrison"
    )
    int32 FriendlyGarrison = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Garrison"
    )
    int32 EnemyGarrison = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Intelligence",
        meta = (ClampMin = "0.0", ClampMax = "100.0")
    )
    float IntelConfidence = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Population",
        meta = (ClampMin = "0.0", ClampMax = "100.0")
    )
    float CivilianSupport = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float FriendlyStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float EnemyStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float Supply = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    float ReinforcementRate = 3.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Response",
        meta = (ClampMin = "0.0", ClampMax = "100.0")
    )
    float EnemyResponsePressure = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Adaptation"
    )
    EBHWarPriorityType AnticipatedOperationType =
        EBHWarPriorityType::None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Adaptation",
        meta = (ClampMin = "0")
    )
    int32 RepeatedOperationCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "War")
    TArray<FName> ConnectedSectorIDs;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War"
    )
    int32 LastBattleTurn = INDEX_NONE;
};

UENUM(BlueprintType)
enum class EBHWarConvoyCargoType : uint8
{
    MilitarySupply,
    CivilianAid
};

UENUM(BlueprintType)
enum class EBHCampaignDifficultyPreset : uint8
{
    Recruit UMETA(DisplayName = "Recruit"),
    Operator UMETA(DisplayName = "Operator"),
    Veteran UMETA(DisplayName = "Veteran"),
    Custom UMETA(DisplayName = "Custom")
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHCampaignDifficultyProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Difficulty")
    EBHCampaignDifficultyPreset Preset =
        EBHCampaignDifficultyPreset::Operator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Difficulty", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float IncomingDamageMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Difficulty", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float EnemyPerceptionMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Difficulty", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float EnemyCoordinationMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Difficulty", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float MedicalPressureMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Difficulty", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float StrategicPressureMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Difficulty", meta = (ClampMin = "0.25", ClampMax = "2.0"))
    float CheckpointIntervalMultiplier = 1.0f;
};

namespace BHDifficulty
{
    inline FBHCampaignDifficultyProfile BuildPreset(
        EBHCampaignDifficultyPreset Preset
    )
    {
        FBHCampaignDifficultyProfile Profile;
        Profile.Preset = Preset;
        if (Preset == EBHCampaignDifficultyPreset::Recruit)
        {
            Profile.IncomingDamageMultiplier = 0.75f;
            Profile.EnemyPerceptionMultiplier = 0.80f;
            Profile.EnemyCoordinationMultiplier = 0.80f;
            Profile.MedicalPressureMultiplier = 0.75f;
            Profile.StrategicPressureMultiplier = 0.80f;
            Profile.CheckpointIntervalMultiplier = 0.50f;
        }
        else if (Preset == EBHCampaignDifficultyPreset::Veteran)
        {
            Profile.IncomingDamageMultiplier = 1.15f;
            Profile.EnemyPerceptionMultiplier = 1.15f;
            Profile.EnemyCoordinationMultiplier = 1.25f;
            Profile.MedicalPressureMultiplier = 1.25f;
            Profile.StrategicPressureMultiplier = 1.25f;
        }
        return Profile;
    }

    inline FBHCampaignDifficultyProfile Sanitize(
        const FBHCampaignDifficultyProfile& Candidate
    )
    {
        FBHCampaignDifficultyProfile Result = Candidate;
        Result.IncomingDamageMultiplier = FMath::Clamp(Result.IncomingDamageMultiplier, 0.5f, 2.0f);
        Result.EnemyPerceptionMultiplier = FMath::Clamp(Result.EnemyPerceptionMultiplier, 0.5f, 2.0f);
        Result.EnemyCoordinationMultiplier = FMath::Clamp(Result.EnemyCoordinationMultiplier, 0.5f, 2.0f);
        Result.MedicalPressureMultiplier = FMath::Clamp(Result.MedicalPressureMultiplier, 0.5f, 2.0f);
        Result.StrategicPressureMultiplier = FMath::Clamp(Result.StrategicPressureMultiplier, 0.5f, 2.0f);
        Result.CheckpointIntervalMultiplier = FMath::Clamp(Result.CheckpointIntervalMultiplier, 0.25f, 2.0f);
        return Result;
    }
}

UENUM(BlueprintType)
enum class EBHAfterActionGrade : uint8
{
    Diminished,
    Effective,
    Strong,
    Exceptional
};

UENUM(BlueprintType)
enum class EBHCampaignCapability : uint8
{
    IntelligenceNetwork,
    CasualtyRecoveryNetwork,
    TransportSupportNetwork
};

UENUM(BlueprintType)
enum class EBHOperationTacticalOption : uint8
{
    None UMETA(DisplayName = "Standard Planning"),
    ReconPlanning UMETA(DisplayName = "Recon Planning"),
    ReinforcementPriority UMETA(DisplayName = "Reinforcement Priority"),
    MedicalPreparation UMETA(DisplayName = "Medical Preparation")
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHOperationAfterActionRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    FName OperationID = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    FName SectorID = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    EBHWarPriorityType OperationType = EBHWarPriorityType::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    bool bSucceeded = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 FriendlyCasualties = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 EnemyCasualties = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 EnemyRouted = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    float StrategicSupplyDelta = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    float RecoveredMateriel = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 MissionResultScore = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 ForcePreservationScore = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 EnemyOutcomeScore = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 ResourceEfficiencyScore = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 OperationalEffectScore = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    EBHOperationTacticalOption TacticalOption =
        EBHOperationTacticalOption::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    float TacticalSupplyCost = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 TacticalExecutionScore = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 TotalScore = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    EBHAfterActionGrade Grade = EBHAfterActionGrade::Diminished;
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHCampaignProgressionState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 CampaignMerit = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 CompletedOperations = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    int32 SuccessfulOperations = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    TArray<EBHCampaignCapability> UnlockedCapabilities;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    EBHOperationTacticalOption ActiveTacticalOption =
        EBHOperationTacticalOption::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression")
    FBHOperationAfterActionRecord LastAfterAction;
};

UENUM(BlueprintType)
enum class EBHRouteOperationVariation : uint8
{
    Standard UMETA(DisplayName = "Standard Route"),
    Ambush UMETA(DisplayName = "Heavy Ambush"),
    DamagedVehicle UMETA(DisplayName = "Damaged Vehicle"),
    TimeCritical UMETA(DisplayName = "Time Critical")
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHRouteOperationProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "War|Route Operation")
    EBHRouteOperationVariation Variation =
        EBHRouteOperationVariation::Standard;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "War|Route Operation")
    int32 AdditionalAmbushers = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "War|Route Operation")
    float InitialIntegrity = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "War|Route Operation")
    float CompletionDeadlineSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHWarSupplyConvoyState
{
    GENERATED_BODY()

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics"
    )
    FName ConvoyID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics"
    )
    FName SourceSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics"
    )
    FName DestinationSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics"
    )
    EBHWarFaction Owner = EBHWarFaction::Neutral;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics"
    )
    EBHWarConvoyCargoType CargoType =
        EBHWarConvoyCargoType::MilitarySupply;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics"
    )
    float SupplyPayload = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics"
    )
    int32 TurnsRemaining = 1;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics"
    )
    int32 DispatchTurn = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics|Route Operation"
    )
    FBHRouteOperationProfile RouteOperationProfile;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics|Route Operation"
    )
    FName SelectedWorldRouteID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics|Route Operation"
    )
    float OperationDeadlineSecondsRemaining = 0.0f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Logistics|Route Operation"
    )
    bool bRouteOperationInitialized = false;
};

namespace BHRouteOperations
{
    inline FBHRouteOperationProfile BuildProfile(
        const FBHWarSupplyConvoyState& ConvoyState
    )
    {
        FBHRouteOperationProfile Profile;
        const uint32 StableSeed = HashCombine(
            GetTypeHash(ConvoyState.ConvoyID),
            HashCombine(
                GetTypeHash(ConvoyState.SourceSectorID),
                GetTypeHash(ConvoyState.DestinationSectorID)
            )
        );
        Profile.Variation =
            static_cast<EBHRouteOperationVariation>(StableSeed % 4);

        switch (Profile.Variation)
        {
        case EBHRouteOperationVariation::Ambush:
            Profile.AdditionalAmbushers = 2;
            break;
        case EBHRouteOperationVariation::DamagedVehicle:
            Profile.InitialIntegrity = 0.55f;
            break;
        case EBHRouteOperationVariation::TimeCritical:
            Profile.CompletionDeadlineSeconds = 150.0f;
            break;
        default:
            break;
        }
        return Profile;
    }

    inline void Initialize(FBHWarSupplyConvoyState& ConvoyState)
    {
        ConvoyState.RouteOperationProfile = BuildProfile(ConvoyState);
        ConvoyState.OperationDeadlineSecondsRemaining =
            ConvoyState.RouteOperationProfile.CompletionDeadlineSeconds;
        ConvoyState.bRouteOperationInitialized = true;
    }
}

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHWarGarrisonTransferState
{
    GENERATED_BODY()

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Manpower"
    )
    FName TransferID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Manpower"
    )
    FName SourceSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Manpower"
    )
    FName DestinationSectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Manpower"
    )
    int32 TroopCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Manpower"
    )
    int32 TurnsRemaining = 1;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|Manpower"
    )
    int32 DispatchTurn = 0;
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHWarEventRecord
{
    GENERATED_BODY()

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|History"
    )
    int32 TurnNumber = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|History"
    )
    FName EventType = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|History"
    )
    FName SectorID = NAME_None;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        SaveGame,
        Category = "War|History"
    )
    FString Summary;
};
