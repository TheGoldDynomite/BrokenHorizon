#include "BHLoadoutWeight.h"

FBHCarryLoadProfile UBHLoadoutWeight::BuildCarryLoadProfile(
    EBHWeaponRole WeaponRole,
    int32 MagazineRounds,
    int32 ReserveRounds,
    int32 FragGrenades,
    int32 SmokeGrenades,
    int32 EngineeringCharges,
    int32 Medkits,
    int32 FieldDressings
)
{
    float WeaponKilograms = 4.1f;
    float RoundKilograms = 0.012f;
    if (WeaponRole == EBHWeaponRole::Marksman)
    {
        WeaponKilograms = 5.6f;
        RoundKilograms = 0.022f;
    }
    else if (WeaponRole == EBHWeaponRole::Support)
    {
        WeaponKilograms = 8.2f;
        RoundKilograms = 0.013f;
    }

    FBHCarryLoadProfile Profile;
    Profile.TotalKilograms = 16.0f + WeaponKilograms +
        FMath::Max(0, MagazineRounds + ReserveRounds) * RoundKilograms +
        FMath::Max(0, FragGrenades) * 0.40f +
        FMath::Max(0, SmokeGrenades) * 0.30f +
        FMath::Max(0, EngineeringCharges) * 1.80f +
        FMath::Max(0, Medkits) * 0.75f +
        FMath::Max(0, FieldDressings) * 0.20f;
    const float BurdenAlpha = FMath::GetMappedRangeValueClamped(
        FVector2D(24.0f, 40.0f), FVector2D(0.0f, 1.0f),
        Profile.TotalKilograms);
    Profile.State = Profile.TotalKilograms >= 36.0f
        ? EBHCarryLoadState::Overloaded
        : Profile.TotalKilograms >= 28.0f
            ? EBHCarryLoadState::Heavy : EBHCarryLoadState::FightingLoad;
    Profile.MovementSpeedMultiplier = FMath::Lerp(1.0f, 0.72f, BurdenAlpha);
    Profile.StaminaDrainMultiplier = FMath::Lerp(1.0f, 1.70f, BurdenAlpha);
    Profile.StaminaRecoveryMultiplier = FMath::Lerp(1.0f, 0.65f, BurdenAlpha);
    Profile.TraversalCostMultiplier = FMath::Lerp(1.0f, 1.35f, BurdenAlpha);
    Profile.MovementNoiseMultiplier = FMath::Lerp(1.0f, 1.25f, BurdenAlpha);
    Profile.WeaponSpreadMultiplier = FMath::Lerp(1.0f, 1.10f, BurdenAlpha);
    return Profile;
}
