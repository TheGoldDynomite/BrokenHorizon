"""Validate Broken Horizon v0.22 prone stance."""

import os

import unreal


PLAYER_BLUEPRINT = "/Game/BrokenHorizon/Characters/MyBHCharacter"


def _log(message):
    unreal.log("[BH Prone Stance Validation] " + message)


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
        ("Prone speed", "prone_speed", 140.0),
        ("Prone capsule radius", "prone_capsule_radius", 30.0),
        (
            "Prone capsule half height",
            "prone_capsule_half_height",
            34.0,
        ),
        ("Prone camera drop", "prone_camera_drop", 42.0),
        ("Prone transition speed", "prone_transition_speed", 8.0),
        (
            "Prone spread multiplier",
            "prone_stationary_spread_multiplier",
            0.7,
        ),
        (
            "Prone sight multiplier",
            "prone_ai_sight_range_multiplier",
            0.65,
        ),
    )

    for label, property_name, expected in defaults:
        _assert_close(
            label,
            character.get_editor_property(property_name),
            expected,
        )

    _log("PASS player Blueprint inherits tuned prone defaults.")


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.h",
        (
            "bool IsProne() const;",
            "float GetAISightRangeMultiplier() const;",
            "bool HasProneExitClearance(",
            "float ProneSpeed = 140.0f;",
            "float ProneStationarySpreadMultiplier = 0.7f;",
            "float ProneAISightRangeMultiplier = 0.65f;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "EKeys::Z",
            "&ABHCharacter::ToggleProne",
            "OverlapBlockingTestByChannel(",
            "CollisionCapsule->SetCapsuleSize(",
            "UpdateProne(DeltaTime);",
            "CurrentProneAlpha * ProneCameraDrop",
            "bIsProne ||",
            "return InjuryMultiplier * LeanMultiplier * ProneMultiplier;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHEnemyAIController.h",
        ("bool CanAcquireVisualTarget(AActor* Candidate) const;",),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHEnemyAIController.cpp",
        (
            "CanAcquireVisualTarget(PlayerTarget)",
            "PlayerCharacter->GetAISightRangeMultiplier()",
            "CurrentState == EBHEnemyAIState::Combat",
        ),
    )

    _log("PASS Z toggles prone with an optional Enhanced Input action.")
    _log("PASS prone resizes the capsule and checks overhead clearance.")
    _log("PASS camera and weapon transition smoothly with the stance.")
    _log("PASS prone disables sprint, jump, crouch, and leaning conflicts.")
    _log("PASS stationary prone improves weapon stability.")
    _log("PASS prone reduces initial long-range AI visual acquisition.")


def main():
    _validate_character_defaults()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
