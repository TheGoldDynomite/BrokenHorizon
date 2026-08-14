"""Report First Light deployment and route actors without changing the map."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    for actor in actors:
        name = actor.get_name()
        cls = actor.get_class().get_name()
        if (
            "PlayerStart" in cls
            or "Transport" in cls
            or name.startswith("FirstLightWater")
        ):
            unreal.log(
                "[FirstLight Staging] %s class=%s location=%s label=%s"
                % (
                    name,
                    cls,
                    actor.get_actor_location(),
                    actor.get_actor_label(),
                )
            )


main()
