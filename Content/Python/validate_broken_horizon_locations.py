"""Validate generated settlement and combat-space coverage."""

from collections import Counter

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
GENERATED_TAG = "BHGeneratedWorld"
EXPECTED_SITES = (
    "WesternFOB",
    "DovrenVillage",
    "KoronaCrossroads",
    "SouthBridge",
    "EasternDepot",
    "NorthPass",
)
REQUIRED_LANDMARKS = (
    "BH_World_WesternFOB_Command_Roof",
    "BH_World_DovrenVillage_Clinic_Roof",
    "BH_World_KoronaCrossroads_Municipal_Roof",
    "BH_World_SouthBridge_BridgeDeck",
    "BH_World_EasternDepot_WarehouseA_Roof",
    "BH_World_NorthPass_BunkerNorth_Roof",
)


def _fail(message):
    raise RuntimeError("[BH Location Validation] " + message)


def main():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    level_subsystem.load_level(MAP_PATH)
    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    actors = actor_subsystem.get_all_level_actors()
    generated = [
        actor
        for actor in actors
        if GENERATED_TAG in [
            str(tag)
            for tag in actor.get_editor_property("tags")
        ]
    ]
    by_label = {
        actor.get_actor_label(): actor
        for actor in generated
    }

    missing = [
        label
        for label in REQUIRED_LANDMARKS
        if label not in by_label
    ]
    if missing:
        _fail("Missing landmarks: " + ", ".join(missing))

    site_counts = Counter()
    mesh_count = 0
    cover_count = 0
    collision_failures = []
    spatial_failures = []

    for actor in generated:
        label = actor.get_actor_label()
        for site in EXPECTED_SITES:
            if label.startswith("BH_World_%s_" % site):
                site_counts[site] += 1
                break

        if isinstance(actor, unreal.StaticMeshActor):
            mesh_count += 1
            component = actor.get_editor_property(
                "static_mesh_component"
            )
            if not component.get_editor_property("static_mesh"):
                _fail(label + " has no static mesh.")
            if (
                component.get_collision_enabled()
                == unreal.CollisionEnabled.NO_COLLISION
            ):
                collision_failures.append(label)
        elif isinstance(actor, unreal.BHCoverPoint):
            cover_count += 1

        if not actor.get_editor_property("is_spatially_loaded"):
            spatial_failures.append(label)

    if len(generated) < 180:
        _fail(
            "Expected at least 180 generated actors; found %d."
            % len(generated)
        )
    if mesh_count < 145:
        _fail(
            "Expected at least 145 collision meshes; found %d."
            % mesh_count
        )
    if cover_count < 24:
        _fail(
            "Expected at least 24 tactical cover points; found %d."
            % cover_count
        )
    sparse_sites = [
        site
        for site in EXPECTED_SITES
        if site_counts[site] < 12
    ]
    if sparse_sites:
        _fail(
            "Sites lack playable structure density: " +
            ", ".join(sparse_sites)
        )
    if collision_failures:
        _fail(
            "Generated meshes without collision: " +
            ", ".join(collision_failures[:5])
        )
    if spatial_failures:
        _fail(
            "Generated actors must stream spatially: " +
            ", ".join(spatial_failures[:5])
        )

    unreal.log(
        "[BH Location Validation] PASS generated=%d meshes=%d "
        "cover=%d site_counts=%s"
        % (
            len(generated),
            mesh_count,
            cover_count,
            ",".join(
                "%s:%d" % (site, site_counts[site])
                for site in EXPECTED_SITES
            ),
        )
    )
    unreal.log("[BH Location Validation] ALL CHECKS PASSED")


main()
