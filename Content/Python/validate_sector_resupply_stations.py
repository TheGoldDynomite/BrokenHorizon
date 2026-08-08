"""Validate persistent-war resupply placement in the master world."""

import math

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
FOLDER_PATH = "PersistentWar/Resupply"
CENTIMETERS_PER_METER = 100.0

EXPECTED_SECTORS = {
    "NorthPass": (-1800.0, 7200.0),
    "KoronaCrossroads": (0.0, -500.0),
    "EasternDepot": (9000.0, 4500.0),
    "WesternFOB": (-9500.0, -4500.0),
    "DovrenVillage": (-5200.0, -2850.0),
    "SouthBridge": (3100.0, -5200.0),
}


def _fail(message):
    raise RuntimeError("[BH Sector Resupply Validation] " + message)


def _distance_meters(actor, sector_location):
    actor_location = actor.get_actor_location()
    sector_x_cm = sector_location[0] * CENTIMETERS_PER_METER
    sector_y_cm = sector_location[1] * CENTIMETERS_PER_METER
    delta_x = actor_location.x - sector_x_cm
    delta_y = actor_location.y - sector_y_cm
    return math.sqrt((delta_x * delta_x) + (delta_y * delta_y)) / (
        CENTIMETERS_PER_METER
    )


def run_validation():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        _fail("The master world does not exist.")

    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )

    if not level_subsystem.load_level(MAP_PATH):
        _fail("The master world could not be loaded.")

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    stations = [
        actor
        for actor in actor_subsystem.get_all_level_actors()
        if isinstance(actor, unreal.BHSectorResupplyStation)
    ]

    if len(stations) != len(EXPECTED_SECTORS):
        _fail(
            "Expected %d resupply stations but found %d."
            % (len(EXPECTED_SECTORS), len(stations))
        )

    stations_by_sector = {}

    for station in stations:
        sector_id = str(station.get_sector_id())

        if sector_id not in EXPECTED_SECTORS:
            _fail("Unexpected station sector ID: " + sector_id)

        if sector_id in stations_by_sector:
            _fail("Duplicate station for sector: " + sector_id)

        expected_label = "Resupply_%s" % sector_id

        if station.get_actor_label() != expected_label:
            _fail(
                "%s has label %s."
                % (sector_id, station.get_actor_label())
            )

        if str(station.get_folder_path()) != FOLDER_PATH:
            _fail("%s is outside the resupply folder." % sector_id)

        if station.get_editor_property("is_spatially_loaded"):
            _fail("%s is spatially loaded." % sector_id)

        distance = _distance_meters(
            station,
            EXPECTED_SECTORS[sector_id],
        )

        if distance > 100.0:
            _fail(
                "%s is %.1f m from its sector anchor."
                % (sector_id, distance)
            )

        stations_by_sector[sector_id] = station

    missing = set(EXPECTED_SECTORS) - set(stations_by_sector)

    if missing:
        _fail("Missing sectors: " + ", ".join(sorted(missing)))

    unreal.log(
        "[BH Sector Resupply Validation] PASS "
        "stations=6 sectors=6 map=L_BrokenHorizon_World"
    )


if __name__ == "__main__":
    run_validation()
