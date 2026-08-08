"""Place persistent-war resupply stations in the master open world."""

import math

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
FOLDER_PATH = "PersistentWar/Resupply"
CENTIMETERS_PER_METER = 100.0

SECTORS = (
    ("NorthPass", -1800.0, 7200.0, -12.0),
    ("KoronaCrossroads", 0.0, -500.0, 18.0),
    ("EasternDepot", 9000.0, 4500.0, -145.0),
    ("WesternFOB", -9500.0, -4500.0, 28.0),
    ("DovrenVillage", -5200.0, -2850.0, 24.0),
    ("SouthBridge", 3100.0, -5200.0, 32.0),
)


def _log(message):
    unreal.log("[BH Sector Resupply Setup] " + message)


def _world_location(x_meters, y_meters, height_offset=0.0):
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


def _station_location(x_meters, y_meters, yaw_degrees):
    yaw_radians = math.radians(yaw_degrees)
    forward_x = math.cos(yaw_radians)
    forward_y = math.sin(yaw_radians)
    right_x = -forward_y
    right_y = forward_x

    # Keep the station near its sector anchor while leaving the anchor's
    # operation spawn center unobstructed.
    return _world_location(
        x_meters + (forward_x * 30.0) + (right_x * 40.0),
        y_meters + (forward_y * 30.0) + (right_y * 40.0),
        0.35,
    )


def _find_by_label(actor_subsystem, label):
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def run_setup():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing master world: " + MAP_PATH)

    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )

    if not level_subsystem.load_level(MAP_PATH):
        raise RuntimeError("Could not load the Broken Horizon master world.")

    station_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHSectorResupplyStation",
    )

    if station_class is None:
        raise RuntimeError(
            "BHSectorResupplyStation is not reflected in the editor."
        )

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )

    for sector_id, x_meters, y_meters, yaw_degrees in SECTORS:
        label = "Resupply_%s" % sector_id
        station = _find_by_label(actor_subsystem, label)
        location = _station_location(
            x_meters,
            y_meters,
            yaw_degrees,
        )
        rotation = unreal.Rotator(
            pitch=0.0,
            yaw=yaw_degrees,
            roll=0.0,
        )

        if station is None:
            station = actor_subsystem.spawn_actor_from_class(
                station_class,
                location,
                rotation,
            )

            if station is None:
                raise RuntimeError(
                    "Could not spawn the %s resupply station."
                    % sector_id
                )

            station.set_actor_label(label)
            _log("Placed " + label)
        else:
            station.set_actor_location(location, False, True)
            station.set_actor_rotation(rotation, False)
            _log("Updated " + label)

        station.set_folder_path(unreal.Name(FOLDER_PATH))
        station.set_editor_property("is_spatially_loaded", False)
        station.configure_station(unreal.Name(sector_id))

    if (
        not level_subsystem.save_all_dirty_levels()
        or not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
            True,
            True,
        )
    ):
        raise RuntimeError("Sector resupply stations could not be saved.")

    _log("COMPLETE stations=%d map=%s" % (len(SECTORS), MAP_PATH))


if __name__ == "__main__":
    run_setup()
