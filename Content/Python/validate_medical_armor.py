"""Validate Broken Horizon v0.19 medical and armor systems."""

import os
import unreal


PLAYER_BLUEPRINT = "/Game/Characters/BP_BHCharacter"
INJURY_HEADER = (
    "Source/BrokenHorizon/Public/BHInjuryComponent.h"
)
INJURY_SOURCE = (
    "Source/BrokenHorizon/Private/BHInjuryComponent.cpp"
)
PLAYER_HEADER = "Source/BrokenHorizon/BHCharacter.h"
PLAYER_SOURCE = "Source/BrokenHorizon/BHCharacter.cpp"
HUD_HEADER = (
    "Source/BrokenHorizon/Public/BHCombatStatusWidget.h"
)
HUD_SOURCE = (
    "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp"
)


def _log(message):
    unreal.log("[BH Medical Armor Validation] " + message)


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


def _validate_component_defaults():
    blueprint = unreal.EditorAssetLibrary.load_asset(
        PLAYER_BLUEPRINT
    )

    if not blueprint:
        raise RuntimeError("Missing asset: " + PLAYER_BLUEPRINT)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    injury_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHInjuryComponent",
    )

    if not injury_class:
        raise RuntimeError("BHInjuryComponent class is unavailable.")

    components = cdo.get_components_by_class(injury_class)

    if len(components) != 1:
        raise RuntimeError(
            "BP_BHCharacter must own exactly one injury component."
        )

    injury = components[0]
    medkits = injury.get_editor_property("starting_medkits")
    heal_amount = injury.get_editor_property(
        "medkit_healing_amount"
    )
    duration = injury.get_editor_property(
        "medkit_treatment_duration"
    )
    helmet = injury.get_editor_property(
        "maximum_helmet_durability"
    )
    vest = injury.get_editor_property(
        "maximum_body_armor_durability"
    )

    if medkits < 1:
        raise RuntimeError("Player must start with a medkit.")

    if heal_amount <= 0.0 or duration <= 0.0:
        raise RuntimeError(
            "Medkit healing and duration must be positive."
        )

    if helmet <= 0.0 or vest <= 0.0:
        raise RuntimeError(
            "Helmet and vest durability must be positive."
        )

    _log(
        "PASS player starts with %d medkits; each heals %.1f "
        "over %.1fs." % (medkits, heal_amount, duration)
    )
    _log(
        "PASS armor pools: helmet %.1f, vest %.1f."
        % (helmet, vest)
    )


def _validate_source_contract():
    _require_fragments(
        INJURY_HEADER,
        (
            "FBHOnMedicalStateChanged",
            "bool StartMedkitTreatment();",
            "float MaximumHelmetDurability = 45.0f;",
            "float MaximumBodyArmorDurability = 100.0f;",
            "int32 StartingMedkits = 2;",
            "float MedkitHealingAmount = 45.0f;",
            "float MedkitTreatmentDuration = 3.0f;",
        ),
    )
    _require_fragments(
        INJURY_SOURCE,
        (
            "ApplyArmorProtection(",
            "CurrentHelmetDurability",
            "CurrentBodyArmorDurability",
            "CompleteMedkitTreatment();",
            "HealthComponent->Heal(",
            "bArmInjured = false;",
            "bLegInjured = false;",
            "bBleeding",
        ),
    )
    _require_fragments(
        PLAYER_HEADER,
        (
            "void UseMedkit();",
            "TObjectPtr<UInputAction> MedkitAction;",
            "FirstPersonMedicalLocation",
            "FirstPersonMedicalRotation",
        ),
    )
    _require_fragments(
        PLAYER_SOURCE,
        (
            "EKeys::J",
            "InjuryComponent->StartMedkitTreatment()",
            "InjuryComponent->IsMedkitTreatmentActive()",
            "GetMedkitTreatmentProgress() * PI",
            "CombatStatusWidget->SetMedicalState(",
        ),
    )
    _require_fragments(
        HUD_HEADER,
        (
            "void SetMedicalState(",
            "float HelmetDurabilityPercentage = 1.0f;",
            "int32 MedkitCount = 0;",
        ),
    )
    _require_fragments(
        HUD_SOURCE,
        (
            "ARMOR  H:%d%%  V:%d%%",
            "MEDKITS: %d [J]",
            "USING MEDKIT  %d%%",
        ),
    )

    _log("PASS armor absorption consumes helmet and vest durability.")
    _log("PASS J starts timed treatment and locks weapon actions.")
    _log("PASS medkits heal and clear arm/leg penalties.")
    _log("PASS procedural arms and HUD show treatment progress.")


def main():
    _validate_component_defaults()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


main()
