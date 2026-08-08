import math

import unreal


DESTINATION_MAP = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
OPEN_WORLD_TEMPLATE = "/Engine/Maps/Templates/OpenWorld"
PLACEHOLDER_LANDSCAPE_MATERIAL = (
    "/Game/BrokenHorizon/Environment/Materials/"
    "M_BH_Terrain_Prototype.M_BH_Terrain_Prototype"
)

CENTIMETERS_PER_METER = 100.0

SECTOR_ANCHORS = (
    {
        "sector_id": "NorthPass",
        "display_name": "North Pass",
        "location": (-1800.0, 7200.0),
        "yaw": -12.0,
    },
    {
        "sector_id": "KoronaCrossroads",
        "display_name": "Korona Crossroads",
        "location": (0.0, -500.0),
        "yaw": 18.0,
    },
    {
        "sector_id": "EasternDepot",
        "display_name": "Eastern Logistics Depot",
        "location": (9000.0, 4500.0),
        "yaw": -145.0,
    },
    {
        "sector_id": "WesternFOB",
        "display_name": "Western Forward Base",
        "location": (-9500.0, -4500.0),
        "yaw": 28.0,
    },
    {
        "sector_id": "DovrenVillage",
        "display_name": "Dovren Village",
        "location": (-5200.0, -2850.0),
        "yaw": 24.0,
    },
    {
        "sector_id": "SouthBridge",
        "display_name": "South Bridge",
        "location": (3100.0, -5200.0),
        "yaw": 32.0,
    },
)

WORLD_SITES = (
    ("DovrenVillage", "Settlement", -5200.0, -2850.0),
    ("NorthPass", "MilitaryPass", -1800.0, 7200.0),
    ("KoronaCrossroads", "Settlement", 0.0, -500.0),
    ("SouthBridge", "Bridge", 3100.0, -5200.0),
    ("EasternDepot", "Logistics", 9000.0, 4500.0),
)

WORLD_ROUTES = (
    {
        "route_id": "WesternFOB_DovrenVillage",
        "display_name": "Western FOB - Dovren Village",
        "points": (
            (-9500.0, -4500.0),
            (-8500.0, -4200.0),
            (-7400.0, -3650.0),
            (-6300.0, -3300.0),
            (-5200.0, -2850.0),
        ),
    },
    {
        "route_id": "DovrenVillage_KoronaCrossroads",
        "display_name": "Dovren Village - Korona Crossroads",
        "points": (
            (-5200.0, -2850.0),
            (-4000.0, -2400.0),
            (-2800.0, -1700.0),
            (-1400.0, -1000.0),
            (0.0, -500.0),
        ),
    },
    {
        "route_id": "DovrenVillage_NorthPass",
        "display_name": "Dovren Village - North Pass",
        "points": (
            (-5200.0, -2850.0),
            (-4700.0, -500.0),
            (-3900.0, 2100.0),
            (-2900.0, 4700.0),
            (-1800.0, 7200.0),
        ),
    },
    {
        "route_id": "NorthPass_KoronaCrossroads",
        "display_name": "North Pass - Korona Crossroads",
        "points": (
            (-1800.0, 7200.0),
            (-1450.0, 5300.0),
            (-1000.0, 3300.0),
            (-500.0, 1300.0),
            (0.0, -500.0),
        ),
    },
    {
        "route_id": "KoronaCrossroads_SouthBridge",
        "display_name": "Korona Crossroads - South Bridge",
        "points": (
            (0.0, -500.0),
            (700.0, -1700.0),
            (1500.0, -2900.0),
            (2300.0, -4100.0),
            (3100.0, -5200.0),
        ),
    },
    {
        "route_id": "SouthBridge_EasternDepot",
        "display_name": "South Bridge - Eastern Depot",
        "points": (
            (3100.0, -5200.0),
            (4200.0, -3400.0),
            (5300.0, -1500.0),
            (6500.0, 800.0),
            (7800.0, 2800.0),
            (9000.0, 4500.0),
        ),
    },
)


def world_location(x_meters, y_meters, height_offset=0.0):
    x_cm = x_meters * CENTIMETERS_PER_METER
    y_cm = y_meters * CENTIMETERS_PER_METER
    height_meters = (
        unreal.BHWorldBuilderLibrary.get_initial_region_height_meters(
            unreal.Vector2D(x_cm, y_cm)
        )
    )
    return unreal.Vector(
        x_cm,
        y_cm,
        (height_meters + height_offset) * CENTIMETERS_PER_METER,
    )


def find_actor(actor_subsystem, actor_label):
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_actor_label() == actor_label:
            return actor
    return None


def ensure_placeholder_landscape_material():
    material = unreal.load_asset(PLACEHOLDER_LANDSCAPE_MATERIAL)

    if material is not None:
        return material

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        "M_BH_Terrain_Prototype",
        "/Game/BrokenHorizon/Environment/Materials",
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )

    if material is None:
        raise RuntimeError(
            "The prototype Landscape material could not be loaded."
        )

    base_color = (
        unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionConstant3Vector,
            -320,
            0,
        )
    )
    base_color.set_editor_property(
        "constant",
        unreal.LinearColor(0.10, 0.18, 0.065, 1.0),
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        base_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )

    roughness = (
        unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionConstant,
            -320,
            160,
        )
    )
    roughness.set_editor_property("r", 0.92)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def apply_placeholder_landscape_material(editor_world):
    material = ensure_placeholder_landscape_material()

    if not unreal.BHWorldBuilderLibrary.apply_initial_region_landscape_material(
        editor_world,
        material,
    ):
        raise RuntimeError(
            "The prototype Landscape material could not be applied."
        )


def ensure_sector_anchors(actor_subsystem):
    anchor_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHSectorAnchor",
    )

    if anchor_class is None:
        raise RuntimeError(
            "BHSectorAnchor was not available to the editor."
        )

    for sector in SECTOR_ANCHORS:
        actor_label = "SA_%s" % sector["sector_id"]
        anchor = find_actor(actor_subsystem, actor_label)
        location = world_location(
            sector["location"][0],
            sector["location"][1],
            2.0,
        )
        rotation = unreal.Rotator(
            pitch=0.0,
            yaw=sector["yaw"],
            roll=0.0,
        )

        if anchor is None:
            anchor = actor_subsystem.spawn_actor_from_class(
                anchor_class,
                location,
                rotation,
            )
            anchor.set_actor_label(actor_label)
        else:
            anchor.set_actor_location(
                location,
                False,
                True,
            )
            anchor.set_actor_rotation(
                rotation,
                False,
            )

        anchor.set_folder_path("PersistentWar/SectorAnchors")
        anchor.set_editor_property("is_spatially_loaded", False)
        anchor.set_editor_property("enemy_spawn_radius", 35000.0)
        anchor.set_editor_property(
            "operation_activation_radius",
            125000.0,
        )
        anchor.configure_sector(
            unreal.Name(sector["sector_id"]),
            unreal.Text(sector["display_name"]),
        )


def ensure_field_fortification_positions(actor_subsystem):
    fortification_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHFieldFortification",
    )
    if fortification_class is None:
        raise RuntimeError(
            "BHFieldFortification was not available to the editor."
        )

    for sector in SECTOR_ANCHORS:
        yaw = sector["yaw"]
        yaw_radians = math.radians(yaw)
        forward_x = math.cos(yaw_radians)
        forward_y = math.sin(yaw_radians)
        right_x = -forward_y
        right_y = forward_x

        for position_index, side in enumerate((-1.0, 1.0), start=1):
            x_meters = (
                sector["location"][0]
                + forward_x * 14.0
                + right_x * side * 9.0
            )
            y_meters = (
                sector["location"][1]
                + forward_y * 14.0
                + right_y * side * 9.0
            )
            actor_label = "Fortification_%s_%02d" % (
                sector["sector_id"],
                position_index,
            )
            fortification = find_actor(actor_subsystem, actor_label)
            location = world_location(x_meters, y_meters, 1.0)
            rotation = unreal.Rotator(
                pitch=0.0,
                yaw=yaw + 90.0,
                roll=0.0,
            )
            if fortification is None:
                fortification = actor_subsystem.spawn_actor_from_class(
                    fortification_class,
                    location,
                    rotation,
                )
                fortification.set_actor_label(actor_label)
            else:
                fortification.modify()
                fortification.set_actor_location(location, False, True)
                fortification.set_actor_rotation(rotation, False)

            fortification.set_folder_path(
                "PersistentWar/FieldFortifications"
            )
            fortification.set_editor_property(
                "is_spatially_loaded",
                False,
            )
            fortification.configure_fortification(
                unreal.Name(actor_label),
                unreal.Name(sector["sector_id"]),
            )


def ensure_field_armories(actor_subsystem):
    armory_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHFieldArmory",
    )
    if armory_class is None:
        raise RuntimeError("BHFieldArmory was not available to the editor.")

    for sector in SECTOR_ANCHORS:
        yaw_radians = math.radians(sector["yaw"])
        forward_x = math.cos(yaw_radians)
        forward_y = math.sin(yaw_radians)
        right_x = -forward_y
        right_y = forward_x
        x_meters = (
            sector["location"][0]
            - forward_x * 10.0
            + right_x * 8.0
        )
        y_meters = (
            sector["location"][1]
            - forward_y * 10.0
            + right_y * 8.0
        )
        actor_label = "FieldArmory_%s" % sector["sector_id"]
        armory = find_actor(actor_subsystem, actor_label)
        location = world_location(x_meters, y_meters, 1.0)
        rotation = unreal.Rotator(
            pitch=0.0,
            yaw=sector["yaw"],
            roll=0.0,
        )
        if armory is None:
            armory = actor_subsystem.spawn_actor_from_class(
                armory_class,
                location,
                rotation,
            )
            armory.set_actor_label(actor_label)
        else:
            armory.modify()
            armory.set_actor_location(location, False, True)
            armory.set_actor_rotation(rotation, False)

        armory.set_folder_path("PersistentWar/FieldArmories")
        armory.set_editor_property("is_spatially_loaded", False)
        armory.configure_armory(unreal.Name(sector["sector_id"]))


def ensure_field_support_relays(actor_subsystem):
    relay_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHFieldSupportRelay",
    )
    if relay_class is None:
        raise RuntimeError(
            "BHFieldSupportRelay was not available to the editor."
        )

    support_types = (
        (
            "Smoke",
            unreal.BHTacticalSupportType.SMOKE_SCREEN,
            -14.0,
            -8.0,
        ),
        (
            "Mortar",
            unreal.BHTacticalSupportType.MORTAR_BARRAGE,
            -14.0,
            8.0,
        ),
    )
    for sector in SECTOR_ANCHORS:
        yaw_radians = math.radians(sector["yaw"])
        forward_x = math.cos(yaw_radians)
        forward_y = math.sin(yaw_radians)
        right_x = -forward_y
        right_y = forward_x
        for support_name, support_type, forward_offset, right_offset in support_types:
            x_meters = (
                sector["location"][0]
                + forward_x * forward_offset
                + right_x * right_offset
            )
            y_meters = (
                sector["location"][1]
                + forward_y * forward_offset
                + right_y * right_offset
            )
            actor_label = "FieldSupport%s_%s" % (
                support_name,
                sector["sector_id"],
            )
            relay = find_actor(actor_subsystem, actor_label)
            location = world_location(x_meters, y_meters, 1.0)
            rotation = unreal.Rotator(
                pitch=0.0,
                yaw=sector["yaw"],
                roll=0.0,
            )
            if relay is None:
                relay = actor_subsystem.spawn_actor_from_class(
                    relay_class,
                    location,
                    rotation,
                )
                relay.set_actor_label(actor_label)
            else:
                relay.modify()
                relay.set_actor_location(location, False, True)
                relay.set_actor_rotation(rotation, False)

            relay.set_folder_path("PersistentWar/FieldSupportRelays")
            relay.set_editor_property("is_spatially_loaded", False)
            relay.configure_relay(
                unreal.Name(sector["sector_id"]),
                support_type,
            )


def ensure_world_sites(actor_subsystem):
    for site_id, site_type, x_meters, y_meters in WORLD_SITES:
        actor_label = "Site_%s" % site_id
        site = find_actor(actor_subsystem, actor_label)
        location = world_location(
            x_meters,
            y_meters,
            5.0,
        )

        if site is None:
            site = actor_subsystem.spawn_actor_from_class(
                unreal.TargetPoint,
                location,
                unreal.Rotator(
                    pitch=0.0,
                    yaw=0.0,
                    roll=0.0,
                ),
            )
            site.set_actor_label(actor_label)
        else:
            site.set_actor_location(location, False, True)

        site.set_folder_path("WorldLayout/Sites")
        site.set_editor_property("is_spatially_loaded", False)
        site.set_editor_property(
            "tags",
            [
                unreal.Name("BHWorldSite"),
                unreal.Name(site_type),
            ],
        )


def ensure_world_routes(actor_subsystem):
    route_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHWorldRoute",
    )

    if route_class is None:
        raise RuntimeError(
            "BHWorldRoute was not available to the editor."
        )

    expected_labels = {
        "Route_BH_%s" % route_spec["route_id"]
        for route_spec in WORLD_ROUTES
    }

    for actor in actor_subsystem.get_all_level_actors():
        if not isinstance(actor, unreal.BHWorldRoute):
            continue

        actor_label = actor.get_actor_label()
        route_id = str(actor.get_route_id())

        if (
            actor_label == "Route_KoronaMainSupply"
            or actor_label == "Route_BH_RetiredLegacy"
            or route_id == "KoronaMainSupply"
        ):
            actor.set_actor_label("Route_BH_RetiredLegacy")
            actor.set_folder_path("WorldLayout/Routes/Retired")
            actor.set_editor_property("is_spatially_loaded", False)
            actor.configure_route(
                unreal.Name(),
                unreal.Text("Retired Legacy Route"),
                [],
            )
        elif (
            actor_label.startswith("Route_BH_")
            and actor_label not in expected_labels
        ):
            actor_subsystem.destroy_actor(actor)

    for route_spec in WORLD_ROUTES:
        route_label = "Route_BH_%s" % route_spec["route_id"]
        route = find_actor(actor_subsystem, route_label)

        if route is None:
            route = actor_subsystem.spawn_actor_from_class(
                route_class,
                unreal.Vector(),
                unreal.Rotator(
                    pitch=0.0,
                    yaw=0.0,
                    roll=0.0,
                ),
            )
            route.set_actor_label(route_label)

        route.set_folder_path("WorldLayout/Routes")
        route.set_editor_property("is_spatially_loaded", False)
        route.configure_route(
            unreal.Name(route_spec["route_id"]),
            unreal.Text(route_spec["display_name"]),
            [
                world_location(x_meters, y_meters, 3.0)
                for x_meters, y_meters in route_spec["points"]
            ],
        )


def ensure_player_start(actor_subsystem):
    player_start = find_actor(
        actor_subsystem,
        "PlayerStart_WesternFOB",
    )

    if player_start is None:
        for actor in actor_subsystem.get_all_level_actors():
            if isinstance(actor, unreal.PlayerStart):
                player_start = actor
                break

    location = world_location(-9520.0, -4480.0, 2.5)
    rotation = unreal.Rotator(
        pitch=0.0,
        yaw=28.0,
        roll=0.0,
    )

    if player_start is None:
        player_start = actor_subsystem.spawn_actor_from_class(
            unreal.PlayerStart,
            location,
            rotation,
        )

    # Apply the transform explicitly after spawning. UE 5.8's Python
    # spawn path can transpose pitch and yaw for Rotators. Existing World
    # Partition actors also need Modify() so their external package is saved.
    player_start.modify()
    player_start.set_actor_location(
        location,
        False,
        True,
    )
    player_start.set_actor_rotation(rotation, False)

    player_start.set_actor_label("PlayerStart_WesternFOB")
    player_start.set_folder_path("Gameplay")


def remove_obsolete_sky_dome(actor_subsystem):
    sky_dome = find_actor(actor_subsystem, "SM_SkySphere")

    if sky_dome is not None:
        actor_subsystem.destroy_actor(sky_dome)


def ensure_region_navigation(actor_subsystem):
    nav_label = "Nav_RegionAlpha_Dynamic"
    nav_bounds = find_actor(actor_subsystem, nav_label)

    if nav_bounds is None:
        nav_bounds = actor_subsystem.spawn_actor_from_class(
            unreal.NavMeshBoundsVolume,
            unreal.Vector(0.0, 0.0, 30000.0),
            unreal.Rotator(
                pitch=0.0,
                yaw=0.0,
                roll=0.0,
            ),
        )

    nav_bounds.modify()
    nav_bounds.set_actor_label(nav_label)
    nav_bounds.set_folder_path("WorldLayout/Navigation")
    nav_bounds.set_actor_location(
        unreal.Vector(0.0, 0.0, 30000.0),
        False,
        True,
    )
    nav_bounds.set_actor_scale3d(
        unreal.Vector(12600.0, 12600.0, 800.0)
    )
    nav_bounds.set_editor_property("is_spatially_loaded", False)


def ensure_region_recast(editor_world):
    navmeshes = unreal.GameplayStatics.get_all_actors_of_class(
        editor_world,
        unreal.RecastNavMesh,
    )

    if len(navmeshes) != 1:
        raise RuntimeError(
            "Expected one OpenWorld Recast navmesh; found %d."
            % len(navmeshes)
        )

    navmesh = navmeshes[0]
    navmesh.modify()
    navmesh.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.DYNAMIC)
    navmesh.set_editor_property("force_rebuild_on_load", True)
    navmesh.set_editor_property("tile_size_uu", 9216.0)
    navmesh.set_editor_property("fixed_tile_pool_size", False)
    navmesh.set_editor_property("tile_pool_size", 1024)
    navmesh.set_editor_property("average_layers_per_tile", 12.0)
    navmesh.set_editor_property("is_spatially_loaded", False)


def create_master_world():
    asset_library = unreal.EditorAssetLibrary
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )

    if asset_library.does_asset_exist(DESTINATION_MAP):
        unreal.log(
            "Broken Horizon master world already exists; "
            "loading it and updating the first region."
        )
        level_subsystem.load_level(DESTINATION_MAP)
    elif not level_subsystem.new_level_from_template(
        DESTINATION_MAP,
        OPEN_WORLD_TEMPLATE,
    ):
        raise RuntimeError(
            "UE5 could not create the Broken Horizon master world."
        )

    editor_world = unreal.get_editor_subsystem(
        unreal.UnrealEditorSubsystem
    ).get_editor_world()

    if not unreal.BHWorldBuilderLibrary.build_initial_region_landscape(
        editor_world
    ):
        raise RuntimeError(
            "The first 25 km region landscape could not be built."
        )

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )

    remove_obsolete_sky_dome(actor_subsystem)
    if (
        not unreal.BHWorldBuilderLibrary
        .repair_initial_region_landscape_height(editor_world)
    ):
        raise RuntimeError(
            "The first region landscape height could not be repaired."
        )
    apply_placeholder_landscape_material(editor_world)
    ensure_sector_anchors(actor_subsystem)
    ensure_field_fortification_positions(actor_subsystem)
    ensure_field_armories(actor_subsystem)
    ensure_field_support_relays(actor_subsystem)
    ensure_world_sites(actor_subsystem)
    ensure_world_routes(actor_subsystem)
    ensure_player_start(actor_subsystem)
    ensure_region_navigation(actor_subsystem)
    ensure_region_recast(editor_world)

    if (
        not level_subsystem.save_all_dirty_levels()
        or not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
            True,
            True,
        )
    ):
        raise RuntimeError(
            "The first world region was built but could not be saved."
        )

    unreal.log(
        "BH_REGION_READY map=L_BrokenHorizon_World "
        "size_km=25.2 sectors=6 sites=5 routes=6 "
        "fortifications=12 armories=6"
    )


if __name__ == "__main__":
    create_master_world()
