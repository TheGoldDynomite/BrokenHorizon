"""Idempotently author a small battlefield salvage cache in First Light.

This script is deliberately separate from the graybox builder so it can add
content to an existing map without rebuilding or deleting any authored actor.
"""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_FirstLight_Salvage"
PICKUP_CLASS = "/Script/BrokenHorizon.BHSalvagePickup"
PICKUP_ID = "FirstLightSalvageCache01"
LOCATION = unreal.Vector(6250.0, 420.0, 90.0)


def _find_existing(world):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_name().startswith("FirstLightSalvageCache01"):
            return actor
    return None


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        unreal.log_error("[FirstLight Salvage] Editor world unavailable")
        return False

    existing = _find_existing(world)
    if existing:
        unreal.log("[FirstLight Salvage] Existing salvage cache preserved")
        return True

    pickup_class = unreal.load_class(None, PICKUP_CLASS)
    if not pickup_class:
        unreal.log_error("[FirstLight Salvage] Native pickup class unavailable")
        return False

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        pickup_class, LOCATION, unreal.Rotator(0.0, 0.0, 0.0)
    )
    if not actor:
        unreal.log_error("[FirstLight Salvage] Spawn failed")
        return False

    actor.set_actor_label("FirstLight Salvage Cache")
    actor.tags = list(actor.tags) + [unreal.Name(TAG)]
    actor.set_editor_property("persistence_id", unreal.Name(PICKUP_ID))
    actor.set_editor_property("quantity", 30)
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[FirstLight Salvage] Authored %s at %s" % (PICKUP_ID, LOCATION))
    return True


main()
