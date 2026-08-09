#include "BHRifle.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHReloadInterruptionPresentationContractTest,
    "BrokenHorizon.Gameplay.Combat.ReloadInterruptionPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHReloadInterruptionPresentationContractTest::RunTest(const FString& Parameters)
{
    const UFunction* CancelReloadPresentation = ABHRifle::StaticClass()->FindFunctionByName(TEXT("CancelReloadPresentation"));
    TestTrue(TEXT("ABHRifle exposes reload presentation cancellation"), CancelReloadPresentation != nullptr);

    if (CancelReloadPresentation)
    {
        TestTrue(TEXT("Reload presentation cancellation is Blueprint-callable"), CancelReloadPresentation->HasAnyFunctionFlags(FUNC_BlueprintCallable));
    }

    return true;
}