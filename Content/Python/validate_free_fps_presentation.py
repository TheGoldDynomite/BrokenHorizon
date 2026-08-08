"""Validate the Free FPS art-only integration in Broken Horizon."""

import unreal


CHARACTER_BLUEPRINT = "/Game/Characters/BP_BHCharacter"
RIFLE_BLUEPRINT = "/Game/BP_Rifle"

ARMS_MESH = (
    "/Game/InfimaGames/FreeFPSTemplate/Demo/Mannequins/Meshes/"
    "SK_FP_Manny_Simple.SK_FP_Manny_Simple"
)
RIFLE_MESH = (
    "/Game/InfimaGames/FreeFPSTemplate/Art/AssaultRifle/Meshes/"
    "SK_AssaultRifle.SK_AssaultRifle"
)
FIRE_SOUND = (
    "/Game/Weapons/GrenadeLauncher/Audio/"
    "FirstPersonTemplateWeaponFire02."
    "FirstPersonTemplateWeaponFire02"
)
CASING_MESH = (
    "/Game/InfimaGames/FreeFPSTemplate/Art/AssaultRifle/Meshes/"
    "SM_AssaultRifle_Casing.SM_AssaultRifle_Casing"
)
ANIMATION_ROOT = (
    "/Game/InfimaGames/FreeFPSTemplate/Art/AssaultRifle/Animations/"
)

EXPECTED_CHARACTER_ANIMATIONS = {
    "first_person_idle_animation": "A_FP_AssaultRifle_Idle_Loop",
    "first_person_walk_animation": "A_FP_AssaultRifle_Walk_F_Loop",
    "first_person_run_animation": "A_FP_AssaultRifle_Run_Loop",
    "first_person_aim_idle_animation":
        "A_FP_AssaultRifle_Idle_Loop_Aimed",
    "first_person_aim_walk_animation":
        "A_FP_AssaultRifle_Walk_F_Loop_Aimed",
}

def _log(message):
    unreal.log("[BH Free FPS Validation] " + message)


def _path(value):
    if value is None:
        return "None"
    return value.get_path_name()


def _load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset


def _component(cdo, name, expected_type):
    for component in cdo.get_components_by_class(
        unreal.ActorComponent
    ):
        if component.get_name() == name:
            if not isinstance(component, expected_type):
                raise RuntimeError(
                    "%s has unexpected type %s"
                    % (name, component.get_class().get_name())
                )
            return component
    raise RuntimeError("Missing component: " + name)


def _expected_animation_path(name):
    return ANIMATION_ROOT + name + "." + name


def _require_equal(actual, expected, label):
    if actual != expected:
        raise RuntimeError(
            "%s mismatch: expected %s, got %s"
            % (label, expected, actual)
        )


def _validate_character():
    blueprint = _load(CHARACTER_BLUEPRINT)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    arms = _component(
        cdo,
        "FirstPersonArms",
        unreal.SkeletalMeshComponent,
    )

    _require_equal(
        _path(arms.get_editor_property("skeletal_mesh_asset")),
        ARMS_MESH,
        "FirstPersonArms mesh",
    )

    if not arms.does_socket_exist(unreal.Name("ik_hand_gun")):
        raise RuntimeError("FirstPersonArms is missing ik_hand_gun.")

    for property_name, asset_name in (
        EXPECTED_CHARACTER_ANIMATIONS.items()
    ):
        _require_equal(
            _path(cdo.get_editor_property(property_name)),
            _expected_animation_path(asset_name),
            "BP_BHCharacter." + property_name,
        )

    arms_skeleton = arms.get_editor_property(
        "skeletal_mesh_asset"
    ).get_editor_property("skeleton")
    idle_skeleton = cdo.get_editor_property(
        "first_person_idle_animation"
    ).get_editor_property("skeleton")
    _require_equal(
        _path(idle_skeleton),
        _path(arms_skeleton),
        "First-person animation skeleton",
    )
    _log("PASS character arms, socket, animations, and skeleton.")


def _validate_rifle():
    blueprint = _load(RIFLE_BLUEPRINT)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
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

    _require_equal(
        _path(static_mesh.get_editor_property("static_mesh")),
        "None",
        "Legacy static rifle mesh",
    )
    _require_equal(
        _path(skeletal_mesh.get_editor_property(
            "skeletal_mesh_asset"
        )),
        RIFLE_MESH,
        "RifleSkeletalMesh",
    )
    _require_equal(
        _path(cdo.get_editor_property(
            "first_person_fire_montage"
        )),
        "None",
        "First-person fire montage",
    )
    _require_equal(
        _path(cdo.get_editor_property(
            "first_person_reload_montage"
        )),
        "None",
        "First-person reload montage",
    )

    _require_equal(
        _path(cdo.get_editor_property("first_person_fire_animation")),
        "None",
        "Incompatible additive first-person fire sequence",
    )
    _require_equal(
        _path(cdo.get_editor_property("first_person_reload_animation")),
        "None",
        "Incompatible additive first-person reload sequence",
    )
    _require_equal(
        _path(cdo.get_editor_property("weapon_fire_animation")),
        "None",
        "Unsafe direct weapon fire sequence",
    )
    _require_equal(
        _path(cdo.get_editor_property("weapon_reload_animation")),
        "None",
        "Unsafe direct weapon reload sequence",
    )
    _require_equal(
        _path(cdo.get_editor_property("fire_sound")),
        FIRE_SOUND,
        "Local placeholder fire sound",
    )
    _require_equal(
        _path(cdo.get_editor_property("casing_mesh")),
        CASING_MESH,
        "Rifle casing mesh",
    )
    _log("PASS rifle mesh and single-node-safe animation fallback.")


def main():
    _validate_character()
    _validate_rifle()
    _log("ALL CHECKS PASSED")


main()
