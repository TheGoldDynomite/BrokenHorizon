"""Read-only validator for /Game/BrokenHorizon/Maps/L_FirstLight_Graybox."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
PROTOTYPE_MAP_PATH = "/Game/BrokenHorizon/Maps/L_Prototype"
TAG = "BH_Auto_FirstLight"


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
    _log("Keycard IDs: " + ", ".join(_names(_actors(world, "/Script/BrokenHorizon.BHKeycard"), "keycard_id")))
    _log("Door required keycards: " + ", ".join(_names(_actors(world, "/Script/BrokenHorizon.BHDoor"), "required_keycard")))
    _log("Door persistence IDs: " + ", ".join(_names(_actors(world, "/Script/BrokenHorizon.BHDoor"), "persistence_id")))
    _log("Enemy objective IDs: " + ", ".join(_names(_actors(world, "/Script/BrokenHorizon.BHEnemySoldier"), "objective_id_to_complete_on_death")))
    _log("Extraction prerequisites: " + ", ".join(_names(_actors(world, "/Script/BrokenHorizon.BHExtractionZone"), "required_objective_id")))
    required = (counts["PlayerStart"] >= 1 and counts["Keycard"] == 1 and counts["Door"] == 1 and counts["Enemy"] == 3 and counts["CoverPoint"] >= 6 and counts["PatrolPoint"] >= 3 and counts["Checkpoint"] == 1 and counts["AmmoSupply"] == 1 and counts["MedicalSupply"] == 1 and counts["Extraction"] == 1 and counts["NavMeshBounds"] >= 1)
    _log("PASS" if required else "FAIL: required actor count is incomplete")
    _report_enemy_configuration(MAP_PATH, "FirstLight")
    if unreal.EditorAssetLibrary.does_asset_exist(PROTOTYPE_MAP_PATH):
        _report_enemy_configuration(PROTOTYPE_MAP_PATH, "Prototype")
    return required


if __name__ == "__main__":
    validate()
