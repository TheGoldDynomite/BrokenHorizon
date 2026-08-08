"""Read-only OpenWorld field-armory validation."""

import unreal

import create_broken_horizon_world as master_world


def validate_field_armories():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(master_world.DESTINATION_MAP)
    actors = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    ).get_all_level_actors()
    armories = {
        actor.get_actor_label(): actor
        for actor in actors
        if isinstance(actor, unreal.BHFieldArmory)
    }

    expected = {
        "FieldArmory_%s" % sector["sector_id"]: sector["sector_id"]
        for sector in master_world.SECTOR_ANCHORS
    }
    missing = sorted(set(expected) - set(armories))
    if missing:
        raise RuntimeError("Missing field armories: %s" % ", ".join(missing))

    for label, sector_id in expected.items():
        if str(armories[label].get_sector_id()) != sector_id:
            raise RuntimeError(
                "%s has sector %s, expected %s"
                % (label, armories[label].get_sector_id(), sector_id)
            )

    unreal.log(
        "BH_FIELD_ARMORIES_VALID "
        "map=L_BrokenHorizon_World armories=6 sectors=6"
    )


if __name__ == "__main__":
    validate_field_armories()
