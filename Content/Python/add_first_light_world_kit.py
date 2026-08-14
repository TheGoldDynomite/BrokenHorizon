"""Idempotently author the first modular facility kit in First Light.

The native actor is intentionally reusable and source-driven; this script is
the authored placement layer for named sites in the current theater slice.
Final project-owned meshes, materials, LODs, collision, and visual review
remain release gates.
"""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
WORLD_KIT_CLASS = "/Script/BrokenHorizon.BHWorldKitModule"
TAG = "BH_Auto_FirstLight_WorldKit"

MODULES = (
    ("FirstLight Western FOB Kit", "FirstLightWesternFOBKit01", "RESISTANCE_BASE", (-9500.0, -4500.0, 0.0)),
    ("FirstLight Dovren Checkpoint Kit", "FirstLightDovrenCheckpoint01", "CHECKPOINT", (-5200.0, -2850.0, 0.0)),
    ("FirstLight Eastern Depot Kit", "FirstLightEasternDepotKit01", "DEPOT", (9000.0, 4500.0, 0.0)),
)


def _find_module(persistence_id, label):
    matches = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == label:
            matches.append(actor)
            continue
        try:
            if actor.get_editor_property("persistence_id") == unreal.Name(
                persistence_id
            ):
                matches.append(actor)
        except Exception:
            pass

    if not matches:
        return None

    primary = matches[0]
    for duplicate in matches[1:]:
        unreal.EditorLevelLibrary.destroy_actor(duplicate)
        unreal.log_warning(
            "[FirstLight WorldKit] Removed duplicate %s" % duplicate.get_name()
        )
    return primary


def _ensure_module(label, persistence_id, module_type, location):
    module = _find_module(persistence_id, label)
    if module is None:
        world_kit_class = unreal.load_class(None, WORLD_KIT_CLASS)
        if not world_kit_class:
            unreal.log_error("[FirstLight WorldKit] Native class unavailable")
            return None
        module = unreal.EditorLevelLibrary.spawn_actor_from_class(
            world_kit_class,
            unreal.Vector(*location),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if not module:
            unreal.log_error("[FirstLight WorldKit] Spawn failed for %s" % label)
            return None
        module.set_actor_label(label)
        module.tags = list(module.tags) + [unreal.Name(TAG)]
        module.set_editor_property("persistence_id", unreal.Name(persistence_id))

    module.set_actor_location(unreal.Vector(*location), False, True)
    module.configure_module_type_for_authoring(module_type)
    return module


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    if not unreal.EditorLevelLibrary.get_editor_world():
        unreal.log_error("[FirstLight WorldKit] Editor world unavailable")
        return False

    authored = []
    for label, persistence_id, module_type, location in MODULES:
        module = _ensure_module(label, persistence_id, module_type, location)
        if module is None:
            return False
        authored.append(module)

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log(
        "[FirstLight WorldKit] Authored modules=%d ids=%s"
        % (
            len(authored),
            ",".join(persistence_id for _, persistence_id, _, _ in MODULES),
        )
    )
    return True


main()
