"""Inspect First Light's door obstruction; pass --apply to lower only its west bank.

Run in a fresh Unreal Editor Python process. A verified map backup precedes any
mutation; only the fixed map is saved. Actor identities/transforms, mesh/material
and collision are compared again after unloading and reloading the saved map.
"""

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import uuid

import unreal

MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
WEST_LABEL = "FirstLightWaterShoreWest"
WEST_TAG = "BH_Auto_FirstLight_Shoreline"
CUBE = "/Engine/BasicShapes/Cube.Cube"
MATERIAL = "/Game/BrokenHorizon/Environment/Materials/M_BH_Terrain_Prototype.M_BH_Terrain_Prototype"
TARGET_Z = -41.0
TOLERANCE = 0.001


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def near(actual, expected):
    return len(actual) == len(expected) and all(abs(a - b) <= TOLERANCE for a, b in zip(actual, expected))


def dirty_packages():
    return sorted({package.get_path_name() for package in
        list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
        list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())})


def actor_record(actor):
    rotation = actor.get_actor_rotation()
    record = {
        "path": actor.get_path_name(), "label": actor.get_actor_label(),
        "class": actor.get_class().get_path_name(),
        "tags": sorted(str(tag) for tag in actor.tags),
        "location": vector(actor.get_actor_location()),
        "rotation": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
        "scale": vector(actor.get_actor_scale3d()),
        "meshes": [],
    }
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
        record["meshes"].append({
            "path": component.get_path_name(),
            "mesh": mesh.get_path_name() if mesh else None,
            "materials": [component.get_material(i).get_path_name() if component.get_material(i) else None
                          for i in range(component.get_num_materials())],
            "collisionProfile": str(component.get_collision_profile_name()),
            "collisionEnabled": str(component.get_collision_enabled()),
        })
    return record


def inventory(actors):
    result = {actor.get_path_name(): actor_record(actor) for actor in actors}
    require(len(result) == len(actors), "Duplicate actor paths")
    return result


def unique(actors, label):
    matches = [actor for actor in actors if actor.get_actor_label() == label]
    require(len(matches) == 1, "Expected exactly one actor labeled " + label)
    actor = matches[0]
    require(actor.get_path_name().startswith(MAP_PATH + ".L_FirstLight_Graybox:PersistentLevel."),
            "Actor is outside target persistent level: " + label)
    return actor


def bounds_top(actor):
    origin, extent = actor.get_actor_bounds(False)
    return float(origin.z + extent.z)


def preflight(actors):
    west = unique(actors, WEST_LABEL)
    record = actor_record(west)
    require(west.get_name() == "StaticMeshActor_0" and record["class"] == "/Script/Engine.StaticMeshActor"
            and record["tags"] == [WEST_TAG], "West bank identity changed")
    require(len(record["meshes"]) == 1 and record["meshes"][0]["mesh"] == CUBE
            and record["meshes"][0]["materials"] == [MATERIAL]
            and record["meshes"][0]["collisionProfile"] == "BlockAll", "West bank mesh/material/collision changed")
    require(near(record["rotation"], [0, 0, 0]) and near(record["scale"], [7, 10, 0.8])
            and (near(record["location"], [4300, 300, 80]) or
                 near(record["location"], [4300, 300, TARGET_Z])), "Unexpected west bank transform")
    require(abs(bounds_top(west) - (record["location"][2] + 40)) <= TOLERANCE,
            "West bank bounds differ from expected cube bounds")
    ground = unique(actors, "FL_Ground")
    ground_record = actor_record(ground)
    require(ground_record["class"] == "/Script/Engine.StaticMeshActor"
            and "BH_Auto_FirstLight" in ground_record["tags"]
            and near(ground_record["location"], [4200, 0, -25])
            and near(ground_record["rotation"], [0, 0, 0])
            and near(ground_record["scale"], [90, 30, 0.5])
            and len(ground_record["meshes"]) == 1 and ground_record["meshes"][0]["mesh"] == CUBE
            and abs(bounds_top(ground)) <= TOLERANCE, "FL_Ground identity, transform or surface changed")
    door = unique(actors, "FL_LockedSecurityDoor")
    door_record = actor_record(door)
    require(door_record["class"] == "/Game/BP_Door.BP_Door_C"
            and "BH_Auto_FirstLight" in door_record["tags"]
            and (near(door_record["location"], [4200, 0, 0]) or near(door_record["location"], [4200, -50, 0]))
            and near(door_record["rotation"], [0, 0, 0]) and near(door_record["scale"], [1, 1, 1])
            and str(door.get_editor_property("persistence_id")) == "FirstLightSecurityDoor",
            "Security door identity or transform changed")
    return west, ground


def main(apply=False):
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    map_file = project / "Content/BrokenHorizon/Maps/L_FirstLight_Graybox.umap"
    output = project / "Saved/FirstLightDoorShorelineRepair" / (
        datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8])
    output.mkdir(parents=True, exist_ok=False)
    report = {"map": MAP_PATH, "mode": "apply" if apply else "inspect", "result": "failure"}
    try:
        require(not dirty_packages(), "Refusing map load with dirty Editor packages: " + repr(dirty_packages()))
        require(map_file.is_file(), "Target map file is missing")
        original = map_file.read_bytes()
        report["beforeSha256"] = hashlib.sha256(original).hexdigest()
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        # Drop any clean previously loaded map so inspection starts from disk.
        require(unreal.EditorLoadingAndSavingUtils.new_blank_map(False) is not None, "Could not unload current map")
        require(editor.load_level(MAP_PATH), "Could not load target map")
        require(not dirty_packages(), "Map load dirtied packages; review before repair")
        actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors = list(actors_api.get_all_level_actors())
        west, ground = preflight(actors)
        before = inventory(actors)
        report.update(beforeActors=before, actorCount=len(before), groundTop=bounds_top(ground),
                      westTopBefore=bounds_top(west), alreadyCorrected=abs(west.get_actor_location().z - TARGET_Z) <= TOLERANCE)
        if apply and not report["alreadyCorrected"]:
            require(map_file.read_bytes() == original, "Map file changed during inspection")
            backup = output / map_file.name
            backup.write_bytes(original)
            require(backup.read_bytes() == original, "Map backup verification failed")
            report["backup"] = str(backup)
            expected = copy.deepcopy(before)
            expected[west.get_path_name()]["location"][2] = TARGET_Z
            west.set_actor_location(unreal.Vector(4300, 300, TARGET_Z), False, True)
            require(inventory(list(actors_api.get_all_level_actors())) == expected,
                    "Unexpected actor identity, transform, mesh, material or collision change")
            require(bounds_top(west) <= bounds_top(ground) - 1.0 + TOLERANCE, "Bank still obstructs ground surface")
            require(set(dirty_packages()).issubset({MAP_PATH}), "Unexpected package dirtied; refusing save")
            require(editor.save_current_level(), "Target map save failed")
            require(not dirty_packages(), "Dirty packages remain after target map save")
            require(unreal.EditorLoadingAndSavingUtils.new_blank_map(False) is not None, "Could not unload saved map")
            require(editor.load_level(MAP_PATH), "Could not reload saved map")
            actors = list(actors_api.get_all_level_actors())
            west, ground = preflight(actors)
            require(inventory(actors) == expected, "Saved actor inventory differs after reload")
            require(bounds_top(west) <= bounds_top(ground) - 1.0 + TOLERANCE, "Saved bank still obstructs surface")
            report["serializedReloadVerified"] = True
        elif not apply:
            require(map_file.read_bytes() == original, "Map changed during read-only inspection")
        require(not dirty_packages(), "Unexpected dirty packages at completion")
        report.update(result="success", westTopAfter=bounds_top(west),
                      afterSha256=hashlib.sha256(map_file.read_bytes()).hexdigest())
        unreal.log("BH_FIRST_LIGHT_DOOR_SHORELINE result=success mode=%s actors=%d west_top=%s report=%s" %
                   (report["mode"], len(before), report["westTopAfter"], output / "report.json"))
    except Exception as error:
        report["error"] = str(error)
        unreal.log_error("BH_FIRST_LIGHT_DOOR_SHORELINE result=failure reason=" + str(error))
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Back up and lower only the existing west bank")
    main(apply=parser.parse_args().apply)
