#include "BHBattlefieldConditions.h"

#include "BHWarGameState.h"
#include "Engine/World.h"

FBHBattlefieldConditionProfile
UBHBattlefieldConditions::BuildProfileForTurn(int32 TurnNumber)
{
    FBHBattlefieldConditionProfile Profile;
    const int32 SafeTurn = FMath::Max(0, TurnNumber);

    static const EBHBattlefieldWeather WeatherCycle[] = {
        EBHBattlefieldWeather::Clear,
        EBHBattlefieldWeather::Overcast,
        EBHBattlefieldWeather::Rain,
        EBHBattlefieldWeather::Fog,
        EBHBattlefieldWeather::Clear,
        EBHBattlefieldWeather::Storm,
        EBHBattlefieldWeather::Rain,
        EBHBattlefieldWeather::Overcast
    };
    static const EBHDayPhase PhaseCycle[] = {
        EBHDayPhase::Dawn,
        EBHDayPhase::Day,
        EBHDayPhase::Day,
        EBHDayPhase::Dusk,
        EBHDayPhase::Night,
        EBHDayPhase::Night
    };

    Profile.Weather = WeatherCycle[SafeTurn % UE_ARRAY_COUNT(WeatherCycle)];
    Profile.DayPhase = PhaseCycle[SafeTurn % UE_ARRAY_COUNT(PhaseCycle)];

    switch (Profile.Weather)
    {
    case EBHBattlefieldWeather::Overcast:
        Profile.SightRangeMultiplier *= 0.92f;
        Profile.ConditionLabel = TEXT("OVERCAST");
        break;
    case EBHBattlefieldWeather::Rain:
        Profile.SightRangeMultiplier *= 0.78f;
        Profile.MovementNoiseMultiplier *= 0.78f;
        Profile.GunfireNoiseMultiplier *= 0.88f;
        Profile.WeaponSpreadMultiplier *= 1.08f;
        Profile.InfantrySpeedMultiplier *= 0.94f;
        Profile.VehicleTractionMultiplier *= 0.82f;
        Profile.VehicleFuelBurnMultiplier *= 1.10f;
        Profile.MortarDispersionMultiplier *= 1.18f;
        Profile.ConditionLabel = TEXT("RAIN");
        break;
    case EBHBattlefieldWeather::Fog:
        Profile.SightRangeMultiplier *= 0.48f;
        Profile.MovementNoiseMultiplier *= 1.08f;
        Profile.MortarDispersionMultiplier *= 1.28f;
        Profile.ConditionLabel = TEXT("DENSE FOG");
        break;
    case EBHBattlefieldWeather::Storm:
        Profile.SightRangeMultiplier *= 0.58f;
        Profile.MovementNoiseMultiplier *= 0.62f;
        Profile.GunfireNoiseMultiplier *= 0.65f;
        Profile.WeaponSpreadMultiplier *= 1.16f;
        Profile.InfantrySpeedMultiplier *= 0.90f;
        Profile.VehicleTractionMultiplier *= 0.68f;
        Profile.VehicleFuelBurnMultiplier *= 1.18f;
        Profile.MortarDispersionMultiplier *= 1.42f;
        Profile.ConditionLabel = TEXT("SEVERE STORM");
        break;
    default:
        Profile.ConditionLabel = TEXT("CLEAR");
        break;
    }

    const TCHAR* PhaseLabel = TEXT("DAY");
    switch (Profile.DayPhase)
    {
    case EBHDayPhase::Dawn:
        Profile.SightRangeMultiplier *= 0.82f;
        PhaseLabel = TEXT("DAWN");
        break;
    case EBHDayPhase::Dusk:
        Profile.SightRangeMultiplier *= 0.72f;
        Profile.WeaponSpreadMultiplier *= 1.04f;
        PhaseLabel = TEXT("DUSK");
        break;
    case EBHDayPhase::Night:
        Profile.SightRangeMultiplier *= 0.52f;
        Profile.MovementNoiseMultiplier *= 1.12f;
        Profile.WeaponSpreadMultiplier *= 1.10f;
        Profile.MortarDispersionMultiplier *= 1.12f;
        PhaseLabel = TEXT("NIGHT");
        break;
    default:
        break;
    }

    Profile.ConditionLabel = FName(*FString::Printf(
        TEXT("%s // %s"),
        *Profile.ConditionLabel.ToString(),
        PhaseLabel
    ));
    return Profile;
}

FBHBattlefieldConditionProfile
UBHBattlefieldConditions::GetCurrentProfile(const UObject* WorldContextObject)
{
    const UWorld* World = IsValid(WorldContextObject)
        ? WorldContextObject->GetWorld()
        : nullptr;
    const ABHWarGameState* WarGameState = IsValid(World)
        ? World->GetGameState<ABHWarGameState>()
        : nullptr;
    return BuildProfileForTurn(
        IsValid(WarGameState) ? WarGameState->GetReplicatedWarTurn() : 0
    );
}
