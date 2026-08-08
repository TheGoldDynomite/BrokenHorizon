"""Create and place Broken Horizon v0.20 field-resupply assets.

The script is repeatable. It reuses existing assets, configures only their
native supply defaults, and places only missing persistence IDs in First Light.
"""

import unreal


CONTENT_ROOT = "/Game/BrokenHorizon"
SUPPLY_FOLDER = CONTENT_ROOT + "/Core"
MAP_PATH = CONTENT_ROOT + "/Maps/L_FirstLight_Graybox"
MEDICAL_BLUEPRINT = SUPPLY_FOLDER + "/BP_MedicalSupply"
ARMOR_PLATE_BLUEPRINT = SUPPLY_FOLDER + "/BP_ArmorPlateSupply"
HELMET_BLUEPRINT = SUPPLY_FOLDER + "/BP_HelmetSupply"
AUTO_TAG = "BH_Auto_FieldResupply"
SUPPLY_FOLDER_PATH = "FirstLight/Gameplay/Field Resupply"


def _log(message):
    unreal.log("[BH Field Resupply Setup] " + message)


def _text(value):
    try:
        return unreal.Text.from_string(value)
    except AttributeError:
        return unreal.Text(value)


def _class(path):
    result = unreal.load_class(None, path)
    if not result:
        raise RuntimeError("Could not load native class " + path)
    return result


def _set(target, property_name, value):
    try:
        target.set_editor_property(property_name, value)
    except Exception as error:
        raise RuntimeError(
            "%s: could not set %s (%s)"
            % (target.get_name(), property_name, error)
        )


def _create_or_reuse_blueprint(asset_path, parent_class):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
        _log("Reusing " + asset_path)
        return blueprint

    asset_name = asset_path.rsplit("/", 1)[1]
    package_path = asset_path.rsplit("/", 1)[0]
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        package_path,
        unreal.Blueprint,
        factory,
    )
    if not blueprint:
        raise RuntimeError("Could not create " + asset_path)

    _log("Created " + asset_path)
    return blueprint


def _configure_placeholder_mesh(cdo):
    component = cdo.get_component_by_class(unreal.StaticMeshComponent)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")

    if component and cube:
        component.set_editor_property("static_mesh", cube)


def _configure_medical_blueprint():
    blueprint = unreal.EditorAssetLibrary.load_asset(MEDICAL_BLUEPRINT)
    if not blueprint:
        raise RuntimeError(
            "Missing medical supply Blueprint: " + MEDICAL_BLUEPRINT
        )

    cdo = unreal.get_default_object(blueprint.generated_class())
    _set(
        cdo,
        "interaction_text",
        _text("Restock Medical Supplies"),
    )
    _set(cdo, "heal_amount", 0.0)
    _set(cdo, "medkit_amount", 1)
    _set(cdo, "field_dressing_amount", 2)
    _configure_placeholder_mesh(cdo)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    _log("Configured BP_MedicalSupply for +1 medkit and +2 dressings.")


def _configure_armor_blueprint(
    asset_path,
    interaction_text,
    helmet_amount,
    body_armor_amount,
):
    parent_class = _class("/Script/BrokenHorizon.BHArmorSupply")
    blueprint = _create_or_reuse_blueprint(asset_path, parent_class)
    cdo = unreal.get_default_object(blueprint.generated_class())
    _set(cdo, "interaction_text", _text(interaction_text))
    _set(cdo, "helmet_durability_amount", helmet_amount)
    _set(
        cdo,
        "body_armor_durability_amount",
        body_armor_amount,
    )
    _configure_placeholder_mesh(cdo)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    _log("Configured " + asset_path)


def _supply_by_persistence_id(world, persistence_id):
    supply_class = _class("/Script/BrokenHorizon.BHSupplyBase")
    actors = unreal.GameplayStatics.get_all_actors_of_class(
        world,
        supply_class,
    )

    for actor in actors:
        if str(actor.get_editor_property("persistence_id")) == persistence_id:
            return actor

    return None


def _place_missing_supply(
    world,
    blueprint_path,
    persistence_id,
    label,
    location,
):
    existing = _supply_by_persistence_id(world, persistence_id)

    if existing:
        _log("Reusing placed supply " + persistence_id)
        return existing

    actor_class = unreal.EditorAssetLibrary.load_blueprint_class(
        blueprint_path
    )
    if not actor_class:
        raise RuntimeError(
            "Could not load Blueprint class " + blueprint_path
        )

    subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    actor = subsystem.spawn_actor_from_class(
        actor_class,
        location,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not actor:
        raise RuntimeError("Could not spawn " + label)

    actor.set_actor_label(label)
    actor.set_folder_path(unreal.Name(SUPPLY_FOLDER_PATH))
    actor.tags = list(actor.tags) + [AUTO_TAG]
    _set(actor, "persistence_id", unreal.Name(persistence_id))
    actor.set_actor_scale3d(unreal.Vector(0.55, 0.55, 0.35))
    _log("Placed " + persistence_id)
    return actor


def run_setup():
    _configure_medical_blueprint()
    _configure_armor_blueprint(
        ARMOR_PLATE_BLUEPRINT,
        "Install Armor Plate",
        0.0,
        50.0,
    )
    _configure_armor_blueprint(
        HELMET_BLUEPRINT,
        "Replace Helmet",
        45.0,
        0.0,
    )

    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing First Light map: " + MAP_PATH)

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()

    if not world:
        raise RuntimeError("First Light editor world is unavailable.")

    _place_missing_supply(
        world,
        ARMOR_PLATE_BLUEPRINT,
        "FirstLightArmorPlateSupply",
        "FL_ArmorPlateSupply",
        unreal.Vector(6000.0, -650.0, 80.0),
    )
    _place_missing_supply(
        world,
        HELMET_BLUEPRINT,
        "FirstLightHelmetSupply",
        "FL_HelmetSupply",
        unreal.Vector(6900.0, 650.0, 80.0),
    )

    unreal.EditorLevelLibrary.save_current_level()
    _log("ALL SETUP COMPLETE")


if __name__ == "__main__":
    run_setup()
