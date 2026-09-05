#include "BHOpenWorldOperationDirector.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHOperationSnapshotIdentityTest,
    "BrokenHorizon.Persistence.OperationSnapshot.Identity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHOperationSnapshotIdentityTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    const FName OperationA(TEXT("Operation_A"));
    const FName OperationB(TEXT("Operation_B"));

    TestTrue(
        TEXT("A snapshot restores for the same committed operation"),
        ABHOpenWorldOperationDirector::IsOperationSnapshotCompatible(
            OperationA,
            OperationA
        )
    );
    TestFalse(
        TEXT("A snapshot is rejected for a different committed operation"),
        ABHOpenWorldOperationDirector::IsOperationSnapshotCompatible(
            OperationA,
            OperationB
        )
    );
    TestFalse(
        TEXT("A snapshot is rejected when no operation is committed"),
        ABHOpenWorldOperationDirector::IsOperationSnapshotCompatible(
            OperationA,
            NAME_None
        )
    );
    TestTrue(
        TEXT("Legacy snapshots without an identity remain compatible"),
        ABHOpenWorldOperationDirector::IsOperationSnapshotCompatible(
            NAME_None,
            OperationB
        )
    );

    return true;
}
