"""Replace only First Light's generated guard with the verified Prototype Blueprint.

Preserves transform, tag, label, folder, and the assigned patrol route. It
does not modify any untagged actor or other map.
"""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_FirstLight"
GUARD_LABEL = "FL_Guard"
ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"


def _log(message):
    unreal.log("[FirstLight Guard Repair] " + message)


def repair():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("First Light map was not found: " + MAP_PATH)
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    enemy_class = unreal.EditorAssetLibrary.load_blueprint_class(ENEMY_BLUEPRINT)
    enemy_base_class = unreal.load_class(None, "/Script/BrokenHorizon.BHEnemySoldier")
    if not enemy_class or not enemy_base_class:
        raise RuntimeError("Verified enemy Blueprint/class could not be loaded.")

    guards = [
        actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, enemy_base_class)
        if TAG in actor.tags and actor.get_actor_label() == GUARD_LABEL
    ]
    if len(guards) != 1:
        raise RuntimeError("Expected exactly one generated First Light guard; found %d." % len(guards))
    old_guard = guards[0]
    if old_guard.get_class() == enemy_class:
        _log("Guard already uses the verified BP_EnemySoldier Blueprint; no change needed.")
        return True

    location = old_guard.get_actor_location()
    rotation = old_guard.get_actor_rotation()
    scale = old_guard.get_actor_scale3d()
    patrol_points = old_guard.get_editor_property("patrol_points")
    tags = list(old_guard.tags)
    label = old_guard.get_actor_label()
    folder = old_guard.get_folder_path()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not subsystem.destroy_actor(old_guard):
        raise RuntimeError("Could not remove the generated child-Blueprint guard.")
    guard = subsystem.spawn_actor_from_class(enemy_class, location, rotation)
    if not guard:
        raise RuntimeError("Could not place the verified BP_EnemySoldier guard.")
    guard.set_actor_scale3d(scale)
    guard.tags = tags
    guard.set_actor_label(label)
    guard.set_folder_path(folder)
    guard.set_editor_property("patrol_points", patrol_points)
    unreal.EditorLevelLibrary.save_current_level()
    _log("Replaced FL_Guard with the verified BP_EnemySoldier Blueprint and preserved patrol points.")
    return True


if __name__ == "__main__":
    repair()
