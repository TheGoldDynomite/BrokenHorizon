"""Author the Defense A facility perimeter and fallback cover layout."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_DefenseA_Layout"
FOLDER = "FirstLight/Operations/DefenseA/Layout"


def _native(path):
    result = unreal.load_class(None, path)
    if not result:
        raise RuntimeError("Missing native class: " + path)
    return result


def author():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing First Light map: " + MAP_PATH)

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cover_class = _native("/Script/BrokenHorizon.BHCoverPoint")

    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, TAG):
        subsystem.destroy_actor(actor)

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

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log(
        "[FirstLight Defense A Layout] PASS: cover=%d "
        "perimeter=fallback=recovery" % len(covers)
    )
    return True


if __name__ == "__main__":
    author()
