"""Read-only local presentation asset audit for Broken Horizon.

This script never creates, modifies, or saves assets or maps. It is safe to run
from Tools > Execute Python Script or with the Unreal Python commandlet.
"""

import unreal


SEARCH_PATHS = (
    "/Game/FirstPerson",
    "/Game/Weapons/Rifle",
    "/Game/Characters/Mannequins",
    "/Game/Variant_Shooter/Anims",
    "/Game/BrokenHorizon/Presentation",
)

BLUEPRINT_PATHS = (
    "/Game/Characters/BP_BHCharacter",
    "/Game/BP_Rifle",
    "/Game/Characters/BP_EnemySoldier",
    "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter",
    "/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter",
    "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Rifle",
)


def _log(message):
    unreal.log("[BH Presentation Audit] " + message)


def _path(value):
    if value is None:
        return "None"
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _get(obj, property_name):
    try:
        return obj.get_editor_property(property_name)
    except Exception:
        return None


def _class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        return str(asset_data.asset_class)


def _asset_skeleton(asset):
    skeleton = _get(asset, "skeleton")
    if skeleton:
        return skeleton
    return _get(asset, "target_skeleton")


def _socket_names(asset):
    sockets = _get(asset, "sockets")
    names = []
    if sockets:
        for socket in sockets:
            socket_name = _get(socket, "socket_name")
            names.append(
                str(socket_name) if socket_name else socket.get_name()
            )

    if isinstance(asset, unreal.StaticMesh):
        muzzle_socket = asset.find_socket("Muzzle")
        if muzzle_socket and "Muzzle" not in names:
            names.append("Muzzle")
    return names


def _audit_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    _log("ASSET INVENTORY BEGIN")

    for search_path in SEARCH_PATHS:
        assets = registry.get_assets_by_path(
            unreal.Name(search_path), recursive=True
        )
        for asset_data in sorted(
            assets, key=lambda item: str(item.package_name)
        ):
            class_name = _class_name(asset_data)
            if class_name not in {
                "AnimBlueprint",
                "AnimMontage",
                "AnimSequence",
                "BlendSpace",
                "BlendSpace1D",
                "Skeleton",
                "SkeletalMesh",
                "StaticMesh",
            }:
                continue

            asset = asset_data.get_asset()
            skeleton = _asset_skeleton(asset)
            socket_names = _socket_names(asset)
            _log(
                "ASSET|%s|class=%s|skeleton=%s|sockets=%s"
                % (
                    str(asset_data.package_name),
                    class_name,
                    _path(skeleton),
                    ",".join(socket_names) if socket_names else "None",
                )
            )
            if (
                str(asset_data.package_name)
                == "/Game/BrokenHorizon/Presentation/Weapons/"
                "SM_FirstLight_Rifle"
            ):
                muzzle_socket = asset.find_socket("Muzzle")
                if muzzle_socket:
                    _log(
                        "SOCKET|%s|Muzzle|location=%s|rotation=%s"
                        % (
                            str(asset_data.package_name),
                            str(
                                _get(
                                    muzzle_socket,
                                    "relative_location",
                                )
                            ),
                            str(
                                _get(
                                    muzzle_socket,
                                    "relative_rotation",
                                )
                            ),
                        )
                    )
                else:
                    _log(
                        "SOCKET|%s|Muzzle|MISSING; BP_Rifle MuzzlePoint "
                        "fallback is expected."
                        % str(asset_data.package_name)
                    )

    _log("ASSET INVENTORY END")


def _audit_blueprint(asset_path):
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not blueprint:
        _log("BLUEPRINT|%s|MISSING" % asset_path)
        return

    generated_class = blueprint.generated_class()
    cdo = unreal.get_default_object(generated_class)
    _log(
        "BLUEPRINT|%s|class=%s|parent=%s"
        % (
            asset_path,
            _path(generated_class),
            _path(_get(blueprint, "parent_class")),
        )
    )

    components = cdo.get_components_by_class(unreal.ActorComponent)
    for component in sorted(components, key=lambda item: item.get_name()):
        fields = [
            "COMPONENT",
            asset_path,
            component.get_name(),
            "class=%s" % component.get_class().get_name(),
        ]

        if isinstance(component, unreal.SkeletalMeshComponent):
            fields.extend(
                (
                    "skeletal_mesh=%s"
                    % _path(_get(component, "skeletal_mesh_asset")),
                    "anim_class=%s"
                    % _path(_get(component, "anim_class")),
                    "animation_mode=%s"
                    % str(_get(component, "animation_mode")),
                    "pause_anims=%s"
                    % str(_get(component, "pause_anims")),
                    "global_anim_rate_scale=%s"
                    % str(_get(component, "global_anim_rate_scale")),
                    "only_owner_see=%s"
                    % str(_get(component, "only_owner_see")),
                    "cast_shadow=%s"
                    % str(_get(component, "cast_shadow")),
                )
            )
        elif isinstance(component, unreal.StaticMeshComponent):
            fields.append(
                "static_mesh=%s"
                % _path(_get(component, "static_mesh"))
            )

        if isinstance(component, unreal.SceneComponent):
            fields.extend(
                (
                    "location=%s"
                    % str(_get(component, "relative_location")),
                    "rotation=%s"
                    % str(_get(component, "relative_rotation")),
                    "scale=%s"
                    % str(_get(component, "relative_scale3d")),
                )
            )

        _log("|".join(fields))

    for property_name in (
        "first_person_fire_montage",
        "first_person_reload_montage",
        "fire_montage",
        "muzzle_socket_name",
        "default_rifle_class",
        "ai_controller_class",
        "auto_possess_ai",
    ):
        value = _get(cdo, property_name)
        if value is not None:
            _log(
                "DEFAULT|%s|%s=%s"
                % (asset_path, property_name, _path(value))
            )


def inspect_local_presentation_assets():
    _audit_assets()
    for blueprint_path in BLUEPRINT_PATHS:
        _audit_blueprint(blueprint_path)
    _log("Read-only audit complete. No assets or maps were saved.")


if __name__ == "__main__":
    inspect_local_presentation_assets()
