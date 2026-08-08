"""Validate the Broken Horizon functional frag-grenade combat loop."""

import os

import unreal


def _log(message):
    unreal.log("[BH Frag Grenade Validation] " + message)


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


def _validate_reflection():
    grenade_class = getattr(unreal, "BHFragGrenade", None)
    character_class = getattr(unreal, "BHCharacter", None)
    save_class = getattr(unreal, "BHSaveGame", None)
    station_class = getattr(
        unreal,
        "BHSectorResupplyStation",
        None,
    )

    if not grenade_class:
        raise RuntimeError("BHFragGrenade is not reflected.")
    if not character_class:
        raise RuntimeError("BHCharacter is not reflected.")
    if not save_class:
        raise RuntimeError("BHSaveGame is not reflected.")
    if not station_class:
        raise RuntimeError("BHSectorResupplyStation is not reflected.")

    grenade = unreal.get_default_object(grenade_class)
    character = unreal.get_default_object(character_class)
    save_data = unreal.get_default_object(save_class)
    station = unreal.get_default_object(station_class)

    fuse = float(grenade.get_editor_property("fuse_duration"))
    inner_radius = float(
        grenade.get_editor_property("inner_damage_radius")
    )
    outer_radius = float(
        grenade.get_editor_property("outer_damage_radius")
    )
    maximum_damage = float(
        grenade.get_editor_property("maximum_damage")
    )
    noise_range = float(
        grenade.get_editor_property("explosion_noise_range")
    )
    warning_lead_time = float(
        grenade.get_editor_property("threat_warning_lead_time")
    )
    maximum_inventory = int(
        character.get_editor_property("max_frag_grenades")
    )
    initial_inventory = int(
        character.get_editor_property("frag_grenade_count")
    )
    throw_speed = float(
        character.get_editor_property("frag_grenade_throw_speed")
    )
    resupply_target = int(
        station.get_editor_property("target_frag_grenade_count")
    )

    if abs(fuse - 3.5) > 0.01:
        raise RuntimeError("Frag fuse must be 3.5 seconds.")
    if inner_radius <= 0.0 or outer_radius <= inner_radius:
        raise RuntimeError("Frag damage radii are invalid.")
    if maximum_damage < 100.0:
        raise RuntimeError("Frag maximum damage is not lethal.")
    if noise_range < outer_radius:
        raise RuntimeError("Frag blast does not alert nearby AI.")
    if warning_lead_time <= 0.0 or warning_lead_time >= fuse:
        raise RuntimeError("Grenade threat-warning lead time is invalid.")
    if maximum_inventory != 2 or initial_inventory != 2:
        raise RuntimeError("Player frag inventory must start at 2/2.")
    if throw_speed < 1000.0:
        raise RuntimeError("Frag throw speed is too low.")
    if resupply_target != maximum_inventory:
        raise RuntimeError("Sector resupply does not refill frag inventory.")
    if int(save_data.get_editor_property("schema_version")) != 34:
        raise RuntimeError("Frag persistence requires save schema 34.")

    _log(
        "PASS reflected defaults: fuse %.1fs, %.0fcm radius, "
        "%.0f damage, %.2fs warning, inventory %d/%d."
        % (
            fuse,
            outer_radius,
            maximum_damage,
            warning_lead_time,
            initial_inventory,
            maximum_inventory,
        )
    )


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/Private/BHFragGrenade.cpp",
        (
            "SetSimulatePhysics(true)",
            "ApplyRadialDamageWithFalloff(",
            "ECC_Visibility",
            "BlastImpulse->FireImpulse()",
            "UAISense_Hearing::ReportNoiseEvent(",
            "OverlapMultiByObjectType(",
            "Controller->NotifyGrenadeThreat(",
            "PlayerCharacter->NotifyGrenadeThreat(",
            "GrenadeInstigator->IsHostileTo(PlayerCharacter)",
            "TimeUntilDetonation >",
            "ThreatWarningLeadTime",
            "BH_FRAG_EXPLODED",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHEnemyAIController.h",
        (
            "EvadeExplosive,",
            "bool NotifyGrenadeThreat(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHEnemyAIController.cpp",
        (
            "bool ABHEnemyAIController::EnterExplosiveEvade(",
            "BH_AI_GRENADE_EVADE",
            "ResumeAfterExplosiveEvade()",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "EKeys::G",
            "&ABHCharacter::ThrowFragGrenade",
            "FragGrenadeCount = FMath::Max(",
            "SaveSubsystem->SavePlayerResources()",
            "BH_FRAG_THROWN",
            "void ABHCharacter::NotifyGrenadeThreat(",
            "CombatStatusWidget->NotifyGrenadeThreat(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSaveGame.h",
        (
            "CurrentSchemaVersion = 34;",
            "int32 SavedFragGrenadeCount = -1;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp",
        (
            "SaveData->SavedFragGrenadeCount =",
            "bool UBHSaveSubsystem::SavePlayerResources()",
            "Character->RestoreFragGrenadeCount(",
            "without moving checkpoint",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSectorResupplyStation.cpp",
        (
            "TargetFragGrenadeCount -",
            "Character->AddFragGrenades(FragGrenadesNeeded)",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp",
        (
            "void UBHCombatStatusWidget::SetFragGrenadeCount(",
            "void UBHCombatStatusWidget::NotifyGrenadeThreat(",
            "GRENADE // %.0f M // %.1f S",
            "FRAGS: %d  [G]",
        ),
    )
    _log("PASS throw, blast, HUD, persistence, and resupply contracts.")


def main():
    _validate_reflection()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
