"""Author First Light's hinged security door. Default inspect; --apply saves asset/map.

Existing materials and gameplay configuration are preserved. Run in a fresh
Editor Python process; a verified map backup precedes any authoring.
Apply requires full UnrealEditor.exe -nullrhi -ExecutePythonScript="<script> --apply".
Do not use -run=pythonscript: commandlets lack StaticMeshEditorSubsystem. A rendered
RHI process can crash rebuilding the duplicated mesh's initialized render resources.
"""
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import uuid
import unreal
import repair_first_light_door_shoreline as inspection

MAP_PATH = inspection.MAP_PATH
ASSET_PATH = "/Game/BrokenHorizon/Environment/WorldKit/Meshes/SM_FirstLightSecurityDoor"
LEGACY_MESH = "/Game/LevelPrototyping/Interactable/Door/Meshes/SM_Door.SM_Door"
MATERIAL_ROOT = "/Game/BrokenHorizon/Environment/WorldKit/Materials/"
MATERIAL_NAMES = ("M_BH_Military", "M_BH_Roof", "M_BH_Warning")
VERSION = "FirstLightSecurityDoor_v1"
require = inspection.require


def door_state(door):
    component = door.get_editor_property("door_mesh")
    rotation = component.get_editor_property("relative_rotation")
    return {
        "actor": inspection.actor_record(door),
        "relativeLocation": inspection.vector(component.get_editor_property("relative_location")),
        "relativeRotation": [rotation.pitch, rotation.yaw, rotation.roll],
        "relativeScale": inspection.vector(component.get_editor_property("relative_scale3d")),
        "gameplay": {key: str(door.get_editor_property(key)) for key in
                     ("locked", "required_keycard", "persistence_id", "open_angle", "door_open_speed", "is_open")},
    }


def validate_mesh(mesh):
    require(unreal.EditorAssetLibrary.get_metadata_tag(mesh, "BHAuthoringVersion") == VERSION,
            "Existing target mesh has unknown provenance; refusing overwrite")
    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    require(mesh_editor.get_simple_collision_count(mesh) == 1,
            "Authored door has no simple collision")
    box = mesh.get_bounding_box()
    require(inspection.near(inspection.vector(box.min), [-9, -100, -100])
            and inspection.near(inspection.vector(box.max), [9, 100, 100]), "Authored door bounds mismatch")
    require([mesh.get_material(i).get_path_name() for i in range(3)] ==
            [MATERIAL_ROOT + name + "." + name for name in MATERIAL_NAMES], "Authored materials mismatch")
    return {"minimum": inspection.vector(box.min), "maximum": inspection.vector(box.max)}


def author_mesh():
    materials = [unreal.load_asset(MATERIAL_ROOT + name) for name in MATERIAL_NAMES]
    require(all(materials), "Required existing military/roof/warning materials missing")
    require(not unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH),
            "Refusing to overwrite an existing destination mesh")
    # Duplicate a known UStaticMesh container; the complete LOD0 description is replaced below.
    mesh = unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Cube", ASSET_PATH)
    require(mesh is not None, "Could not create security door mesh")
    slots = []
    for material, name in zip(materials, MATERIAL_NAMES):
        slot = unreal.StaticMaterial(material_interface=material, material_slot_name=unreal.Name(name))
        # The slow builder falls back to polygon-group index when imported names are absent.
        slots.append(slot)
    mesh.set_editor_property("static_materials", slots)
    description = mesh.create_static_mesh_description()
    # Keep fresh polygon groups 0/1/2 in the exact same order as material slots 0/1/2.
    groups = []
    for name in MATERIAL_NAMES:
        group = description.create_polygon_group()
        description.set_polygon_group_material_slot_name(group, unreal.Name(name))
        groups.append(group)

    def box(center, half_extent, material):
        # UE CreateCube does not reliably apply Center. Its six returned polygons
        # identify this box's vertices; shared corners must move exactly once.
        polygons = description.create_cube(unreal.Vector(0, 0, 0), unreal.Vector(*half_extent), groups[material])
        vertices = {}
        for polygon in polygons:
            for vertex in description.get_polygon_vertices(polygon):
                vertices[vertex.get_editor_property("id_value")] = vertex
        require(len(vertices) == 8, "Cube did not expose eight unique vertices")
        offset = unreal.Vector(*center)
        for vertex in vertices.values():
            description.set_vertex_position(vertex, description.get_vertex_position(vertex) + offset)

    box((0, 0, 0), (4, 100, 100), 0)  # Thin steel leaf; width/height match existing opening.
    for face in (-1, 1):
        # Recessed-looking dark panels surrounded by the steel leaf on both faces.
        for z in (-48, 43):
            box((face * 4.25, 0, z), (0.25, 77, 33), 1)
            for y in (-79, 79):
                box((face * 4.6, y, z), (0.6, 1.5, 35), 0)
            for border_z in (z - 35, z + 35):
                box((face * 4.6, 0, border_z), (0.6, 80, 1.5), 0)
        # Warning plate, latch escutcheon, stand-offs, and graspable horizontal lever.
        box((face * 4.6, 0, 88), (0.6, 33, 5), 2)
        box((face * 5, 76, -4), (1, 7, 14), 1)
        box((face * 6.5, 76, 0), (2.5, 2, 2), 1)
        box((face * 8, 63, 0), (1, 15, 2), 1)
        for z in (-70, 0, 70):
            box((face * 5, -96, z), (1, 4, 8), 1)
    mesh.build_from_static_mesh_descriptions([description], build_simple_collision=True, fast_build=False)
    # The full editor build ignores build_simple_collision; author it explicitly.
    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    require(mesh_editor.remove_collisions(mesh), "Could not remove inherited cube collision")
    require(mesh_editor.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX) >= 0,
            "Could not add the door's simple box collision")
    require(mesh_editor.get_simple_collision_count(mesh) == 1,
            "Door collision was not created")
    unreal.EditorAssetLibrary.set_metadata_tag(mesh, "BHAuthoringVersion", VERSION)
    validate_mesh(mesh)
    return mesh


def main(apply=False):
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    output = project / "Saved/FirstLightSecurityDoor" / (datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8])
    output.mkdir(parents=True, exist_ok=False)
    report = {"mode": "apply" if apply else "inspect", "result": "failure", "map": MAP_PATH, "asset": ASSET_PATH}
    try:
        if apply:
            require(unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) is not None,
                    "Apply requires full UnrealEditor.exe -nullrhi -ExecutePythonScript, not a commandlet")
            require("-nullrhi" in unreal.SystemLibrary.get_command_line().lower().split(),
                    "Apply requires -nullrhi to safely rebuild the duplicated mesh render resources")
        require(not inspection.dirty_packages(), "Refusing map load with dirty packages")
        map_file = project / "Content/BrokenHorizon/Maps/L_FirstLight_Graybox.umap"
        original = map_file.read_bytes()
        report["beforeSha256"] = hashlib.sha256(original).hexdigest()
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        require(unreal.EditorLoadingAndSavingUtils.new_blank_map(False) is not None, "Could not unload clean map")
        require(editor.load_level(MAP_PATH), "Could not load First Light")
        require(not inspection.dirty_packages(), "Map load dirtied packages")
        actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors = list(actors_api.get_all_level_actors())
        door = inspection.unique(actors, "FL_LockedSecurityDoor")
        before = inspection.inventory(actors)
        state = door_state(door)
        report["beforeDoor"] = state
        require(state["actor"]["class"] == "/Game/BP_Door.BP_Door_C"
                and "BH_Auto_FirstLight" in state["actor"]["tags"]
                and state["gameplay"]["persistence_id"] == "FirstLightSecurityDoor"
                and state["gameplay"]["required_keycard"] == "RedKeycard"
                and state["gameplay"]["locked"] == "True" and state["gameplay"]["is_open"] == "False",
                "Unexpected door identity or lock state")
        require(inspection.near(state["actor"]["rotation"], [0, 0, 0])
                and inspection.near(state["actor"]["scale"], [1, 1, 1])
                and inspection.near(state["relativeRotation"], [0, 0, 0])
                and inspection.near(state["relativeScale"], [1, 1, 1]), "Unexpected door rotation/scale")
        component = door.get_editor_property("door_mesh")
        existing_path = component.get_editor_property("static_mesh").get_path_name()
        corrected = existing_path == ASSET_PATH + ".SM_FirstLightSecurityDoor"
        require(inspection.near(state["actor"]["location"], [4200, -50 if corrected else 0, 0])
                and inspection.near(state["relativeLocation"], [0, 100 if corrected else 50, 100])
                and (corrected or existing_path == LEGACY_MESH), "Unexpected door mesh or hinge placement")
        mesh = unreal.load_asset(ASSET_PATH) if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH) else None
        if mesh:
            report["meshBounds"] = validate_mesh(mesh)
        report["alreadyCorrected"] = corrected
        if apply and not corrected:
            require(map_file.read_bytes() == original, "Map changed during inspection")
            backup = output / map_file.name
            backup.write_bytes(original)
            require(backup.read_bytes() == original, "Map backup failed")
            report["backup"] = str(backup)
            if mesh is None:
                mesh = author_mesh()
                require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), "Mesh save failed")
            component.set_static_mesh(mesh)
            component.set_editor_property("override_materials", [])
            component.set_editor_property("relative_location", unreal.Vector(0, 100, 100))
            door.set_actor_location(unreal.Vector(4200, -50, 0), False, True)
            after = inspection.inventory(list(actors_api.get_all_level_actors()))
            expected = copy.deepcopy(before)
            expected[door.get_path_name()] = after[door.get_path_name()]
            require(after == expected, "An unrelated actor changed")
            require(door_state(door)["gameplay"] == state["gameplay"], "Door gameplay configuration changed")
            require(set(inspection.dirty_packages()).issubset({MAP_PATH}), "Unexpected dirty package before map save")
            require(editor.save_current_level(), "Map save failed")
            require(not inspection.dirty_packages(), "Dirty packages remain after saving")
            require(unreal.EditorLoadingAndSavingUtils.new_blank_map(False) is not None, "Could not unload map")
            require(editor.load_level(MAP_PATH), "Saved map reload failed")
            actors = list(actors_api.get_all_level_actors())
            require(inspection.inventory(actors) == expected, "Serialized actor inventory changed")
            door = inspection.unique(actors, "FL_LockedSecurityDoor")
            require(door_state(door)["gameplay"] == state["gameplay"], "Serialized door configuration changed")
            report["serializedReloadVerified"] = True
        elif not apply:
            require(map_file.read_bytes() == original, "Read-only inspection changed map")
        require(not inspection.dirty_packages(), "Unexpected dirty packages at completion")
        report.update(result="success", afterDoor=door_state(door), actorCount=len(before),
                      afterSha256=hashlib.sha256(map_file.read_bytes()).hexdigest())
        unreal.log("BH_FIRST_LIGHT_SECURITY_DOOR result=success mode=%s report=%s" % (report["mode"], output / "report.json"))
    except Exception as error:
        report["error"] = str(error)
        unreal.log_error("BH_FIRST_LIGHT_SECURITY_DOOR result=failure reason=" + str(error))
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    main(apply=parser.parse_args().apply)
