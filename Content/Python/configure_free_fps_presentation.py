"""Configure Broken Horizon to use the audited Free FPS Template assets.

This script intentionally changes only BP_BHCharacter and BP_Rifle. The
donor's gameplay Blueprints, input, game mode, and controllers are not used.
"""

import unreal


CHARACTER_BLUEPRINT = "/Game/Characters/BP_BHCharacter"
RIFLE_BLUEPRINT = "/Game/BP_Rifle"

ARMS_MESH = (
    "/Game/InfimaGames/FreeFPSTemplate/Demo/Mannequins/Meshes/"
    "SK_FP_Manny_Simple"
)
RIFLE_MESH = (
    "/Game/InfimaGames/FreeFPSTemplate/Art/AssaultRifle/Meshes/"
    "SK_AssaultRifle"
)
FIRE_SOUND = "/Game/BrokenHorizon/Audio/SW_FirstLight_WeaponFire"
CASING_MESH = (
    "/Game/InfimaGames/FreeFPSTemplate/Art/AssaultRifle/Meshes/"
    "SM_AssaultRifle_Casing"
)

ANIMATION_ROOT = (
    "/Game/InfimaGames/FreeFPSTemplate/Art/AssaultRifle/Animations/"
)

CHARACTER_ANIMATIONS = {
    "first_person_idle_animation": "A_FP_AssaultRifle_Idle_Loop",
    "first_person_walk_animation": "A_FP_AssaultRifle_Walk_F_Loop",
    "first_person_run_animation": "A_FP_AssaultRifle_Run_Loop",
    "first_person_aim_idle_animation":
        "A_FP_AssaultRifle_Idle_Loop_Aimed",
    "first_person_aim_walk_animation":
        "A_FP_AssaultRifle_Walk_F_Loop_Aimed",
}

ARMS_LOCATION = unreal.Vector(
    -0.708016,
    0.000002,
    -162.574875,
)
ARMS_ROTATION = unreal.Rotator(
    pitch=0.0,
    yaw=-90.000033,
    roll=0.015826,
)
UNIT_SCALE = unreal.Vector(1.0, 1.0, 1.0)


def _log(message):
    unreal.log("[BH Free FPS Integration] " + message)


def _load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Required asset is missing: " + path)
    return asset


def _component(cdo, component_name, expected_type):
    for component in cdo.get_components_by_class(
        unreal.ActorComponent
    ):
        if component.get_name() == component_name:
            if not isinstance(component, expected_type):
                raise RuntimeError(
                    "%s is %s, expected %s"
                    % (
                        component_name,
                        component.get_class().get_name(),
                        expected_type.__name__,
                    )
                )
            return component
    raise RuntimeError("Component was not found: " + component_name)


def _compile_and_save(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        raise RuntimeError(
            "Failed to save " + blueprint.get_path_name()
        )


def _configure_character():
    blueprint = _load(CHARACTER_BLUEPRINT)
    cdo = unreal.get_default_object(blueprint.generated_class())
    arms = _component(
        cdo,
        "FirstPersonArms",
        unreal.SkeletalMeshComponent,
    )
    arms_mesh = _load(ARMS_MESH)

    arms.set_editor_property("skeletal_mesh_asset", arms_mesh)
    arms.set_editor_property("anim_class", None)
    arms.set_editor_property(
        "animation_mode",
        unreal.AnimationMode.ANIMATION_SINGLE_NODE,
    )
    arms.set_editor_property("relative_location", ARMS_LOCATION)
    arms.set_editor_property("relative_rotation", ARMS_ROTATION)
    arms.set_editor_property("relative_scale3d", UNIT_SCALE)
    arms.set_editor_property("visible", True)
    arms.set_editor_property("hidden_in_game", False)
    arms.set_editor_property("only_owner_see", True)
    arms.set_editor_property("cast_shadow", False)

    for property_name, asset_name in CHARACTER_ANIMATIONS.items():
        cdo.set_editor_property(
            property_name,
            _load(ANIMATION_ROOT + asset_name),
        )

    _compile_and_save(blueprint)

    if not arms.does_socket_exist(unreal.Name("ik_hand_gun")):
        raise RuntimeError(
            "First-person arms do not expose ik_hand_gun."
        )

    _log(
        "Configured BP_BHCharacter with FP Manny arms and five "
        "locomotion/aim sequences."
    )


def _configure_rifle():
    blueprint = _load(RIFLE_BLUEPRINT)
    cdo = unreal.get_default_object(blueprint.generated_class())
    static_mesh = _component(
        cdo,
        "RifleMesh",
        unreal.StaticMeshComponent,
    )
    skeletal_mesh = _component(
        cdo,
        "RifleSkeletalMesh",
        unreal.SkeletalMeshComponent,
    )

    static_mesh.set_editor_property("static_mesh", None)
    static_mesh.set_editor_property("visible", False)
    static_mesh.set_editor_property("hidden_in_game", True)

    skeletal_mesh.set_editor_property(
        "skeletal_mesh_asset",
        _load(RIFLE_MESH),
    )
    skeletal_mesh.set_editor_property("anim_class", None)
    skeletal_mesh.set_editor_property(
        "animation_mode",
        unreal.AnimationMode.ANIMATION_SINGLE_NODE,
    )
    skeletal_mesh.set_editor_property(
        "relative_location",
        unreal.Vector(0.0, 0.0, 0.0),
    )
    skeletal_mesh.set_editor_property(
        "relative_rotation",
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0),
    )
    skeletal_mesh.set_editor_property(
        "relative_scale3d",
        UNIT_SCALE,
    )
    skeletal_mesh.set_editor_property("visible", True)
    skeletal_mesh.set_editor_property("hidden_in_game", False)
    skeletal_mesh.set_editor_property("only_owner_see", True)
    skeletal_mesh.set_editor_property("cast_shadow", False)

    cdo.set_editor_property("first_person_fire_montage", None)
    cdo.set_editor_property("first_person_reload_montage", None)
    # The donor fire clip is additive. AnimSingleNodeInstance cannot play
    # additive assets safely, so firing uses BH's camera recoil instead.
    cdo.set_editor_property("first_person_fire_animation", None)
    # The donor reload clip is additive too. Playing it on the arms'
    # AnimSingleNodeInstance makes the socket-attached rifle disappear.
    # Reload timing and ammunition remain controlled by the C++ weapon code.
    cdo.set_editor_property("first_person_reload_animation", None)
    # The donor's weapon-only sequences expect its Animation Blueprint.
    # Playing them directly on our socket-attached rifle can leave the
    # weapon outside the camera view. The arms sequences already drive
    # the visible recoil/reload motion through ik_hand_gun.
    cdo.set_editor_property("weapon_fire_animation", None)
    cdo.set_editor_property("weapon_reload_animation", None)
    cdo.set_editor_property("fire_sound", _load(FIRE_SOUND))
    cdo.set_editor_property("casing_mesh", _load(CASING_MESH))

    _compile_and_save(blueprint)
    _log(
        "Configured BP_Rifle with the donor skeletal rifle and safe "
        "single-node-compatible presentation."
    )


def main():
    _configure_character()
    _configure_rifle()
    unreal.EditorAssetLibrary.save_directory(
        "/Game/Characters",
        only_if_is_dirty=True,
        recursive=False,
    )
    unreal.EditorAssetLibrary.save_directory(
        "/Game",
        only_if_is_dirty=True,
        recursive=False,
    )
    _log("Configuration complete.")


main()
