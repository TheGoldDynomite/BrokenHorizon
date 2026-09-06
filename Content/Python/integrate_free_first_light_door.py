"""Integrate the imported free bunker door into First Light; default inspect, --apply saves.

Run --apply in the full Unreal Editor via -ExecutePythonScript. Only the target
map and imported mesh collision are saved; imported geometry is not rebuilt.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import uuid
import unreal
import repair_first_light_door_shoreline as inspection
from author_first_light_security_door import door_state

MAP_PATH = inspection.MAP_PATH
FREE_MESH = "/Game/BrokenHorizon/Environment/FreeMetalDoor/bunkerdoor"
OLD_MESH = "/Game/BrokenHorizon/Environment/WorldKit/Meshes/SM_FirstLightSecurityDoor.SM_FirstLightSecurityDoor"
MATERIAL = "/Game/BrokenHorizon/Environment/WorldKit/Materials/M_BH_Military"
PANEL_LABEL = "FL_SecurityDoorSidePanel"
MINIMUM = [-61.0878296, -8.7121019, 0.1590304]
MAXIMUM = [53.653244, 12.3557663, 204.5534973]
SCALE = 200.0 / (MAXIMUM[2] - MINIMUM[2])
WIDTH = (MAXIMUM[0] - MINIMUM[0]) * SCALE
LOCAL_LOCATION = [0, -MINIMUM[0] * SCALE, -MINIMUM[2] * SCALE]
PANEL_LOCATION = [4200, -50 + WIDTH + (200 - WIDTH) / 2, 100]
PANEL_SCALE = [0.16, (200 - WIDTH) / 100, 2]
require = inspection.require


def assets():
    mesh = unreal.load_asset(FREE_MESH)
    material = unreal.load_asset(MATERIAL)
    cube = unreal.load_asset(inspection.CUBE)
    require(mesh is not None and material is not None and cube is not None, "Required existing asset is missing")
    bounds = mesh.get_bounding_box()
    require(inspection.near(inspection.vector(bounds.min), MINIMUM)
            and inspection.near(inspection.vector(bounds.max), MAXIMUM), "Imported door geometry changed")
    editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    require(editor is not None, "Use full UnrealEditor -ExecutePythonScript for collision inspection")
    require(editor.get_simple_collision_count(mesh) <= 1, "Unexpected imported collision shapes")
    return mesh, material, cube


def configure_door(door, mesh, material):
    """Shared with the graybox generator; preserve the native door root and behavior."""
    require(unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(mesh) == 1,
            "Run free door integration --apply to initialize collision before generating the map")
    component = door.get_editor_property("door_mesh")
    component.set_static_mesh(mesh)
    component.set_editor_property("relative_location", unreal.Vector(*LOCAL_LOCATION))
    component.set_editor_property("relative_rotation", unreal.Rotator(pitch=0, yaw=90, roll=0))
    component.set_editor_property("relative_scale3d", unreal.Vector(SCALE, SCALE, SCALE))
    component.set_editor_property("override_materials", [])
    for index in range(component.get_num_materials()):
        component.set_material(index, material)


def configure_panel(panel, cube, material):
    panel.set_actor_label(PANEL_LABEL)
    panel.set_editor_property("tags", [unreal.Name("BH_Auto_FirstLight")])
    panel.set_actor_location(unreal.Vector(*PANEL_LOCATION), False, True)
    panel.set_actor_scale3d(unreal.Vector(*PANEL_SCALE))
    component = panel.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(cube)
    component.set_material(0, material)
    component.set_collision_profile_name("BlockAll")
    component.set_mobility(unreal.ComponentMobility.STATIC)


def validate_pose(door, panel):
    state = door_state(door)
    require(inspection.near(state["relativeLocation"], LOCAL_LOCATION)
            and inspection.near(state["relativeRotation"], [0, 90, 0])
            and inspection.near(state["relativeScale"], [SCALE] * 3), "Imported door pose mismatch")
    door_mesh = door.get_editor_property("door_mesh")
    require(door_mesh.get_editor_property("static_mesh").get_path_name() == FREE_MESH + ".bunkerdoor",
            "Door mesh assignment mismatch")
    require(all(door_mesh.get_material(i).get_path_name() == MATERIAL + ".M_BH_Military"
                for i in range(door_mesh.get_num_materials())), "Door material assignment mismatch")
    require(panel.get_attach_parent_actor() is None, "Side panel must remain stationary and unattached")
    record = inspection.actor_record(panel)
    require(record["class"] == "/Script/Engine.StaticMeshActor" and record["tags"] == ["BH_Auto_FirstLight"]
            and inspection.near(record["location"], PANEL_LOCATION)
            and inspection.near(record["rotation"], [0, 0, 0]) and inspection.near(record["scale"], PANEL_SCALE)
            and len(record["meshes"]) == 1 and record["meshes"][0]["mesh"] == inspection.CUBE
            and record["meshes"][0]["materials"] == [MATERIAL + ".M_BH_Military"]
            and record["meshes"][0]["collisionProfile"] == "BlockAll", "Side panel mismatch")
    return state


def main(apply=False):
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    output = project / "Saved/FreeFirstLightDoor" / (datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8])
    output.mkdir(parents=True, exist_ok=False)
    report = {"mode": "apply" if apply else "inspect", "result": "failure", "map": MAP_PATH}
    try:
        require(not inspection.dirty_packages(), "Refusing map load with dirty packages")
        mesh, material, cube = assets()
        map_file = project / "Content/BrokenHorizon/Maps/L_FirstLight_Graybox.umap"
        original = map_file.read_bytes()
        report["beforeSha256"] = hashlib.sha256(original).hexdigest()
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        require(unreal.EditorLoadingAndSavingUtils.new_blank_map(False) is not None, "Could not unload clean map")
        require(editor.load_level(MAP_PATH), "Could not load First Light")
        require(not inspection.dirty_packages(), "Map load dirtied packages")
        actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors = list(actors_api.get_all_level_actors())
        before = inspection.inventory(actors)
        door = inspection.unique(actors, "FL_LockedSecurityDoor")
        state = door_state(door)
        report["beforeDoor"] = state
        require(state["actor"]["class"] == "/Game/BP_Door.BP_Door_C"
                and "BH_Auto_FirstLight" in state["actor"]["tags"]
                and state["gameplay"]["persistence_id"] == "FirstLightSecurityDoor"
                and state["gameplay"]["required_keycard"] == "RedKeycard"
                and state["gameplay"]["locked"] == "True" and state["gameplay"]["is_open"] == "False"
                and inspection.near(state["actor"]["location"], [4200, -50, 0])
                and inspection.near(state["actor"]["rotation"], [0, 0, 0])
                and inspection.near(state["actor"]["scale"], [1, 1, 1]), "Unexpected door identity/configuration/transform")
        current_mesh = door.get_editor_property("door_mesh").get_editor_property("static_mesh").get_path_name()
        corrected = current_mesh == FREE_MESH + ".bunkerdoor"
        panels = [actor for actor in actors if actor.get_actor_label() == PANEL_LABEL]
        if corrected:
            require(len(panels) == 1, "Expected exactly one side panel")
            validate_pose(door, panels[0])
        else:
            require(current_mesh == OLD_MESH and not panels
                    and inspection.near(state["relativeLocation"], [0, 100, 100])
                    and inspection.near(state["relativeRotation"], [0, 0, 0])
                    and inspection.near(state["relativeScale"], [1, 1, 1]), "Unexpected old door mesh/pose or side panel")
        collision_count = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(mesh)
        if corrected:
            require(collision_count == 1, "Integrated door lacks expected collision")
        report.update(alreadyCorrected=corrected, uniformScale=SCALE, leafWidth=WIDTH, collisionCount=collision_count)
        if apply and not corrected:
            require(map_file.read_bytes() == original, "Map changed during inspection")
            backup = output / map_file.name
            backup.write_bytes(original)
            require(backup.read_bytes() == original, "Map backup verification failed")
            report["backup"] = str(backup)
            mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
            if mesh_editor.get_simple_collision_count(mesh) == 0:
                mesh_file = project / "Content/BrokenHorizon/Environment/FreeMetalDoor/bunkerdoor.uasset"
                mesh_bytes = mesh_file.read_bytes()
                mesh_backup = output / mesh_file.name
                mesh_backup.write_bytes(mesh_bytes)
                require(mesh_backup.read_bytes() == mesh_bytes, "Imported mesh backup failed")
                report["meshBackup"] = str(mesh_backup)
                require(mesh_editor.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX) >= 0,
                        "Could not add imported door collision")
                require(mesh_editor.get_simple_collision_count(mesh) == 1, "Expected one door collision shape")
                require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), "Imported mesh collision save failed")
            configure_door(door, mesh, material)
            panel = actors_api.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*PANEL_LOCATION), unreal.Rotator(0, 0, 0))
            require(panel is not None, "Could not create side panel")
            configure_panel(panel, cube, material)
            validate_pose(door, panel)
            require(door_state(door)["gameplay"] == state["gameplay"], "Door gameplay changed")
            after = inspection.inventory(list(actors_api.get_all_level_actors()))
            require(set(after) - set(before) == {panel.get_path_name()} and len(after) == len(before) + 1,
                    "Unexpected actor addition/removal")
            require(all(after.get(path) == record for path, record in before.items() if path != door.get_path_name()),
                    "Unrelated actor changed")
            require(set(inspection.dirty_packages()).issubset({MAP_PATH}), "Unexpected dirty assets; refusing save")
            require(editor.save_current_level(), "Target map save failed")
            require(not inspection.dirty_packages(), "Dirty packages remain after saving")
            require(unreal.EditorLoadingAndSavingUtils.new_blank_map(False) is not None, "Could not unload map")
            require(editor.load_level(MAP_PATH), "Saved map reload failed")
            actors = list(actors_api.get_all_level_actors())
            require(inspection.inventory(actors) == after, "Serialized inventory differs")
            door = inspection.unique(actors, "FL_LockedSecurityDoor")
            panel = inspection.unique(actors, PANEL_LABEL)
            validate_pose(door, panel)
            require(door_state(door)["gameplay"] == state["gameplay"], "Serialized gameplay differs")
            require(unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(
                unreal.load_asset(FREE_MESH)) == 1, "Saved door collision missing")
            report["serializedReloadVerified"] = True
        elif not apply:
            require(map_file.read_bytes() == original, "Inspection changed map")
        require(not inspection.dirty_packages(), "Unexpected dirty packages at completion")
        report.update(result="success", afterDoor=door_state(door), actorsBefore=len(before), actorsAfter=len(actors),
                      afterSha256=hashlib.sha256(map_file.read_bytes()).hexdigest())
        unreal.log("BH_FREE_FIRST_LIGHT_DOOR result=success mode=%s report=%s" % (report["mode"], output / "report.json"))
    except Exception as error:
        report["error"] = str(error)
        unreal.log_error("BH_FREE_FIRST_LIGHT_DOOR result=failure reason=" + str(error))
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    main(apply=parser.parse_args().apply)
