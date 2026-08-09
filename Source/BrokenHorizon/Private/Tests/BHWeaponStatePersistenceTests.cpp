#include "BHSaveGame.h"
#include "BHWeaponComponent.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBHWeaponStatePersistenceContractTest,
    "BrokenHorizon.PersistentWar.WeaponStatePersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FBHWeaponStatePersistenceContractTest::RunTest(const FString& Parameters)
{
    const UClass* SaveClass = UBHSaveGame::StaticClass();
    const FProperty* SavedHeatProperty = FindFProperty<FProperty>(
        SaveClass,
        FName(TEXT("SavedWeaponHeatNormalized"))
    );
    const FProperty* SavedOverheatProperty = FindFProperty<FProperty>(
        SaveClass,
        FName(TEXT("bSavedWeaponOverheated"))
    );
    const FProperty* SavedHeatValidityProperty = FindFProperty<FProperty>(
        SaveClass,
        FName(TEXT("bSavedWeaponHeatStateValid"))
    );
    const FProperty* SavedFireModeProperty = FindFProperty<FProperty>(
        SaveClass,
        FName(TEXT("SavedFireMode"))
    );
    const FProperty* SavedFireModeValidityProperty = FindFProperty<FProperty>(
        SaveClass,
        FName(TEXT("bSavedFireModeStateValid"))
    );

    TestTrue(
        TEXT("Saved weapon heat is a SaveGame field"),
        SavedHeatProperty && SavedHeatProperty->HasAnyPropertyFlags(CPF_SaveGame)
    );
    TestTrue(
        TEXT("Saved overheat lockout is a SaveGame field"),
        SavedOverheatProperty && SavedOverheatProperty->HasAnyPropertyFlags(CPF_SaveGame)
    );
    TestTrue(
        TEXT("Saved heat validity is a SaveGame field"),
        SavedHeatValidityProperty && SavedHeatValidityProperty->HasAnyPropertyFlags(CPF_SaveGame)
    );
    TestTrue(
        TEXT("Saved fire mode is a SaveGame field"),
        SavedFireModeProperty && SavedFireModeProperty->HasAnyPropertyFlags(CPF_SaveGame)
    );
    TestTrue(
        TEXT("Saved fire mode validity is a SaveGame field"),
        SavedFireModeValidityProperty && SavedFireModeValidityProperty->HasAnyPropertyFlags(CPF_SaveGame)
    );

    const UClass* WeaponClass = UBHWeaponComponent::StaticClass();
    const FProperty* WeaponHeatProperty = FindFProperty<FProperty>(
        WeaponClass,
        FName(TEXT("WeaponHeatNormalized"))
    );
    const FProperty* WeaponOverheatProperty = FindFProperty<FProperty>(
        WeaponClass,
        FName(TEXT("bWeaponOverheated"))
    );
    const FProperty* FireModeProperty = FindFProperty<FProperty>(
        WeaponClass,
        FName(TEXT("FireMode"))
    );

    TestTrue(
        TEXT("Weapon heat replicates"),
        WeaponHeatProperty && WeaponHeatProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Overheat lockout replicates"),
        WeaponOverheatProperty && WeaponOverheatProperty->HasAnyPropertyFlags(CPF_Net)
    );
    TestTrue(
        TEXT("Fire mode replicates"),
        FireModeProperty && FireModeProperty->HasAnyPropertyFlags(CPF_Net)
    );

    const UEnum* FireModeEnum = StaticEnum<EBHFireMode>();
    TestTrue(
        TEXT("Semi-automatic fire mode remains a reflected option"),
        FireModeEnum &&
            FireModeEnum->GetValueByNameString(TEXT("SemiAutomatic")) != INDEX_NONE
    );
    TestTrue(
        TEXT("Automatic fire mode remains a reflected option"),
        FireModeEnum &&
            FireModeEnum->GetValueByNameString(TEXT("Automatic")) != INDEX_NONE
    );

    TestTrue(
        TEXT("Heat accumulation clamps at full heat"),
        FMath::IsNearlyEqual(
            UBHWeaponComponent::CalculateHeatAfterShot(0.98f, 0.10f),
            1.0f
        )
    );
    TestTrue(
        TEXT("Heat spread rises with accumulated heat"),
        UBHWeaponComponent::CalculateHeatSpreadMultiplier(0.90f) >
            UBHWeaponComponent::CalculateHeatSpreadMultiplier(0.20f)
    );

    UBHSaveGame* DefaultSave = NewObject<UBHSaveGame>();
    TestTrue(TEXT("Weapon persistence uses the current save schema"), DefaultSave != nullptr);
    if (DefaultSave)
    {
        TestEqual(
            TEXT("Current save schema includes weapon persistence"),
            DefaultSave->SchemaVersion,
            BHSave::CurrentSchemaVersion
        );
        TestFalse(
            TEXT("Legacy weapon heat is not treated as valid without its validity bit"),
            DefaultSave->bSavedWeaponHeatStateValid
        );
        TestFalse(
            TEXT("Legacy fire mode is not treated as valid without its validity bit"),
            DefaultSave->bSavedFireModeStateValid
        );
    }

    return true;
}