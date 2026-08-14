#include "BHFieldTransport.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHFieldTransportHandlingContractTest,
    "BrokenHorizon.Gameplay.Logistics.TransportHandling",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHFieldTransportHandlingContractTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;
    const float EmptyLoad =
        ABHFieldTransport::CalculateCargoControlMultiplier(0.0f, 0.82f);
    const float HalfLoad =
        ABHFieldTransport::CalculateCargoControlMultiplier(0.5f, 0.82f);
    const float FullLoad =
        ABHFieldTransport::CalculateCargoControlMultiplier(1.0f, 0.82f);
    const float ClampedLoad =
        ABHFieldTransport::CalculateCargoControlMultiplier(2.0f, 0.82f);
    const float ClampedTuning =
        ABHFieldTransport::CalculateCargoControlMultiplier(1.0f, -1.0f);

    TestTrue(
        TEXT("Empty cargo preserves full control response"),
        FMath::IsNearlyEqual(EmptyLoad, 1.0f)
    );
    TestTrue(
        TEXT("Cargo control response degrades gradually"),
        EmptyLoad > HalfLoad && HalfLoad > FullLoad
    );
    TestTrue(
        TEXT("Full cargo uses the authorable capacity response"),
        FMath::IsNearlyEqual(FullLoad, 0.82f)
    );
    TestTrue(
        TEXT("Cargo load fraction clamps above capacity"),
        FMath::IsNearlyEqual(ClampedLoad, FullLoad)
    );
    TestTrue(
        TEXT("Capacity tuning stays above the safe control floor"),
        ClampedTuning >= 0.25f && ClampedTuning <= 1.0f
    );

    TestTrue(
        TEXT("Land transport keeps its configured speed response"),
        FMath::IsNearlyEqual(
            ABHFieldTransport::CalculateWaterSpeedMultiplier(false, 0.72f),
            1.0f
        )
    );
    TestTrue(
        TEXT("Waterborne transport applies its handling multiplier"),
        FMath::IsNearlyEqual(
            ABHFieldTransport::CalculateWaterSpeedMultiplier(true, 0.72f),
            0.72f
        )
    );
    TestTrue(
        TEXT("Water handling stays inside safe tuning bounds"),
        FMath::IsNearlyEqual(
            ABHFieldTransport::CalculateWaterSpeedMultiplier(true, -2.0f),
            0.1f
        )
    );
    return true;
}
