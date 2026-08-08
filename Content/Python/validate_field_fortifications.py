"""Read-only validation for OpenWorld field-fortification positions."""

import unreal

import create_broken_horizon_world as master_world


def validate_field_fortifications():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(master_world.DESTINATION_MAP)
    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    actors = actor_subsystem.get_all_level_actors()
    labels = {
        actor.get_actor_label(): actor
        for actor in actors
        if isinstance(actor, unreal.BHFieldFortification)
    }

    expected = {}
    for sector in master_world.SECTOR_ANCHORS:
        for position_index in (1, 2):
            label = "Fortification_%s_%02d" % (
                sector["sector_id"],
                position_index,
            )
            expected[label] = sector["sector_id"]

    missing = sorted(set(expected) - set(labels))
    if missing:
        raise RuntimeError(
            "Missing field fortification positions: %s"
            % ", ".join(missing)
        )

    duplicate_ids = []
    persistence_ids = set()
    for label, sector_id in expected.items():
        actor = labels[label]
        if str(actor.get_sector_id()) != sector_id:
            raise RuntimeError(
                "%s has sector %s, expected %s"
                % (label, actor.get_sector_id(), sector_id)
            )
        persistence_id = str(actor.get_persistence_id())
        if not persistence_id or persistence_id == "None":
            raise RuntimeError("%s has no persistence ID" % label)
        if persistence_id in persistence_ids:
            duplicate_ids.append(persistence_id)
        persistence_ids.add(persistence_id)

    if duplicate_ids:
        raise RuntimeError(
            "Duplicate fortification persistence IDs: %s"
            % ", ".join(sorted(duplicate_ids))
        )

    unreal.log(
        "BH_FIELD_FORTIFICATIONS_VALID "
        "map=L_BrokenHorizon_World positions=12 unique_ids=12 sectors=6"
    )


if __name__ == "__main__":
    validate_field_fortifications()
