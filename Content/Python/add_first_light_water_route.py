"""Idempotently author the first water-route volume in First Light.

This adds only the reusable native water actor to the existing map. Final
terrain, materials, shoreline art, and route review remain content gates.
"""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
WATER_CLASS = "/Script/BrokenHorizon.BHWaterSurface"
TRANSPORT_CLASS = "/Script/BrokenHorizon.BHFieldTransport"
WATER_ID = "FirstLightWaterRoute01"
TRANSPORT_ID = "FirstLightWatercraft01"
TAG = "BH_Auto_FirstLight_Water"
LOCATION = unreal.Vector(8500.0, 300.0, 0.0)
# Stage the boat near the west edge of the authored route so the existing
# First Light deployment can reach the crossing without a long dead transit.
TRANSPORT_LOCATION = unreal.Vector(5000.0, 300.0, 110.0)
TRANSPORT_CARGO_SUPPLY = 15.0
TRANSPORT_CARGO_SOURCE = unreal.Name("WesternFOB")
TRANSPORT_CARGO_DESTINATION = unreal.Name("EasternDepot")


def _find_existing():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_name().startswith(WATER_ID):
            return actor
    return None


def _find_transport():
    matches = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == "FirstLight Waterborne Cargo Transport":
            matches.append(actor)
            continue
        try:
            if actor.get_editor_property("persistence_id") == unreal.Name(
                TRANSPORT_ID
            ):
                matches.append(actor)
        except Exception:
            pass

    if not matches:
        return None

    # Keep the first stable authored instance and remove only exact duplicate
    # watercraft IDs left by older non-idempotent runs.
    primary = matches[0]
    for duplicate in matches[1:]:
        unreal.EditorLevelLibrary.destroy_actor(duplicate)
        unreal.log_warning(
            "[FirstLight Water] Removed duplicate %s" % duplicate.get_name()
        )
    return primary


def _ensure_watercraft():
    existing = _find_transport()
    if existing:
        existing.set_actor_location(TRANSPORT_LOCATION, False, True)
        existing.set_actor_rotation(unreal.Rotator(0.0, 90.0, 0.0), False)
        existing.set_editor_property("waterborne_transport", True)
        existing.configure_authored_cargo(
            TRANSPORT_CARGO_SUPPLY,
            TRANSPORT_CARGO_SOURCE,
            TRANSPORT_CARGO_DESTINATION,
        )
        return existing

    transport_class = unreal.load_class(None, TRANSPORT_CLASS)
    if not transport_class:
        unreal.log_error("[FirstLight Water] Native transport class unavailable")
        return None

    transport = unreal.EditorLevelLibrary.spawn_actor_from_class(
        transport_class, TRANSPORT_LOCATION, unreal.Rotator(0.0, 90.0, 0.0)
    )
    if not transport:
        unreal.log_error("[FirstLight Water] Watercraft spawn failed")
        return None

    transport.set_actor_label("FirstLight Waterborne Cargo Transport")
    transport.tags = list(transport.tags) + [unreal.Name(TAG)]
    transport.set_editor_property("persistence_id", unreal.Name(TRANSPORT_ID))
    transport.set_editor_property("waterborne_transport", True)
    transport.set_editor_property("water_surface_z", 0.0)
    transport.configure_authored_cargo(
        TRANSPORT_CARGO_SUPPLY,
        TRANSPORT_CARGO_SOURCE,
        TRANSPORT_CARGO_DESTINATION,
    )
    return transport


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    if not unreal.EditorLevelLibrary.get_editor_world():
        unreal.log_error("[FirstLight Water] Editor world unavailable")
        return False

    existing = _find_existing()
    if existing:
        existing.set_editor_property("infantry_speed_multiplier", 0.60)
        actor = existing
    else:
        water_class = unreal.load_class(None, WATER_CLASS)
        if not water_class:
            unreal.log_error("[FirstLight Water] Native water class unavailable")
            return False

        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            water_class, LOCATION, unreal.Rotator(0.0, 0.0, 0.0)
        )
        if not actor:
            unreal.log_error("[FirstLight Water] Spawn failed")
            return False

        actor.set_actor_label("FirstLight Water Route")
        actor.tags = list(actor.tags) + [unreal.Name(TAG)]
        actor.set_editor_property("water_id", unreal.Name(WATER_ID))
        actor.set_editor_property(
            "surface_extents", unreal.Vector(4200.0, 900.0, 120.0)
        )
        actor.set_editor_property("surface_height", 0.0)
        actor.set_editor_property("infantry_speed_multiplier", 0.60)

    if not _ensure_watercraft():
        return False
    unreal.EditorLevelLibrary.save_current_level()
    watercraft = _find_transport()
    unreal.log(
        "[FirstLight Water] Authored %s and %s cargo=%.1f source=%s destination=%s"
        % (
            WATER_ID,
            TRANSPORT_ID,
            watercraft.get_cargo_supply() if watercraft else 0.0,
            watercraft.get_cargo_source_sector_id() if watercraft else "None",
            watercraft.get_cargo_destination_sector_id()
            if watercraft
            else "None",
        )
    )
    return True


main()
