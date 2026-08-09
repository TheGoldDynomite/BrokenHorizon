#include "BHSupplyConvoyTarget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHSupplyConvoyDamageSpeedTest,
    "BrokenHorizon.Gameplay.Convoy.DamageSpeed",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHSupplyConvoyDamageSpeedTest::RunTest(
    const FString& Parameters
)
{
    const float FullSpeed =
        ABHSupplyConvoyTarget::CalculateRouteSpeedMultiplier(
            1.0f,
            0.40f,
            0.45f
        );
    const float ThresholdSpeed =
        ABHSupplyConvoyTarget::CalculateRouteSpeedMultiplier(
            0.40f,
            0.40f,
            0.45f
        );
    const float HalfCriticalSpeed =
        ABHSupplyConvoyTarget::CalculateRouteSpeedMultiplier(
            0.20f,
            0.40f,
            0.45f
        );
    const float CriticalSpeed =
        ABHSupplyConvoyTarget::CalculateRouteSpeedMultiplier(
            0.0f,
            0.40f,
            0.45f
        );

    TestTrue(
        TEXT("Healthy convoy keeps full route speed"),
        FMath::IsNearlyEqual(FullSpeed, 1.0f)
    );
    TestTrue(
        TEXT("Critical-threshold convoy keeps full route speed until the threshold"),
        FMath::IsNearlyEqual(ThresholdSpeed, 1.0f)
    );
    TestTrue(
        TEXT("Damaged convoy route speed degrades progressively"),
        FMath::IsNearlyEqual(HalfCriticalSpeed, 0.725f)
    );
    TestTrue(
        TEXT("Critical convoy damage reaches the configured speed floor"),
        FMath::IsNearlyEqual(CriticalSpeed, 0.45f)
    );

    TestTrue(
        TEXT("Undamaged convoy preserves the full recovery fraction"),
        FMath::IsNearlyEqual(
            ABHSupplyConvoyTarget::CalculateDamageAdjustedRecoverableSupply(
                100.0f,
                0.60f,
                1.0f,
                0.50f
            ),
            60.0f
        )
    );
    TestTrue(
        TEXT("Partially damaged convoy loses recoverable cargo"),
        FMath::IsNearlyEqual(
            ABHSupplyConvoyTarget::CalculateDamageAdjustedRecoverableSupply(
                100.0f,
                0.60f,
                0.50f,
                0.50f
            ),
            45.0f
        )
    );
    TestTrue(
        TEXT("Destroyed convoy keeps only the configured minimum cargo condition"),
        FMath::IsNearlyEqual(
            ABHSupplyConvoyTarget::CalculateDamageAdjustedRecoverableSupply(
                100.0f,
                0.60f,
                0.0f,
                0.50f
            ),
            30.0f
        )
    );

    return true;
}

#endif
