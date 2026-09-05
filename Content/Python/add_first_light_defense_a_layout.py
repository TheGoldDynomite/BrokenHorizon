"""Author the Defense A facility perimeter and fallback cover layout."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_DefenseA_Layout"
SUPPORT_TAG = "BH_Auto_DefenseA_Support"
FOLDER = "FirstLight/Operations/DefenseA/Layout"
PAD_LOCATION = (-9500, -4500, -25)
PAD_DIMENSIONS = (1800, 1400, 50)
NAV_LOCATION = (-9500, -4500, 300)
NAV_SCALE = (10.0, 8.0, 5.0)


def _native(path):
    result = unreal.load_class(None, path)
    if not result:
        raise RuntimeError("Missing native class: " + path)
    return result


def _spawn_collision_pad(subsystem):
    pad = subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(*PAD_LOCATION),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not pad:
        raise RuntimeError("Could not spawn Defense A collision pad")

    component = pad.get_editor_property("static_mesh_component")
    cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if not component or not cube_mesh:
        raise RuntimeError("Engine cube is unavailable for Defense A collision pad")

    component.set_editor_property("static_mesh", cube_mesh)
    pad.set_actor_scale3d(unreal.Vector(
        PAD_DIMENSIONS[0] / 100.0,
        PAD_DIMENSIONS[1] / 100.0,
        PAD_DIMENSIONS[2] / 100.0,
    ))
    pad.tags = list(pad.tags) + [SUPPORT_TAG]
    pad.set_actor_label("FL_DefenseA_FacilityPad")
    pad.set_folder_path(unreal.Name(FOLDER))
    return pad


def _spawn_navigation_bounds(subsystem):
    nav = subsystem.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume,
        unreal.Vector(*NAV_LOCATION),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not nav:
        raise RuntimeError("Could not spawn Defense A navigation bounds")

    nav.set_actor_scale3d(unreal.Vector(*NAV_SCALE))
    nav.tags = list(nav.tags) + [SUPPORT_TAG]
    nav.set_actor_label("FL_DefenseA_NavMeshBounds")
    nav.set_folder_path(unreal.Name(FOLDER))
    return nav


def author():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing First Light map: " + MAP_PATH)

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cover_class = _native("/Script/BrokenHorizon.BHCoverPoint")

    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, TAG):
        subsystem.destroy_actor(actor)
    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, SUPPORT_TAG):
        subsystem.destroy_actor(actor)

    # The operation site is outside the compact First Light corridor. Give
    # the authored garrison a real collision floor and a local nav volume so
    # placed enemies remain grounded before a Defense A operation activates.
    collision_pad = _spawn_collision_pad(subsystem)
    navigation_bounds = _spawn_navigation_bounds(subsystem)

    # Facility ring, two lateral fallback positions, and a rear casualty /
    # recovery line. All positions remain close enough to the authored site
    # to support a readable defense rather than a distant spawn circle.
    placements = (
        ((-9800, -5000, 95), "FacilityWest"),
        ((-9200, -5000, 95), "FacilityEast"),
        ((-9800, -4400, 95), "NorthWest"),
        ((-9200, -4400, 95), "NorthEast"),
        ((-10200, -4700, 95), "OuterWest"),
        ((-8800, -4700, 95), "OuterEast"),
        ((-9800, -3900, 95), "FallbackWest"),
        ((-9200, -3900, 95), "FallbackEast"),
        ((-9500, -3500, 95), "RecoveryLine"),
    )

    for index, (location, suffix) in enumerate(placements, 1):
        cover = subsystem.spawn_actor_from_class(
            cover_class,
            unreal.Vector(*location),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if not cover:
            raise RuntimeError("Could not spawn Defense A cover point")
        cover.tags = list(cover.tags) + [TAG]
        cover.set_actor_label("FL_DefenseA_Cover_%02d_%s" % (index, suffix))
        cover.set_folder_path(unreal.Name(FOLDER))

    covers = unreal.GameplayStatics.get_all_actors_with_tag(world, TAG)
    if len(covers) != len(placements):
        raise RuntimeError(
            "Expected %d Defense A cover points, found %d"
            % (len(placements), len(covers))
        )
    support_actors = (
        unreal.GameplayStatics.get_all_actors_with_tag(world, SUPPORT_TAG)
    )
    if collision_pad not in support_actors or navigation_bounds not in support_actors:
        raise RuntimeError("Defense A collision/nav support was not authored")

    if hasattr(unreal, "EditorBuildUtils"):
        unreal.EditorBuildUtils.editor_build(
            world, unreal.EditorBuildType.PATHS
        )
    elif hasattr(unreal.SystemLibrary, "execute_console_command"):
        unreal.SystemLibrary.execute_console_command(world, "BUILDPATHS")
    else:
        unreal.log_warning(
            "[FirstLight Defense A Layout] EditorBuildUtils is unavailable; "
            "navigation will rebuild dynamically during play."
        )

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log(
        "[FirstLight Defense A Layout] PASS: cover=%d "
        "perimeter=fallback=recovery pad=1 nav=1" % len(covers)
    )
    return True


if __name__ == "__main__":
    author()
