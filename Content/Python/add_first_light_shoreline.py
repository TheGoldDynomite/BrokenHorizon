"""Idempotently author simple shoreline access banks for the water route."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_FirstLight_Shoreline"
MESH = "/Engine/BasicShapes/Cube.Cube"
MATERIAL = "/Game/BrokenHorizon/Environment/Materials/M_BH_Terrain_Prototype"

SHORES = (
    ("FirstLightWaterShoreWest", unreal.Vector(4300.0, 300.0, -41.0), unreal.Vector(7.0, 10.0, 0.8)),
    ("FirstLightWaterShoreEast", unreal.Vector(12700.0, 300.0, 80.0), unreal.Vector(7.0, 10.0, 0.8)),
)


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    mesh = unreal.load_asset(MESH)
    material = unreal.load_asset(MATERIAL)
    actor_class = unreal.load_class(None, "/Script/Engine.StaticMeshActor")
    if not mesh or not actor_class:
        unreal.log_error("[First Light Shoreline] required asset/class unavailable")
        return False

    actors = {actor.get_actor_label(): actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()}
    for label, location, scale in SHORES:
        actor = actors.get(label)
        if not actor:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                actor_class, location, unreal.Rotator(0.0, 0.0, 0.0))
            if not actor:
                unreal.log_error("[First Light Shoreline] spawn failed for %s" % label)
                return False
            actor.set_actor_label(label)
            actor.tags = list(actor.tags) + [unreal.Name(TAG)]
        actor.set_actor_location(location, False, True)
        actor.set_actor_scale3d(scale)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            component.set_static_mesh(mesh)
            if material:
                component.set_material(0, material)

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[First Light Shoreline] PASS banks=%d" % len(SHORES))
    return True


main()
