"""Read-only GDD-08 asset readiness audit for Broken Horizon.

Run through UnrealEditor-Cmd. The audit never saves or modifies assets; it emits
machine-readable evidence under Saved/Reports and concise markers in the log.
"""

import collections
import datetime
import json
import os
import warnings

import unreal


SCAN_ROOTS = (
    "/Game/BrokenHorizon",
    "/Game/Characters",
    "/Game/InfimaGames/FreeFPSTemplate/Art/AssaultRifle",
    "/Game/Weapons",
)
EXTRA_ASSETS = ("/Game/BP_Rifle",)
SHIPPING_ROOTS = (
    "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox",
    "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World",
    "/Game/BrokenHorizon/Maps/L_MainMenu",
    "/Game/Characters/BP_EnemySoldier",
    "/Game/BP_Rifle",
)
SHIPPING_PREFIXES = ("/Game/BrokenHorizon/Core/",)
PLACEHOLDER_TERMS = ("placeholder", "temp", "template", "scratch", "default", "_dflt")
MAX_TEXTURE_DIMENSION = 4096
LARGE_NON_STREAMING_DIMENSION = 2048
MAX_REFERENCE_DEPTH = 8


def _asset_class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        try:
            return str(asset_data.asset_class)
        except Exception:
            return "Unknown"


def _object_path(asset_data):
    try:
        return str(asset_data.get_soft_object_path()).split(".", 1)[0]
    except Exception:
        return str(asset_data.package_name)


def _read_property(obj, name, default=None):
    if obj is None:
        return default
    try:
        return obj.get_editor_property(name)
    except Exception:
        try:
            return getattr(obj, name)
        except Exception:
            return default


def _import_sources(asset):
    import_data = _read_property(asset, "asset_import_data", None)
    if import_data is None:
        return []
    try:
        return [str(item) for item in import_data.extract_filenames()]
    except Exception:
        return []


def _bounds_metrics(asset):
    try:
        bounds = asset.get_bounds()
        extent = bounds.box_extent
        return {
            "extentCm": [round(float(extent.x), 3), round(float(extent.y), 3), round(float(extent.z), 3)],
            "sphereRadiusCm": round(float(bounds.sphere_radius), 3),
        }
    except Exception:
        return {"extentCm": None, "sphereRadiusCm": None}


def _static_mesh_review_metrics(mesh, lod_count, collision_count):
    metrics = _bounds_metrics(mesh)
    metrics.update({
        "lodCount": lod_count,
        "simpleCollisionCount": collision_count,
        "materialSlots": len(_read_property(mesh, "static_materials", []) or []),
        "naniteEnabled": bool(_read_property(_read_property(mesh, "nanite_settings", None), "enabled", False)),
        "sourceFiles": _import_sources(mesh),
    })
    try:
        metrics["lod0Vertices"] = int(unreal.EditorStaticMeshLibrary.get_number_vertices(mesh, 0))
    except Exception:
        metrics["lod0Vertices"] = None
    return metrics


def _skeletal_mesh_review_metrics(mesh, lod_count):
    metrics = _bounds_metrics(mesh)
    metrics.update({
        "lodCount": lod_count,
        "materialSlots": len(_read_property(mesh, "materials", []) or []),
        "sourceFiles": _import_sources(mesh),
    })
    return metrics


def _sound_wave_review_metrics(sound):
    duration = _read_property(sound, "duration", None)
    sample_rate = None
    try:
        sample_rate = int(sound.get_sample_rate_for_current_platform())
    except Exception:
        pass
    return {
        "durationSeconds": round(float(duration), 3) if duration is not None else None,
        "channels": int(_read_property(sound, "num_channels", 0) or 0),
        "sampleRateHz": sample_rate,
        "streaming": bool(_read_property(sound, "streaming", False)),
        "sourceFiles": _import_sources(sound),
    }


def _texture_dimensions(texture):
    for x_name, y_name in (("blueprint_get_size_x", "blueprint_get_size_y"), ("get_size_x", "get_size_y")):
        try:
            return int(getattr(texture, x_name)()), int(getattr(texture, y_name)())
        except Exception:
            pass
    return int(_read_property(texture, "size_x", 0) or 0), int(_read_property(texture, "size_y", 0) or 0)


def _skeletal_lod_count(mesh):
    try:
        return int(mesh.get_lod_num())
    except Exception:
        lod_info = _read_property(mesh, "lod_info", None)
        return len(lod_info) if lod_info is not None else None


def _static_mesh_metrics(mesh):
    lod_count = None
    try:
        lod_count = int(mesh.get_num_lods())
    except Exception:
        pass

    subsystem = None
    try:
        subsystem = unreal.get_editor_subsystem(
            unreal.StaticMeshEditorSubsystem
        )
    except Exception:
        pass

    if subsystem is not None:
        try:
            return (
                lod_count if lod_count is not None else int(subsystem.get_lod_count(mesh)),
                int(subsystem.get_simple_collision_count(mesh)),
            )
        except Exception:
            pass

    # Some UE 5.8 commandlet configurations do not expose the modern
    # subsystem. Keep the read-only compatibility fallback without emitting a
    # deprecation warning that obscures actual audit findings.
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        if lod_count is None:
            lod_count = int(
                unreal.EditorStaticMeshLibrary.get_lod_count(mesh)
            )
        collision_count = int(
            unreal.EditorStaticMeshLibrary.get_simple_collision_count(mesh)
        )
    return lod_count, collision_count


def _is_shipping_root(path):
    return path in SHIPPING_ROOTS or any(path.startswith(prefix) for prefix in SHIPPING_PREFIXES)


def _find_shipping_reference_chain(candidate):
    queue = collections.deque([(candidate, [candidate], 0)])
    visited = {candidate}
    while queue:
        current, chain, depth = queue.popleft()
        if current != candidate and _is_shipping_root(current):
            return chain
        if depth >= MAX_REFERENCE_DEPTH:
            continue
        try:
            referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(current, False)
        except Exception:
            referencers = []
        for referencer in sorted(str(item).split(".", 1)[0] for item in referencers):
            if referencer not in visited:
                visited.add(referencer)
                queue.append((referencer, chain + [referencer], depth + 1))
    return []


def _validation_result(asset_data):
    try:
        subsystem = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
        result = subsystem.is_asset_valid(asset_data, unreal.DataValidationUsecase.COMMANDLET)
        return str(result)
    except Exception as exc:
        return "Unavailable: {}".format(exc)


def _collect_asset_paths():
    paths = set()
    for root in SCAN_ROOTS:
        for path in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            paths.add(str(path).split(".", 1)[0])
    for path in EXTRA_ASSETS:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            paths.add(path)
    return sorted(paths)


def run_audit():
    report = {
        "schemaVersion": 1,
        "generatedUtc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "readOnly": True,
        "policy": {
            "source": "GDD-08 requires disciplined LODs, shared materials, and scalable content but specifies no numeric texture cap.",
            "maximumTextureDimensionWarning": MAX_TEXTURE_DIMENSION,
            "largeNonStreamingTextureDimensionWarning": LARGE_NON_STREAMING_DIMENSION,
            "singleLodAndCollisionlessMeshes": "review candidates, not automatic blockers",
            "placeholderNames": "candidates only; shipping relevance requires an Asset Registry reference chain",
        },
        "scanRoots": list(SCAN_ROOTS),
        "summary": {},
        "oversizedTextures": [],
        "largeNonStreamingTextures": [],
        "singleLodStaticMeshes": [],
        "unknownStaticMeshLods": [],
        "zeroSimpleCollisionStaticMeshes": [],
        "singleLodSkeletalMeshes": [],
        "placeholderCandidates": [],
        "errors": [],
    }
    counts = collections.Counter()
    for path in _collect_asset_paths():
        counts["assets"] += 1
        try:
            asset_data = unreal.EditorAssetLibrary.find_asset_data(path)
            class_name = _asset_class_name(asset_data)
            asset = unreal.EditorAssetLibrary.load_asset(path)
            if asset is None:
                raise RuntimeError("load_asset returned None")

            if class_name == "Texture2D":
                counts["textures"] += 1
                width, height = _texture_dimensions(asset)
                detail = {"path": path, "width": width, "height": height}
                if max(width, height) > MAX_TEXTURE_DIMENSION:
                    report["oversizedTextures"].append(detail)
                if max(width, height) > LARGE_NON_STREAMING_DIMENSION and bool(_read_property(asset, "never_stream", False)):
                    report["largeNonStreamingTextures"].append(detail)
            elif class_name == "StaticMesh":
                counts["staticMeshes"] += 1
                lod_count, collision_count = _static_mesh_metrics(asset)
                if lod_count < 0:
                    report["unknownStaticMeshLods"].append({"path": path, "lodCount": None})
                elif lod_count == 1:
                    chain = _find_shipping_reference_chain(path)
                    report["singleLodStaticMeshes"].append({
                        "path": path,
                        "lodCount": lod_count,
                        "shippingReferenced": bool(chain),
                        "referenceChain": chain,
                        "reviewMetrics": _static_mesh_review_metrics(asset, lod_count, collision_count),
                    })
                    if chain:
                        counts["shippingReferencedSingleLodStaticMeshes"] += 1
                if collision_count == 0:
                    report["zeroSimpleCollisionStaticMeshes"].append({"path": path, "simpleCollisionCount": 0})
            elif class_name in ("SkeletalMesh", "SkinnedAsset"):
                counts["skeletalMeshes"] += 1
                lod_count = _skeletal_lod_count(asset)
                if lod_count is not None and lod_count <= 1:
                    report["singleLodSkeletalMeshes"].append({"path": path, "lodCount": lod_count})
            elif class_name == "Material":
                counts["materials"] += 1
            elif class_name in ("MaterialInstanceConstant", "MaterialInstance"):
                counts["materialInstances"] += 1

            leaf = path.rsplit("/", 1)[-1].lower()
            if any(term in leaf for term in PLACEHOLDER_TERMS):
                chain = _find_shipping_reference_chain(path)
                item = {
                    "path": path,
                    "class": class_name,
                    "shippingReferenced": bool(chain),
                    "referenceChain": chain,
                    "validation": _validation_result(asset_data) if chain else "NotRun",
                }
                if class_name == "StaticMesh":
                    item["reviewMetrics"] = _static_mesh_review_metrics(asset, *_static_mesh_metrics(asset))
                elif class_name in ("SkeletalMesh", "SkinnedAsset"):
                    item["reviewMetrics"] = _skeletal_mesh_review_metrics(asset, _skeletal_lod_count(asset))
                elif class_name == "SoundWave":
                    item["reviewMetrics"] = _sound_wave_review_metrics(asset)
                report["placeholderCandidates"].append(item)
                if chain:
                    counts["shippingReferencedPlaceholders"] += 1
                    if "invalid" in item["validation"].lower():
                        counts["invalidShippingReferencedPlaceholders"] += 1
                unreal.log("BH_ASSET_PLACEHOLDER_REFERENCE candidate={} shipping_referenced={} chain={} validation={}".format(
                    path, bool(chain), " -> ".join(chain) if chain else "none", item["validation"]))
        except Exception as exc:
            report["errors"].append({"path": path, "error": str(exc)})
            unreal.log_warning("BH_ASSET_READINESS_ITEM_ERROR path={} error={}".format(path, exc))

    counts["oversizedTextures"] = len(report["oversizedTextures"])
    counts["largeNonStreamingTextures"] = len(report["largeNonStreamingTextures"])
    counts["singleLodStaticMeshes"] = len(report["singleLodStaticMeshes"])
    counts["unknownStaticMeshLods"] = len(report["unknownStaticMeshLods"])
    counts["zeroSimpleCollisionStaticMeshes"] = len(report["zeroSimpleCollisionStaticMeshes"])
    counts["singleLodSkeletalMeshes"] = len(report["singleLodSkeletalMeshes"])
    counts["placeholderCandidates"] = len(report["placeholderCandidates"])
    counts["errors"] = len(report["errors"])
    material_total = counts["materials"] + counts["materialInstances"]
    report["summary"] = dict(counts)
    report["summary"]["materialInstanceRatio"] = (float(counts["materialInstances"]) / material_total) if material_total else None

    report_dir = os.path.join(unreal.Paths.project_saved_dir(), "Reports")
    os.makedirs(report_dir, exist_ok=True)
    report_path = os.path.join(report_dir, "BHAssetReadiness.json")
    with open(report_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)

    unreal.log("BH_ASSET_READINESS_AUDIT result=complete assets={} textures={} oversized={} nonstreaming_large={} static_meshes={} single_lod_static={} unknown_static_lods={} zero_simple_collision={} skeletal_meshes={} single_lod_skeletal={} materials={} instances={} placeholder_candidates={} shipping_referenced={} invalid={} errors={} report={}".format(
        counts["assets"], counts["textures"], counts["oversizedTextures"], counts["largeNonStreamingTextures"],
        counts["staticMeshes"], counts["singleLodStaticMeshes"], counts["unknownStaticMeshLods"], counts["zeroSimpleCollisionStaticMeshes"],
        counts["skeletalMeshes"], counts["singleLodSkeletalMeshes"], counts["materials"], counts["materialInstances"],
        counts["placeholderCandidates"], counts["shippingReferencedPlaceholders"], counts["invalidShippingReferencedPlaceholders"],
        counts["errors"], report_path))


run_audit()
