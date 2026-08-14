"""Author the operation-gated Defense A facility garrison."""

import unreal

MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
TAG = "BH_Auto_DefenseA_Garrison"
FOLDER = "FirstLight/Operations/DefenseA"


def author():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    enemy_class = unreal.EditorAssetLibrary.load_blueprint_class(ENEMY_BLUEPRINT)
    if not enemy_class:
        raise RuntimeError("Verified enemy Blueprint unavailable")

    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, TAG):
        subsystem.destroy_actor(actor)

    placements = (
        ((-9800, -4700, 95), "West"),
        ((-9200, -4700, 95), "East"),
        ((-9800, -4300, 95), "NorthWest"),
        ((-9200, -4300, 95), "NorthEast"),
        ((-10100, -4500, 95), "OuterWest"),
        ((-8900, -4500, 95), "OuterEast"),
    )
    for index, (location, role) in enumerate(placements, 1):
        enemy = subsystem.spawn_actor_from_class(
            enemy_class, unreal.Vector(*location), unreal.Rotator(0.0, 0.0, 0.0)
        )
        if not enemy:
            raise RuntimeError("Could not spawn Defense A garrison member")
        enemy.tags = list(enemy.tags) + [TAG]
        enemy.set_actor_label("FL_DefenseA_Garrison_%02d_%s" % (index, role))
        enemy.set_folder_path(unreal.Name(FOLDER))

    count = len(unreal.GameplayStatics.get_all_actors_with_tag(world, TAG))
    if count != len(placements):
        raise RuntimeError("Expected %d Defense A garrison members, found %d" % (len(placements), count))
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[FirstLight Defense A Garrison] PASS: members=%d" % count)
    return True


if __name__ == "__main__":
    author()
