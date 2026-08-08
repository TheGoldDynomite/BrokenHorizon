"""Idempotently add the six OpenWorld field armories."""

import unreal

import create_broken_horizon_world as master_world


def setup_field_armories():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(master_world.DESTINATION_MAP)
    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    master_world.ensure_field_armories(actor_subsystem)

    # A fresh commandlet changes only these armory actors. Save only the
    # current level so unrelated dirty packages can never be swept in.
    if not level_subsystem.save_current_level():
        raise RuntimeError("Field armories could not be saved.")

    unreal.log(
        "BH_FIELD_ARMORIES_READY "
        "map=L_BrokenHorizon_World armories=6 sectors=6"
    )


if __name__ == "__main__":
    setup_field_armories()
