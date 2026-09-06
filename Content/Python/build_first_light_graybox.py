"""Generate the non-destructive Operation First Light graybox map for UE 5.8.

Run from Tools > Execute Python Script.  By default this creates
/Game/BrokenHorizon/Maps/L_FirstLight_Graybox only when it does not exist.
It never opens or edits L_Prototype.  Set REBUILD_EXISTING to True only when
you deliberately want to remove actors tagged by this script in the target map.
"""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_FirstLight"
OBJECTIVE_ID = "EliminateGuard"
REBUILD_EXISTING = False

FOLDERS = {
    "geometry": "FirstLight/Graybox Geometry",
    "gameplay": "FirstLight/Gameplay",
    "patrol": "FirstLight/Gameplay/Patrol",
    "lighting": "FirstLight/Lighting",
    "navigation": "FirstLight/Navigation",
}

ASSETS = {
    "keycard": "/Game/BH_Keycard",
    "door": "/Game/BP_Door",
    "checkpoint": "/Game/BP_Checkpoint",
    "extraction": "/Game/BH_ExtractionZone",
    "enemy": "/Game/Characters/BP_EnemySoldier",
    "ammo": "/Game/BrokenHorizon/Core/BP_AmmoSupply",
    "medical": "/Game/BrokenHorizon/Core/BP_MedicalSupply",
    "armor_plate": "/Game/BrokenHorizon/Core/BP_ArmorPlateSupply",
    "helmet": "/Game/BrokenHorizon/Core/BP_HelmetSupply",
    "game_mode": "/Game/BrokenHorizon/Core/BP_BHGameMode",
}


def _log(message):
    unreal.log("[FirstLight Graybox] " + message)


def _warning(message):
    unreal.log_warning("[FirstLight Graybox] " + message)


def _class(asset_path):
    asset_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    if not asset_class:
        raise RuntimeError("Required Blueprint class is missing: " + asset_path)
    return asset_class


def _tag_and_label(actor, label, folder):
    actor.tags = list(actor.tags) + [TAG]
    actor.set_actor_label(label)
    actor.set_folder_path(unreal.Name(folder))
    return actor


def _spawn_actor(actor_class, location, label, folder, rotation=None):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = subsystem.spawn_actor_from_class(
        actor_class,
        location,
        rotation if rotation else unreal.Rotator(0, 0, 0),
    )
    if not actor:
        raise RuntimeError("Could not spawn " + label)
    return _tag_and_label(actor, label, folder)


def _set(actor, property_name, value, required=True):
    try:
        actor.set_editor_property(property_name, value)
        return True
    except Exception as error:
        message = "%s: could not set %s (%s)" % (
            actor.get_actor_label(), property_name, error
        )
        if required:
            raise RuntimeError(message)
        _warning(message)
        return False


def _cube(location, dimensions, label, folder="geometry", rotation=None):
    """Spawn a collision-enabled 100 cm Engine cube and scale it to cm dimensions."""
    actor = _spawn_actor(
        unreal.StaticMeshActor,
        unreal.Vector(*location),
        label,
        FOLDERS[folder],
        rotation,
    )
    component = actor.get_editor_property("static_mesh_component")
    cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    component.set_editor_property("static_mesh", cube_mesh)
    actor.set_actor_scale3d(unreal.Vector(
        dimensions[0] / 100.0,
        dimensions[1] / 100.0,
        dimensions[2] / 100.0,
    ))
    return actor


def _place_wall_x(x, y, length, label):
    return _cube((x, y, 175), (20, length, 350), label)


def _place_wall_y(x, y, length, label):
    return _cube((x, y, 175), (length, 20, 350), label)


def _build_geometry():
    # A compact 8.5 km-by-1 km playable corridor, split into readable spaces.
    _cube((4200, 0, -25), (9000, 3000, 50), "FL_Ground")
    _cube((1200, 0, 2), (2400, 550, 10), "FL_ApproachRoad")
    _cube((6000, 0, 2), (2600, 700, 10), "FL_CompoundRoad")

    # Start/orientation shelter.
    _place_wall_y(300, -400, 900, "FL_StartShelterSouth")
    _place_wall_y(300, 400, 900, "FL_StartShelterNorth")
    _place_wall_x(-150, 0, 800, "FL_StartShelterRear")
    _cube((250, -260, 90), (220, 120, 180), "FL_StartCover")

    # Outdoor approach: two route choices around the central road.
    _place_wall_y(1550, -1450, 2900, "FL_SouthBoundaryWall")
    _place_wall_y(1550, 1450, 2900, "FL_NorthBoundaryWall")
    for index, (x, y) in enumerate(((900, 620), (1400, -600), (1850, 600), (2100, -650), (2500, 700))):
        _cube((x, y, 90), (220, 120, 180), "FL_ApproachCover_%02d" % index)
    _cube((1650, 0, 100), (500, 180, 200), "FL_RoadBarrier")

    # Administration/security building: open front, internal keycard room.
    _cube((2900, 0, 2), (1500, 1200, 10), "FL_AdminFloor")
    _place_wall_y(2900, -600, 1500, "FL_AdminSouthWall")
    _place_wall_y(2900, 600, 1500, "FL_AdminNorthWall")
    _place_wall_x(3650, 0, 1200, "FL_AdminEastWall")
    _place_wall_x(2150, -390, 420, "FL_AdminFrontWallSouth")
    _place_wall_x(2150, 390, 420, "FL_AdminFrontWallNorth")
    _place_wall_y(3025, 0, 1000, "FL_AdminInteriorWall")
    _cube((2750, -260, 90), (350, 120, 180), "FL_AdminDesk")
    _cube((3320, 280, 90), (220, 120, 180), "FL_AdminCover")

    # Security choke: the only route through this wall uses the locked door.
    _place_wall_x(4200, -850, 1200, "FL_SecurityWallSouth")
    _place_wall_x(4200, 850, 1200, "FL_SecurityWallNorth")
    _cube((4200, -260, 90), (220, 120, 180), "FL_SecurityCheckpointCoverSouth")
    _cube((4200, 260, 90), (220, 120, 180), "FL_SecurityCheckpointCoverNorth")

    # Communications compound: open-roof rooms keep the graybox readable.
    _cube((6100, 0, 2), (2600, 1800, 10), "FL_CompoundFloor")
    _place_wall_y(6100, -900, 2600, "FL_CompoundSouthWall")
    _place_wall_y(6100, 900, 2600, "FL_CompoundNorthWall")
    _place_wall_x(7400, 0, 1800, "FL_CompoundEastWall")
    _place_wall_y(6000, -280, 900, "FL_CommsInteriorSouth")
    _place_wall_y(6000, 280, 900, "FL_CommsInteriorNorth")
    _cube((5350, -430, 90), (260, 140, 180), "FL_CompoundCoverSouth")
    _cube((5600, 430, 90), (260, 140, 180), "FL_CompoundCoverNorth")
    _cube((6500, -420, 90), (400, 160, 180), "FL_CommsConsoleCoverSouth")
    _cube((6800, 420, 90), (400, 160, 180), "FL_CommsConsoleCoverNorth")

    # Extraction pad beyond the guard compound.
    _cube((8000, 0, 2), (1000, 1300, 10), "FL_ExtractionPad")
    _place_wall_y(8000, -650, 1000, "FL_ExtractionBoundarySouth")
    _place_wall_y(8000, 650, 1000, "FL_ExtractionBoundaryNorth")
    _cube((7700, -300, 90), (200, 120, 180), "FL_ExtractionCoverSouth")
    _cube((7700, 300, 90), (200, 120, 180), "FL_ExtractionCoverNorth")


def _place_gameplay():
    classes = {key: _class(path) for key, path in ASSETS.items()}
    _spawn_actor(unreal.PlayerStart, unreal.Vector(0, 0, 120), "FL_PlayerStart", FOLDERS["gameplay"])

    # Place the card on clear floor immediately in front of the admin desk.
    keycard = _spawn_actor(classes["keycard"], unreal.Vector(2450, 0, 20), "FL_RedKeycard", FOLDERS["gameplay"])
    _set(keycard, "keycard_id", unreal.Name("RedKeycard"))
    _set(keycard, "persistence_id", unreal.Name("FirstLightRedKeycard"))
    keycard_mesh = keycard.get_editor_property("keycard_mesh")
    keycard_material = unreal.load_asset(
        "/Game/BrokenHorizon/Environment/WorldKit/Materials/M_BH_Keycard"
    )
    if keycard_mesh and keycard_material:
        # Keep the actor/root at unit scale so its interaction collision is
        # not flattened by the graybox card's presentation scale.
        keycard.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        keycard_mesh.set_editor_property(
            "relative_scale3d",
            unreal.Vector(0.5, 1.0, 0.05),
        )
        keycard_mesh.set_material(0, keycard_material)
    else:
        _warning("Red keycard material is unavailable; visual review remains pending.")

    door = _spawn_actor(classes["door"], unreal.Vector(4200, -50, 0), "FL_LockedSecurityDoor", FOLDERS["gameplay"])
    free_door = unreal.load_asset("/Game/BrokenHorizon/Environment/FreeMetalDoor/bunkerdoor")
    security_mesh = unreal.load_asset("/Game/BrokenHorizon/Environment/WorldKit/Meshes/SM_FirstLightSecurityDoor")
    if free_door:
        import integrate_free_first_light_door as free_door_setup
        free_mesh, free_material, panel_cube = free_door_setup.assets()
        free_door_setup.configure_door(door, free_mesh, free_material)
        side_panel = _spawn_actor(unreal.StaticMeshActor, unreal.Vector(*free_door_setup.PANEL_LOCATION),
                                  free_door_setup.PANEL_LABEL, FOLDERS["geometry"])
        free_door_setup.configure_panel(side_panel, panel_cube, free_material)
    elif security_mesh:
        door_mesh = door.get_editor_property("door_mesh")
        door_mesh.set_static_mesh(security_mesh)
        door_mesh.set_editor_property("override_materials", [])
        door_mesh.set_editor_property("relative_location", unreal.Vector(0, 100, 100))
    else:
        door.set_actor_location(unreal.Vector(4200, 0, 0), False, True)
        _warning("Authored First Light security door mesh missing; run author_first_light_security_door.py --apply.")
    # UE's Python reflection removes the leading b from bool UPROPERTY names.
    _set(door, "locked", True)
    _set(door, "required_keycard", unreal.Name("RedKeycard"))
    _set(door, "persistence_id", unreal.Name("FirstLightSecurityDoor"))

    checkpoint = _spawn_actor(classes["checkpoint"], unreal.Vector(4550, -300, 80), "FL_MidpointCheckpoint", FOLDERS["gameplay"])
    ammo = _spawn_actor(classes["ammo"], unreal.Vector(5350, 430, 80), "FL_AmmoSupply", FOLDERS["gameplay"])
    _set(ammo, "persistence_id", unreal.Name("FirstLightAmmoSupply"))
    medical = _spawn_actor(classes["medical"], unreal.Vector(5700, -430, 80), "FL_MedicalSupply", FOLDERS["gameplay"])
    _set(medical, "persistence_id", unreal.Name("FirstLightMedicalSupply"))
    armor_plate = _spawn_actor(classes["armor_plate"], unreal.Vector(6000, -650, 80), "FL_ArmorPlateSupply", FOLDERS["gameplay"])
    _set(armor_plate, "persistence_id", unreal.Name("FirstLightArmorPlateSupply"))
    helmet = _spawn_actor(classes["helmet"], unreal.Vector(6900, 650, 80), "FL_HelmetSupply", FOLDERS["gameplay"])
    _set(helmet, "persistence_id", unreal.Name("FirstLightHelmetSupply"))

    patrol_locations = ((5650, 0, 95), (6400, -450, 95), (6900, 350, 95))
    patrol_points = []
    patrol_class = unreal.load_class(None, "/Script/BrokenHorizon.BHPatrolPoint")
    for index, location in enumerate(patrol_locations, 1):
        patrol_points.append(_spawn_actor(patrol_class, unreal.Vector(*location), "FL_PatrolPoint_%02d" % index, FOLDERS["patrol"]))

    # All three guards share EliminateGuard. Native group completion keeps
    # that objective active until the final living peer is eliminated.
    enemy_locations = (
        (5650, 0, 95),
        (6150, -450, 95),
        (6650, 450, 95),
    )
    for index, location in enumerate(enemy_locations, 1):
        enemy = _spawn_actor(
            classes["enemy"],
            unreal.Vector(*location),
            "FL_Guard_%02d" % index,
            FOLDERS["gameplay"],
        )
        enemy.set_objective_id_to_complete_on_death(
            unreal.Name(OBJECTIVE_ID)
        )
        _set(enemy, "patrol_points", patrol_points)

    cover_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHCoverPoint",
    )
    cover_locations = (
        (5525, -430, 95),
        (5775, 430, 95),
        (6700, -420, 95),
        (7000, 420, 95),
        (7850, -300, 95),
        (7850, 300, 95),
    )
    for index, location in enumerate(cover_locations, 1):
        _spawn_actor(
            cover_class,
            unreal.Vector(*location),
            "FL_CoverPoint_%02d" % index,
            FOLDERS["gameplay"],
        )

    extraction = _spawn_actor(classes["extraction"], unreal.Vector(8000, 0, 80), "FL_ExtractionZone", FOLDERS["gameplay"])
    _set(extraction, "required_objective_id", unreal.Name(OBJECTIVE_ID))
    _set(extraction, "extraction_objective_id", unreal.Name("ReachExtraction"))

    nav = _spawn_actor(unreal.NavMeshBoundsVolume, unreal.Vector(4300, 0, 300), "FL_NavMeshBounds", FOLDERS["navigation"])
    nav.set_actor_scale3d(unreal.Vector(45, 15, 5))
    return checkpoint


def _configure_world():
    world = unreal.EditorLevelLibrary.get_editor_world()
    game_mode = _class(ASSETS["game_mode"])
    _set(world.get_world_settings(), "default_game_mode", game_mode, required=False)
    sun = _spawn_actor(unreal.DirectionalLight, unreal.Vector(2500, 0, 1800), "FL_DirectionalLight", FOLDERS["lighting"], unreal.Rotator(-45, -35, 0))
    sky = _spawn_actor(unreal.SkyLight, unreal.Vector(2500, 0, 900), "FL_SkyLight", FOLDERS["lighting"])
    return sun


def _build_navigation_in_world(world):
    """Build paths when supported, otherwise persist dynamic Recast data."""
    recast_navmeshes = unreal.GameplayStatics.get_all_actors_of_class(
        world,
        unreal.RecastNavMesh,
    )

    for navmesh in recast_navmeshes:
        navmesh.set_editor_property(
            "runtime_generation",
            unreal.RuntimeGenerationType.DYNAMIC,
        )
        navmesh.set_editor_property("force_rebuild_on_load", True)
        # Keep serialized map data aligned with DefaultEngine.ini. A stale
        # 8192 UU build serializes as 8189 UU in Recast and is discarded when
        # the project-wide 9216 UU settings load at runtime.
        navmesh.set_editor_property("tile_size_uu", 9216.0)
        navmesh.set_editor_property("fixed_tile_pool_size", False)
        navmesh.set_editor_property("tile_pool_size", 1024)
        navmesh.set_editor_property("average_layers_per_tile", 12.0)
        navmesh.set_editor_property("is_spatially_loaded", False)

    if hasattr(unreal, "EditorBuildUtils"):
        unreal.EditorBuildUtils.editor_build(
            world, unreal.EditorBuildType.PATHS
        )
        _log(
            "Built First Light navigation paths using "
            "9216 cm tiles, 12 average layers, and a 1024-tile pool."
        )
        return True

    if recast_navmeshes:
        _log(
            "Configured %d dynamic Recast navmesh actor(s) using "
            "9216 cm tiles, 12 average layers, and a 1024-tile pool."
            % len(recast_navmeshes)
        )
    else:
        _log(
            "No serialized Recast actor was present; project-wide "
            "dynamic navigation will create it during play."
        )

    return True


def build_navigation_for_first_light():
    """Build paths for the existing First Light map without rebuilding actors."""
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("First Light map does not exist: " + MAP_PATH)
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    success = _build_navigation_in_world(
        unreal.EditorLevelLibrary.get_editor_world()
    )
    if success:
        unreal.EditorLevelLibrary.save_current_level()
        _log("Saved First Light navigation paths.")
    return success


def _existing_generated_actors():
    world = unreal.EditorLevelLibrary.get_editor_world()
    return unreal.GameplayStatics.get_all_actors_with_tag(world, TAG)


def _clear_generated_only():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in _existing_generated_actors():
        subsystem.destroy_actor(actor)


def build_map():
    """Create the map, or refuse safely when a map already exists."""
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not REBUILD_EXISTING:
            _warning("Target map already exists. Nothing was changed: " + MAP_PATH)
            _warning("Set REBUILD_EXISTING = True only to replace tagged First Light actors in that map.")
            return False
        unreal.EditorLevelLibrary.load_level(MAP_PATH)
        _clear_generated_only()
        _log("Removed only actors tagged " + TAG)
    else:
        subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not subsystem.new_level(MAP_PATH):
            raise RuntimeError("Could not create " + MAP_PATH)
        _log("Created " + MAP_PATH)

    _configure_world()
    _build_geometry()
    _place_gameplay()
    _build_navigation_in_world(unreal.EditorLevelLibrary.get_editor_world())
    unreal.EditorLevelLibrary.save_current_level()
    _log("Saved %s. Build Paths (P) once in the editor before testing patrol movement." % MAP_PATH)
    _log("Objectives wired: FindRedKeycard -> UnlockSecurityDoor -> EliminateGuard -> ReachExtraction")
    return True


if __name__ == "__main__":
    build_map()
