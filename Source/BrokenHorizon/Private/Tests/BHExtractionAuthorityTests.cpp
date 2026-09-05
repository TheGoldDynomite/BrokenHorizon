#include "BHExtractionZone.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHExtractionReadinessPolicyTest,
    "BrokenHorizon.Gameplay.Extraction.ReadinessPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHExtractionReadinessPolicyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FName RequiredObjectiveID(TEXT("EliminateGuard"));
    const FName ExtractionObjectiveID(TEXT("ReachExtraction"));
    const FName WrongObjectiveID(TEXT("UnlockSecurityDoor"));
    const TArray<FName> CompletedObjectives = { RequiredObjectiveID };
    const TArray<FName> MissingRequiredObjective;

    TestTrue(
        TEXT("Extraction accepts the required completed objective"),
        ABHExtractionZone::CanCompleteExtraction(
            RequiredObjectiveID,
            ExtractionObjectiveID,
            ExtractionObjectiveID,
            CompletedObjectives
        )
    );
    TestFalse(
        TEXT("Extraction rejects a missing required objective"),
        ABHExtractionZone::CanCompleteExtraction(
            RequiredObjectiveID,
            ExtractionObjectiveID,
            ExtractionObjectiveID,
            MissingRequiredObjective
        )
    );
    TestFalse(
        TEXT("Extraction rejects the wrong current objective"),
        ABHExtractionZone::CanCompleteExtraction(
            RequiredObjectiveID,
            ExtractionObjectiveID,
            WrongObjectiveID,
            CompletedObjectives
        )
    );
    TestFalse(
        TEXT("Extraction rejects an unset extraction objective"),
        ABHExtractionZone::CanCompleteExtraction(
            RequiredObjectiveID,
            NAME_None,
            NAME_None,
            CompletedObjectives
        )
    );
    TestTrue(
        TEXT("Extraction accepts an optional prerequisite"),
        ABHExtractionZone::CanCompleteExtraction(
            NAME_None,
            ExtractionObjectiveID,
            ExtractionObjectiveID,
            MissingRequiredObjective
        )
    );

    return true;
}
