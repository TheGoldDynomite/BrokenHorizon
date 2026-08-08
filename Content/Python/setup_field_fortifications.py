"""Idempotently add tactical field-fortification positions to OpenWorld."""

import unreal

import create_broken_horizon_world as master_world


def setup_field_fortifications():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(master_world.DESTINATION_MAP)
    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    master_world.ensure_field_fortification_positions(actor_subsystem)

    # This commandlet loads the map in a fresh process, changes only the
    # fortification actors above, and saves only the current level. Never use
    # save_all_dirty_levels/save_dirty_packages here: those APIs can sweep up
    # unrelated user work in an interactive editor session.
    if not level_subsystem.save_current_level():
        raise RuntimeError(
            "Field fortification positions could not be saved."
        )

    unreal.log(
        "BH_FIELD_FORTIFICATIONS_READY "
        "map=L_BrokenHorizon_World positions=12 sectors=6"
    )


if __name__ == "__main__":
    setup_field_fortifications()
