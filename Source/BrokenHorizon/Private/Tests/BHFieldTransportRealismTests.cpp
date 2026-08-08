#include "BHFieldTransport.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldTransportDamageMobilityTest,
    "BrokenHorizon.Gameplay.Transport.DamageMobility",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldTransportDamageMobilityTest::RunTest(
    const FString& Parameters
)
{
    const float FullMobility =
        ABHFieldTransport::CalculateHullMobilityMultiplier(
            1.0f,
            0.35f,
            0.55f
        );
    const float CriticalMobility =
        ABHFieldTransport::CalculateHullMobilityMultiplier(
            0.35f,
            0.35f,
            0.55f
        );
    const float HalfCriticalMobility =
        ABHFieldTransport::CalculateHullMobilityMultiplier(
            0.175f,
            0.35f,
            0.55f
        );
    const float DisabledMobility =
        ABHFieldTransport::CalculateHullMobilityMultiplier(
            0.0f,
            0.35f,
            0.55f
        );

    TestTrue(
        TEXT("Undamaged transport keeps full mobility"),
        FMath::IsNearlyEqual(FullMobility, 1.0f)
    );
    TestTrue(
        TEXT("Critical-threshold transport keeps full mobility until the threshold"),
        FMath::IsNearlyEqual(CriticalMobility, 1.0f)
    );
    TestTrue(
        TEXT("Damage reduces mobility progressively"),
        FMath::IsNearlyEqual(HalfCriticalMobility, 0.775f)
    );
    TestTrue(
        TEXT("Destroyed transport reaches the configured damaged mobility floor"),
        FMath::IsNearlyEqual(DisabledMobility, 0.55f)
    );

    const float FullFuelBurn =
        ABHFieldTransport::CalculateHullFuelBurnMultiplier(
            1.0f,
            0.35f,
            1.30f
        );
    const float HalfCriticalFuelBurn =
        ABHFieldTransport::CalculateHullFuelBurnMultiplier(
            0.175f,
            0.35f,
            1.30f
        );
    const float CriticalFuelBurn =
        ABHFieldTransport::CalculateHullFuelBurnMultiplier(
            0.0f,
            0.35f,
            1.30f
        );

    TestTrue(
        TEXT("Undamaged transport keeps normal fuel burn"),
        FMath::IsNearlyEqual(FullFuelBurn, 1.0f)
    );
    TestTrue(
        TEXT("Mechanical damage increases fuel burn progressively"),
        FMath::IsNearlyEqual(HalfCriticalFuelBurn, 1.15f)
    );
    TestTrue(
        TEXT("Critical damage reaches the configured fuel-burn penalty"),
        FMath::IsNearlyEqual(CriticalFuelBurn, 1.30f)
    );

    return true;
}

#endif
