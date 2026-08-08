import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
NAV_BOUNDS_LABEL = "Nav_RegionAlpha_Dynamic"
TILE_SIZE_UU = 9216.0


def fail(message):
    raise RuntimeError("[BH OpenWorld Navigation Repair] " + message)


def main():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    if not level_subsystem.load_level(MAP_PATH):
        fail("Could not load the canonical OpenWorld map.")

    world = unreal.get_editor_subsystem(
        unreal.UnrealEditorSubsystem
    ).get_editor_world()
    navmeshes = unreal.GameplayStatics.get_all_actors_of_class(
        world,
        unreal.RecastNavMesh,
    )
    bounds = [
        actor
        for actor in unreal.GameplayStatics.get_all_actors_of_class(
            world,
            unreal.NavMeshBoundsVolume,
        )
        if actor.get_actor_label() == NAV_BOUNDS_LABEL
    ]

    if len(navmeshes) != 1:
        fail("Expected one Recast navmesh; found %d." % len(navmeshes))
    if len(bounds) != 1:
        fail(
            "Expected one %s volume; found %d."
            % (NAV_BOUNDS_LABEL, len(bounds))
        )

    navmesh = navmeshes[0]
    navmesh.modify()
    navmesh.set_editor_property(
        "runtime_generation",
        unreal.RuntimeGenerationType.DYNAMIC,
    )
    navmesh.set_editor_property("force_rebuild_on_load", True)
    navmesh.set_editor_property("tile_size_uu", TILE_SIZE_UU)
    navmesh.set_editor_property("fixed_tile_pool_size", False)
    navmesh.set_editor_property("tile_pool_size", 1024)
    navmesh.set_editor_property("average_layers_per_tile", 12.0)
    navmesh.set_editor_property("is_spatially_loaded", False)

    bounds[0].modify()
    bounds[0].set_editor_property("is_spatially_loaded", False)

    if (
        not level_subsystem.save_all_dirty_levels()
        or not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
            True,
            True,
        )
    ):
        fail("Could not save the repaired OpenWorld navigation settings.")

    unreal.log(
        "BH_OPEN_WORLD_NAV_REPAIR result=success "
        "tile_size_uu=%.1f runtime=dynamic bounds=%s"
        % (TILE_SIZE_UU, NAV_BOUNDS_LABEL)
    )


main()
