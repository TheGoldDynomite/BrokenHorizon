#pragma once

#include "CoreMinimal.h"
#include "BHWarTypes.h"
#include "BHLoadoutWeight.h"
#include "Blueprint/UserWidget.h"
#include "BHCombatStatusWidget.generated.h"

class UProgressBar;
class UTextBlock;
class AActor;

UCLASS()
class BROKENHORIZON_API UBHCombatStatusWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetHealth(float CurrentHealth, float MaxHealth);

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetStamina(float CurrentStamina, float MaxStamina);

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void NotifyPlayerDamaged(
        float DamageAmount,
        float HealthPercentage,
        FVector DamageSourceDirection,
        AActor* DamageCauser
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void NotifyNearMiss(
        FVector SourceDirection,
        float Intensity
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetSuppression(float SuppressionPercentage);

    void NotifyGrenadeThreat(
        AActor* SourceActor,
        FVector SourceDirection,
        float DistanceCentimeters,
        float TimeUntilDetonation
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetInjuryState(
        bool bBleeding,
        float BleedRate,
        bool bArmInjured,
        bool bLegInjured,
        int32 FieldDressings
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetMedicalState(
        int32 Medkits,
        float HelmetDurabilityPercentage,
        float BodyArmorDurabilityPercentage,
        bool bTreatmentActive,
        float TreatmentProgress
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetFragGrenadeCount(int32 GrenadeCount);

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetEngineeringChargeState(
        int32 CarriedCharges,
        int32 ActiveCharges
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetCarryLoad(
        float TotalKilograms,
        EBHCarryLoadState LoadState,
        float MovementSpeedMultiplier,
        float StaminaDrainMultiplier
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetOperationWaypoint(
        bool bVisible,
        bool bOperationActive,
        const FText& SectorDisplayName,
        const FText& OperationStatus,
        const FVector& WorldDirection,
        float DistanceCentimeters,
        bool bTravelEstimateVisible,
        float EstimatedTravelMinutes,
        float EstimatedRangeKilometers,
        bool bFuelShortfall
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetOperationArrivalDeadlineRisk(bool bAtRisk);

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetCasualtyWaypoint(
        bool bVisible,
        int32 IncapacitatedOperatives,
        const FVector& WorldDirection,
        float DistanceCentimeters,
        float RecoverySecondsRemaining
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetSquadCommandWaypoint(
        bool bVisible,
        const FVector& WorldDirection,
        float DistanceCentimeters
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetResupplyWaypoint(
        bool bVisible,
        const FText& SectorDisplayName,
        const FVector& WorldDirection,
        float DistanceCentimeters
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetLogisticsWaypoint(
        bool bVisible,
        const FText& SectorDisplayName,
        const FVector& WorldDirection,
        float DistanceCentimeters,
        float CargoSupply
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetSalvageWaypoint(
        bool bVisible,
        const FVector& WorldDirection,
        float DistanceCentimeters,
        float RecoverableSupply
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetConvoyWaypoint(
        bool bVisible,
        EBHWarFaction ConvoyOwner,
        const FVector& WorldDirection,
        float DistanceCentimeters,
        float SupplyPayload,
        float IntegrityPercentage
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetTransportWaypoint(
        bool bVisible,
        const FVector& WorldDirection,
        float DistanceCentimeters,
        float FuelPercentage,
        float HullPercentage,
        bool bImmobilized
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetVehicleReadiness(
        bool bVisible,
        float FuelPercentage,
        float HullPercentage,
        float SpeedKPH,
        bool bImmobilized
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetStrategicSituation(
        bool bVisible,
        const FText& SectorDisplayName,
        EBHWarFaction SectorOwner,
        float SectorSupply,
        float SupplyFlowPerTurn,
        int32 WarTurn,
        int32 ConstructedFortifications,
        int32 FortificationCapacity,
        float FortificationCoverage,
        float FortificationDefenseMultiplier,
        bool bFortificationsConnected
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetEnemyResponsePressure(float ResponsePressure);

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetCivilianSupport(float CivilianSupport);

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetFieldReconStatus(
        bool bActive,
        float IntelConfidence,
        float MovementProgress,
        float MovementRequired,
        float ObservationProgress,
        float ObservationRequired,
        float ReportCooldownRemaining
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetFieldSquadStatus(
        bool bVisible,
        int32 LivingOperatives,
        int32 MaximumOperatives,
        bool bHolding,
        bool bEmbarked
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetFieldSquadServiceNeeds(
        int32 MembersNeedingService,
        int32 MembersRequiringEvacuation = 0
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetSquadPingWaypoint(
        bool bVisible,
        const FVector& WorldDirection,
        float DistanceCentimeters,
        const FString& ContextLabel,
        const FString& IssuerLabel,
        bool bTrackedTarget = false,
        bool bLineOfSightVisible = true
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetFieldSquadReadiness(
        float AverageReadiness,
        float LowestReadiness
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetFieldSquadContextStatus(
        const FString& ActionLabel,
        const FString& TargetLabel,
        bool bReachedTarget
    );

    UFUNCTION(BlueprintCallable, Category = "Combat Status")
    void SetConvoyOperationProfile(
        EBHRouteOperationVariation Variation,
        float DeadlineSecondsRemaining
    );

    static FString BuildFieldSquadStatusLabel(
        int32 LivingOperatives,
        int32 MaximumOperatives,
        bool bHolding,
        bool bEmbarked,
        int32 MembersNeedingService,
        int32 MembersRequiringEvacuation = 0,
        float AverageReadiness = -1.0f,
        float LowestReadiness = -1.0f,
        const FString& ContextStatusLine = FString(),
        const FString& SquadOrderPrompt = FString(TEXT("C"))
    );

    static FString BuildFieldSquadContextStatusLine(
        const FString& ActionLabel,
        const FString& TargetLabel,
        bool bReachedTarget
    );

    static FString BuildSquadCommandWaypointLabel(
        float DistanceCentimeters,
        const FString& SquadOrderPrompt = FString(TEXT("C"))
    );

    static FString BuildSquadPingWaypointLabel(
        float DistanceCentimeters,
        const FString& ContextLabel,
        const FString& IssuerLabel,
        bool bTrackedTarget = false,
        bool bLineOfSightVisible = true
    );

    static bool IsSquadPingTargetVisible(
        bool bBlockingHit,
        const AActor* HitActor,
        const AActor* TrackedActor
    );

protected:
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime
    ) override;

    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Combat Status")
    void OnPlayerDamaged(
        float DamageAmount,
        float HealthPercentage,
        FVector DamageSourceDirection,
        AActor* DamageCauser
    );

    UFUNCTION(BlueprintImplementableEvent, Category = "Combat Status")
    void OnNearMiss(
        FVector SourceDirection,
        float Intensity
    );

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> StaminaBar;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> HealthText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StaminaText;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Combat Status|Incoming Fire",
        meta = (ClampMin = "0.05", Units = "s")
    )
    float DamageFlashDuration = 0.35f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Combat Status|Incoming Fire",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float MaximumDamageFlashOpacity = 0.18f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Combat Status|Incoming Fire",
        meta = (ClampMin = "0.05", Units = "s")
    )
    float NearMissFeedbackDuration = 0.55f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Combat Status|Incoming Fire",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float MaximumNearMissOpacity = 0.10f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Combat Status|Low Health",
        meta = (ClampMin = "0.01", ClampMax = "1.0")
    )
    float LowHealthThreshold = 0.25f;

private:
    struct FGrenadeThreatState
    {
        TWeakObjectPtr<AActor> SourceActor;
        float DirectionAngleRadians = 0.0f;
        float DistanceCentimeters = 0.0f;
        float TimeUntilDetonation = 0.0f;
        float RefreshRemaining = 0.0f;
    };

    float ResolveRelativeDirectionAngle(
        const FVector& SourceDirection
    ) const;

    TArray<FGrenadeThreatState> GrenadeThreats;
    float CurrentHealthPercentage = 1.0f;
    float DamageFeedbackRemaining = 0.0f;
    float NearMissFeedbackRemaining = 0.0f;
    float NearMissIntensity = 0.0f;
    float CurrentSuppressionPercentage = 0.0f;
    float DamageDirectionAngleRadians = 0.0f;
    float NearMissDirectionAngleRadians = 0.0f;
    float GrenadeWarningPulseTime = 0.0f;
    float LowHealthPulseTime = 0.0f;
    float InjuryPulseTime = 0.0f;
    float CurrentBleedRate = 0.0f;
    float HelmetDurabilityPercentage = 1.0f;
    float BodyArmorDurabilityPercentage = 1.0f;
    float MedicalTreatmentProgress = 0.0f;
    int32 FieldDressingCount = 0;
    int32 MedkitCount = 0;
    int32 FragGrenadeCount = 0;
    int32 EngineeringChargeCount = 0;
    int32 ActiveEngineeringChargeCount = 0;
    float CarryLoadKilograms = 0.0f;
    EBHCarryLoadState CarryLoadState = EBHCarryLoadState::FightingLoad;
    float CarryMovementSpeedMultiplier = 1.0f;
    float CarryStaminaDrainMultiplier = 1.0f;
    bool bIsBleeding = false;
    bool bArmInjured = false;
    bool bLegInjured = false;
    bool bMedicalTreatmentActive = false;
    bool bOperationWaypointVisible = false;
    bool bOperationWaypointActive = false;
    FText OperationSectorDisplayName;
    FText OperationStatusText;
    float OperationDistanceCentimeters = 0.0f;
    float OperationDirectionAngleRadians = 0.0f;
    float OperationEstimatedTravelMinutes = 0.0f;
    float OperationEstimatedRangeKilometers = 0.0f;
    bool bOperationTravelEstimateVisible = false;
    bool bOperationFuelShortfall = false;
    bool bOperationArrivalDeadlineRisk = false;
    bool bCasualtyWaypointVisible = false;
    int32 CasualtyWaypointOperativeCount = 0;
    float CasualtyWaypointDistanceCentimeters = 0.0f;
    float CasualtyWaypointDirectionAngleRadians = 0.0f;
    float CasualtyWaypointRecoverySecondsRemaining = 0.0f;
    bool bSquadCommandWaypointVisible = false;
    float SquadCommandWaypointDirectionAngleRadians = 0.0f;
    float SquadCommandWaypointDistanceCentimeters = 0.0f;
    bool bSquadPingWaypointVisible = false;
    float SquadPingWaypointDirectionAngleRadians = 0.0f;
    float SquadPingWaypointDistanceCentimeters = 0.0f;
    FString SquadPingContextLabel;
    FString SquadPingIssuerLabel;
    bool bSquadPingTrackedTarget = false;
    bool bSquadPingLineOfSightVisible = true;
    bool bResupplyWaypointVisible = false;
    FText ResupplySectorDisplayName;
    float ResupplyDistanceCentimeters = 0.0f;
    float ResupplyDirectionAngleRadians = 0.0f;
    bool bLogisticsWaypointVisible = false;
    FText LogisticsSectorDisplayName;
    float LogisticsDistanceCentimeters = 0.0f;
    float LogisticsDirectionAngleRadians = 0.0f;
    float LogisticsCargoSupply = 0.0f;
    bool bSalvageWaypointVisible = false;
    float SalvageDistanceCentimeters = 0.0f;
    float SalvageDirectionAngleRadians = 0.0f;
    float SalvageSupply = 0.0f;
    bool bConvoyWaypointVisible = false;
    EBHWarFaction ConvoyWaypointOwner =
        EBHWarFaction::Neutral;
    float ConvoyDistanceCentimeters = 0.0f;
    float ConvoyDirectionAngleRadians = 0.0f;
    float ConvoySupplyPayload = 0.0f;
    float ConvoyIntegrityPercentage = 0.0f;
    EBHRouteOperationVariation ConvoyOperationVariation =
        EBHRouteOperationVariation::Standard;
    float ConvoyDeadlineSecondsRemaining = 0.0f;
    bool bTransportWaypointVisible = false;
    bool bTransportWaypointImmobilized = false;
    float TransportWaypointDistanceCentimeters = 0.0f;
    float TransportWaypointDirectionAngleRadians = 0.0f;
    float TransportWaypointFuelPercentage = 1.0f;
    float TransportWaypointHullPercentage = 1.0f;
    bool bVehicleReadinessVisible = false;
    bool bVehicleImmobilized = false;
    float VehicleFuelPercentage = 1.0f;
    float VehicleHullPercentage = 1.0f;
    float VehicleSpeedKPH = 0.0f;
    bool bStrategicSituationVisible = false;
    FText StrategicSectorDisplayName;
    EBHWarFaction StrategicSectorOwner =
        EBHWarFaction::Neutral;
    float StrategicSectorSupply = 0.0f;
    float StrategicSupplyFlowPerTurn = 0.0f;
    float StrategicEnemyResponsePressure = 0.0f;
    float StrategicCivilianSupport = 50.0f;
    int32 StrategicFortificationConstructed = 0;
    int32 StrategicFortificationCapacity = 0;
    float StrategicFortificationCoverage = 0.0f;
    float StrategicFortificationDefenseMultiplier = 1.0f;
    bool bFortificationsConnected = true;
    float StrategicIntelConfidence = 0.0f;
    float ReconMovementProgress = 0.0f;
    float ReconMovementRequired = 1.0f;
    float ReconObservationProgress = 0.0f;
    float ReconObservationRequired = 1.0f;
    float ReconReportCooldownRemaining = 0.0f;
    int32 StrategicWarTurn = 0;
    bool bFieldReconActive = false;
    bool bFieldSquadStatusVisible = false;
    bool bFieldSquadHolding = false;
    bool bFieldSquadEmbarked = false;
    int32 FieldSquadMembersNeedingService = 0;
    int32 FieldSquadMembersRequiringEvacuation = 0;
    int32 LivingFieldSquadOperatives = 0;
    int32 MaximumFieldSquadOperatives = 0;
    float FieldSquadAverageReadiness = 1.0f;
    float FieldSquadLowestReadiness = 1.0f;
    FString FieldSquadContextStatusLine;
};
