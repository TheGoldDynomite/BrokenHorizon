"""Read-only world-map mesh-cost audit for the 1.0 performance gate."""

import unreal
from collections import defaultdict


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"


def _triangles(mesh):
    if not mesh:
        return 0
    try:
        return int(mesh.get_num_triangles(0))
    except Exception:
        try:
            return int(mesh.get_editor_property("num_triangles"))
        except Exception:
            return 0


def audit():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing world map: " + MAP_PATH)
    if not unreal.EditorLevelLibrary.load_level(MAP_PATH):
        raise RuntimeError("Could not load world map: " + MAP_PATH)

    grouped = defaultdict(lambda: {"instances": 0, "triangles": 0, "actors": []})
    component_classes = defaultdict(int)
    landscape_actors = 0
    instanced_components = 0
    instanced_instances = 0
    skeletal_components = 0
    landscape_components = 0
    landscape_quads = 0
    landscape_tiles = []
    total_instances = 0
    total_estimated_triangles = 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class().get_name().lower().find("landscape") >= 0:
            landscape_actors += 1
            for landscape_component in actor.get_components_by_class(
                unreal.LandscapeComponent
            ):
                landscape_components += 1
                try:
                    subsection_size = int(
                        landscape_component.get_editor_property(
                            "subsection_size_quads"
                        )
                    )
                    subsection_count = int(
                        landscape_component.get_editor_property(
                            "num_subsections"
                        )
                    )
                    size = subsection_size * subsection_count
                    landscape_quads += size * size
                except Exception:
                    subsection_size = 0
                    subsection_count = 0
                    size = 0
                if len(landscape_tiles) < 12:
                    landscape_tiles.append(
                        "%s:%d/%d/%d"
                        % (
                            actor.get_actor_label(),
                            subsection_size,
                            subsection_count,
                            size,
                        )
                    )
        for component in actor.get_components_by_class(
            unreal.InstancedStaticMeshComponent
        ):
            instanced_components += 1
            try:
                instanced_instances += int(component.get_instance_count())
            except Exception:
                pass
            component_classes[component.get_class().get_name()] += 1
        for component in actor.get_components_by_class(
            unreal.SkeletalMeshComponent
        ):
            skeletal_components += 1
            component_classes[component.get_class().get_name()] += 1
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            continue
        mesh = component.get_editor_property("static_mesh")
        if not mesh:
            continue
        path = mesh.get_path_name()
        triangles = _triangles(mesh)
        entry = grouped[path]
        entry["instances"] += 1
        entry["triangles"] += triangles
        if len(entry["actors"]) < 3:
            entry["actors"].append(actor.get_actor_label())
        total_instances += 1
        total_estimated_triangles += triangles

    rows = sorted(
        grouped.items(),
        key=lambda item: item[1]["triangles"],
        reverse=True,
    )
    unreal.log(
        "BH_WORLD_MESH_AUDIT map=%s actors=%d mesh_instances=%d "
        "estimated_triangles=%d unique_meshes=%d"
        % (
            MAP_PATH,
            len(unreal.EditorLevelLibrary.get_all_level_actors()),
            total_instances,
            total_estimated_triangles,
            len(rows),
        )
    )
    unreal.log(
        "BH_WORLD_RENDER_COMPONENT_AUDIT landscape_actors=%d "
        "landscape_components=%d landscape_quad_area=%d "
        "landscape_samples=%s instanced_components=%d instanced_instances=%d "
        "skeletal_components=%d component_classes=%s"
        % (
            landscape_actors,
            landscape_components,
            landscape_quads,
            ",".join(landscape_tiles),
            instanced_components,
            instanced_instances,
            skeletal_components,
            ";".join(
                "%s:%d" % (name, count)
                for name, count in sorted(component_classes.items())
            ),
        )
    )
    for rank, (path, entry) in enumerate(rows[:25], 1):
        unreal.log(
            "BH_WORLD_MESH_COST rank=%d instances=%d triangles=%d "
            "mesh=%s examples=%s"
            % (
                rank,
                entry["instances"],
                entry["triangles"],
                path,
                ",".join(entry["actors"]),
            )
        )


if __name__ == "__main__":
    audit()
