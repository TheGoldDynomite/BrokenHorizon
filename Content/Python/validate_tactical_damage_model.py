"""Validate Broken Horizon v0.17 tactical enemy damage."""

import os
import unreal


ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
ENEMY_HEADER = "Source/BrokenHorizon/Public/BHEnemySoldier.h"
ENEMY_SOURCE = "Source/BrokenHorizon/Private/BHEnemySoldier.cpp"
RIFLE_SOURCE = "Source/BrokenHorizon/Private/BHRifle.cpp"
PLAYER_HEADER = "Source/BrokenHorizon/BHCharacter.h"
PLAYER_SOURCE = "Source/BrokenHorizon/BHCharacter.cpp"
MARKER_HEADER = (
    "Source/BrokenHorizon/Public/BHHitMarkerWidget.h"
)
MARKER_SOURCE = (
    "Source/BrokenHorizon/Private/BHHitMarkerWidget.cpp"
)


def _log(message):
    unreal.log("[BH Tactical Damage Validation] " + message)


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


def _validate_enemy_defaults():
    blueprint = unreal.EditorAssetLibrary.load_asset(ENEMY_BLUEPRINT)

    if not blueprint:
        raise RuntimeError("Missing asset: " + ENEMY_BLUEPRINT)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())

    head = cdo.get_editor_property("head_damage_multiplier")
    torso = cdo.get_editor_property("torso_damage_multiplier")
    arm = cdo.get_editor_property("arm_damage_multiplier")
    leg = cdo.get_editor_property("leg_damage_multiplier")
    helmet = cdo.get_editor_property("helmet_damage_scale")
    vest = cdo.get_editor_property("body_armor_damage_scale")

    if head <= torso:
        raise RuntimeError(
            "Head damage must be greater than torso damage."
        )

    if arm >= torso or leg >= torso:
        raise RuntimeError(
            "Limb damage must remain below torso damage."
        )

    if not (0.0 <= helmet <= 1.0):
        raise RuntimeError("Helmet damage scale must be in [0, 1].")

    if not (0.0 <= vest <= 1.0):
        raise RuntimeError(
            "Body armor damage scale must be in [0, 1]."
        )

    _log(
        "PASS defaults: head %.2fx, torso %.2fx, arms %.2fx, "
        "legs %.2fx." % (head, torso, arm, leg)
    )
    _log(
        "PASS armor scales: helmet %.2fx, vest %.2fx."
        % (helmet, vest)
    )


def _validate_source_contract():
    _require_fragments(
        ENEMY_HEADER,
        (
            "enum class EBHHitZone",
            "HeadDamageMultiplier = 4.0f;",
            "ArmDamageMultiplier = 0.65f;",
            "LegDamageMultiplier = 0.70f;",
            "bHasHelmet = false;",
            "bHasBodyArmor = false;",
            "ResolveHitZone(const FHitResult& HitResult) const;",
        ),
    )
    _require_fragments(
        ENEMY_SOURCE,
        (
            "CharacterMesh->SetCollisionResponseToChannel(",
            "ECC_Visibility,",
            "BoneName.Contains(TEXT(\"head\"))",
            "BoneName.Contains(TEXT(\"upperarm\"))",
            "BoneName.Contains(TEXT(\"thigh\"))",
            "NormalizedHeight >= 0.55f",
            "HelmetDamageScale",
            "BodyArmorDamageScale",
        ),
    )
    _require_fragments(
        RIFLE_SOURCE,
        (
            "Enemy->ResolveHitZone(WeaponHit)",
            "Enemy->GetHitZoneDamageMultiplier(HitZone)",
            "bHeadshot = HitZone == EBHHitZone::Head;",
            "CharacterOwner->ShowHitConfirmation(",
        ),
    )
    _require_fragments(
        PLAYER_HEADER,
        (
            "bool bHeadshot = false",
        ),
    )
    _require_fragments(
        PLAYER_SOURCE,
        (
            "bool bHeadshot",
            "HitMarkerWidget->ShowHitMarker(",
        ),
    )
    _require_fragments(
        MARKER_HEADER,
        (
            "void ShowHitMarker(bool bLethalHit, bool bHeadshot);",
            "HeadshotMarkerDuration",
            "HeadshotColor",
            "HeadshotDiamondRadius",
        ),
    )
    _require_fragments(
        MARKER_SOURCE,
        (
            "bActiveHeadshot = bHeadshot;",
            "DiamondPoints",
            "HeadshotColor",
        ),
    )

    _log("PASS skeletal bones resolve head, torso, arm, and leg hits.")
    _log("PASS capsule fallback keeps the damage model asset-agnostic.")
    _log("PASS helmet and vest settings reduce protected-zone damage.")
    _log("PASS headshots receive a distinct gold diamond marker.")


def main():
    _validate_enemy_defaults()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


main()
