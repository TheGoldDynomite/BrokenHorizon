#pragma once

#include "CoreMinimal.h"
#include "BHWeaponComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BHLoadoutWeight.generated.h"

UENUM(BlueprintType)
enum class EBHCarryLoadState : uint8 { FightingLoad, Heavy, Overloaded };

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHCarryLoadProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TotalKilograms = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ContainerCapacityKilograms = 40.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ContainerRemainingKilograms = 40.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EBHCarryLoadState State = EBHCarryLoadState::FightingLoad;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float MovementSpeedMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float StaminaDrainMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float StaminaRecoveryMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TraversalCostMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float MovementNoiseMultiplier = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float WeaponSpreadMultiplier = 1.0f;
};

UCLASS()
class BROKENHORIZON_API UBHLoadoutWeight : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category = "Loadout|Weight")
    static FBHCarryLoadProfile BuildCarryLoadProfile(
        EBHWeaponRole WeaponRole, int32 MagazineRounds, int32 ReserveRounds,
        int32 FragGrenades, int32 SmokeGrenades, int32 EngineeringCharges,
        int32 Medkits, int32 FieldDressings);
};
