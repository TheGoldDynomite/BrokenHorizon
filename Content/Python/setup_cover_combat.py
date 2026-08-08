"""Upgrade the existing First Light map to the v0.14 cover encounter."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
TAG = "BH_Auto_Cover_v014"
GAMEPLAY_FOLDER = "FirstLight/Gameplay"


def _log(message):
    unreal.log("[BH Cover Combat Setup] " + message)


def _class(path):
    result = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not result:
        raise RuntimeError("Missing Blueprint class: " + path)
    return result


def _native_class(path):
    result = unreal.load_class(None, path)
    if not result:
        raise RuntimeError("Missing native class: " + path)
    return result


def _spawn(actor_class, location, label):
    subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    actor = subsystem.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not actor:
        raise RuntimeError("Could not spawn " + label)
    actor.tags = list(actor.tags) + [TAG]
    actor.set_actor_label(label)
    actor.set_folder_path(unreal.Name(GAMEPLAY_FOLDER))
    return actor


def _actors(world, class_path):
    actor_class = _native_class(class_path)
    return unreal.GameplayStatics.get_all_actors_of_class(
        world,
        actor_class,
    )


def setup():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing First Light map: " + MAP_PATH)

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )

    for actor in unreal.GameplayStatics.get_all_actors_with_tag(
        world,
        TAG,
    ):
        subsystem.destroy_actor(actor)

    enemies = _actors(
        world,
        "/Script/BrokenHorizon.BHEnemySoldier",
    )
    patrol_points = _actors(
        world,
        "/Script/BrokenHorizon.BHPatrolPoint",
    )

    if not enemies:
        raise RuntimeError("First Light has no existing guard.")
    if len(patrol_points) < 3:
        raise RuntimeError("First Light needs three patrol points.")

    enemies[0].set_actor_label("FL_Guard_01")
    enemies[0].set_editor_property("patrol_points", patrol_points)

    enemy_class = _class(ENEMY_BLUEPRINT)
    for index, location in enumerate(
        ((6150, -450, 95), (6650, 450, 95)),
        2,
    ):
        enemy = _spawn(
            enemy_class,
            location,
            "FL_Guard_%02d" % index,
        )
        enemy.set_editor_property("patrol_points", patrol_points)

    cover_class = _native_class(
        "/Script/BrokenHorizon.BHCoverPoint"
    )
    for index, location in enumerate(
        (
            (5525, -430, 95),
            (5775, 430, 95),
            (6700, -420, 95),
            (7000, 420, 95),
            (7850, -300, 95),
            (7850, 300, 95),
        ),
        1,
    ):
        _spawn(
            cover_class,
            location,
            "FL_CoverPoint_%02d" % index,
        )

    if len(_actors(
        world,
        "/Script/BrokenHorizon.BHEnemySoldier",
    )) != 3:
        raise RuntimeError("Expected exactly three guards after setup.")

    unreal.EditorLevelLibrary.save_current_level()
    _log("PASS: saved three guards and six cover points.")
    return True


setup()
