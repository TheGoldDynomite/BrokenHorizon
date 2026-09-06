"""Inventory authored First Light logistics without mutation or BeginPlay.

Run with Unreal Editor -ExecutePythonScript=<this file>. The only write is a
unique JSON report under Project/Saved/Reports; no assets or levels are saved.
"""

from datetime import datetime, timezone
import json
import math
from pathlib import Path
import uuid
import unreal

MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
BOAT_ID = "FirstLightWatercraft01"
WATER_ID = "FirstLightWaterRoute01"
STATION_IDS = ("WesternFOB", "EasternDepot")
BOAT_PROPERTIES = (
    "persistence_id", "maximum_cargo_supply",
    "waterborne_transport", "water_surface_z", "water_speed_multiplier",
    "maximum_forward_speed", "maximum_reverse_speed", "boost_forward_speed",
    "acceleration_response", "steering_rate", "ground_clearance",
    "loaded_speed_multiplier_at_capacity", "loaded_control_multiplier_at_capacity",
    "loaded_fuel_burn_multiplier_at_capacity", "logistics_station_radius",
    "maximum_fuel", "maximum_hull",
)
WATER_PROPERTIES = ("water_id", "surface_extents", "surface_height", "infantry_speed_multiplier")


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def plain(value):
    if value is None or isinstance(value, (str, bool, int, float)):
        return value
    if isinstance(value, unreal.Vector):
        return vector(value)
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    return str(value)


def read(report, owner, operation, function):
    """A failed reflected read remains explicit, never a guessed default."""
    try:
        return function()
    except Exception as error:
        report["readErrors"].append({"owner": owner.get_path_name(), "operation": operation,
                                     "error": str(error)})
        return None


def properties(report, owner, names):
    return {name: read(report, owner, "property:" + name,
                       lambda name=name: plain(owner.get_editor_property(name))) for name in names}


def actor_record(report, actor):
    rotation = read(report, actor, "rotation", actor.get_actor_rotation)
    return {
        "path": actor.get_path_name(), "name": actor.get_name(),
        "class": actor.get_class().get_path_name(),
        "label": read(report, actor, "label", actor.get_actor_label),
        "tags": read(report, actor, "tags", lambda: [str(tag) for tag in actor.get_editor_property("tags")]),
        "locationCm": read(report, actor, "location", lambda: vector(actor.get_actor_location())),
        "rotationPitchYawRoll": [rotation.pitch, rotation.yaw, rotation.roll] if rotation else None,
        "scale": read(report, actor, "scale", lambda: vector(actor.get_actor_scale3d())),
        "actorCollisionEnabled": read(report, actor, "actor_collision", actor.get_actor_enable_collision),
    }


def bounds_record(component):
    origin, extent, radius = unreal.SystemLibrary.get_component_bounds(component)
    center, half = vector(origin), vector(extent)
    if not all(math.isfinite(number) for number in center + half) or any(number < 0 for number in half):
        raise ValueError("Invalid component bounds")
    return {"centerCm": center, "extentCm": half, "sphereRadiusCm": float(radius),
            "minCm": [center[i] - half[i] for i in range(3)],
            "maxCm": [center[i] + half[i] for i in range(3)]}


def component_record(report, component):
    result = {"path": component.get_path_name(), "class": component.get_class().get_path_name(),
              "bounds": read(report, component, "world_bounds", lambda: bounds_record(component)),
              "properties": properties(report, component, ("relative_location", "relative_rotation", "relative_scale3d")),
              "collisionEnabled": read(report, component, "collision_enabled", lambda: str(component.get_collision_enabled())),
              "collisionProfile": read(report, component, "collision_profile", lambda: str(component.get_collision_profile_name())),
              "collisionResponses": {}}
    for name, channel in (("Pawn", unreal.CollisionChannel.ECC_PAWN),
                          ("Vehicle", unreal.CollisionChannel.ECC_VEHICLE),
                          ("WorldStatic", unreal.CollisionChannel.ECC_WORLD_STATIC)):
        result["collisionResponses"][name] = read(report, component, "response:" + name,
                                                   lambda channel=channel: str(component.get_collision_response_to_channel(channel)))
    if isinstance(component, unreal.BoxComponent):
        result["scaledBoxExtentCm"] = read(report, component, "scaled_box_extent", lambda: vector(component.get_scaled_box_extent()))
    if isinstance(component, unreal.StaticMeshComponent):
        result["staticMesh"] = properties(report, component, ("static_mesh",))["static_mesh"]
    return result


def intersects(left, right, dimensions=3):
    return all(left["minCm"][i] <= right["maxCm"][i] and left["maxCm"][i] >= right["minCm"][i]
               for i in range(dimensions))


def point_distance(point, box):
    return math.sqrt(sum(max(box["minCm"][i] - point[i], 0.0, point[i] - box["maxCm"][i]) ** 2 for i in range(3)))


def inventory(report):
    if not unreal.EditorLevelLibrary.load_level(MAP_PATH):
        raise RuntimeError("Could not load fixed First Light map")
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world or world.get_path_name().split(".")[0] != MAP_PATH:
        raise RuntimeError("Loaded editor world is not the fixed First Light map")
    report["loadedWorld"] = world.get_path_name()
    actors = sorted(unreal.EditorLevelLibrary.get_all_level_actors(), key=lambda actor: actor.get_path_name())
    report["actorCount"] = len(actors)
    components = {}
    actor_records = {}
    routes = []
    for actor in actors:
        record = actor_record(report, actor)
        actor_records[record["path"]] = record
        primitives = read(report, actor, "primitive_components", lambda: list(actor.get_components_by_class(unreal.PrimitiveComponent)))
        components[record["path"]] = primitives or []
        if isinstance(actor, unreal.BHFieldTransport):
            record["authoredProperties"] = properties(report, actor, BOAT_PROPERTIES)
            # These reflected fields are not exposed through Python properties.
            # Verified native getters only read them; normalized results stay labeled.
            for field, getter in (("cargo_source_sector_id", "get_cargo_source_sector_id"),
                                  ("cargo_destination_sector_id", "get_cargo_destination_sector_id"),
                                  ("cargo_type", "get_cargo_type")):
                record["authoredProperties"][field] = read(report, actor, "getter:" + getter,
                    lambda getter=getter: plain(getattr(actor, getter)()))
            record["exactFieldGetterAccess"] = {
                "cargo_source_sector_id": "GetCargoSourceSectorID returns CargoSourceSectorID",
                "cargo_destination_sector_id": "GetCargoDestinationSectorID returns CargoDestinationSectorID",
                "cargo_type": "GetCargoType returns CargoType"}
            record["readOnlyGetterObservations"] = {}
            for getter, formula in (
                ("get_cargo_supply", "max(0, CurrentCargoSupply)"),
                ("get_fuel_percentage", "clamp(CurrentFuel / max(1, MaximumFuel), 0, 1)"),
                ("get_hull_percentage", "clamp(CurrentHull / max(1, MaximumHull), 0, 1)"),
            ):
                record["readOnlyGetterObservations"][getter] = {
                    "value": read(report, actor, "getter:" + getter, lambda getter=getter: plain(getattr(actor, getter)())),
                    "nativeFormula": formula, "runtimeInitialized": False}
            record["rawPropertiesUnavailable"] = ["current_cargo_supply", "current_fuel", "current_hull"]
            record["rawAccessLimitation"] = "Python property access unavailable; getters clamp values. Raw values are not reconstructed from clamp boundaries."
            report["transports"].append(record)
            if record["authoredProperties"]["persistence_id"] == BOAT_ID:
                record["components"] = [component_record(report, component) for component in components[record["path"]]]
                surface_z = record["authoredProperties"]["water_surface_z"]
                record["predictedRuntimeActorZCm"] = surface_z if record["authoredProperties"]["waterborne_transport"] is True else None
                record["runtimeZBasis"] = "BHFieldTransport::UpdateWaterPosition uses absolute WaterSurfaceZ; prediction only, no Tick run"
        elif isinstance(actor, unreal.BHWaterSurface):
            record["authoredProperties"] = properties(report, actor, WATER_PROPERTIES)
            record["components"] = [component_record(report, component) for component in components[record["path"]]]
            report["waterSurfaces"].append(record)
            if record["authoredProperties"]["water_id"] == WATER_ID:
                volume = read(report, actor, "property:water_volume", lambda: actor.get_editor_property("water_volume"))
                bounds = read(report, volume, "route_bounds", lambda: bounds_record(volume)) if volume else None
                if volume is None:
                    report["issues"].append({"kind": "missing_water_volume", "actor": record["path"]})
                if bounds:
                    routes.append({"actor": record["path"], "bounds": bounds})
        elif isinstance(actor, unreal.BHSectorResupplyStation):
            record["authoredProperties"] = properties(report, actor, ("sector_id", "vehicle_service_radius"))
            report["stations"].append(record)
    report["requestedIds"] = {}
    for kind, records, property_name, identifiers in (
        ("boat", report["transports"], "persistence_id", (BOAT_ID,)),
        ("water", report["waterSurfaces"], "water_id", (WATER_ID,)),
        ("station", report["stations"], "sector_id", STATION_IDS),
    ):
        for identifier in identifiers:
            matches = [record["path"] for record in records if record["authoredProperties"][property_name] == identifier]
            report["requestedIds"][identifier] = {"kind": kind, "count": len(matches), "actors": matches}
            if len(matches) != 1:
                report["issues"].append({"kind": "missing_id" if not matches else "duplicate_id", "id": identifier, "actors": matches})
    boats = [record for record in report["transports"] if record["authoredProperties"]["persistence_id"] == BOAT_ID]
    for boat in boats:
        for station in report["stations"]:
            left, right = boat["locationCm"], station["locationCm"]
            radius = boat["authoredProperties"]["logistics_station_radius"]
            if left is None or right is None or radius is None:
                continue  # Original failed reads remain in readErrors; no distance fabricated.
            predicted = list(left)
            if boat["predictedRuntimeActorZCm"] is not None:
                predicted[2] = boat["predictedRuntimeActorZCm"]
            distance = math.dist(left, right)
            predicted_distance = math.dist(predicted, right)
            report["stationDistances"].append({"boat": boat["path"], "station": station["path"],
                "sectorId": station["authoredProperties"]["sector_id"], "authoredDistance3DCm": distance,
                "predictedSnappedDistance3DCm": predicted_distance, "boatAuthoredLogisticsRadiusCm": radius,
                "effectiveRadiusCm": max(100.0, radius), "authoredPositionWithinRadius": distance <= max(100.0, radius),
                "predictedPositionWithinRadius": predicted_distance <= max(100.0, radius),
                "distanceToWaterBoundsCm": [{"route": route["actor"], "distance": point_distance(right, route["bounds"])} for route in routes]})
    for boat in boats:
        distances = [item for item in report["stationDistances"] if item["boat"] == boat["path"]]
        if distances:
            # Preserve ties: native <= selection also depends on actor iteration order.
            for key, field in (("nearestStationsAuthored", "authoredDistance3DCm"),
                               ("nearestStationsAfterPredictedZSnap", "predictedSnappedDistance3DCm")):
                nearest = min(item[field] for item in distances)
                boat[key] = {"distance3DCm": nearest, "stations": [item["station"] for item in distances if item[field] == nearest]}
        else:
            boat["nearestStationsAuthored"] = None
            boat["nearestStationsAfterPredictedZSnap"] = None
    report["routeBounds"] = routes
    for actor in actors:
        actor_info = actor_records[actor.get_path_name()]
        if actor_info["actorCollisionEnabled"] is not True:
            continue
        for component in components[actor.get_path_name()]:
            enabled = read(report, component, "collision_enabled", component.get_collision_enabled)
            if enabled is None or enabled == unreal.CollisionEnabled.NO_COLLISION:
                continue
            bounds = read(report, component, "world_bounds", lambda: bounds_record(component))
            if bounds is None:
                continue
            overlaps = [{"route": route["actor"], "overlaps3D": intersects(bounds, route["bounds"]),
                         "overlapsXY": intersects(bounds, route["bounds"], 2)} for route in routes
                        if intersects(bounds, route["bounds"], 2)]
            if overlaps:
                report["nearbyCollidableGeometry"].append({"actor": actor_info,
                    "component": component_record(report, component), "routeOverlaps": overlaps})
    report["inventoryReadComplete"] = not report["readErrors"]
    report["requestedActorsUnique"] = all(item["count"] == 1 for item in report["requestedIds"].values())


def main():
    report = {"schemaVersion": 2, "map": MAP_PATH, "utc": datetime.now(timezone.utc).isoformat(),
              "mode": "Editor authored inventory; no BeginPlay", "inventoryReadComplete": False,
              "requestedActorsUnique": False, "rawTransportPropertyCoverageComplete": False,
              "collisionPathOrPlayabilityVerified": False,
              "limitations": ["Bounds intersections are AABB inventory, not blocking-hit or route traversal proof",
                              "Nearest station distance alone does not prove faction, logistics eligibility or delivery",
                              "Pure getter observations read serialized fields without BeginPlay; cargo/fuel/hull getters clamp and are not raw values",
                              "Component bounds reflect the loaded editor scene; instanced meshes may have aggregate bounds"],
              "authoringIntentReference": {"script": "Content/Python/add_first_light_water_route.py",
                                           "cargoSupply": 15.0, "source": "WesternFOB", "destination": "EasternDepot"},
              "transports": [], "waterSurfaces": [], "stations": [], "stationDistances": [],
              "nearbyCollidableGeometry": [], "readErrors": [], "issues": [], "fatalError": None}
    output = Path(unreal.Paths.project_saved_dir()).resolve() / "Reports" / (
        "FirstLightLogistics-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8] + ".json")
    try:
        inventory(report)
    except Exception as error:
        report["fatalError"] = str(error)
        report["inventoryReadComplete"] = False
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, allow_nan=False)
    unreal.log("[FirstLight Logistics] report=%s complete=%s issues=%d read_errors=%d boats=%d waters=%d stations=%d" % (
        output, report["inventoryReadComplete"], len(report["issues"]), len(report["readErrors"]),
        len(report["transports"]), len(report["waterSurfaces"]), len(report["stations"])))
    if report["fatalError"] or report["readErrors"]:
        unreal.log_error("[FirstLight Logistics] Incomplete inventory; inspect structured errors in %s" % output)


main()
