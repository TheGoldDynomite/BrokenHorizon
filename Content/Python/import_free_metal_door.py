"""Import a locally licensed bunkerdoor.fbx without overwriting existing assets.

Pass --source with your local FBX path in the Unreal Editor Python invocation.
Then run integrate_free_first_light_door.py --apply to add collision and place it.
"""
import argparse
import hashlib
import json
from pathlib import Path
import unreal

DESTINATION = "/Game/BrokenHorizon/Environment/FreeMetalDoor"


def main(source):
    source = Path(source).resolve()
    if not source.is_file() or source.suffix.lower() != ".fbx":
        raise RuntimeError("Supply an existing locally licensed bunkerdoor.fbx")
    if source.stem.lower() != "bunkerdoor":
        raise RuntimeError("Expected bunkerdoor.fbx for the scoped First Light integration")
    if unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        raise RuntimeError("Destination already exists; refusing overwrite")
    if (unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or
            unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
        raise RuntimeError("Refusing import with dirty Editor packages")
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.automated = True
    task.save = True
    task.replace_existing = False
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = True
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.auto_generate_collision = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(DESTINATION + "/bunkerdoor")
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Import did not produce expected bunkerdoor static mesh")
    bounds = mesh.get_bounding_box()
    report = {"source": str(source), "sourceSha256": hashlib.sha256(source.read_bytes()).hexdigest(),
              "mesh": mesh.get_path_name(), "minimum": [bounds.min.x, bounds.min.y, bounds.min.z],
              "maximum": [bounds.max.x, bounds.max.y, bounds.max.z],
              "assets": list(unreal.EditorAssetLibrary.list_assets(DESTINATION, recursive=True))}
    output = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / "Reports/FreeDoorImport.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("BH_FREE_DOOR_IMPORT result=success report=" + str(output))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    main(parser.parse_args().source)
