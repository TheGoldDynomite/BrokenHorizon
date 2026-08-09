#include "BHEnemySoldier.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEnemyDeathStateContractTest,
    "BrokenHorizon.PersistentWar.EnemyDeathStateContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHEnemyDeathStateContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const UClass* EnemyClass = ABHEnemySoldier::StaticClass();
    const TCHAR* ReplicatedStateNames[] = {
        TEXT("bIncapacitated"),
        TEXT("bRequiresMedicalEvacuation"),
        TEXT("IncapacitationSecondsRemaining"),
        TEXT("bSurrendered"),
        TEXT("SurrenderEscapeSecondsRemaining"),
        TEXT("bSurrenderSecured")
    };

    for (const TCHAR* StateName : ReplicatedStateNames)
    {
        const FProperty* StateProperty = FindFProperty<FProperty>(
            EnemyClass,
            FName(StateName)
        );
        TestTrue(
            FString::Printf(
                TEXT("Terminal death state exposes replicated field %s"),
                StateName
            ),
            StateProperty && StateProperty->HasAnyPropertyFlags(CPF_Net)
        );
    }

    const UFunction* RestoreFunction = EnemyClass->FindFunctionByName(
        FName(TEXT("RestorePersistentDeathState"))
    );
    TestTrue(
        TEXT("Persistent death restore remains Blueprint-callable"),
        RestoreFunction &&
            RestoreFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );

    return true;
}