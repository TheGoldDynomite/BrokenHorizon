import unreal
import math


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
PLACEHOLDER_LANDSCAPE_MATERIAL = (
    "/Game/BrokenHorizon/Environment/Materials/"
    "M_BH_Terrain_Prototype.M_BH_Terrain_Prototype"
)

EXPECTED_ANCHORS = {
    "NorthPass": unreal.Vector(-180000.0, 720000.0, 0.0),
    "KoronaCrossroads": unreal.Vector(0.0, -50000.0, 0.0),
    "EasternDepot": unreal.Vector(900000.0, 450000.0, 0.0),
    "WesternFOB": unreal.Vector(-950000.0, -450000.0, 0.0),
    "DovrenVillage": unreal.Vector(-520000.0, -285000.0, 0.0),
    "SouthBridge": unreal.Vector(310000.0, -520000.0, 0.0),
}

EXPECTED_ROUTES = {
    "WesternFOB_DovrenVillage": (
        "WesternFOB",
        "DovrenVillage",
    ),
    "DovrenVillage_KoronaCrossroads": (
        "DovrenVillage",
        "KoronaCrossroads",
    ),
    "DovrenVillage_NorthPass": (
        "DovrenVillage",
        "NorthPass",
    ),
    "NorthPass_KoronaCrossroads": (
        "NorthPass",
        "KoronaCrossroads",
    ),
    "KoronaCrossroads_SouthBridge": (
        "KoronaCrossroads",
        "SouthBridge",
    ),
    "SouthBridge_EasternDepot": (
        "SouthBridge",
        "EasternDepot",
    ),
}


def fail(message):
    raise RuntimeError("[BH Region Validation] " + message)


def main():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(MAP_PATH)

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    actors = actor_subsystem.get_all_level_actors()

    landscapes = [
        actor
        for actor in actors
        if isinstance(actor, unreal.Landscape)
    ]
    proxies = [
        actor
        for actor in actors
        if isinstance(actor, unreal.LandscapeStreamingProxy)
    ]
    descriptor_result = (
        unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    )
    descriptors = (
        descriptor_result[1]
        if isinstance(descriptor_result, tuple)
        else descriptor_result
    )
    proxy_descriptors = [
        descriptor
        for descriptor in descriptors
        if "LandscapeStreamingProxy" in (
            descriptor
            .get_editor_property("class_")
            .export_text()
        )
    ]
    stale_hlod_descriptors = [
        descriptor
        for descriptor in descriptors
        if "WorldPartitionHLOD" in (
            descriptor
            .get_editor_property("class_")
            .export_text()
        )
    ]
    anchors = {
        str(actor.get_sector_id()): actor
        for actor in actors
        if isinstance(actor, unreal.BHSectorAnchor)
    }
    routes = [
        actor
        for actor in actors
        if isinstance(actor, unreal.BHWorldRoute)
    ]
    active_routes = [
        route
        for route in routes
        if (
            str(route.get_route_id()) not in ("", "None")
            and route.get_route_length() > 0.0
        )
    ]
    sites = [
        actor
        for actor in actors
        if "BHWorldSite" in [
            str(tag)
            for tag in actor.get_editor_property("tags")
        ]
    ]
    player_starts = [
        actor
        for actor in actors
        if isinstance(actor, unreal.PlayerStart)
    ]
    nav_bounds = [
        actor
        for actor in actors
        if isinstance(actor, unreal.NavMeshBoundsVolume)
        and actor.get_actor_label() == "Nav_RegionAlpha_Dynamic"
    ]
    navmeshes = [
        actor
        for actor in actors
        if isinstance(actor, unreal.RecastNavMesh)
    ]

    if len(landscapes) != 1:
        fail(
            "Expected one landscape root; found %d."
            % len(landscapes)
        )

    landscape_material = landscapes[0].get_editor_property(
        "landscape_material"
    )
    if (
        landscape_material is None
        or landscape_material.get_path_name()
        != PLACEHOLDER_LANDSCAPE_MATERIAL
    ):
        fail(
            "The root landscape does not use the visible "
            "prototype material."
        )

    if len(proxy_descriptors) < 25:
        fail(
            "Expected partitioned landscape descriptors; found %d."
            % len(proxy_descriptors)
        )

    highest_proxy_bound = max(
        descriptor.get_editor_property("bounds").max.z
        for descriptor in proxy_descriptors
    )
    if highest_proxy_bound < 5000.0:
        fail(
            "Landscape proxy height bounds were not generated; "
            "highest terrain bound is %.1f cm."
            % highest_proxy_bound
        )

    if stale_hlod_descriptors:
        fail(
            "Found %d stale template HLOD actors."
            % len(stale_hlod_descriptors)
        )

    if len(anchors) != len(EXPECTED_ANCHORS):
        fail(
            "Expected %d sector anchors; found %d."
            % (len(EXPECTED_ANCHORS), len(anchors))
        )

    for sector_id, expected_xy in EXPECTED_ANCHORS.items():
        anchor = anchors.get(sector_id)
        if anchor is None:
            fail("Missing sector anchor %s." % sector_id)

        location = anchor.get_actor_location()
        delta_x = location.x - expected_xy.x
        delta_y = location.y - expected_xy.y
        distance_xy = (
            (delta_x * delta_x) +
            (delta_y * delta_y)
        ) ** 0.5

        if distance_xy > 100.0:
            fail(
                "%s is %.1f cm from its planned location."
                % (sector_id, distance_xy)
            )

        if anchor.get_editor_property("is_spatially_loaded"):
            fail("%s must remain always loaded." % sector_id)

    routes_by_id = {
        str(route.get_route_id()): route
        for route in active_routes
    }

    if set(routes_by_id) != set(EXPECTED_ROUTES):
        fail(
            "Strategic route IDs do not match the sector graph. "
            "Expected %s; found %s."
            % (
                sorted(EXPECTED_ROUTES),
                sorted(routes_by_id),
            )
        )

    for route_id, sector_ids in EXPECTED_ROUTES.items():
        route = routes_by_id[route_id]
        source_location = EXPECTED_ANCHORS[sector_ids[0]]
        destination_location = EXPECTED_ANCHORS[sector_ids[1]]
        source_distance = (
            route.get_distance_along_route_closest_to_world_location(
                source_location
            )
        )
        destination_distance = (
            route.get_distance_along_route_closest_to_world_location(
                destination_location
            )
        )
        source_route_location = route.get_world_location_at_distance(
            source_distance
        )
        destination_route_location = route.get_world_location_at_distance(
            destination_distance
        )
        source_connection_error = math.hypot(
            source_location.x - source_route_location.x,
            source_location.y - source_route_location.y,
        )
        destination_connection_error = math.hypot(
            destination_location.x - destination_route_location.x,
            destination_location.y - destination_route_location.y,
        )

        if (
            source_connection_error > 5000.0
            or destination_connection_error > 5000.0
        ):
            fail(
                "Route %s is not connected to both sector anchors."
                % route_id
            )

        if abs(destination_distance - source_distance) < 100000.0:
            fail(
                "Route %s has no usable travel segment."
                % route_id
            )

    if len(sites) != 5:
        fail("Expected five world sites; found %d." % len(sites))

    if len(player_starts) != 1:
        fail(
            "Expected one PlayerStart; found %d."
            % len(player_starts)
        )

    player_rotation = player_starts[0].get_actor_rotation()
    if abs(player_rotation.pitch) > 0.1:
        fail(
            "PlayerStart pitch must be level; found %.1f."
            % player_rotation.pitch
        )

    if abs(player_rotation.yaw - 28.0) > 0.1:
        fail(
            "PlayerStart yaw expected 28; found %.1f."
            % player_rotation.yaw
        )

    if any(
        actor.get_actor_label() == "SM_SkySphere"
        for actor in actors
    ):
        fail("The obsolete finite sky dome is still present.")

    if len(nav_bounds) != 1:
        fail(
            "Expected one dynamic region navigation volume; found %d."
            % len(nav_bounds)
        )

    if nav_bounds[0].get_editor_property("is_spatially_loaded"):
        fail("The region navigation volume must remain always loaded.")

    if len(navmeshes) != 1:
        fail("Expected one Recast navmesh; found %d." % len(navmeshes))

    navmesh = navmeshes[0]
    if navmesh.get_editor_property("is_spatially_loaded"):
        fail("The OpenWorld Recast navmesh must remain always loaded.")

    if (
        navmesh.get_editor_property("runtime_generation")
        != unreal.RuntimeGenerationType.DYNAMIC
    ):
        fail("The OpenWorld Recast navmesh must generate dynamically.")

    nav_origin, nav_extent = nav_bounds[0].get_actor_bounds(False)
    if (
        nav_extent.x < 1250000.0
        or nav_extent.y < 1250000.0
        or nav_extent.x > 1300000.0
        or nav_extent.y > 1300000.0
    ):
        fail(
            "The region navigation volume has unexpected coverage: "
            "extent=(%.1f, %.1f, %.1f)."
            % (nav_extent.x, nav_extent.y, nav_extent.z)
        )

    tile_size_uu = navmesh.get_editor_property("tile_size_uu")
    average_layers = navmesh.get_editor_property(
        "average_layers_per_tile"
    )
    tile_hard_limit = navmesh.get_editor_property(
        "tile_number_hard_limit"
    )
    required_tiles = (
        math.ceil((nav_extent.x * 2.0) / tile_size_uu)
        * math.ceil((nav_extent.y * 2.0) / tile_size_uu)
        * math.ceil(average_layers)
    )

    if tile_size_uu < 9216.0:
        fail(
            "OpenWorld Recast tiles are too small: %.1f UU."
            % tile_size_uu
        )

    if required_tiles > tile_hard_limit:
        fail(
            "OpenWorld navigation requires %d tiles but the hard "
            "limit is %d."
            % (required_tiles, tile_hard_limit)
        )

    unreal.log(
        "BH_REGION_VERIFY landscape_roots=%d "
        "loaded_proxies=%d proxy_descriptors=%d "
        "anchors=%d sites=%d routes=%d stale_hlods=%d "
        "player_pitch=%.1f player_yaw=%.1f "
        "terrain_max_z=%.1f nav_bounds=1 "
        "nav_extent=(%.1f,%.1f,%.1f) "
        "nav_tile_size=%.1f nav_required_tiles=%d "
        "nav_hard_limit=%d sky_dome=0"
        % (
            len(landscapes),
            len(proxies),
            len(proxy_descriptors),
            len(anchors),
            len(sites),
            len(active_routes),
            len(stale_hlod_descriptors),
            player_rotation.pitch,
            player_rotation.yaw,
            highest_proxy_bound,
            nav_extent.x,
            nav_extent.y,
            nav_extent.z,
            tile_size_uu,
            required_tiles,
            tile_hard_limit,
        )
    )


main()
