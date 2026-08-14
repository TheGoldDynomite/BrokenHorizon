"""Idempotently author the first armored-threat encounter in First Light."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
THREAT_CLASS = "/Script/BrokenHorizon.BHArmoredThreat"
THREAT_ID = "FirstLightArmoredThreat01"
TAG = "BH_Auto_FirstLight_ArmoredThreat"
LOCATION = unreal.Vector(7600.0, 300.0, 120.0)


def _find_existing():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_name().startswith(THREAT_ID):
            return actor
    return None


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    if not unreal.EditorLevelLibrary.get_editor_world():
        unreal.log_error("[FirstLight ArmoredThreat] Editor world unavailable")
        return False

    if _find_existing():
        unreal.log("[FirstLight ArmoredThreat] Existing threat preserved")
        return True

    threat_class = unreal.load_class(None, THREAT_CLASS)
    if not threat_class:
        unreal.log_error("[FirstLight ArmoredThreat] Native threat class unavailable")
        return False

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        threat_class, LOCATION, unreal.Rotator(0.0, 180.0, 0.0)
    )
    if not actor:
        unreal.log_error("[FirstLight ArmoredThreat] Spawn failed")
        return False

    actor.set_actor_label("FirstLight Armored Threat")
    actor.tags = list(actor.tags) + [unreal.Name(TAG)]
    actor.set_editor_property("persistence_id", unreal.Name(THREAT_ID))
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[FirstLight ArmoredThreat] Authored %s at %s" % (THREAT_ID, LOCATION))
    return True


main()
