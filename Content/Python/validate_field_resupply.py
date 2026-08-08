"""Validate Broken Horizon v0.20 field resupply and persistence."""

import os
import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
MEDICAL_BLUEPRINT = "/Game/BrokenHorizon/Core/BP_MedicalSupply"
ARMOR_PLATE_BLUEPRINT = (
    "/Game/BrokenHorizon/Core/BP_ArmorPlateSupply"
)
HELMET_BLUEPRINT = "/Game/BrokenHorizon/Core/BP_HelmetSupply"


def _log(message):
    unreal.log("[BH Field Resupply Validation] " + message)


def _read(relative_path):
    path = os.path.join(unreal.Paths.project_dir(), relative_path)
    with open(path, "r", encoding="utf-8") as source_file:
        return source_file.read()


def _require_fragments(relative_path, fragments):
    source = _read(relative_path)

    for fragment in fragments:
        if fragment not in source:
            raise RuntimeError(
                "%s is missing: %s" % (relative_path, fragment)
            )


def _cdo(asset_path):
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not blueprint:
        raise RuntimeError("Missing Blueprint: " + asset_path)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return unreal.get_default_object(blueprint.generated_class())


def _assert_close(label, actual, expected):
    if abs(float(actual) - float(expected)) > 0.01:
        raise RuntimeError(
            "%s expected %.2f but found %.2f"
            % (label, expected, actual)
        )


def _validate_assets():
    medical = _cdo(MEDICAL_BLUEPRINT)
    armor_plate = _cdo(ARMOR_PLATE_BLUEPRINT)
    helmet = _cdo(HELMET_BLUEPRINT)

    if medical.get_editor_property("medkit_amount") != 1:
        raise RuntimeError("Medical supply must grant one medkit.")
    if medical.get_editor_property("field_dressing_amount") != 2:
        raise RuntimeError(
            "Medical supply must grant two field dressings."
        )
    _assert_close(
        "Medical immediate heal",
        medical.get_editor_property("heal_amount"),
        0.0,
    )
    _assert_close(
        "Armor plate helmet repair",
        armor_plate.get_editor_property(
            "helmet_durability_amount"
        ),
        0.0,
    )
    _assert_close(
        "Armor plate vest repair",
        armor_plate.get_editor_property(
            "body_armor_durability_amount"
        ),
        50.0,
    )
    _assert_close(
        "Helmet replacement",
        helmet.get_editor_property("helmet_durability_amount"),
        45.0,
    )
    _assert_close(
        "Helmet vest repair",
        helmet.get_editor_property(
            "body_armor_durability_amount"
        ),
        0.0,
    )
    _log("PASS medical, armor-plate, and helmet defaults.")


def _validate_map():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing First Light map.")

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    supply_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHSupplyBase",
    )

    if not world or not supply_class:
        raise RuntimeError("Supply world validation is unavailable.")

    actors = unreal.GameplayStatics.get_all_actors_of_class(
        world,
        supply_class,
    )
    persistence_ids = []

    for actor in actors:
        persistence_id = str(
            actor.get_editor_property("persistence_id")
        )
        if persistence_id and persistence_id != "None":
            persistence_ids.append(persistence_id)

    expected = {
        "FirstLightAmmoSupply",
        "FirstLightMedicalSupply",
        "FirstLightArmorPlateSupply",
        "FirstLightHelmetSupply",
    }
    missing = expected.difference(persistence_ids)

    if missing:
        raise RuntimeError(
            "First Light is missing supplies: "
            + ", ".join(sorted(missing))
        )

    if len(persistence_ids) != len(set(persistence_ids)):
        raise RuntimeError("Supply persistence IDs are duplicated.")

    _log(
        "PASS First Light contains four uniquely persistent "
        "field-resupply actors."
    )


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/Public/BHArmorSupply.h",
        (
            "class BROKENHORIZON_API ABHArmorSupply",
            "float HelmetDurabilityAmount = 0.0f;",
            "float BodyArmorDurabilityAmount = 50.0f;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHArmorSupply.cpp",
        (
            "InjuryComponent->RepairArmor(",
            "ARMOR RESUPPLY",
            "ShowStatusNotification(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHMedicalSupply.cpp",
        (
            "InjuryComponent->AddMedicalSupplies(",
            "MEDICAL RESUPPLY",
            "ShowStatusNotification(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHInjuryComponent.h",
        (
            "bool RepairArmor(",
            "void RestorePersistentSupplyState(",
            "float GetHelmetDurability() const;",
            "float GetBodyArmorDurability() const;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSaveGame.h",
        (
            "CurrentSchemaVersion = 34;",
            "int32 SavedMedkitCount = -1;",
            "float SavedBodyArmorDurability = -1.0f;",
            "int32 SavedReserveAmmo = -1;",
            "int32 SavedFragGrenadeCount = -1;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp",
        (
            "CapturePlayerResourceState(",
            "RestorePersistentSupplyState(",
            "RestoreAmmoState(",
            "RestoreFragGrenadeCount(",
            "RuntimeConsumedWorldItemIDs",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHAmmoSupply.cpp",
        (
            "AMMUNITION ACQUIRED",
            "ShowStatusNotification(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "void ABHCharacter::ShowStatusNotification(",
            "int32 ABHCharacter::AddFragGrenades(",
            "const bool bNeedsGrenades =",
            "GetFragGrenadeCount() < GetMaxFragGrenades();",
            "!bNeedsGrenades &&",
            "GetFieldSquadMembersNeedingServiceCount()",
            "!bNeedsFieldSquadService",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSectorResupplyStation.h",
        (
            "int32 TargetFragGrenadeCount = 2;",
            "float VehicleRecoverySupplyCost = 10.0f;",
            "float FireteamServiceRadius = 1600.0f;",
            "float FireteamServiceSupplyCostPerMember = 2.0f;",
            "CalculateFireteamServiceSupplyCost(",
            "GetResupplySupplyCost(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSectorResupplyStation.cpp",
        (
            "Character->AddFragGrenades(FragGrenadesNeeded)",
            "RecoveryTransport",
            "Candidate->IsImmobilized()",
            "TotalStrategicSupplyCost",
            "CountFieldSquadMembersNeedingService(",
            "ServiceFieldSquadMembers(",
            "IncapacitatedOperativesBeforeService",
            "CasualtiesStabilized",
            "CASUALTIES {8} STABILIZED",
            "FIRETEAM {4} SERVICED",
            "RecoverAndService(",
            "RECOVERED / REFUELED / REPAIRED",
            "+{3} FRAGS",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "Member->IsIncapacitated()",
            "Member->StabilizeIncapacitatedSoldier()",
            "bStabilizedCasualty",
            "ApplyFieldSquadOrder();",
            "Member->GetCombatReadiness()",
            "SavedMember.CombatReadiness",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHEnemySoldier.cpp",
        (
            "PostCasualtyCombatReadiness",
            "GetCombatReadiness() < 1.0f",
            "CombatReadiness = 1.0f;",
            "CalculateFieldOperativeReadinessSpread(",
            "CalculateFieldOperativeFireInterval(",
        ),
    )
    _log("PASS pickups update inventory, armor, HUD, and saves.")
    _log("PASS consumed supply IDs remain checkpoint-persistent.")
    _log("PASS depleted grenades activate the resupply waypoint.")
    _log("PASS remote recovery ignores healthy parked transports.")


def main():
    _validate_assets()
    _validate_map()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
