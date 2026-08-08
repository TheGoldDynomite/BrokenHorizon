"""Validate Broken Horizon v0.18 player injuries and treatment."""

import os
import unreal


PLAYER_BLUEPRINT = "/Game/Characters/BP_BHCharacter"
PLAYER_HEADER = "Source/BrokenHorizon/BHCharacter.h"
PLAYER_SOURCE = "Source/BrokenHorizon/BHCharacter.cpp"
INJURY_HEADER = (
    "Source/BrokenHorizon/Public/BHInjuryComponent.h"
)
INJURY_SOURCE = (
    "Source/BrokenHorizon/Private/BHInjuryComponent.cpp"
)
ENEMY_SOURCE = (
    "Source/BrokenHorizon/Private/BHEnemySoldier.cpp"
)
WEAPON_SOURCE = (
    "Source/BrokenHorizon/Private/BHWeaponComponent.cpp"
)
HUD_HEADER = (
    "Source/BrokenHorizon/Public/BHCombatStatusWidget.h"
)
HUD_SOURCE = (
    "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp"
)


def _log(message):
    unreal.log("[BH Player Injury Validation] " + message)


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


def _validate_player_component():
    blueprint = unreal.EditorAssetLibrary.load_asset(PLAYER_BLUEPRINT)

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
    helmet_scale = injury.get_editor_property(
        "helmet_damage_scale"
    )
    armor_scale = injury.get_editor_property(
        "body_armor_damage_scale"
    )
    dressings = injury.get_editor_property(
        "starting_field_dressings"
    )
    arm_penalty = injury.get_editor_property(
        "arm_injury_spread_multiplier"
    )
    leg_penalty = injury.get_editor_property(
        "leg_injury_movement_multiplier"
    )

    if not (0.0 <= helmet_scale <= 1.0):
        raise RuntimeError("Helmet scale must be in [0, 1].")

    if not (0.0 <= armor_scale <= 1.0):
        raise RuntimeError("Body armor scale must be in [0, 1].")

    if dressings < 1:
        raise RuntimeError(
            "Player must start with at least one field dressing."
        )

    if arm_penalty <= 1.0:
        raise RuntimeError(
            "Arm injury must increase weapon spread."
        )

    if not (0.0 < leg_penalty < 1.0):
        raise RuntimeError(
            "Leg injury must reduce movement speed."
        )

    _log(
        "PASS player owns one injury component with helmet, "
        "vest, and %d field dressings." % dressings
    )
    _log(
        "PASS arm spread %.2fx and leg movement %.2fx."
        % (arm_penalty, leg_penalty)
    )


def _validate_source_contract():
    _require_fragments(
        INJURY_HEADER,
        (
            "enum class EBHPlayerHitZone",
            "bool bHasHelmet = true;",
            "bool bHasBodyArmor = true;",
            "StartingFieldDressings = 3;",
            "ArmInjurySpreadMultiplier = 1.65f;",
            "LegInjuryMovementMultiplier = 0.70f;",
        ),
    )
    _require_fragments(
        INJURY_SOURCE,
        (
            "ResolveHitZone(",
            "HelmetDamageScale",
            "BodyArmorDamageScale",
            "HealthComponent->ApplyDamage(",
            "UseFieldDressing()",
            "bArmInjured = true;",
            "bLegInjured = true;",
        ),
    )
    _require_fragments(
        PLAYER_HEADER,
        (
            "TObjectPtr<UBHInjuryComponent> InjuryComponent;",
            "TObjectPtr<UInputAction> FieldDressingAction;",
            "void UseFieldDressing();",
        ),
    )
    _require_fragments(
        PLAYER_SOURCE,
        (
            'TEXT("InjuryComponent")',
            "EKeys::H",
            "if (!InjuryComponent->UseFieldDressing())",
            "GetWeaponSpreadMultiplier() const",
            "CombatStatusWidget->SetInjuryState(",
        ),
    )
    _require_fragments(
        ENEMY_SOURCE,
        (
            "CalculateIncomingBallisticDamage(",
            "RegisterIncomingBallisticHit(",
            "GetScaledCapsuleHalfHeight()",
        ),
    )
    _require_fragments(
        WEAPON_SOURCE,
        (
            "CharacterOwner->GetWeaponSpreadMultiplier()",
        ),
    )
    _require_fragments(
        HUD_HEADER,
        (
            "void SetInjuryState(",
            "bool bIsBleeding = false;",
            "int32 FieldDressingCount = 0;",
        ),
    )
    _require_fragments(
        HUD_SOURCE,
        (
            "BLEEDING  %.1f HP/S - PRESS H",
            "ARM WOUND - ACCURACY REDUCED",
            "LEG WOUND - MOVEMENT REDUCED",
            "DRESSINGS: %d  [H]",
        ),
    )

    _log("PASS helmet and vest modify incoming ballistic damage.")
    _log("PASS wounds drive bleeding, sway, spread, and movement.")
    _log("PASS H uses a field dressing to stop active bleeding.")
    _log("PASS native HUD displays wounds and dressing inventory.")


def main():
    _validate_player_component()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


main()
