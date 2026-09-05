"""Read-only validator for /Game/BrokenHorizon/Maps/L_FirstLight_Graybox."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
PROTOTYPE_MAP_PATH = "/Game/BrokenHorizon/Maps/L_Prototype"
TAG = "BH_Auto_FirstLight"
EXPECTED_KEYCARD_ID = "RedKeycard"
EXPECTED_KEYCARD_PERSISTENCE_ID = "FirstLightRedKeycard"
EXPECTED_DOOR_PERSISTENCE_ID = "FirstLightSecurityDoor"
EXPECTED_GUARD_OBJECTIVE_ID = "EliminateGuard"
EXPECTED_EXTRACTION_OBJECTIVE_ID = "ReachExtraction"
CANONICAL_GUARD_LABEL_PREFIX = "FL_Guard_"


def _log(message):
    unreal.log("[FirstLight Validator] " + message)


def _actors(world, class_path):
    actor_class = unreal.load_class(None, class_path)
    return unreal.GameplayStatics.get_all_actors_of_class(world, actor_class) if actor_class else []


def _names(actors, property_name):
    result = []
    for actor in actors:
        try:
            result.append(str(actor.get_editor_property(property_name)))
        except Exception as error:
            result.append("<unreadable: %s>" % error)
    return result


def _safe_property(actor, property_name):
    try:
        return str(actor.get_editor_property(property_name))
    except Exception as error:
        return "<unreadable: %s>" % error


def _raw_property(actor, property_name):
    try:
        return actor.get_editor_property(property_name)
    except Exception:
        return None


def _report_enemy_configuration(map_path, label):
    """Read-only comparison of the placed enemy and its inherited AI defaults."""
    unreal.EditorLevelLibrary.load_level(map_path)
    world = unreal.EditorLevelLibrary.get_editor_world()
    game_mode_class = world.get_world_settings().get_editor_property("default_game_mode")
    _log("%s default game mode: %s" % (label, game_mode_class))
    if game_mode_class:
        game_mode_cdo = unreal.get_default_object(game_mode_class)
        _log("%s default pawn: %s" % (
            label, _safe_property(game_mode_cdo, "default_pawn_class")
        ))
        _log("%s player controller: %s" % (
            label, _safe_property(game_mode_cdo, "player_controller_class")
        ))
    enemies = _actors(world, "/Script/BrokenHorizon.BHEnemySoldier")
    _log("%s enemy count: %d" % (label, len(enemies)))
    for enemy in enemies:
        _log("%s class: %s" % (label, enemy.get_class().get_path_name()))
        for property_name in (
            "auto_possess_ai", "ai_controller_class", "sight_radius",
            "lose_sight_radius", "hearing_range", "shot_damage",
            "fire_interval", "shot_range", "maximum_engagement_distance",
            "objective_id_to_complete_on_death", "patrol_points",
        ):
            _log("%s %s: %s" % (
                label, property_name, _safe_property(enemy, property_name)
            ))


def validate():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        _log("FAIL: map does not exist: " + MAP_PATH)
        return False
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    counts = {
        "PlayerStart": len(_actors(world, "/Script/Engine.PlayerStart")),
        "Keycard": len(_actors(world, "/Script/BrokenHorizon.BHKeycard")),
        "Door": len(_actors(world, "/Script/BrokenHorizon.BHDoor")),
        "Enemy": len(_actors(world, "/Script/BrokenHorizon.BHEnemySoldier")),
        "CoverPoint": len(_actors(world, "/Script/BrokenHorizon.BHCoverPoint")),
        "PatrolPoint": len(_actors(world, "/Script/BrokenHorizon.BHPatrolPoint")),
        "Checkpoint": len(_actors(world, "/Script/BrokenHorizon.BHCheckpoint")),
        "AmmoSupply": len(_actors(world, "/Script/BrokenHorizon.BHAmmoSupply")),
        "MedicalSupply": len(_actors(world, "/Script/BrokenHorizon.BHMedicalSupply")),
        "Extraction": len(_actors(world, "/Script/BrokenHorizon.BHExtractionZone")),
        "NavMeshBounds": len(_actors(world, "/Script/NavigationSystem.NavMeshBoundsVolume")),
        "BlockingGeometry": len(_actors(world, "/Script/Engine.StaticMeshActor")),
        "GeneratedTag": len(unreal.GameplayStatics.get_all_actors_with_tag(world, TAG)),
    }
    for name, count in counts.items():
        _log("%s: %d" % (name, count))
    keycards = _actors(world, "/Script/BrokenHorizon.BHKeycard")
    doors = _actors(world, "/Script/BrokenHorizon.BHDoor")
    enemies = _actors(world, "/Script/BrokenHorizon.BHEnemySoldier")
    canonical_guards = [
        enemy for enemy in enemies
        if enemy.get_actor_label().startswith(CANONICAL_GUARD_LABEL_PREFIX)
    ]
    extractions = _actors(world, "/Script/BrokenHorizon.BHExtractionZone")
    _log("Keycard IDs: " + ", ".join(_names(keycards, "keycard_id")))
    _log("Door required keycards: " + ", ".join(_names(doors, "required_keycard")))
    _log("Door persistence IDs: " + ", ".join(_names(doors, "persistence_id")))
    _log("Enemy objective IDs: " + ", ".join(_names(enemies, "objective_id_to_complete_on_death")))
    _log("Canonical guard count: %d" % len(canonical_guards))
    _log("Canonical guard objective IDs: " + ", ".join(_names(canonical_guards, "objective_id_to_complete_on_death")))
    _log("Extraction prerequisites: " + ", ".join(_names(extractions, "required_objective_id")))

    required_counts = (
        counts["PlayerStart"] >= 1
        and counts["Keycard"] == 1
        and counts["Door"] == 1
        and counts["Enemy"] >= 3
        and counts["CoverPoint"] >= 6
        and counts["PatrolPoint"] >= 3
        and counts["Checkpoint"] == 1
        and counts["AmmoSupply"] == 1
        and counts["MedicalSupply"] == 1
        and counts["Extraction"] == 1
        and counts["NavMeshBounds"] >= 1
    )
    keycard_contract = (
        len(keycards) == 1
        and _safe_property(keycards[0], "keycard_id") == EXPECTED_KEYCARD_ID
        and _safe_property(keycards[0], "persistence_id")
        == EXPECTED_KEYCARD_PERSISTENCE_ID
    )
    door_contract = (
        len(doors) == 1
        and bool(_raw_property(doors[0], "locked"))
        and _safe_property(doors[0], "required_keycard")
        == EXPECTED_KEYCARD_ID
        and _safe_property(doors[0], "persistence_id")
        == EXPECTED_DOOR_PERSISTENCE_ID
    )
    guard_contract = (
        len(canonical_guards) == 3
        and {
            _safe_property(guard, "objective_id_to_complete_on_death")
            for guard in canonical_guards
        }
        == {EXPECTED_GUARD_OBJECTIVE_ID}
    )
    extraction_contract = (
        len(extractions) == 1
        and _safe_property(extractions[0], "required_objective_id")
        == EXPECTED_GUARD_OBJECTIVE_ID
        and _safe_property(extractions[0], "extraction_objective_id")
        == EXPECTED_EXTRACTION_OBJECTIVE_ID
    )
    required = (
        required_counts
        and keycard_contract
        and door_contract
        and guard_contract
        and extraction_contract
    )
    _log(
        "PASS: First Light route contracts are complete"
        if required
        else "FAIL: First Light actor counts or route contracts are incomplete"
    )
    _report_enemy_configuration(MAP_PATH, "FirstLight")
    if unreal.EditorAssetLibrary.does_asset_exist(PROTOTYPE_MAP_PATH):
        _report_enemy_configuration(PROTOTYPE_MAP_PATH, "Prototype")
    return required


if __name__ == "__main__":
    validate()
