"""Build the first playable settlements in the Broken Horizon open world.

The script is deliberately idempotent.  It only creates or updates actors
whose labels start with ``BH_World_`` and never touches the landscape, war
directors, sector anchors, routes, or the player's saved campaign.
"""

import math

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
MATERIAL_FOLDER = "/Game/BrokenHorizon/Environment/WorldKit/Materials"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"
GENERATED_TAG = unreal.Name("BHGeneratedWorld")
CENTIMETERS_PER_METER = 100.0

SITES = {
    "WesternFOB": (-9500.0, -4500.0, 28.0),
    "DovrenVillage": (-5200.0, -2850.0, 24.0),
    "KoronaCrossroads": (0.0, -500.0, 18.0),
    "SouthBridge": (3100.0, -5200.0, 32.0),
    "EasternDepot": (9000.0, 4500.0, -145.0),
    "NorthPass": (-1800.0, 7200.0, -12.0),
}

MATERIAL_SPECS = {
    "Concrete": ((0.18, 0.20, 0.21), 0.88, 0.0),
    "Plaster": ((0.46, 0.43, 0.36), 0.82, 0.0),
    "Roof": ((0.18, 0.12, 0.095), 0.80, 0.0),
    "Military": ((0.18, 0.24, 0.15), 0.72, 0.05),
    "Rust": ((0.36, 0.12, 0.055), 0.78, 0.15),
    "Road": ((0.035, 0.040, 0.045), 0.96, 0.0),
    "Wood": ((0.24, 0.13, 0.065), 0.86, 0.0),
    "DepotBlue": ((0.055, 0.18, 0.28), 0.70, 0.12),
    "Warning": ((0.70, 0.34, 0.025), 0.68, 0.05),
}


def _log(message):
    unreal.log("[BH World Population] " + message)


def _asset(path):
    loaded = unreal.EditorAssetLibrary.load_asset(path)
    if not loaded:
        raise RuntimeError("Missing required asset: " + path)
    return loaded


def _ensure_material(name, color, roughness, metallic):
    path = "%s/M_BH_%s" % (MATERIAL_FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        existing = unreal.EditorAssetLibrary.load_asset(path)
        if existing:
            return existing

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_BH_%s" % name,
        MATERIAL_FOLDER,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        raise RuntimeError("Could not create material: " + path)

    base_color = (
        unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionConstant3Vector,
            -360,
            -80,
        )
    )
    base_color.set_editor_property(
        "constant",
        unreal.LinearColor(color[0], color[1], color[2], 1.0),
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        base_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )

    roughness_node = (
        unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionConstant,
            -360,
            70,
        )
    )
    roughness_node.set_editor_property("r", roughness)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness_node,
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )

    metallic_node = (
        unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionConstant,
            -360,
            210,
        )
    )
    metallic_node.set_editor_property("r", metallic)
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic_node,
        "",
        unreal.MaterialProperty.MP_METALLIC,
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def _site_height_cm(site):
    x_m, y_m, _ = SITES[site]
    height_m = (
        unreal.BHWorldBuilderLibrary.get_initial_region_height_meters(
            unreal.Vector2D(
                x_m * CENTIMETERS_PER_METER,
                y_m * CENTIMETERS_PER_METER,
            )
        )
    )
    return height_m * CENTIMETERS_PER_METER


def _rotate(x_value, y_value, yaw_degrees):
    radians = math.radians(yaw_degrees)
    cosine = math.cos(radians)
    sine = math.sin(radians)
    return (
        (x_value * cosine) - (y_value * sine),
        (x_value * sine) + (y_value * cosine),
    )


class RegionBuilder:
    def __init__(self, actor_subsystem):
        self.actor_subsystem = actor_subsystem
        self.cube = _asset(CUBE_PATH)
        self.cylinder_mesh = _asset(CYLINDER_PATH)
        self.materials = {
            name: _ensure_material(name, *spec)
            for name, spec in MATERIAL_SPECS.items()
        }
        self.cover_class = unreal.load_class(
            None,
            "/Script/BrokenHorizon.BHCoverPoint",
        )
        if not self.cover_class:
            raise RuntimeError("BHCoverPoint is not reflected.")
        self.actors_by_label = {
            actor.get_actor_label(): actor
            for actor in actor_subsystem.get_all_level_actors()
        }
        self.generated_labels = set()
        self.site_base_heights = {
            site: _site_height_cm(site)
            for site in SITES
        }

    def _site_transform(
        self,
        site,
        local_x,
        local_y,
        height_m,
        local_yaw=0.0,
    ):
        site_x, site_y, site_yaw = SITES[site]
        offset_x, offset_y = _rotate(local_x, local_y, site_yaw)
        return (
            unreal.Vector(
                (site_x + offset_x) * CENTIMETERS_PER_METER,
                (site_y + offset_y) * CENTIMETERS_PER_METER,
                self.site_base_heights[site] +
                (height_m * CENTIMETERS_PER_METER),
            ),
            unreal.Rotator(
                pitch=0.0,
                yaw=site_yaw + local_yaw,
                roll=0.0,
            ),
        )

    def _ensure_static_actor(self, label, folder):
        actor = self.actors_by_label.get(label)
        if actor and not isinstance(actor, unreal.StaticMeshActor):
            raise RuntimeError(
                "%s exists but is not a StaticMeshActor." % label
            )
        if not actor:
            actor = self.actor_subsystem.spawn_actor_from_class(
                unreal.StaticMeshActor,
                unreal.Vector(),
                unreal.Rotator(),
            )
            if not actor:
                raise RuntimeError("Could not spawn " + label)
            actor.set_actor_label(label)
            self.actors_by_label[label] = actor

        actor.modify()
        actor.set_folder_path(folder)
        actor.set_editor_property("is_spatially_loaded", True)
        actor.set_editor_property("tags", [GENERATED_TAG])
        self.generated_labels.add(label)
        return actor

    def box(
        self,
        site,
        name,
        local_x,
        local_y,
        bottom_m,
        dimensions_m,
        material_name,
        local_yaw=0.0,
        folder_suffix="Structures",
    ):
        width, depth, height = dimensions_m
        label = "BH_World_%s_%s" % (site, name)
        actor = self._ensure_static_actor(
            label,
            "WorldLayout/Generated/%s/%s" %
            (site, folder_suffix),
        )
        location, rotation = self._site_transform(
            site,
            local_x,
            local_y,
            bottom_m + (height * 0.5),
            local_yaw,
        )
        actor.set_actor_location(location, False, True)
        actor.set_actor_rotation(rotation, False)
        actor.set_actor_scale3d(
            unreal.Vector(width, depth, height)
        )

        component = actor.get_editor_property(
            "static_mesh_component"
        )
        component.set_static_mesh(self.cube)
        component.set_material(0, self.materials[material_name])
        component.set_editor_property(
            "mobility",
            unreal.ComponentMobility.STATIC,
        )
        component.set_collision_enabled(
            unreal.CollisionEnabled.QUERY_AND_PHYSICS
        )
        return actor

    def cylinder(
        self,
        site,
        name,
        local_x,
        local_y,
        bottom_m,
        diameter_m,
        height_m,
        material_name,
        folder_suffix="Props",
    ):
        label = "BH_World_%s_%s" % (site, name)
        actor = self._ensure_static_actor(
            label,
            "WorldLayout/Generated/%s/%s" %
            (site, folder_suffix),
        )
        location, rotation = self._site_transform(
            site,
            local_x,
            local_y,
            bottom_m + (height_m * 0.5),
        )
        actor.set_actor_location(location, False, True)
        actor.set_actor_rotation(rotation, False)
        actor.set_actor_scale3d(
            unreal.Vector(diameter_m, diameter_m, height_m)
        )
        component = actor.get_editor_property(
            "static_mesh_component"
        )
        component.set_static_mesh(self.cylinder_mesh)
        component.set_material(0, self.materials[material_name])
        component.set_editor_property(
            "mobility",
            unreal.ComponentMobility.STATIC,
        )
        component.set_collision_enabled(
            unreal.CollisionEnabled.QUERY_AND_PHYSICS
        )
        return actor

    def cover(
        self,
        site,
        name,
        local_x,
        local_y,
        local_yaw=0.0,
        height_m=0.65,
    ):
        label = "BH_World_%s_Cover_%s" % (site, name)
        actor = self.actors_by_label.get(label)
        if actor and not isinstance(actor, unreal.BHCoverPoint):
            raise RuntimeError(
                "%s exists but is not a BHCoverPoint." % label
            )
        location, rotation = self._site_transform(
            site,
            local_x,
            local_y,
            height_m,
            local_yaw,
        )
        if not actor:
            actor = self.actor_subsystem.spawn_actor_from_class(
                self.cover_class,
                location,
                rotation,
            )
            if not actor:
                raise RuntimeError("Could not spawn " + label)
            actor.set_actor_label(label)
            self.actors_by_label[label] = actor
        else:
            actor.modify()
            actor.set_actor_location(location, False, True)
            actor.set_actor_rotation(rotation, False)

        actor.set_folder_path(
            "WorldLayout/Generated/%s/Cover" % site
        )
        actor.set_editor_property("is_spatially_loaded", True)
        actor.set_editor_property("tags", [GENERATED_TAG])
        self.generated_labels.add(label)
        return actor

    def road(
        self,
        site,
        name,
        local_x,
        local_y,
        length_m,
        width_m,
        local_yaw=0.0,
    ):
        return self.box(
            site,
            "Road_%s" % name,
            local_x,
            local_y,
            0.02,
            (length_m, width_m, 0.18),
            "Road",
            local_yaw,
            "Ground",
        )

    def barrier(
        self,
        site,
        name,
        local_x,
        local_y,
        local_yaw=0.0,
        length_m=3.2,
    ):
        self.box(
            site,
            "Barrier_%s" % name,
            local_x,
            local_y,
            0.25,
            (length_m, 0.65, 1.05),
            "Concrete",
            local_yaw,
            "Defenses",
        )
        self.cover(
            site,
            name,
            local_x,
            local_y - 0.8,
            local_yaw,
        )

    def container(
        self,
        site,
        name,
        local_x,
        local_y,
        local_yaw=0.0,
        color="DepotBlue",
        stacked=False,
    ):
        self.box(
            site,
            "Container_%s" % name,
            local_x,
            local_y,
            0.45 if not stacked else 3.05,
            (6.1, 2.45, 2.6),
            color,
            local_yaw,
            "Props",
        )

    def crates(self, site, name, local_x, local_y):
        for index, (dx, dy, size, level) in enumerate(
            (
                (-0.75, 0.0, 1.25, 0),
                (0.75, 0.0, 1.25, 0),
                (0.0, 0.0, 1.15, 1),
            )
        ):
            self.box(
                site,
                "Crate_%s_%02d" % (name, index),
                local_x + dx,
                local_y + dy,
                0.35 + (level * 1.25),
                (size, size, 1.25),
                "Wood",
                0.0,
                "Props",
            )
        self.cover(
            site,
            "Crates_%s" % name,
            local_x,
            local_y - 1.2,
        )

    def building(
        self,
        site,
        name,
        local_x,
        local_y,
        width_m,
        depth_m,
        height_m=3.4,
        local_yaw=0.0,
        wall_material="Plaster",
        roof_material="Roof",
        doorway_width_m=2.0,
    ):
        foundation_height = 0.5
        wall_thickness = 0.35
        wall_bottom = foundation_height
        wall_center_height = height_m * 0.5

        self.box(
            site,
            "%s_Foundation" % name,
            local_x,
            local_y,
            0.0,
            (width_m + 1.0, depth_m + 1.0, foundation_height),
            "Concrete",
            local_yaw,
        )

        def building_offset(offset_x, offset_y):
            rotated_x, rotated_y = _rotate(
                offset_x,
                offset_y,
                local_yaw,
            )
            return local_x + rotated_x, local_y + rotated_y

        side_left = building_offset(0.0, -depth_m * 0.5)
        side_right = building_offset(0.0, depth_m * 0.5)
        rear = building_offset(-width_m * 0.5, 0.0)
        front_x = width_m * 0.5
        front_segment = max(
            1.0,
            (depth_m - doorway_width_m) * 0.5,
        )
        front_left = building_offset(
            front_x,
            -(doorway_width_m + front_segment) * 0.5,
        )
        front_right = building_offset(
            front_x,
            (doorway_width_m + front_segment) * 0.5,
        )
        header = building_offset(front_x, 0.0)

        self.box(
            site,
            "%s_WallLeft" % name,
            side_left[0],
            side_left[1],
            wall_bottom,
            (width_m, wall_thickness, height_m),
            wall_material,
            local_yaw,
        )
        self.box(
            site,
            "%s_WallRight" % name,
            side_right[0],
            side_right[1],
            wall_bottom,
            (width_m, wall_thickness, height_m),
            wall_material,
            local_yaw,
        )
        self.box(
            site,
            "%s_WallRear" % name,
            rear[0],
            rear[1],
            wall_bottom,
            (wall_thickness, depth_m, height_m),
            wall_material,
            local_yaw,
        )
        self.box(
            site,
            "%s_WallFrontA" % name,
            front_left[0],
            front_left[1],
            wall_bottom,
            (wall_thickness, front_segment, height_m),
            wall_material,
            local_yaw,
        )
        self.box(
            site,
            "%s_WallFrontB" % name,
            front_right[0],
            front_right[1],
            wall_bottom,
            (wall_thickness, front_segment, height_m),
            wall_material,
            local_yaw,
        )
        self.box(
            site,
            "%s_DoorHeader" % name,
            header[0],
            header[1],
            wall_bottom + 2.4,
            (wall_thickness, doorway_width_m, height_m - 2.4),
            wall_material,
            local_yaw,
        )
        self.box(
            site,
            "%s_Roof" % name,
            local_x,
            local_y,
            wall_bottom + height_m,
            (width_m + 0.8, depth_m + 0.8, 0.38),
            roof_material,
            local_yaw,
        )

        rear_cover = building_offset(
            -(width_m * 0.5 + 0.75),
            0.0,
        )
        self.cover(
            site,
            "%s_Rear" % name,
            rear_cover[0],
            rear_cover[1],
            local_yaw + 180.0,
        )

    def tower(self, site, name, local_x, local_y):
        for index, (dx, dy) in enumerate(
            ((-1.5, -1.5), (-1.5, 1.5), (1.5, -1.5), (1.5, 1.5))
        ):
            self.box(
                site,
                "%s_Pillar%02d" % (name, index),
                local_x + dx,
                local_y + dy,
                0.4,
                (0.35, 0.35, 4.8),
                "Military",
                0.0,
                "Defenses",
            )
        self.box(
            site,
            "%s_Platform" % name,
            local_x,
            local_y,
            5.0,
            (4.2, 4.2, 0.4),
            "Military",
            0.0,
            "Defenses",
        )
        self.box(
            site,
            "%s_Roof" % name,
            local_x,
            local_y,
            7.0,
            (4.8, 4.8, 0.35),
            "Roof",
            0.0,
            "Defenses",
        )

    def cleanup_stale_generated_actors(self):
        stale = []
        for actor in self.actor_subsystem.get_all_level_actors():
            tags = actor.get_editor_property("tags")
            if (
                GENERATED_TAG in tags
                and actor.get_actor_label() not in self.generated_labels
            ):
                stale.append(actor)
        for actor in stale:
            self.actor_subsystem.destroy_actor(actor)
        return len(stale)


def _build_western_fob(builder):
    site = "WesternFOB"
    builder.box(
        site,
        "GroundPad",
        0.0,
        0.0,
        -0.35,
        (150.0, 115.0, 0.7),
        "Concrete",
        0.0,
        "Ground",
    )
    builder.road(site, "Entry", 0.0, -69.0, 75.0, 10.0, 90.0)
    builder.building(
        site, "Command", -22.0, 2.0, 24.0, 16.0, 4.2,
        0.0, "Military", "Roof", 2.4,
    )
    builder.building(
        site, "BarracksA", 22.0, -28.0, 30.0, 11.0, 3.6,
        0.0, "Plaster", "Roof", 2.2,
    )
    builder.building(
        site, "BarracksB", 22.0, 28.0, 30.0, 11.0, 3.6,
        0.0, "Plaster", "Roof", 2.2,
    )
    builder.building(
        site, "MotorPool", -24.0, 31.0, 28.0, 18.0, 5.0,
        0.0, "Military", "Roof", 5.5,
    )
    for index, y_value in enumerate((-45.0, -30.0, -15.0, 15.0, 30.0, 45.0)):
        builder.barrier(
            site,
            "WestWall%02d" % index,
            -72.0,
            y_value,
            90.0,
            13.0,
        )
        builder.barrier(
            site,
            "EastWall%02d" % index,
            72.0,
            y_value,
            90.0,
            13.0,
        )
    for index, x_value in enumerate((-60.0, -42.0, -24.0, 24.0, 42.0, 60.0)):
        builder.barrier(
            site,
            "NorthWall%02d" % index,
            x_value,
            55.0,
            0.0,
            15.0,
        )
        builder.barrier(
            site,
            "SouthWall%02d" % index,
            x_value,
            -55.0,
            0.0,
            15.0,
        )
    builder.tower(site, "TowerNW", -66.0, 49.0)
    builder.tower(site, "TowerSE", 66.0, -49.0)
    builder.crates(site, "Command", -6.0, 8.0)
    builder.container(site, "MotorA", -10.0, 40.0, 90.0, "Military")
    builder.container(site, "MotorB", 0.0, 40.0, 90.0, "Rust")


def _build_dovren_village(builder):
    site = "DovrenVillage"
    builder.road(site, "Main", 0.0, 0.0, 210.0, 11.0, 0.0)
    builder.road(site, "Market", 5.0, 0.0, 125.0, 8.0, 90.0)
    houses = (
        ("HouseA", -65.0, -25.0, 14.0, 10.0, 4.0),
        ("HouseB", -35.0, 24.0, 16.0, 11.0, -8.0),
        ("HouseC", -8.0, -27.0, 14.0, 12.0, 5.0),
        ("HouseD", 25.0, 24.0, 18.0, 12.0, -4.0),
        ("HouseE", 56.0, -25.0, 16.0, 11.0, 7.0),
        ("HouseF", 82.0, 24.0, 13.0, 10.0, -7.0),
        ("Clinic", 7.0, 48.0, 22.0, 13.0, 90.0),
        ("Workshop", -70.0, 30.0, 20.0, 15.0, 90.0),
    )
    for name, x_value, y_value, width, depth, yaw in houses:
        builder.building(
            site,
            name,
            x_value,
            y_value,
            width,
            depth,
            3.4 if name != "Workshop" else 4.2,
            yaw,
        )
    builder.crates(site, "MarketA", 8.0, -10.0)
    builder.crates(site, "MarketB", 14.0, 10.0)
    builder.barrier(site, "WestCheckpoint", -96.0, 0.0, 0.0, 4.5)
    builder.barrier(site, "EastCheckpoint", 96.0, 0.0, 0.0, 4.5)


def _build_crossroads(builder):
    site = "KoronaCrossroads"
    builder.road(site, "EastWest", 0.0, 0.0, 240.0, 12.0, 0.0)
    builder.road(site, "NorthSouth", 0.0, 0.0, 210.0, 12.0, 90.0)
    builder.building(site, "Municipal", 34.0, 32.0, 26.0, 18.0, 4.6, 0.0)
    builder.building(site, "Garage", -38.0, 33.0, 28.0, 18.0, 4.5, 90.0)
    builder.building(site, "ShopA", 34.0, -30.0, 18.0, 12.0, 3.6, 0.0)
    builder.building(site, "ShopB", -34.0, -30.0, 18.0, 12.0, 3.6, 180.0)
    builder.barrier(site, "RoadblockNorth", 0.0, 36.0, 35.0, 5.0)
    builder.barrier(site, "RoadblockSouth", 0.0, -36.0, -35.0, 5.0)
    builder.barrier(site, "RoadblockEast", 42.0, 0.0, 55.0, 5.0)
    builder.crates(site, "Municipal", 21.0, 25.0)
    builder.container(site, "GarageA", -23.0, 42.0, 90.0, "Rust")


def _build_eastern_depot(builder):
    site = "EasternDepot"
    builder.box(
        site,
        "GroundPad",
        0.0,
        0.0,
        -0.3,
        (190.0, 135.0, 0.6),
        "Concrete",
        0.0,
        "Ground",
    )
    builder.road(site, "Access", -100.0, 0.0, 110.0, 12.0, 0.0)
    builder.building(
        site, "WarehouseA", 20.0, 35.0, 46.0, 24.0, 6.5,
        0.0, "DepotBlue", "Roof", 7.0,
    )
    builder.building(
        site, "WarehouseB", 20.0, -35.0, 46.0, 24.0, 6.5,
        0.0, "Military", "Roof", 7.0,
    )
    builder.building(
        site, "Office", -55.0, 28.0, 22.0, 14.0, 4.0,
        90.0, "Plaster", "Roof", 2.2,
    )
    for row in range(3):
        for column in range(4):
            builder.container(
                site,
                "Yard_%02d_%02d" % (row, column),
                -50.0 + (column * 10.0),
                -38.0 + (row * 8.0),
                90.0,
                "DepotBlue" if (row + column) % 2 == 0 else "Rust",
                stacked=(row == 2 and column in (1, 2)),
            )
    builder.tower(site, "TowerNorth", -80.0, 54.0)
    builder.tower(site, "TowerSouth", 80.0, -54.0)
    for index, y_value in enumerate((-48.0, -30.0, -12.0, 12.0, 30.0, 48.0)):
        builder.barrier(
            site,
            "WestWall%02d" % index,
            -92.0,
            y_value,
            90.0,
            15.0,
        )
        builder.barrier(
            site,
            "EastWall%02d" % index,
            92.0,
            y_value,
            90.0,
            15.0,
        )


def _build_north_pass(builder):
    site = "NorthPass"
    builder.road(site, "Pass", 0.0, 0.0, 260.0, 12.0, 0.0)
    builder.building(
        site, "BunkerNorth", 0.0, 23.0, 24.0, 14.0, 3.0,
        0.0, "Concrete", "Military", 2.0,
    )
    builder.building(
        site, "BunkerSouth", 0.0, -23.0, 24.0, 14.0, 3.0,
        180.0, "Concrete", "Military", 2.0,
    )
    builder.barrier(site, "GateA", -7.0, 0.0, 25.0, 6.0)
    builder.barrier(site, "GateB", 7.0, 0.0, -25.0, 6.0)
    builder.barrier(site, "WestA", -38.0, 9.0, 10.0, 8.0)
    builder.barrier(site, "WestB", -38.0, -9.0, -10.0, 8.0)
    builder.barrier(site, "EastA", 38.0, 9.0, -10.0, 8.0)
    builder.barrier(site, "EastB", 38.0, -9.0, 10.0, 8.0)
    builder.tower(site, "TowerA", -18.0, 38.0)
    builder.tower(site, "TowerB", 18.0, -38.0)
    builder.crates(site, "Bunker", 13.0, 18.0)


def _build_south_bridge(builder):
    site = "SouthBridge"
    builder.box(
        site,
        "BridgeDeck",
        0.0,
        0.0,
        1.3,
        (190.0, 14.0, 1.2),
        "Road",
        0.0,
        "Bridge",
    )
    for index, x_value in enumerate((-75.0, -45.0, -15.0, 15.0, 45.0, 75.0)):
        builder.box(
            site,
            "BridgePylonL%02d" % index,
            x_value,
            -5.0,
            -4.0,
            (2.0, 2.0, 5.3),
            "Concrete",
            0.0,
            "Bridge",
        )
        builder.box(
            site,
            "BridgePylonR%02d" % index,
            x_value,
            5.0,
            -4.0,
            (2.0, 2.0, 5.3),
            "Concrete",
            0.0,
            "Bridge",
        )
    builder.box(
        site,
        "BridgeRailLeft",
        0.0,
        -6.6,
        2.5,
        (190.0, 0.35, 1.0),
        "Warning",
        0.0,
        "Bridge",
    )
    builder.box(
        site,
        "BridgeRailRight",
        0.0,
        6.6,
        2.5,
        (190.0, 0.35, 1.0),
        "Warning",
        0.0,
        "Bridge",
    )
    builder.building(
        site, "GuardWest", -105.0, 18.0, 14.0, 10.0, 3.2,
        0.0, "Military", "Roof", 2.0,
    )
    builder.building(
        site, "GuardEast", 105.0, -18.0, 14.0, 10.0, 3.2,
        180.0, "Military", "Roof", 2.0,
    )
    builder.barrier(site, "WestRoadblock", -88.0, 0.0, 15.0, 5.5)
    builder.barrier(site, "EastRoadblock", 88.0, 0.0, -15.0, 5.5)


def populate_region():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(MAP_PATH)
    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    builder = RegionBuilder(actor_subsystem)

    _build_western_fob(builder)
    _build_dovren_village(builder)
    _build_crossroads(builder)
    _build_eastern_depot(builder)
    _build_north_pass(builder)
    _build_south_bridge(builder)

    removed_count = builder.cleanup_stale_generated_actors()

    if (
        not level_subsystem.save_all_dirty_levels()
        or not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
            True,
            True,
        )
    ):
        raise RuntimeError("Generated world locations could not be saved.")

    _log(
        "BH_WORLD_LOCATIONS_READY generated=%d removed_stale=%d "
        "sites=%d"
        % (
            len(builder.generated_labels),
            removed_count,
            len(SITES),
        )
    )


populate_region()
