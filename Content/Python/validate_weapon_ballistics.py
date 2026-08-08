"""Read-only validation for the player weapon ballistics correction."""

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
RIFLE_BLUEPRINT = "/Game/BP_Rifle"
PLAYER_BLUEPRINT = "/Game/Characters/BP_BHCharacter"


def require_source(path, fragments):
    text = (PROJECT_ROOT / path).read_text(encoding="utf-8")
    for fragment in fragments:
        if fragment not in text:
            raise RuntimeError("%s is missing %s" % (path, fragment))


def load_defaults(path):
    blueprint = unreal.EditorAssetLibrary.load_asset(path)
    if not blueprint:
        raise RuntimeError("Missing Blueprint: " + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return unreal.get_default_object(blueprint.generated_class())


require_source(
    "Source/BrokenHorizon/Private/BHRifle.cpp",
    (
        "BHRifleMuzzleObstructionTrace",
        "bMuzzleObstructed || bCameraHit",
        "MuzzleObstructionCheckDistance",
        "ShotDirection",
        "!Enemy->IsHostileTo(WeaponOwner)",
        "RifleConfig.SuppressionMinimumIntensity",
        "SuppressionIntensity",
    ),
)
require_source(
    "Source/BrokenHorizon/Private/BHWeaponComponent.cpp",
    (
        "ADSRecoilMultiplier",
        "RecoilPitchVariation",
        "CurrentSpreadBloomDegrees",
        "UpdateWeaponRecovery(DeltaTime)",
        "SpreadRecoveryDelay",
        "RecoilRecoveryDelay",
        "PendingRecoilOffset",
        "EBHFireMode::Automatic",
        "ToggleFireMode()",
        "ApplyAimingState(false);",
        "if (bWantsToAim)",
        "StartAiming();",
        "Controller->SetControlRotation(RecoilRotation)",
    ),
)

rifle = load_defaults(RIFLE_BLUEPRINT)
config = rifle.get_editor_property("rifle_config")
expected = {
    "range": 50000.0,
    "hip_spread_degrees": 0.9,
    "ads_spread_degrees": 0.08,
    "spread_per_shot_degrees": 0.11,
    "max_spread_bloom_degrees": 1.1,
    "spread_recovery_delay": 0.18,
    "spread_recovery_speed": 2.4,
    "recoil_pitch": 0.55,
    "recoil_yaw": 0.12,
    "ads_recoil_multiplier": 0.72,
    "recoil_pitch_variation": 0.1,
    "recoil_recovery_delay": 0.18,
    "recoil_recovery_speed": 5.0,
    "muzzle_obstruction_check_distance": 150.0,
    "suppression_minimum_intensity": 0.20,
}

for name, expected_value in expected.items():
    actual = float(config.get_editor_property(name))
    if abs(actual - expected_value) > 0.001:
        raise RuntimeError(
            "%s is %.3f, expected %.3f"
            % (name, actual, expected_value)
        )

muzzle = None
for component in rifle.get_components_by_class(unreal.SceneComponent):
    if component.get_name() == "MuzzlePoint":
        muzzle = component
        break

if not muzzle:
    raise RuntimeError("BP_Rifle has no MuzzlePoint.")

muzzle_location = muzzle.get_editor_property("relative_location")
if (
    abs(muzzle_location.x - 100.0) > 0.01
    or abs(muzzle_location.y) > 0.01
    or abs(muzzle_location.z) > 0.01
):
    raise RuntimeError("MuzzlePoint is not centered: %s" % muzzle_location)

player = load_defaults(PLAYER_BLUEPRINT)
if (
    float(
        player.get_editor_property(
            "first_person_fire_kick_recovery_speed"
        )
    )
    < 20.0
):
    raise RuntimeError("First-person weapon kick recovery is too slow.")

player_accuracy_defaults = {
    "moving_weapon_spread_multiplier": 1.8,
    "crouched_stationary_spread_multiplier": 0.85,
    "exhausted_weapon_spread_multiplier": 1.35,
    "stable_weapon_velocity_threshold": 10.0,
}

for name, expected_value in player_accuracy_defaults.items():
    actual = float(player.get_editor_property(name))
    if abs(actual - expected_value) > 0.001:
        raise RuntimeError(
            "%s is %.3f, expected %.3f"
            % (name, actual, expected_value)
        )

require_source(
    "Source/BrokenHorizon/BHCharacter.cpp",
    (
        "MovingWeaponSpreadMultiplier",
        "CrouchedStationarySpreadMultiplier",
        "ExhaustedWeaponSpreadMultiplier",
        "MovementMultiplier *",
        "StanceMultiplier *",
        "ExhaustionMultiplier;",
        "EKeys::B",
        "&ABHCharacter::ToggleFireMode",
        "FIRE MODE // AUTO",
        "FIRE MODE // SEMI",
    ),
)

unreal.log(
    "[BH Weapon Ballistics Validation] PASS: camera-authoritative "
    "shots, muzzle obstruction, 500 m range, centered muzzle, "
    "sustained-fire bloom, recovering recoil, and "
    "movement/stance/stamina accuracy with held-ADS reload recovery "
    "and faction-aware proximity suppression."
)
