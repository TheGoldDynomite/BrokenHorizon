"""Idempotent local-asset presentation setup for Broken Horizon.

The default is a dry run. Before applying, save all open assets/maps, set
APPLY_CHANGES to True, then run from Tools > Execute Python Script.

The script:
- creates project-owned presentation copies only when they do not exist;
- never deletes or overwrites assets;
- replaces only known placeholder/default assignments unless the explicit
  REPLACE_CUSTOM_ASSIGNMENTS flag is enabled;
- does not modify a map;
- leaves FirstPersonArms empty because no dedicated compatible arms mesh is
  present locally for Broken Horizon's camera-attached arms architecture.
"""

import unreal


APPLY_CHANGES = False
REPLACE_CUSTOM_ASSIGNMENTS = False
REPAIR_GENERATED_ALIGNMENT = False

RIFLE_BLUEPRINT = "/Game/BP_Rifle"
ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"

SOURCE_RIFLE_MESH = "/Game/Weapons/Rifle/Meshes/SM_Rifle"
PRESENTATION_RIFLE_MESH = (
    "/Game/BrokenHorizon/Presentation/Weapons/SM_FirstLight_Rifle"
)

SOURCE_ENEMY_MESH = (
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
)
PRESENTATION_ENEMY_MESH = (
    "/Game/BrokenHorizon/Presentation/Characters/"
    "SKM_FirstLight_EnemyPlaceholder"
)

SOURCE_ENEMY_ANIM_BP = "/Game/Variant_Shooter/Anims/ABP_TP_Rifle"
PRESENTATION_ENEMY_ANIM_BP = (
    "/Game/BrokenHorizon/Presentation/Characters/"
    "ABP_FirstLight_EnemyRifle"
)

KNOWN_RIFLE_PLACEHOLDERS = {
    "None",
    "/Game/Variant_Shooter/Blueprints/Pickups/Projectiles/"
    "Meshes/SM_FoamBullet.SM_FoamBullet",
    SOURCE_RIFLE_MESH + ".SM_Rifle",
    PRESENTATION_RIFLE_MESH + ".SM_FirstLight_Rifle",
}

KNOWN_ENEMY_MESHES = {
    "None",
    SOURCE_ENEMY_MESH + ".SKM_Quinn_Simple",
    PRESENTATION_ENEMY_MESH
    + ".SKM_FirstLight_EnemyPlaceholder",
}

KNOWN_ENEMY_ANIM_CLASSES = {
    "None",
    "/Game/Characters/Mannequins/Anims/Unarmed/"
    "ABP_Unarmed.ABP_Unarmed_C",
    "/Game/BrokenHorizon/Presentation/Characters/"
    "ABP_FirstLight_EnemyPlaceholder."
    "ABP_FirstLight_EnemyPlaceholder_C",
    SOURCE_ENEMY_ANIM_BP + ".ABP_TP_Rifle_C",
    PRESENTATION_ENEMY_ANIM_BP
    + ".ABP_FirstLight_EnemyRifle_C",
}

RIFLE_RELATIVE_LOCATION = unreal.Vector(50.0, 15.0, -15.0)
RIFLE_RELATIVE_ROTATION = unreal.Rotator(
    pitch=0.0, yaw=-90.0, roll=0.0
)
RIFLE_RELATIVE_SCALE = unreal.Vector(1.0, 1.0, 1.0)


def _log(message):
    unreal.log("[BH Presentation Setup] " + message)


def _warning(message):
    unreal.log_warning("[BH Presentation Setup] " + message)


def _path(value):
    if value is None:
        return "None"
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Required local asset is missing: " + path)
    return asset


def _component(cdo, component_name, expected_type):
    for component in cdo.get_components_by_class(unreal.ActorComponent):
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


def _duplicate_once(source_path, destination_path):
    if unreal.EditorAssetLibrary.does_asset_exist(destination_path):
        existing = unreal.EditorAssetLibrary.load_asset(destination_path)
        _log("Reusing existing presentation asset " + destination_path)
        return existing, False

    source = _load(source_path)
    if not APPLY_CHANGES:
        _log(
            "DRY RUN: would duplicate %s -> %s"
            % (source_path, destination_path)
        )
        return source, False

    destination_name = destination_path.rsplit("/", 1)[1]
    destination_folder = destination_path.rsplit("/", 1)[0]
    duplicate = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
        destination_name,
        destination_folder,
        source,
    )
    if not duplicate:
        raise RuntimeError(
            "Could not duplicate %s to %s"
            % (source_path, destination_path)
        )

    try:
        unreal.EditorAssetLibrary.set_metadata_tag(
            duplicate,
            "BrokenHorizonGeneratedBy",
            "integrate_first_light_presentation.py",
        )
    except Exception as error:
        _warning("Could not add generator metadata: %s" % error)

    unreal.EditorAssetLibrary.save_loaded_asset(duplicate)
    _log("Created " + destination_path)
    return duplicate, True


def _can_replace(current_path, known_paths, label):
    if current_path in known_paths or REPLACE_CUSTOM_ASSIGNMENTS:
        return True
    _warning(
        "%s has a custom assignment (%s); it was not replaced. "
        "Set REPLACE_CUSTOM_ASSIGNMENTS = True only after reviewing it."
        % (label, current_path)
    )
    return False


def _save_blueprint(blueprint):
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    except Exception as error:
        _warning("Blueprint compile API was unavailable: %s" % error)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)


def _configure_rifle_blueprint(rifle_mesh):
    blueprint = _load(RIFLE_BLUEPRINT)
    cdo = unreal.get_default_object(blueprint.generated_class())
    static_component = _component(
        cdo, "RifleMesh", unreal.StaticMeshComponent
    )
    skeletal_component = _component(
        cdo, "RifleSkeletalMesh", unreal.SkeletalMeshComponent
    )
    current_path = _path(
        static_component.get_editor_property("static_mesh")
    )

    if not _can_replace(
        current_path, KNOWN_RIFLE_PLACEHOLDERS, "BP_Rifle.RifleMesh"
    ):
        return False

    if not APPLY_CHANGES:
        _log(
            "DRY RUN: would assign %s to BP_Rifle.RifleMesh."
            % _path(rifle_mesh)
        )
        return True

    static_component.set_editor_property("static_mesh", rifle_mesh)
    static_component.set_editor_property(
        "relative_location", RIFLE_RELATIVE_LOCATION
    )
    static_component.set_editor_property(
        "relative_rotation", RIFLE_RELATIVE_ROTATION
    )
    static_component.set_editor_property(
        "relative_scale3d", RIFLE_RELATIVE_SCALE
    )
    static_component.set_editor_property("cast_shadow", False)

    # Keep only one rifle presentation mesh active.
    skeletal_component.set_editor_property(
        "skeletal_mesh_asset", None
    )

    _save_blueprint(blueprint)
    _log(
        "Assigned the project-owned rifle mesh and preserved the proven "
        "camera-relative placement."
    )
    return True


def _configure_enemy_blueprint(enemy_mesh, enemy_anim_blueprint):
    blueprint = _load(ENEMY_BLUEPRINT)
    cdo = unreal.get_default_object(blueprint.generated_class())
    mesh_component = _component(
        cdo, "CharacterMesh0", unreal.SkeletalMeshComponent
    )
    current_mesh_path = _path(
        mesh_component.get_editor_property("skeletal_mesh_asset")
    )
    current_anim_path = _path(
        mesh_component.get_editor_property("anim_class")
    )

    can_replace_mesh = _can_replace(
        current_mesh_path,
        KNOWN_ENEMY_MESHES,
        "BP_EnemySoldier.CharacterMesh0",
    )
    can_replace_anim = _can_replace(
        current_anim_path,
        KNOWN_ENEMY_ANIM_CLASSES,
        "BP_EnemySoldier.CharacterMesh0.AnimClass",
    )
    if not can_replace_mesh or not can_replace_anim:
        return False

    mesh_skeleton = enemy_mesh.get_editor_property("skeleton")
    anim_skeleton = enemy_anim_blueprint.get_editor_property(
        "target_skeleton"
    )
    if mesh_skeleton != anim_skeleton:
        raise RuntimeError(
            "Enemy mesh and AnimBP skeletons do not match: %s vs %s"
            % (_path(mesh_skeleton), _path(anim_skeleton))
        )

    if not APPLY_CHANGES:
        _log(
            "DRY RUN: would assign enemy mesh %s and AnimBP %s."
            % (_path(enemy_mesh), _path(enemy_anim_blueprint))
        )
        return True

    mesh_component.set_editor_property(
        "skeletal_mesh_asset", enemy_mesh
    )
    mesh_component.set_editor_property(
        "anim_class", enemy_anim_blueprint.generated_class()
    )
    mesh_component.set_editor_property(
        "animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT
    )
    mesh_component.set_editor_property("pause_anims", False)
    mesh_component.set_editor_property("global_anim_rate_scale", 1.0)

    # Preserve the verified mannequin capsule fit/facing exactly.
    mesh_component.set_editor_property(
        "relative_location", unreal.Vector(-10.0, 0.0, -80.0)
    )
    mesh_component.set_editor_property(
        "relative_rotation",
        unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0),
    )
    mesh_component.set_editor_property(
        "relative_scale3d", unreal.Vector(1.0, 1.0, 1.0)
    )

    _save_blueprint(blueprint)
    _log(
        "Assigned compatible project-owned Quinn placeholder and "
        "rifle locomotion AnimBP without changing collision or AI defaults."
    )
    return True


def run_integration():
    mode = "APPLY" if APPLY_CHANGES else "DRY RUN"
    _log("Starting local presentation integration in %s mode." % mode)

    rifle_mesh, _ = _duplicate_once(
        SOURCE_RIFLE_MESH, PRESENTATION_RIFLE_MESH
    )
    enemy_mesh, _ = _duplicate_once(
        SOURCE_ENEMY_MESH, PRESENTATION_ENEMY_MESH
    )
    enemy_anim_bp, _ = _duplicate_once(
        SOURCE_ENEMY_ANIM_BP, PRESENTATION_ENEMY_ANIM_BP
    )

    _configure_rifle_blueprint(rifle_mesh)
    _configure_enemy_blueprint(enemy_mesh, enemy_anim_bp)

    _warning(
        "FirstPersonArms remains unassigned: the local project and UE 5.8 "
        "installation contain full Manny/Quinn bodies, but no dedicated "
        "arms mesh compatible with BHCharacter's camera-attached hierarchy."
    )

    if APPLY_CHANGES:
        _log(
            "Integration complete. No maps, gameplay values, traces, AI, "
            "or Variant_Shooter source were modified."
        )
    else:
        _log(
            "Dry run complete. Save/close open assets, set APPLY_CHANGES "
            "to True, and run again to apply."
        )


if __name__ == "__main__":
    run_integration()
