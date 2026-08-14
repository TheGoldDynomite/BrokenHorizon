"""Inspect collision settings for the Attack A compound geometry."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if not actor.get_actor_label().startswith("FL_Compound"):
            continue
        component = actor.get_editor_property("static_mesh_component")
        unreal.log(
            "BH_ATTACK_COLLISION label=%s mesh=%s profile=%s mobility=%s"
            % (
                actor.get_actor_label(),
                component.get_editor_property("static_mesh"),
                component.get_editor_property("body_instance").get_editor_property(
                    "collision_profile_name"
                ),
                component.get_editor_property("mobility"),
            )
        )


main()
