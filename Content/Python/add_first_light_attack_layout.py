"""Author the playable Attack A checkpoint cover and approach layout."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_AttackA_Layout"
GARRISON_TAG = "BH_Auto_AttackA_Garrison"
FOLDER = "FirstLight/Operations/AttackA/Layout"


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
    enemy_class = _native("/Script/BrokenHorizon.BHEnemySoldier")

    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, TAG):
        subsystem.destroy_actor(actor)

    # The inner pair protects the checkpoint, the side pairs support fire and
    # movement, and the rear pair forms a bounded fallback rather than a
    # distant circular spawn ring.
    placements = (
        ((5800, -600, 95), "InnerWest"),
        ((6200, -600, 95), "InnerEast"),
        ((5800, 600, 95), "NorthWest"),
        ((6200, 600, 95), "NorthEast"),
        ((5500, -450, 95), "ApproachWest"),
        ((6700, -450, 95), "ApproachEast"),
        ((5500, 450, 95), "FallbackWest"),
        ((6700, 450, 95), "FallbackEast"),
    )
    for index, (location, suffix) in enumerate(placements, 1):
        cover = subsystem.spawn_actor_from_class(
            cover_class,
            unreal.Vector(*location),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if not cover:
            raise RuntimeError("Could not spawn Attack A cover point")
        cover.tags = list(cover.tags) + [TAG]
        cover.set_actor_label("FL_AttackA_Cover_%02d_%s" % (index, suffix))
        cover.set_folder_path(unreal.Name(FOLDER))

    covers = unreal.GameplayStatics.get_all_actors_with_tag(world, TAG)
    enemies = unreal.GameplayStatics.get_all_actors_with_tag(world, GARRISON_TAG)
    if len(covers) != len(placements):
        raise RuntimeError("Expected %d Attack A cover points, found %d" % (len(placements), len(covers)))
    if len(enemies) != 5:
        raise RuntimeError("Expected five Attack A garrison members, found %d" % len(enemies))
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[FirstLight Attack A Layout] PASS: cover=%d garrison=%d approach=bounded_fallback" % (len(covers), len(enemies)))
    return True


if __name__ == "__main__":
    author()
