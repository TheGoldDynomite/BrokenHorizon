#include "BHAmbientWarDirector.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFirstLightAmbientAudioAssetTest,
    "BrokenHorizon.Presentation.Audio.FirstLightAmbientAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHFirstLightAmbientAudioAssetTest::RunTest(const FString& Parameters)
{
    struct FAudioAssetSpec
    {
        const TCHAR* Path;
        bool bLooping;
        float MinimumDuration;
    };
    const FAudioAssetSpec AssetSpecs[] = {
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_Wind.SW_FirstLight_Wind"), true, 10.0f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_Rain.SW_FirstLight_Rain"), true, 10.0f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_WindRain.SW_FirstLight_WindRain"), true, 10.0f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_DistantWar.SW_FirstLight_DistantWar"), true, 10.0f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_WeaponFire.SW_FirstLight_WeaponFire"), false, 0.2f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_WeaponDry.SW_FirstLight_WeaponDry"), false, 0.1f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_Reload.SW_FirstLight_Reload"), false, 1.0f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_IndoorTail.SW_FirstLight_IndoorTail"), false, 0.5f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_OutdoorTail.SW_FirstLight_OutdoorTail"), false, 0.5f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepConcrete.SW_FirstLight_FootstepConcrete"), false, 0.1f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepDirt.SW_FirstLight_FootstepDirt"), false, 0.1f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepGrass.SW_FirstLight_FootstepGrass"), false, 0.1f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepMetal.SW_FirstLight_FootstepMetal"), false, 0.1f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_FootstepWater.SW_FirstLight_FootstepWater"), false, 0.1f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_NearMiss.SW_FirstLight_NearMiss"), false, 0.2f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_UIConfirm.SW_FirstLight_UIConfirm"), false, 0.1f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_UIWarning.SW_FirstLight_UIWarning"), false, 0.1f},
        {TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_UIAlarm.SW_FirstLight_UIAlarm"), false, 0.2f}
    };

    for (const FAudioAssetSpec& AssetSpec : AssetSpecs)
    {
        USoundBase* Sound = LoadObject<USoundBase>(nullptr, AssetSpec.Path);
        TestNotNull(
            *FString::Printf(TEXT("First Light audio asset loads: %s"), AssetSpec.Path),
            Sound
        );

        const USoundWave* Wave = Cast<USoundWave>(Sound);
        TestNotNull(
            *FString::Printf(TEXT("First Light audio asset is a SoundWave: %s"), AssetSpec.Path),
            Wave
        );
        if (Wave != nullptr)
        {
            TestTrue(
                *FString::Printf(TEXT("First Light audio asset has usable duration: %s"), AssetSpec.Path),
                Wave->Duration >= AssetSpec.MinimumDuration
            );
            TestTrue(
                *FString::Printf(TEXT("First Light audio loop flag matches contract: %s"), AssetSpec.Path),
                Wave->IsLooping() == AssetSpec.bLooping
            );
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHDistantWarWeatherMixTest,
    "BrokenHorizon.Presentation.Audio.DistantEventWeatherMix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHDistantWarWeatherMixTest::RunTest(const FString& Parameters)
{
    const float ClearCombatVolume =
        ABHAmbientWarDirector::CalculateDistantEventVolume(
            EBHAmbientAudioState::Combat,
            0.0f,
            0.0f
        );
    const float SevereWeatherVolume =
        ABHAmbientWarDirector::CalculateDistantEventVolume(
            EBHAmbientAudioState::Combat,
            1.0f,
            1.0f
        );
    const float ClearFrontlineVolume =
        ABHAmbientWarDirector::CalculateDistantEventVolume(
            EBHAmbientAudioState::Frontline,
            0.0f,
            0.0f
        );

    TestTrue(
        TEXT("Clear combat events retain the authored combat level"),
        FMath::IsNearlyEqual(ClearCombatVolume, 0.75f)
    );
    TestTrue(
        TEXT("Severe weather masks distant combat without silencing it"),
        SevereWeatherVolume < ClearCombatVolume &&
        SevereWeatherVolume >= 0.75f * 0.55f
    );
    TestTrue(
        TEXT("Clear frontline events retain the authored frontline level"),
        FMath::IsNearlyEqual(ClearFrontlineVolume, 0.55f)
    );
    return true;
}