"""Place the persistent drivable field-transport pool in the master world."""

import math

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
FOLDER_PATH = "Gameplay/Transport"
GENERATED_TAG = unreal.Name("BHFieldTransport")
CENTIMETERS_PER_METER = 100.0

TRANSPORTS = (
    {
        "label": "Transport_WesternFOB_01",
        "persistence_id": unreal.Name("WesternFOBFieldTransport01"),
        "sector_x": -9500.0,
        "sector_y": -4500.0,
        "yaw": 28.0,
        "forward_offset": -5.0,
        "right_offset": -68.0,
    },
    {
        "label": "Transport_DovrenVillage_01",
        "persistence_id": unreal.Name("DovrenVillageFieldTransport01"),
        "sector_x": -5200.0,
        "sector_y": -2850.0,
        "yaw": 24.0,
        "forward_offset": 5.0,
        "right_offset": -55.0,
    },
)


def _log(message):
    unreal.log("[BH Field Transport Setup] " + message)


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


def _local_offset(spec):
    yaw_radians = math.radians(spec["yaw"])
    forward_x = math.cos(yaw_radians)
    forward_y = math.sin(yaw_radians)
    right_x = -forward_y
    right_y = forward_x
    return (
        spec["sector_x"]
        + (forward_x * spec["forward_offset"])
        + (right_x * spec["right_offset"]),
        spec["sector_y"]
        + (forward_y * spec["forward_offset"])
        + (right_y * spec["right_offset"]),
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

    transport_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHFieldTransport",
    )

    if transport_class is None:
        raise RuntimeError(
            "BHFieldTransport is not reflected in the editor."
        )

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )

    for spec in TRANSPORTS:
        transport = _find_by_label(
            actor_subsystem,
            spec["label"],
        )
        x_meters, y_meters = _local_offset(spec)
        location = _world_location(x_meters, y_meters, 0.75)
        rotation = unreal.Rotator(
            pitch=0.0,
            yaw=spec["yaw"],
            roll=0.0,
        )

        if transport is None:
            transport = actor_subsystem.spawn_actor_from_class(
                transport_class,
                location,
                rotation,
            )

            if transport is None:
                raise RuntimeError(
                    "Could not spawn %s." % spec["label"]
                )

            transport.set_actor_label(spec["label"])
            _log("Placed " + spec["label"])
        else:
            transport.modify()
            transport.set_actor_location(location, False, True)
            transport.set_actor_rotation(rotation, False)
            _log("Updated " + spec["label"])

        transport.set_folder_path(unreal.Name(FOLDER_PATH))
        transport.set_editor_property("is_spatially_loaded", False)
        transport.set_editor_property("tags", [GENERATED_TAG])
        transport.set_editor_property(
            "persistence_id",
            spec["persistence_id"],
        )

    if (
        not level_subsystem.save_all_dirty_levels()
        or not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
            True,
            True,
        )
    ):
        raise RuntimeError("The field-transport pool could not be saved.")

    _log(
        "COMPLETE transports=%d map=%s"
        % (len(TRANSPORTS), MAP_PATH)
    )


if __name__ == "__main__":
    run_setup()
