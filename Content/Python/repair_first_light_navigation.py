"""Rebuild serialized First Light navigation without replacing map content."""

import importlib.util
import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
BUILDER_PATH = "C:/UnrealProjects/BrokenHorizon/Content/Python/build_first_light_graybox.py"
LOCAL_NAV_LABEL = "FL_AttackA_NavigationLocal"


def _ensure_attack_a_navigation(world):
    """Keep nav generation explicit around the authored Attack A compound."""
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    local_volume = next(
        (actor for actor in actors if actor.get_actor_label() == LOCAL_NAV_LABEL), None
    )
    if local_volume is None:
        local_volume = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.NavMeshBoundsVolume, unreal.Vector(6100.0, 0.0, 300.0)
        )
        if local_volume is None:
            raise RuntimeError("Could not create local Attack A navigation volume")
        local_volume.set_actor_label(LOCAL_NAV_LABEL)
        local_volume.tags = ["BH_Auto_AttackA_Navigation"]
    local_volume.set_actor_location(unreal.Vector(6100.0, 0.0, 300.0), False, True)
    local_volume.set_actor_scale3d(unreal.Vector(13.0, 9.0, 5.0))
    try:
        local_volume.set_editor_property("is_spatially_loaded", False)
    except Exception:
        pass

    for actor in actors:
        label = actor.get_actor_label()
        if label in ("FL_CompoundFloor", "FL_CompoundRoad"):
            component = actor.get_editor_property("static_mesh_component")
            try:
                component.set_editor_property("can_ever_affect_navigation", True)
            except Exception as error:
                unreal.log_warning(
                    "BH_FIRST_LIGHT_NAV_REPAIR nav-affecting property unavailable actor=%s error=%s"
                    % (actor.get_actor_label(), error)
                )
        elif label in ("FL_CompoundCoverSouth", "FL_CompoundCoverNorth"):
            component = actor.get_editor_property("static_mesh_component")
            try:
                component.set_editor_property("can_ever_affect_navigation", False)
            except Exception as error:
                unreal.log_warning(
                    "BH_FIRST_LIGHT_NAV_REPAIR cover nav property unavailable actor=%s error=%s"
                    % (label, error)
                )


def main():
    spec = importlib.util.spec_from_file_location("bh_first_light_builder", BUILDER_PATH)
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem.load_level(MAP_PATH):
        raise RuntimeError("Could not load First Light map")

    world = unreal.get_editor_subsystem(
        unreal.UnrealEditorSubsystem
    ).get_editor_world()
    _ensure_attack_a_navigation(world)
    if not builder._build_navigation_in_world(world):
        raise RuntimeError("First Light navigation build reported failure")

    if not unreal.EditorAssetLibrary.save_loaded_asset(
        unreal.EditorAssetLibrary.load_asset(MAP_PATH)
    ):
        raise RuntimeError("Could not save First Light map")

    unreal.log("BH_FIRST_LIGHT_NAV_REPAIR result=success map=%s" % MAP_PATH)


main()
