#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BHInteractable.h"
#include "BHSectorResupplyStation.generated.h"

class ABHCharacter;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class BROKENHORIZON_API ABHSectorResupplyStation
    : public AActor,
      public IBHInteractable
{
    GENERATED_BODY()

public:
    ABHSectorResupplyStation();

    virtual void Interact_Implementation(
        AActor* InteractingActor
    ) override;

    virtual FText GetInteractionText_Implementation() const override;

    UFUNCTION(
        BlueprintCallable,
        Category = "Persistent War|Logistics"
    )
    void ConfigureStation(FName NewSectorID);

    UFUNCTION(
        BlueprintPure,
        Category = "Persistent War|Logistics"
    )
    FName GetSectorID() const;

    static float CalculateFireteamServiceSupplyCost(
        int32 MembersNeedingService,
        float SupplyCostPerMember
    );

    static bool IsRescueTreatmentDestination(
        FName StationSectorID,
        FName RescueDestinationSectorID
    );

    static FText BuildRescueTreatmentInteractionText(
        bool bAtTreatmentDestination,
        FName CasualtyID,
        const FText& TreatmentDestination
    );

    static bool ShouldIssueEmergencyFallbackKit(
        int32 ReserveAmmo,
        int32 MedkitCount,
        int32 FieldDressingCount
    );

    static int32 CalculateEmergencyFallbackAmmoRequest(
        int32 ReserveAmmo,
        int32 MaximumReserveAmmo,
        int32 FallbackAmmoAmount
    );

    float GetResupplySupplyCost(
        int32 FireteamMembersNeedingService
    ) const;

protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Sector Resupply|Components"
    )
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Sector Resupply|Components"
    )
    TObjectPtr<UStaticMeshComponent> StationMesh;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply"
    )
    FName SectorID = NAME_None;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float ResupplyCooldownSeconds = 60.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply",
        meta = (ClampMin = "0.1")
    )
    float StrategicSupplyCost = 5.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Fireteam",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float FireteamServiceRadius = 1600.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Fireteam",
        meta = (ClampMin = "0.0")
    )
    float FireteamServiceSupplyCostPerMember = 2.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Loadout",
        meta = (ClampMin = "0")
    )
    int32 ReserveAmmoAmount = 90;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Loadout",
        meta = (ClampMin = "0")
    )
    int32 TargetMedkitCount = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Loadout",
        meta = (ClampMin = "0")
    )
    int32 TargetFieldDressingCount = 3;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Loadout",
        meta = (ClampMin = "0")
    )
    int32 TargetFragGrenadeCount = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Loadout",
        meta = (ClampMin = "0")
    )
    int32 TargetEngineeringChargeCount = 2;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Loadout",
        meta = (ClampMin = "0")
    )
    int32 TargetSmokeGrenadeCount = 1;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Emergency Fallback",
        meta = (ClampMin = "0")
    )
    int32 EmergencyFallbackReserveAmmo = 30;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Emergency Fallback",
        meta = (ClampMin = "0")
    )
    int32 EmergencyFallbackMedkits = 1;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Emergency Fallback",
        meta = (ClampMin = "0")
    )
    int32 EmergencyFallbackDressings = 1;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Vehicle",
        meta = (ClampMin = "100.0", Units = "cm")
    )
    float VehicleServiceRadius = 1400.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Sector Resupply|Vehicle",
        meta = (ClampMin = "0.0")
    )
    float VehicleRecoverySupplyCost = 10.0f;

private:
    void ShowUnavailableMessage(
        ABHCharacter* Character,
        const FText& Message
    ) const;

    float NextAvailableUseTime = 0.0f;
};
