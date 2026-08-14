"""Author the collision and navigation footprint for First Light Attack A."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_AttackA_Ground"
FOLDER = "FirstLight/Operations/AttackA/Ground"


def author():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, TAG):
        subsystem.destroy_actor(actor)

    floor = subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(6100.0, 0.0, -10.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not floor:
        raise RuntimeError("Could not spawn Attack A ground")
    floor.tags = list(floor.tags) + [TAG]
    floor.set_actor_label("FL_AttackA_Ground_Collision")
    floor.set_folder_path(unreal.Name(FOLDER))
    component = floor.get_editor_property("static_mesh_component")
    component.set_editor_property("static_mesh", unreal.load_asset("/Engine/BasicShapes/Cube.Cube"))
    floor.set_actor_scale3d(unreal.Vector(22.0, 18.0, 0.2))

    nav = subsystem.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume,
        unreal.Vector(6100.0, 0.0, 300.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not nav:
        raise RuntimeError("Could not spawn Attack A navigation bounds")
    nav.tags = list(nav.tags) + [TAG]
    nav.set_actor_label("FL_AttackA_Navigation")
    nav.set_folder_path(unreal.Name(FOLDER))
    nav.set_actor_scale3d(unreal.Vector(11.0, 9.0, 5.0))

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[FirstLight Attack A Ground] PASS: collision=1 navigation=1")
    return True


if __name__ == "__main__":
    author()
