#include "BHCoverPoint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHCasualtyCoverHandoffContractTest,
    "BrokenHorizon.Gameplay.AI.CasualtyCoverHandoff",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter
)

bool FBHCasualtyCoverHandoffContractTest::RunTest(
    const FString& Parameters
)
{
    TestFalse(
        TEXT("A healthy claimant keeps the cover reservation"),
        ABHCoverPoint::ShouldReleaseClaimForCasualty(
            false,
            false,
            false
        )
    );
    TestTrue(
        TEXT("A dead claimant releases the cover reservation"),
        ABHCoverPoint::ShouldReleaseClaimForCasualty(
            true,
            false,
            false
        )
    );
    TestTrue(
        TEXT("An incapacitated claimant releases the cover reservation"),
        ABHCoverPoint::ShouldReleaseClaimForCasualty(
            false,
            true,
            false
        )
    );
    TestTrue(
        TEXT("A stabilized evacuation claimant releases the cover reservation"),
        ABHCoverPoint::ShouldReleaseClaimForCasualty(
            false,
            false,
            true
        )
    );
    TestTrue(
        TEXT("Any combined casualty state releases the cover reservation"),
        ABHCoverPoint::ShouldReleaseClaimForCasualty(
            true,
            true,
            true
        )
    );
    return true;
}