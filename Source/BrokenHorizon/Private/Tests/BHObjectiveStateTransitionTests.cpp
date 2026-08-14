#include "BHObjectiveComponent.h"
#include "BHMissionData.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace
{
FBHObjectiveDefinition MakeObjectiveDefinition(const TCHAR* ObjectiveID)
{
    FBHObjectiveDefinition Definition;
    Definition.ObjectiveID = FName(ObjectiveID);
    Definition.DisplayText = FText::FromName(Definition.ObjectiveID);
    return Definition;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHObjectiveFailureStateTest,
    "BrokenHorizon.Gameplay.Objectives.FailureState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHObjectiveFailureStateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FName FirstObjectiveID(TEXT("FailureState_First"));

    TArray<FBHObjectiveDefinition> Definitions;
    Definitions.Add(MakeObjectiveDefinition(TEXT("FailureState_First")));
    Definitions.Add(MakeObjectiveDefinition(TEXT("FailureState_Second")));

    UBHObjectiveComponent* Objectives =
        NewObject<UBHObjectiveComponent>(GetTransientPackage());
    TestNotNull(TEXT("Objective component is created"), Objectives);

    if (Objectives)
    {
        Objectives->StartRuntimeMission(Definitions);

        TestTrue(
            TEXT("Active runtime mission can fail"),
            Objectives->FailMission()
        );
        TestTrue(
            TEXT("Failed mission state is recorded"),
            Objectives->IsMissionFailed()
        );
        TestTrue(
            TEXT("Failure clears the current objective"),
            Objectives->GetCurrentObjectiveID().IsNone()
        );
        TestFalse(
            TEXT("Failure does not complete the mission"),
            Objectives->IsMissionComplete()
        );
        TestFalse(
            TEXT("A failed mission rejects a second failure"),
            Objectives->FailMission()
        );
        TestFalse(
            TEXT("A failed mission rejects later completion"),
            Objectives->CompleteObjectiveByID(FirstObjectiveID)
        );
        TestEqual(
            TEXT("Failed mission leaves completed objective IDs empty"),
            Objectives->GetCompletedObjectiveIDs().Num(),
            0
        );
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHObjectiveRuntimeRestoreTest,
    "BrokenHorizon.Gameplay.Objectives.RuntimeRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHObjectiveRuntimeRestoreTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FName FirstObjectiveID(TEXT("RuntimeRestore_First"));
    const FName SecondObjectiveID(TEXT("RuntimeRestore_Second"));
    const FName ThirdObjectiveID(TEXT("RuntimeRestore_Third"));
    const FName UnknownObjectiveID(TEXT("RuntimeRestore_Unknown"));

    TArray<FBHObjectiveDefinition> Definitions;
    Definitions.Add(MakeObjectiveDefinition(TEXT("RuntimeRestore_First")));
    Definitions.Add(MakeObjectiveDefinition(TEXT("RuntimeRestore_Second")));
    Definitions.Add(MakeObjectiveDefinition(TEXT("RuntimeRestore_Third")));

    UBHObjectiveComponent* Objectives =
        NewObject<UBHObjectiveComponent>(GetTransientPackage());
    TestNotNull(TEXT("Objective component is created"), Objectives);

    if (Objectives)
    {
        const TArray<FName> SavedCompletedObjectiveIDs = {
            FirstObjectiveID,
            FirstObjectiveID,
            NAME_None,
            UnknownObjectiveID
        };

        TestTrue(
            TEXT("Runtime mission state restore succeeds"),
            Objectives->RestoreRuntimeMissionState(
                Definitions,
                NAME_None,
                SavedCompletedObjectiveIDs,
                false,
                false
            )
        );

        const TArray<FName> RestoredCompletedObjectiveIDs =
            Objectives->GetCompletedObjectiveIDs();
        TestEqual(
            TEXT("Only one completed objective ID is restored"),
            RestoredCompletedObjectiveIDs.Num(),
            1
        );
        TestTrue(
            TEXT("The valid completed objective ID is restored"),
            RestoredCompletedObjectiveIDs.Contains(FirstObjectiveID)
        );
        TestEqual(
            TEXT("The first incomplete objective becomes current"),
            Objectives->GetCurrentObjectiveID(),
            SecondObjectiveID
        );
        TestFalse(
            TEXT("Restored incomplete mission is not complete"),
            Objectives->IsMissionComplete()
        );
        TestFalse(
            TEXT("Restored incomplete mission is not failed"),
            Objectives->IsMissionFailed()
        );

        TestTrue(
            TEXT("Runtime failed mission state restore succeeds"),
            Objectives->RestoreRuntimeMissionState(
                Definitions,
                ThirdObjectiveID,
                RestoredCompletedObjectiveIDs,
                false,
                true
            )
        );
        TestTrue(
            TEXT("Failed restore clears the current objective"),
            Objectives->GetCurrentObjectiveID().IsNone()
        );
        TestTrue(
            TEXT("Failed restore sets failed state"),
            Objectives->IsMissionFailed()
        );
        TestFalse(
            TEXT("Failed restore does not mark the mission complete"),
            Objectives->IsMissionComplete()
        );
    }

    return true;
}
