import importlib.util
import os

import unreal


SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
WORLD_BUILDER_PATH = os.path.join(
    SCRIPT_DIRECTORY,
    "create_broken_horizon_world.py",
)


def load_world_builder():
    spec = importlib.util.spec_from_file_location(
        "bh_world_builder",
        WORLD_BUILDER_PATH,
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def setup_route_network():
    builder = load_world_builder()
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(builder.DESTINATION_MAP)

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    builder.ensure_world_routes(actor_subsystem)

    if (
        not level_subsystem.save_all_dirty_levels()
        or not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
            True,
            True,
        )
    ):
        raise RuntimeError(
            "The world route network could not be saved."
        )

    unreal.log(
        "BH_ROUTE_NETWORK_READY active_routes=%d"
        % len(builder.WORLD_ROUTES)
    )


setup_route_network()
