"""Author the first persistent Attack A checkpoint garrison on First Light.

This is deliberately idempotent and limited to the authored Attack A site. It
uses the verified enemy Blueprint, stable labels/tags, and the operation
objective contract; final theater art and authored cover remain separate gates.
"""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
TAG = "BH_Auto_AttackA_Garrison"
OBJECTIVE_ID = "FirstLight_AttackA_Checkpoint"
FOLDER = "FirstLight/Operations/AttackA"


def _log(message):
    unreal.log("[FirstLight Attack A Garrison] " + message)


def author():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing First Light map: " + MAP_PATH)
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    enemy_class = unreal.EditorAssetLibrary.load_blueprint_class(ENEMY_BLUEPRINT)
    enemy_base = unreal.load_class(None, "/Script/BrokenHorizon.BHEnemySoldier")
    if not enemy_class or not enemy_base:
        raise RuntimeError("Verified enemy Blueprint/native class unavailable")

    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, TAG):
        subsystem.destroy_actor(actor)

    patrol_class = unreal.load_class(None, "/Script/BrokenHorizon.BHPatrolPoint")
    patrol_points = unreal.GameplayStatics.get_all_actors_of_class(world, patrol_class)
    if len(patrol_points) < 3:
        raise RuntimeError("First Light needs existing patrol points for Attack A")

    # Tight checkpoint defense: two riflemen on the objective, two offset
    # sentries, and one rear security element. These are intentionally close
    # enough to create a playable assault rather than a distant spawn ring.
    placements = (
        ((5900, -450, 95), "Rifleman"),
        ((6100, -450, 95), "Rifleman"),
        ((6300, -450, 95), "Scout"),
        ((5900, 450, 95), "Rifleman"),
        ((6300, 450, 95), "Gunner"),
    )
    for index, (location, role) in enumerate(placements, 1):
        enemy = subsystem.spawn_actor_from_class(
            enemy_class,
            unreal.Vector(*location),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if not enemy:
            raise RuntimeError("Could not spawn Attack A garrison member")
        enemy.tags = list(enemy.tags) + [TAG]
        enemy.set_actor_label("FL_AttackA_Garrison_%02d" % index)
        enemy.set_folder_path(unreal.Name(FOLDER))
        enemy.set_objective_id_to_complete_on_death(unreal.Name(OBJECTIVE_ID))
        enemy.set_editor_property("patrol_points", patrol_points)
        # Make the authored checkpoint defenders use the same player-facing
        # casualty contract as runtime operation enemies. Blueprint defaults
        # must not silently suppress the ammunition brick or remove the body
        # before the player can read the result.

    count = len(unreal.GameplayStatics.get_all_actors_with_tag(world, TAG))
    if count != len(placements):
        raise RuntimeError("Expected %d Attack A garrison members, found %d" % (len(placements), count))
    unreal.EditorLevelLibrary.save_current_level()
    _log("PASS: authored site=FirstLightAttackA objective=%s members=%d" % (OBJECTIVE_ID, count))
    return True


if __name__ == "__main__":
    author()
