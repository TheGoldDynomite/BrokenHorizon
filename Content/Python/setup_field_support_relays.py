"""Idempotently add paired tactical-support relays to all OpenWorld sectors."""

import unreal

import create_broken_horizon_world as master_world


def setup_field_support_relays():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(master_world.DESTINATION_MAP)
    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    master_world.ensure_field_support_relays(actor_subsystem)

    if not level_subsystem.save_current_level():
        raise RuntimeError("Field support relays could not be saved.")

    unreal.log(
        "BH_FIELD_SUPPORT_RELAYS_READY "
        "map=L_BrokenHorizon_World relays=12 sectors=6 "
        "smoke=6 mortar=6"
    )


if __name__ == "__main__":
    setup_field_support_relays()
