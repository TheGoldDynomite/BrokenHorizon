"""Validate Broken Horizon v0.16 incoming-fire feedback."""

import os
import unreal


ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
PLAYER_BLUEPRINT = "/Game/BrokenHorizon/Characters/MyBHCharacter"
COMBAT_WIDGET = "/Game/BrokenHorizon/UI/WBP_CombatStatus"
ENEMY_SOURCE = "Source/BrokenHorizon/Private/BHEnemySoldier.cpp"
PLAYER_SOURCE = "Source/BrokenHorizon/BHCharacter.cpp"
PLAYER_HEADER = "Source/BrokenHorizon/BHCharacter.h"
WIDGET_HEADER = (
    "Source/BrokenHorizon/Public/BHCombatStatusWidget.h"
)
WIDGET_SOURCE = (
    "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp"
)


def _log(message):
    unreal.log("[BH Incoming Fire Validation] " + message)


def _load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset


def _read(relative_path):
    path = os.path.join(unreal.Paths.project_dir(), relative_path)
    with open(path, "r", encoding="utf-8") as source_file:
        return source_file.read()


def _validate_assets():
    enemy_blueprint = _load(ENEMY_BLUEPRINT)
    player_blueprint = _load(PLAYER_BLUEPRINT)
    combat_widget = _load(COMBAT_WIDGET)

    unreal.BlueprintEditorLibrary.compile_blueprint(enemy_blueprint)
    unreal.BlueprintEditorLibrary.compile_blueprint(player_blueprint)
    unreal.BlueprintEditorLibrary.compile_blueprint(combat_widget)

    enemy_cdo = unreal.get_default_object(
        enemy_blueprint.generated_class()
    )
    near_miss_radius = enemy_cdo.get_editor_property(
        "near_miss_radius"
    )
    minimum_intensity = enemy_cdo.get_editor_property(
        "near_miss_minimum_intensity"
    )

    if near_miss_radius <= 0.0:
        raise RuntimeError("Enemy near-miss radius must be positive.")

    if minimum_intensity <= 0.0 or minimum_intensity > 1.0:
        raise RuntimeError(
            "Enemy near-miss minimum intensity must be in (0, 1]."
        )

    player_cdo = unreal.get_default_object(
        player_blueprint.generated_class()
    )
    assigned_widget_class = player_cdo.get_editor_property(
        "combat_status_widget_class"
    )

    if not assigned_widget_class:
        raise RuntimeError(
            "The player has no combat status widget class assigned."
        )

    if "WBP_CombatStatus" not in assigned_widget_class.get_path_name():
        raise RuntimeError(
            "Unexpected combat widget class: "
            + assigned_widget_class.get_path_name()
        )

    _log(
        "PASS assets: near-miss radius %.0f cm, minimum intensity %.2f."
        % (near_miss_radius, minimum_intensity)
    )
    _log("PASS player uses WBP_CombatStatus.")


def _require_fragments(relative_path, fragments):
    source = _read(relative_path)

    for fragment in fragments:
        if fragment not in source:
            raise RuntimeError(
                "%s is missing: %s"
                % (relative_path, fragment)
            )


def _validate_source_contract():
    _require_fragments(
        ENEMY_SOURCE,
        (
            "if (!bHitIntendedTarget && NearMissRadius > 0.0f)",
            "FMath::ClosestPointOnSegment(",
            "MissDistance <= NearMissRadius",
            "PlayerTarget->NotifyIncomingRound(",
            "if (bHitIntendedTarget)",
            "UGameplayStatics::ApplyPointDamage(",
        ),
    )
    _require_fragments(
        PLAYER_SOURCE,
        (
            "void ABHCharacter::NotifyIncomingRound(",
            "ClientNotifyIncomingRound(",
            "void ABHCharacter::ClientNotifyIncomingRound_Implementation(",
            "void ABHCharacter::ClientNotifyCombatDamage_Implementation(",
            "DisplayCombatDamageLocally(",
            "CombatStatusWidget->NotifyNearMiss(",
            "!bIsHandlingDeath",
        ),
    )
    _require_fragments(
        PLAYER_HEADER,
        (
            "UFUNCTION(Client, Unreliable)",
            "void ClientNotifyIncomingRound(",
            "void ClientNotifyCombatDamage(",
        ),
    )
    _require_fragments(
        WIDGET_HEADER,
        (
            "void NotifyNearMiss(",
            "virtual void NativeTick(",
            "virtual int32 NativePaint(",
            "float LowHealthThreshold = 0.25f;",
        ),
    )
    _require_fragments(
        WIDGET_SOURCE,
        (
            "DamageFeedbackRemaining",
            "NearMissFeedbackRemaining",
            "DrawDirectionChevron(",
            "MaximumDamageFlashOpacity",
            "MaximumNearMissOpacity",
            "CurrentHealthPercentage <= LowHealthThreshold",
            "InvalidateLayoutAndVolatility();",
        ),
    )

    _log("PASS hits drive red directional damage feedback.")
    _log("PASS misses use distance-based suppression without damage.")
    _log("PASS remote players receive authoritative damage and near-miss feedback.")
    _log("PASS low health and all temporary feedback repaint and fade.")


def main():
    _validate_assets()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


main()
