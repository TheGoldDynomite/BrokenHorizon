#pragma once

#include "CoreMinimal.h"
#include "BHInteractable.h"
#include "BHWarTypes.h"
#include "GameFramework/Pawn.h"
#include "BHFieldTransport.generated.h"

class ABHCharacter;
class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class BROKENHORIZON_API ABHFieldTransport
    : public APawn,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHFieldTransport();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void Tick(float DeltaTime) override;

    virtual float TakeDamage(
        float DamageAmount,
        struct FDamageEvent const& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    static float CalculatePassengerDamage(
        float VehicleDamage,
        float CrewFraction,
        float PassengerMultiplier
    );

    static float CalculateHullMobilityMultiplier(
        float HullFraction,
        float CriticalHullFraction,
        float MinimumMobilityMultiplier
    );

    static float CalculateHullFuelBurnMultiplier(
        float HullFraction,
        float CriticalHullFraction,
        float CriticalFuelBurnMultiplier
    );

    static bool ShouldRestoreFieldSquadPassengers(
        bool bRestoreDriver,
        bool bHasExplicitPassengerState,
        bool bSavedFieldSquadEmbarked,
        FName SavedFieldSquadTransportID,
        FName CandidateTransportID
    );

    FName GetPersistenceID() const;
#if !UE_BUILD_SHIPPING
    void SetPersistenceIDForTesting(FName InPersistenceID);
#endif
    ABHCharacter* GetOccupant() const;
    void ForceExitOccupantForRespawn(ABHCharacter* Character);
    void PrepareOccupantForServerTravel(ABHCharacter* Character);

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Readiness"
    )
    float GetFuelPercentage() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Readiness"
    )
    float GetHullPercentage() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Readiness"
    )
    float GetSpeedKPH() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Readiness"
    )
    float GetEstimatedRangeKilometers() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Readiness"
    )
    float GetEstimatedTravelMinutes(
        float DistanceCentimeters
    ) const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Readiness"
    )
    bool NeedsService() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Readiness"
    )
    bool IsImmobilized() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Logistics"
    )
    float GetCargoSupply() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Logistics"
    )
    float GetCargoCapacity() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Logistics"
    )
    FName GetCargoSourceSectorID() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Logistics"
    )
    FName GetCargoDestinationSectorID() const;

    UFUNCTION(
        BlueprintPure,
        Category = "Field Transport|Logistics"
    )
    EBHWarConvoyCargoType GetCargoType() const;

    FName RefreshMilitaryCargoDestination();
    FName RefreshCivilianAidDestination();

    UFUNCTION(
        BlueprintCallable,
        Category = "Field Transport|Readiness"
    )
    bool ServiceVehicle();

    UFUNCTION(
        BlueprintCallable,
        Category = "Field Transport|Logistics"
    )
    float LoadRecoveredMilitarySupply(
        float AvailableSupply,
        FName RecoverySourceSectorID
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Field Transport|Readiness"
    )
    bool RecoverAndService(
        const FTransform& RecoveryTransform
    );

    void RestorePersistentState(
        const FTransform& SavedTransform,
        ABHCharacter* PlayerCharacter,
        bool bRestoreDriver,
        float SavedFuelFraction,
        float SavedHullFraction,
        float SavedCargoSupply,
        FName SavedCargoSourceSectorID,
        FName SavedCargoDestinationSectorID = NAME_None,
        EBHWarConvoyCargoType SavedCargoType =
            EBHWarConvoyCargoType::MilitarySupply,
        bool bRestoreFieldSquadPassengers = true,
        bool bUseSavedFieldSquadPassengerManifest = false
    );

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation()
        const override;

protected:
    virtual void BeginPlay() override;

private:
    void EnterVehicle(
        ABHCharacter* Character,
        bool bBoardFieldSquad = true,
        bool bUseSavedFieldSquadPassengerManifest = false
    );
    void ExitVehicle();
    bool FindSafeExitLocation(
        ABHCharacter* Character,
        FVector& OutExitLocation,
        FRotator& OutExitRotation
    ) const;
    void UpdateDriverInput(float DeltaTime);
    void UpdateMovement(float DeltaTime);
    float GetCargoLoadFraction() const;
    float GetCargoSpeedMultiplier() const;
    float GetCargoFuelBurnMultiplier() const;
    float GetHullMobilityMultiplier() const;
    float GetHullFuelBurnMultiplier() const;
    void UpdateGroundPosition(float DeltaTime);
    void UpdateTransportLabel();
    void TryTransferFieldLogistics();
    void TryTransferCivilianAid();
    void NotifyDriver(const FString& Message) const;

    UFUNCTION(Server, Unreliable)
    void ServerSubmitDriverInput(
        float NewThrottleInput,
        float NewSteeringInput,
        bool bNewBrakeInput,
        bool bNewBoostInput
    );

    UFUNCTION(Server, Reliable)
    void ServerRequestExitVehicle();

    UFUNCTION(Server, Reliable)
    void ServerRequestFieldLogisticsTransfer();

    UFUNCTION(Server, Reliable)
    void ServerRequestCivilianAidTransfer();

    UFUNCTION(Client, Reliable)
    void ClientActivateDriverControls();

    UFUNCTION()
    void OnRep_TransportState();

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Field Transport|Persistence",
        meta = (AllowPrivateAccess = "true")
    )
    FName PersistenceID = TEXT("WesternFOBFieldTransport01");

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UBoxComponent> VehicleCollision;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UStaticMeshComponent> ChassisMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UStaticMeshComponent> CabMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UStaticMeshComponent> HoodMesh;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UCameraComponent> VehicleCamera;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UTextRenderComponent> TransportLabel;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Handling",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "100.0",
            Units = "cm/s"
        )
    )
    float MaximumForwardSpeed = 2800.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Handling",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "100.0",
            Units = "cm/s"
        )
    )
    float BoostForwardSpeed = 3600.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Handling",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "100.0",
            Units = "cm/s"
        )
    )
    float MaximumReverseSpeed = 1200.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Handling",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "1.0"
        )
    )
    float AccelerationResponse = 2.4f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Handling",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "1.0",
            Units = "deg/s"
        )
    )
    float SteeringRate = 72.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Handling",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "10.0",
            Units = "cm"
        )
    )
    float GroundClearance = 72.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Protection",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.0",
            ClampMax = "1.0"
        )
    )
    float CrewDamageFraction = 0.35f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Protection",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.0",
            ClampMax = "1.0"
        )
    )
    float PassengerDamageMultiplier = 0.65f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Readiness",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "1.0"
        )
    )
    float MaximumFuel = 100.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Readiness",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.01"
        )
    )
    float FuelBurnPerKilometer = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Readiness",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "1.0"
        )
    )
    float MaximumHull = 500.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Readiness",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.05",
            ClampMax = "0.95"
        )
    )
    float CriticalHullFraction = 0.35f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Handling",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.10",
            ClampMax = "1.0"
        )
    )
    float MinimumDamagedSpeedMultiplier = 0.55f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Readiness",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "1.0",
            ClampMax = "3.0"
        )
    )
    float CriticalHullFuelBurnMultiplier = 1.30f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Logistics",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "1.0"
        )
    )
    float MaximumCargoSupply = 15.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Handling",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.1",
            ClampMax = "1.0"
        )
    )
    float LoadedSpeedMultiplierAtCapacity = 0.90f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Readiness",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "1.0"
        )
    )
    float LoadedFuelBurnMultiplierAtCapacity = 1.25f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Logistics",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "100.0",
            Units = "cm"
        )
    )
    float LogisticsStationRadius = 1600.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Field Transport|Safety",
        meta = (
            AllowPrivateAccess = "true",
            ClampMin = "0.0",
            Units = "cm/s"
        )
    )
    float MaximumSafeExitSpeed = 300.0f;

    UPROPERTY(Replicated, Transient)
    TObjectPtr<ABHCharacter> Occupant;

    UPROPERTY(Replicated, Transient)
    float CurrentSpeed = 0.0f;
    float ThrottleInput = 0.0f;
    float SteeringInput = 0.0f;
    bool bBrakeInput = false;
    bool bBoostInput = false;
    bool bExitInputArmed = false;
    float DriverInputStaleSeconds = 0.0f;
    UPROPERTY(ReplicatedUsing = OnRep_TransportState)
    float CurrentFuel = 100.0f;

    UPROPERTY(ReplicatedUsing = OnRep_TransportState)
    float CurrentHull = 500.0f;

    UPROPERTY(ReplicatedUsing = OnRep_TransportState)
    float CurrentCargoSupply = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_TransportState)
    FName CargoSourceSectorID = NAME_None;

    UPROPERTY(ReplicatedUsing = OnRep_TransportState)
    FName CargoDestinationSectorID = NAME_None;

    UPROPERTY(ReplicatedUsing = OnRep_TransportState)
    EBHWarConvoyCargoType CargoType =
        EBHWarConvoyCargoType::MilitarySupply;
    bool bLowFuelWarningIssued = false;
    bool bCriticalFuelWarningIssued = false;
    bool bOutOfFuelWarningIssued = false;
    bool bHullDisabledWarningIssued = false;
};
