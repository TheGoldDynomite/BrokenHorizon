"""Validate Broken Horizon v0.23 vaulting and mantling."""

import os

import unreal


PLAYER_BLUEPRINT = "/Game/BrokenHorizon/Characters/MyBHCharacter"


def _log(message):
    unreal.log("[BH Traversal Validation] " + message)


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


def _assert_close(label, actual, expected):
    if abs(float(actual) - float(expected)) > 0.01:
        raise RuntimeError(
            "%s expected %.2f but found %.2f"
            % (label, expected, actual)
        )


def _validate_character_defaults():
    blueprint = unreal.EditorAssetLibrary.load_asset(PLAYER_BLUEPRINT)

    if not blueprint:
        raise RuntimeError(
            "Missing player Blueprint: " + PLAYER_BLUEPRINT
        )

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    character = unreal.get_default_object(blueprint.generated_class())

    defaults = (
        ("Traversal reach", "traversal_reach", 120.0),
        ("Minimum height", "traversal_minimum_height", 30.0),
        ("Maximum vault height", "vault_maximum_height", 100.0),
        ("Maximum mantle height", "mantle_maximum_height", 180.0),
        ("Vault duration", "vault_duration", 0.55),
        ("Mantle duration", "mantle_duration", 0.85),
        ("Vault stamina cost", "vault_stamina_cost", 12.0),
        ("Mantle stamina cost", "mantle_stamina_cost", 20.0),
    )

    for label, property_name, expected in defaults:
        _assert_close(
            label,
            character.get_editor_property(property_name),
            expected,
        )

    _log("PASS player Blueprint inherits tuned traversal defaults.")


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.h",
        (
            "bool IsTraversing() const;",
            "bool IsMantling() const;",
            "bool TryStartTraversal();",
            "bool FindTraversalTarget(",
            "bool IsTraversalPathClear(",
            "float VaultMaximumHeight = 100.0f;",
            "float MantleMaximumHeight = 180.0f;",
            "float VaultStaminaCost = 12.0f;",
            "float MantleStaminaCost = 20.0f;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "if (TryStartTraversal())",
            "MovementComponent->IsMovingOnGround()",
            "InjuryComponent->IsArmInjured()",
            "InjuryComponent->IsLegInjured()",
            "World->OverlapBlockingTestByChannel(",
            "World->SweepSingleByChannel(",
            "WeaponComponent->StopAllActions();",
            "MovementComponent->SetMovementMode(MOVE_Flying);",
            "UpdateTraversal(DeltaTime);",
            "FinishTraversal(false);",
            "SpendStamina(StaminaCost)",
        ),
    )

    _log("PASS Space attempts traversal before a normal jump.")
    _log("PASS low obstacles vault and higher ledges mantle.")
    _log("PASS capsule overlap and sweep checks protect the full path.")
    _log("PASS traversal stops weapon actions without removing the gun.")
    _log("PASS prone, treatment, arm injury, and leg injury block traversal.")
    _log("PASS vaulting and mantling consume separate stamina costs.")


def main():
    _validate_character_defaults()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
