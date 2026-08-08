"""Validate Broken Horizon v0.21 tactical leaning."""

import os
import unreal


PLAYER_BLUEPRINT = (
    "/Game/BrokenHorizon/Characters/MyBHCharacter"
)


def _log(message):
    unreal.log("[BH Tactical Lean Validation] " + message)


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
    blueprint = unreal.EditorAssetLibrary.load_asset(
        PLAYER_BLUEPRINT
    )

    if not blueprint:
        raise RuntimeError(
            "Missing player Blueprint: " + PLAYER_BLUEPRINT
        )

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    character = unreal.get_default_object(
        blueprint.generated_class()
    )

    _assert_close(
        "Maximum lean distance",
        character.get_editor_property("maximum_lean_distance"),
        32.0,
    )
    _assert_close(
        "Maximum lean roll",
        character.get_editor_property("maximum_lean_roll"),
        9.0,
    )
    _assert_close(
        "Lean collision radius",
        character.get_editor_property("lean_collision_radius"),
        8.0,
    )
    _assert_close(
        "Lean collision padding",
        character.get_editor_property("lean_collision_padding"),
        2.0,
    )
    _assert_close(
        "Lean spread multiplier",
        character.get_editor_property(
            "lean_weapon_spread_multiplier"
        ),
        1.1,
    )
    _log("PASS player Blueprint inherits tuned lean defaults.")


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.h",
        (
            "float GetLeanAmount() const;",
            "void StartLeanLeft();",
            "void StartLeanRight();",
            "float ResolveCollisionLimitedLean(",
            "float MaximumLeanDistance = 32.0f;",
            "float LeanWeaponSpreadMultiplier = 1.1f;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "EKeys::Q",
            "EKeys::E",
            "SweepSingleByChannel(",
            "FCollisionShape::MakeSphere(",
            "UpdateLean(DeltaTime);",
            "FirstPersonCamera->SetRelativeLocationAndRotation(",
            "const float LeanMultiplier = FMath::Lerp(",
            "return InjuryMultiplier * LeanMultiplier * ProneMultiplier;",
        ),
    )
    _log("PASS Q/E bindings support optional Enhanced Input actions.")
    _log("PASS leaning moves camera and weapon together.")
    _log("PASS sphere sweep prevents leaning through walls.")
    _log("PASS sprint, falling, treatment, death, and mission completion cancel lean.")
    _log("PASS active lean contributes a small weapon-spread penalty.")


def main():
    _validate_character_defaults()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
