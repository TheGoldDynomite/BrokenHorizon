"""Read-only validation for OpenWorld tactical-support relays."""

import unreal

import create_broken_horizon_world as master_world


def validate_field_support_relays():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(master_world.DESTINATION_MAP)
    actors = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    ).get_all_level_actors()
    relays = {
        actor.get_actor_label(): actor
        for actor in actors
        if isinstance(actor, unreal.BHFieldSupportRelay)
    }

    expected = {}
    for sector in master_world.SECTOR_ANCHORS:
        expected["FieldSupportSmoke_%s" % sector["sector_id"]] = (
            sector["sector_id"],
            unreal.BHTacticalSupportType.SMOKE_SCREEN,
        )
        expected["FieldSupportMortar_%s" % sector["sector_id"]] = (
            sector["sector_id"],
            unreal.BHTacticalSupportType.MORTAR_BARRAGE,
        )

    missing = sorted(set(expected) - set(relays))
    if missing:
        raise RuntimeError(
            "Missing field support relays: %s" % ", ".join(missing)
        )

    for label, (sector_id, support_type) in expected.items():
        relay = relays[label]
        if str(relay.get_sector_id()) != sector_id:
            raise RuntimeError(
                "%s has sector %s, expected %s"
                % (label, relay.get_sector_id(), sector_id)
            )
        if relay.get_support_type() != support_type:
            raise RuntimeError(
                "%s has support type %s, expected %s"
                % (label, relay.get_support_type(), support_type)
            )

    unreal.log(
        "BH_FIELD_SUPPORT_RELAYS_VALID "
        "map=L_BrokenHorizon_World relays=12 sectors=6 "
        "smoke=6 mortar=6"
    )


if __name__ == "__main__":
    validate_field_support_relays()
