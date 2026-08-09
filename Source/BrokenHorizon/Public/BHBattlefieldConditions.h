#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BHBattlefieldConditions.generated.h"

UENUM(BlueprintType)
enum class EBHBattlefieldWeather : uint8
{
    Clear,
    Overcast,
    Rain,
    Fog,
    Storm
};

UENUM(BlueprintType)
enum class EBHDayPhase : uint8
{
    Dawn,
    Day,
    Dusk,
    Night
};

USTRUCT(BlueprintType)
struct BROKENHORIZON_API FBHBattlefieldConditionProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EBHBattlefieldWeather Weather = EBHBattlefieldWeather::Clear;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EBHDayPhase DayPhase = EBHDayPhase::Day;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float SightRangeMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MovementNoiseMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float GunfireNoiseMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ExplosionNoiseMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float WeaponSpreadMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float InfantrySpeedMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float VehicleTractionMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float VehicleFuelBurnMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float MortarDispersionMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName ConditionLabel = TEXT("CLEAR // DAY");
};

UCLASS()
class BROKENHORIZON_API UBHBattlefieldConditions :
    public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Battlefield Conditions")
    static FBHBattlefieldConditionProfile BuildProfileForTurn(int32 TurnNumber);

    UFUNCTION(BlueprintPure, Category = "Battlefield Conditions", meta = (WorldContext = "WorldContextObject"))
    static FBHBattlefieldConditionProfile GetCurrentProfile(const UObject* WorldContextObject);
};
