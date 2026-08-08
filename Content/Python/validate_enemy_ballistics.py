"""Validate Broken Horizon v0.15.1 enemy line-of-fire behavior."""

import os
import unreal


ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
EXPECTED_IMPACT_CLASS = "/Script/BrokenHorizon.BHImpactEffect"
SOURCE_FILE = "Source/BrokenHorizon/Private/BHEnemySoldier.cpp"
PLAYER_SOURCE_FILE = "Source/BrokenHorizon/BHCharacter.cpp"
ENGINE_CONFIG = "Config/DefaultEngine.ini"


def _log(message):
    unreal.log("[BH Enemy Ballistics Validation] " + message)


def _load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset


def _path(value):
    return value.get_path_name() if value else "None"


def _validate_enemy_defaults():
    blueprint = _load(ENEMY_BLUEPRINT)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())

    impact_class = cdo.get_editor_property("impact_actor_class")
    if _path(impact_class) != EXPECTED_IMPACT_CLASS:
        raise RuntimeError(
            "Unexpected enemy impact actor class: " + _path(impact_class)
        )

    if cdo.get_editor_property("shot_spread_degrees") <= 0.0:
        raise RuntimeError(
            "Enemy spread must remain above zero so missed rounds are tested."
        )

    player_blueprint = _load(
        "/Game/BrokenHorizon/Characters/MyBHCharacter"
    )
    player_cdo = unreal.get_default_object(
        player_blueprint.generated_class()
    )
    capsule = player_cdo.get_component_by_class(
        unreal.CapsuleComponent
    )
    if not capsule:
        raise RuntimeError("Player collision capsule is missing.")

    _log("PASS enemy impact presentation and non-zero spread defaults.")
    _log("PASS player collision capsule exists.")


def _validate_source_contract():
    source_path = os.path.join(unreal.Paths.project_dir(), SOURCE_FILE)
    with open(source_path, "r", encoding="utf-8") as source_file:
        source = source_file.read()

    forbidden_fragments = (
        "DamageHit = FHitResult(",
        "if (!bHit || HitResult.GetActor() != TargetActor)",
    )
    for fragment in forbidden_fragments:
        if fragment in source:
            raise RuntimeError(
                "Synthetic enemy target damage fallback remains: " + fragment
            )

    required_fragments = (
        "constexpr ECollisionChannel EnemyWeaponTraceChannel =",
        "ECC_GameTraceChannel2;",
        "EnemyWeaponTraceChannel,",
        "const bool bHitIntendedTarget =",
        "if (bHitIntendedTarget)",
        "if (bHit && !bHitIntendedTarget)",
        "PlayImpactPresentation(HitResult);",
    )
    for fragment in required_fragments:
        if fragment not in source:
            raise RuntimeError(
                "Enemy line-of-fire contract is missing: " + fragment
            )

    _log(
        "PASS missed and cover-blocked rounds cannot apply synthetic damage."
    )


def _validate_player_source_contract():
    source_path = os.path.join(
        unreal.Paths.project_dir(),
        PLAYER_SOURCE_FILE,
    )
    with open(source_path, "r", encoding="utf-8") as source_file:
        source = source_file.read()

    required_fragments = (
        "SetCollisionResponseToChannel(",
        "ECC_GameTraceChannel2,",
        "ECR_Block",
    )
    for fragment in required_fragments:
        if fragment not in source:
            raise RuntimeError(
                "Player enemy-trace collision contract is missing: "
                + fragment
            )

    _log("PASS player explicitly blocks EnemyWeaponTrace at runtime.")


def _validate_collision_config():
    config_path = os.path.join(
        unreal.Paths.project_dir(),
        ENGINE_CONFIG,
    )
    with open(config_path, "r", encoding="utf-8") as config_file:
        config = config_file.read()

    expected = (
        'Channel=ECC_GameTraceChannel2,Name="EnemyWeaponTrace",'
        "DefaultResponse=ECR_Block,bTraceType=True"
    )
    if expected not in config:
        raise RuntimeError(
            "EnemyWeaponTrace channel is missing or does not default to Block."
        )

    trigger_override = (
        "Channel=EnemyWeaponTrace, Response=ECR_Ignore"
    )
    if trigger_override not in config:
        raise RuntimeError(
            "Trigger volumes must ignore EnemyWeaponTrace."
        )

    _log("PASS EnemyWeaponTrace collision-channel configuration.")
    _log("PASS trigger volumes do not absorb enemy rounds.")


def main():
    _validate_enemy_defaults()
    _validate_source_contract()
    _validate_player_source_contract()
    _validate_collision_config()
    _log("ALL CHECKS PASSED")


main()
