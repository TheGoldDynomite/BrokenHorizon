"""Idempotently author the First Light mission-item cache.

This script adds exactly one production ``ABHMissionItemContainer`` to the
existing First Light map. It never removes or replaces actors and refuses to
create a duplicate stable persistence identity.
"""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
CACHE_CLASS_PATH = "/Script/BrokenHorizon.BHMissionItemContainer"
PERSISTENCE_ID = "FirstLightMissionCache01"
MISSION_ITEM_ID = "RedKeycard"
TAG = "BH_Auto_FirstLight_MissionCache"
FOLDER = "FirstLight/Gameplay/MissionCache"
LOCATION = unreal.Vector(4900.0, 450.0, 90.0)


def _log(message):
    unreal.log("[FirstLight Mission Cache] " + message)


def _error(message):
    unreal.log_error("[FirstLight Mission Cache] " + message)


def _native_cache_class():
    cache_class = unreal.load_class(None, CACHE_CLASS_PATH)
    if not cache_class:
        raise RuntimeError("Native mission cache class unavailable: " + CACHE_CLASS_PATH)
    return cache_class


def _find_caches(world, cache_class):
    return unreal.GameplayStatics.get_all_actors_of_class(world, cache_class)


def _property_name(actor, property_name):
    return str(actor.get_editor_property(property_name))


def author():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing First Light map: " + MAP_PATH)

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("Editor world unavailable after loading " + MAP_PATH)

    cache_class = _native_cache_class()
    caches = _find_caches(world, cache_class)
    matching = [
        actor for actor in caches
        if _property_name(actor, "persistence_id") == PERSISTENCE_ID
    ]
    if len(matching) > 1:
        raise RuntimeError(
            "Duplicate mission cache persistence ID: %s" % PERSISTENCE_ID
        )

    if matching:
        existing = matching[0]
        existing_item_id = _property_name(existing, "mission_item_id")
        if existing_item_id != MISSION_ITEM_ID:
            raise RuntimeError(
                "Existing %s uses mission item %s, expected %s"
                % (PERSISTENCE_ID, existing_item_id, MISSION_ITEM_ID)
            )
        _log(
            "Existing cache preserved: %s item=%s"
            % (PERSISTENCE_ID, existing_item_id)
        )
        return True

    tagged = unreal.GameplayStatics.get_all_actors_with_tag(world, TAG)
    if tagged:
        raise RuntimeError(
            "A mission-cache authoring tag already exists without %s"
            % PERSISTENCE_ID
        )

    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = subsystem.spawn_actor_from_class(
        cache_class,
        LOCATION,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not actor:
        raise RuntimeError("Could not spawn mission cache")

    actor.set_actor_label("FirstLight Mission Cache")
    actor.set_folder_path(unreal.Name(FOLDER))
    actor.tags = list(actor.tags) + [unreal.Name(TAG)]
    actor.set_editor_property("persistence_id", unreal.Name(PERSISTENCE_ID))
    actor.set_editor_property("mission_item_id", unreal.Name(MISSION_ITEM_ID))

    unreal.EditorLevelLibrary.save_current_level()
    _log(
        "Authored %s item=%s at %s"
        % (PERSISTENCE_ID, MISSION_ITEM_ID, LOCATION)
    )
    return True


if __name__ == "__main__":
    author()
