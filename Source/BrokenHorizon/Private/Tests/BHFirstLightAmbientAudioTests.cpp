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
    const TCHAR* AssetPaths[] = {
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_Wind.SW_FirstLight_Wind"),
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_Rain.SW_FirstLight_Rain"),
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_WindRain.SW_FirstLight_WindRain"),
        TEXT("/Game/BrokenHorizon/Audio/SW_FirstLight_DistantWar.SW_FirstLight_DistantWar")
    };

    for (const TCHAR* AssetPath : AssetPaths)
    {
        USoundBase* Sound = LoadObject<USoundBase>(nullptr, AssetPath);
        TestNotNull(
            *FString::Printf(TEXT("Ambient asset loads: %s"), AssetPath),
            Sound
        );

        const USoundWave* Wave = Cast<USoundWave>(Sound);
        TestNotNull(
            *FString::Printf(TEXT("Ambient asset is a SoundWave: %s"), AssetPath),
            Wave
        );
        if (Wave != nullptr)
        {
            TestTrue(
                *FString::Printf(TEXT("Ambient asset has usable duration: %s"), AssetPath),
                Wave->Duration >= 10.0f
            );
            TestTrue(
                *FString::Printf(TEXT("Ambient asset is configured to loop: %s"), AssetPath),
                Wave->IsLooping()
            );
        }
    }
    return true;
}
