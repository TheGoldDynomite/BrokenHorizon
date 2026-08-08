#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.h"
#include "GameFramework/GameStateBase.h"
#include "BHWarGameState.generated.h"

class UBHWarSubsystem;
class ABHFieldTransport;
class ABHCharacter;
class ABHExtractionZone;

UENUM(BlueprintType)
enum class EBHActiveOperationPhase : uint8
{
    None,
    Approach,
    Combat,
    AwaitingWave,
    Securing,
    RaidExfiltration,
    DebriefSuccess,
    DebriefFailure
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHActiveOperationSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    int32 Revision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    EBHActiveOperationPhase Phase = EBHActiveOperationPhase::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    FName SectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    FName OperationID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    FName SupplySourceSectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    FName EnemySourceSectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    EBHWarPriorityType OperationType = EBHWarPriorityType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    FVector_NetQuantize OperationCenter = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    float PhaseEndServerWorldTimeSeconds = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Operation")
    FBHOpenWorldOperationState OperationState;

    bool IsActive() const
    {
        return Phase != EBHActiveOperationPhase::None;
    }
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHSquadPingSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Ping")
    int32 Revision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Ping")
    FVector_NetQuantize Location = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Ping")
    TObjectPtr<AActor> TrackedActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Ping")
    FName ContextLabel = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Ping")
    FName IssuerLabel = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Ping")
    float ExpiryServerWorldTimeSeconds = 0.0f;

    bool IsActiveAt(float ServerWorldTimeSeconds) const
    {
        return Revision > 0 &&
            !ContextLabel.IsNone() &&
            ExpiryServerWorldTimeSeconds > ServerWorldTimeSeconds;
    }
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHWarStateSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War")
    int32 Revision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War")
    bool bInitialized = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Difficulty")
    FBHCampaignDifficultyProfile CampaignDifficulty;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War|Progression")
    FBHCampaignProgressionState CampaignProgression;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War")
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
        Category = "Persistent War|History"
    )
    TArray<FBHWarEventRecord> RecentWarEvents;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War")
    int32 TurnNumber = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War")
    float SimulationAccumulator = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War")
    FName PrioritySectorID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War")
    EBHWarPriorityType PriorityType = EBHWarPriorityType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistent War")
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
};

UCLASS()
class BROKENHORIZON_API ABHWarGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ABHWarGameState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Networking")
    bool HasAuthoritativeWarState() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Networking")
    int32 GetWarStateRevision() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Networking")
    int32 GetReplicatedWarTurn() const;

    UFUNCTION(BlueprintPure, Category = "Persistent War|Networking")
    FBHActiveOperationSnapshot GetActiveOperationSnapshot() const;

    UFUNCTION(BlueprintPure, Category = "Squad|Ping")
    FBHSquadPingSnapshot GetSquadPingSnapshot() const;

    void PublishSquadPing(
        const FVector& Location,
        FName ContextLabel,
        FName IssuerLabel,
        float LifetimeSeconds,
        AActor* TrackedActor = nullptr
    );

    void PublishActiveOperationSnapshot(
        const FBHActiveOperationSnapshot& NewSnapshot
    );

    void ClearActiveOperationSnapshot();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleWarStateChanged(
        int32 TurnNumber,
        FName PrioritySectorID,
        EBHWarPriorityType PriorityType
    );

    UFUNCTION()
    void OnRep_WarStateSnapshot();

    UFUNCTION()
    void OnRep_ActiveOperationSnapshot();

    UFUNCTION()
    void OnRep_SquadPingSnapshot();

    void PublishAuthoritativeSnapshot();
    void ApplyReplicatedSnapshot();
    UBHWarSubsystem* ResolveWarSubsystem() const;

#if !UE_BUILD_SHIPPING
    void RunFieldSquadContextOwnershipTest();
    FTimerHandle FieldSquadContextOwnershipTestTimer;
    void RunMedicalRecoveryReplicationTest();
    FTimerHandle MedicalRecoveryReplicationTestTimer;
    void RunNetworkBudgetTelemetryTest();
    void ConfigureNetworkCombatDensityTest();
    void VerifyNetworkCombatDensityReplicationTest();
    void RunRenderedTraversalTest();
    void RunRenderedUIReviewTest();
    void RunFirstLightPlayableRouteTest();
    FTimerHandle NetworkBudgetTelemetryTestTimer;
    FTimerHandle NetworkCombatDensityClientVerificationTimer;
    FTimerHandle RenderedTraversalTestTimer;
    FTimerHandle RenderedUIReviewTestTimer;
    FTimerHandle FirstLightPlayableRouteTestTimer;
    int32 NetworkBudgetTelemetrySampleCount = 0;
    int32 NetworkBudgetTelemetryRequiredConnections = 2;
    int32 NetworkCombatDensityHostileCount = 12;
    int32 NetworkCombatDensityFriendlyCount = 4;
    int32 NetworkCombatDensityWarmupSamples = 0;
    int32 NetworkCombatDensityClientVerificationAttempts = 0;
    bool bNetworkCombatDensityRequested = false;
    bool bNetworkCombatDensityConfigured = false;
    bool bRenderedWorldTraversalTest = false;
    bool bRenderedWorldTraversalTargetSettled = false;
    int32 RenderedTraversalTestStep = 0;
    int32 RenderedTraversalTestSubstep = 0;
    int32 RenderedTraversalTestDwellTicks = 0;
    int32 FirstLightPlayableRouteTestPhase = 0;
    int32 FirstLightPlayableRouteCombatPasses = 0;
    int32 FirstLightPlayableRouteEnemiesDefeated = 0;
    TWeakObjectPtr<ABHCharacter> FirstLightPlayableRouteTestPlayer;
    TWeakObjectPtr<ABHExtractionZone>
        FirstLightPlayableRouteTestExtraction;
    FVector RenderedTraversalSegmentStart = FVector::ZeroVector;
    FVector RenderedTraversalSegmentTarget = FVector::ZeroVector;
    FVector RenderedTraversalFocalTarget = FVector::ZeroVector;
    FName RenderedTraversalRouteLabel = NAME_None;
    int32 MedicalRecoveryReplicationTestPhase = 0;
    TWeakObjectPtr<ABHFieldTransport>
        MedicalRecoveryReplicationTestTransport;
#endif

    UPROPERTY(ReplicatedUsing = OnRep_WarStateSnapshot)
    FBHWarStateSnapshot WarStateSnapshot;

    UPROPERTY(ReplicatedUsing = OnRep_ActiveOperationSnapshot)
    FBHActiveOperationSnapshot ActiveOperationSnapshot;

    UPROPERTY(ReplicatedUsing = OnRep_SquadPingSnapshot)
    FBHSquadPingSnapshot SquadPingSnapshot;

    UPROPERTY(Transient)
    TObjectPtr<UBHWarSubsystem> BoundWarSubsystem;
};
