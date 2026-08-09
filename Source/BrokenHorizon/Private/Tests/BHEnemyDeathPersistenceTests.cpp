#include "BHEnemySoldier.h"
#include "BHHealthComponent.h"
#include "BHSaveGame.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHEnemyDeathPersistenceContractTest,
    "BrokenHorizon.PersistentWar.EnemyDeathPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHEnemyDeathPersistenceContractTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    const UScriptStruct* DeathState =
        FBHPersistentEnemyDeathSaveState::StaticStruct();
    TestTrue(
        TEXT("Defeated enemy save state is reflected"),
        DeathState != nullptr
    );

    if (DeathState != nullptr)
    {
        TestTrue(
            TEXT("Defeated enemy state stores stable field identity"),
            DeathState->FindPropertyByName(
                TEXT("FieldOperativeID")
            ) != nullptr
        );
        TestTrue(
            TEXT("Defeated enemy state stores the sector"),
            DeathState->FindPropertyByName(TEXT("SectorID")) != nullptr
        );
        TestTrue(
            TEXT("Defeated enemy state stores the last transform"),
            DeathState->FindPropertyByName(TEXT("Transform")) != nullptr
        );
    }

    const FProperty* DefeatedEnemyArray =
        UBHSaveGame::StaticClass()->FindPropertyByName(
            TEXT("DefeatedEnemyStates")
        );
    TestTrue(
        TEXT("Save game exposes defeated enemy states"),
        DefeatedEnemyArray != nullptr &&
            DefeatedEnemyArray->HasAnyPropertyFlags(CPF_SaveGame)
    );

    const UFunction* EnemyRestore =
        ABHEnemySoldier::StaticClass()->FindFunctionByName(
            TEXT("RestorePersistentDeathState")
        );
    TestTrue(
        TEXT("Enemy exposes persistent death restoration"),
        EnemyRestore != nullptr &&
            EnemyRestore->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );

    const UFunction* HealthRestore =
        UBHHealthComponent::StaticClass()->FindFunctionByName(
            TEXT("RestorePersistentDeathState")
        );
    TestTrue(
        TEXT("Health exposes zero-health persistence restoration"),
        HealthRestore != nullptr &&
            HealthRestore->HasAnyFunctionFlags(FUNC_BlueprintCallable)
    );

    return true;
}