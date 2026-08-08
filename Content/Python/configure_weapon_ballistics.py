"""Tune Broken Horizon's player rifle for predictable milsim gunplay."""

import unreal


RIFLE_BLUEPRINT = "/Game/BP_Rifle"
PLAYER_BLUEPRINT = "/Game/Characters/BP_BHCharacter"


def load_blueprint(path):
    blueprint = unreal.EditorAssetLibrary.load_asset(path)
    if not blueprint:
        raise RuntimeError("Missing Blueprint: " + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return blueprint


def find_component(owner, name, component_type):
    for component in owner.get_components_by_class(component_type):
        if component.get_name() == name:
            return component
    raise RuntimeError("Missing component: " + name)


rifle_blueprint = load_blueprint(RIFLE_BLUEPRINT)
rifle = unreal.get_default_object(rifle_blueprint.generated_class())
config = rifle.get_editor_property("rifle_config")

for name, value in (
    ("range", 50000.0),
    ("hip_spread_degrees", 0.9),
    ("ads_spread_degrees", 0.08),
    ("recoil_pitch", 0.55),
    ("recoil_yaw", 0.12),
    ("ads_recoil_multiplier", 0.72),
    ("recoil_pitch_variation", 0.1),
    ("muzzle_obstruction_check_distance", 150.0),
):
    config.set_editor_property(name, value)

rifle.set_editor_property("rifle_config", config)
find_component(
    rifle,
    "MuzzlePoint",
    unreal.SceneComponent,
).set_editor_property(
    "relative_location",
    unreal.Vector(100.0, 0.0, 0.0),
)

player_blueprint = load_blueprint(PLAYER_BLUEPRINT)
player = unreal.get_default_object(player_blueprint.generated_class())
player.set_editor_property(
    "first_person_fire_kick_location",
    unreal.Vector(-1.25, 0.0, -0.25),
)
player.set_editor_property(
    "first_person_fire_kick_rotation",
    unreal.Rotator(pitch=1.25, yaw=0.0, roll=0.35),
)
player.set_editor_property("first_person_fire_kick_recovery_speed", 22.0)

unreal.BlueprintEditorLibrary.compile_blueprint(rifle_blueprint)
unreal.BlueprintEditorLibrary.compile_blueprint(player_blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(rifle_blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(player_blueprint)
unreal.log(
    "[BH Weapon Ballistics] Configured 500 m range, tighter spread, "
    "controlled recoil, and an aligned muzzle fallback."
)
