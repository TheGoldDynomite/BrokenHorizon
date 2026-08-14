"""Inspect serialized Attack A ground and navigation coverage."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.get_editor_subsystem(
        unreal.UnrealEditorSubsystem
    ).get_editor_world()
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        label = actor.get_actor_label()
        if (
            "Compound" in label
            or "AttackA" in label
            or "Nav" in label
            or isinstance(actor, unreal.RecastNavMesh)
            or isinstance(actor, unreal.NavMeshBoundsVolume)
        ):
            try:
                origin, extent = actor.get_actor_bounds(False)
                unreal.log(
                    "BH_ATTACK_NAV_INSPECT label=%s class=%s "
                    "location=%s origin=%s extent=%s spatial=%s"
                    % (
                        label,
                        actor.get_class().get_name(),
                        actor.get_actor_location(),
                        origin,
                        extent,
                        actor.get_editor_property("is_spatially_loaded")
                        if actor.get_class().get_name()
                        in ("RecastNavMesh", "NavMeshBoundsVolume")
                        else "n/a",
                    )
                )
            except Exception as error:
                unreal.log_warning(
                    "BH_ATTACK_NAV_INSPECT label=%s error=%s" %
                    (label, error)
                )


main()
