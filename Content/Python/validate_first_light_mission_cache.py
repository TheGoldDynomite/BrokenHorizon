"""Read-only validation for the authored First Light mission cache."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
CACHE_CLASS_PATH = "/Script/BrokenHorizon.BHMissionItemContainer"
PERSISTENCE_ID = "FirstLightMissionCache01"
MISSION_ITEM_ID = "RedKeycard"
TAG = "BH_Auto_FirstLight_MissionCache"


def _log(message):
    unreal.log("[FirstLight Mission Cache Validator] " + message)


def _property_name(actor, property_name):
    try:
        return str(actor.get_editor_property(property_name))
    except Exception as error:
        return "<unreadable: %s>" % error


def validate():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        _log("FAIL: map does not exist: " + MAP_PATH)
        return False

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    cache_class = unreal.load_class(None, CACHE_CLASS_PATH)
    if not world or not cache_class:
        _log("FAIL: world or native cache class unavailable")
        return False

    caches = unreal.GameplayStatics.get_all_actors_of_class(world, cache_class)
    tagged = unreal.GameplayStatics.get_all_actors_with_tag(world, TAG)
    _log("Cache count: %d" % len(caches))
    _log("Tagged cache count: %d" % len(tagged))

    if len(caches) == 1:
        cache = caches[0]
        _log("PersistenceID: " + _property_name(cache, "persistence_id"))
        _log("MissionItemID: " + _property_name(cache, "mission_item_id"))
        _log("StoredMissionItemID: " + _property_name(cache, "stored_mission_item_id"))
        _log("Location: %s" % cache.get_actor_location())

        valid = (
            _property_name(cache, "persistence_id") == PERSISTENCE_ID
            and _property_name(cache, "mission_item_id") == MISSION_ITEM_ID
            and cache in tagged
        )
    else:
        valid = False

    _log(
        "PASS: First Light mission cache contract is complete"
        if valid
        else "FAIL: First Light mission cache contract is incomplete"
    )
    return valid


if __name__ == "__main__":
    validate()
