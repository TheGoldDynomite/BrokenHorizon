"""Safe, repeatable Broken Horizon combat-support Editor setup for UE 5.8.

Run from Tools > Execute Python Script.  The default `run_setup()` call never
opens, saves, or modifies a map.  Call `place_demo_supplies()` separately only
when you deliberately want two tagged demonstration actors in the current
editor world.
"""

import unreal


CONTENT_ROOT = "/Game/BrokenHorizon"
SUPPLY_FOLDER = CONTENT_ROOT + "/Core"
UI_FOLDER = CONTENT_ROOT + "/UI"
AMMO_BLUEPRINT = SUPPLY_FOLDER + "/BP_AmmoSupply"
MEDICAL_BLUEPRINT = SUPPLY_FOLDER + "/BP_MedicalSupply"
COMBAT_WIDGET = UI_FOLDER + "/WBP_CombatStatus"
PLAYER_BLUEPRINT = CONTENT_ROOT + "/Characters/MyBHCharacter"
AUTO_TAG = "BH_AutoCombatSupportDemo"


def _log(message):
    unreal.log("[BH Combat Support Setup] " + message)


def _warning(message):
    unreal.log_warning("[BH Combat Support Setup] " + message)


def _text(value):
    """Create FText across the UE 5.8 Python bindings' supported variants."""
    try:
        return unreal.Text.from_string(value)
    except AttributeError:
        return unreal.Text(value)


def _class(path):
    result = unreal.load_class(None, path)
    if not result:
        raise RuntimeError("Could not load native class " + path)
    return result


def _asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _create_or_reuse_actor_blueprint(asset_path, parent_class):
    """Return an existing Blueprint or create a child without touching graphs."""
    existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    if existing:
        _log("Reusing " + asset_path)
        return existing, False

    asset_name = asset_path.rsplit("/", 1)[1]
    package_path = asset_path.rsplit("/", 1)[0]
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = _asset_tools().create_asset(
        asset_name, package_path, unreal.Blueprint, factory
    )
    if not blueprint:
        raise RuntimeError("Could not create " + asset_path)
    _log("Created " + asset_path)
    return blueprint, True


def _try_set_property(obj, property_name, value):
    try:
        obj.set_editor_property(property_name, value)
        return True
    except Exception as error:
        _warning("Could not set %s: %s" % (property_name, error))
        return False


def _configure_supply_defaults(
    blueprint,
    reserve_ammo=None,
    heal_amount=None,
    medkit_amount=None,
    field_dressing_amount=None,
):
    """Set only native CDO defaults; never touch instance-only Persistence ID."""
    cdo = unreal.get_default_object(blueprint.generated_class())
    changed = False
    changed |= _try_set_property(cdo, "b_consume_on_use", True)
    interaction_text = (
        "Take Ammunition"
        if reserve_ammo is not None
        else "Restock Medical Supplies"
    )
    changed |= _try_set_property(
        cdo,
        "interaction_text",
        _text(interaction_text),
    )
    if reserve_ammo is not None:
        changed |= _try_set_property(cdo, "reserve_ammo_amount", reserve_ammo)
    if heal_amount is not None:
        changed |= _try_set_property(cdo, "heal_amount", heal_amount)
    if medkit_amount is not None:
        changed |= _try_set_property(
            cdo,
            "medkit_amount",
            medkit_amount,
        )
    if field_dressing_amount is not None:
        changed |= _try_set_property(
            cdo,
            "field_dressing_amount",
            field_dressing_amount,
        )

    # SupplyMesh is a native default subobject.  This is best effort because
    # component editing APIs vary slightly between UE point releases.
    try:
        mesh_component = cdo.get_component_by_class(unreal.StaticMeshComponent)
        cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
        if mesh_component and cube:
            mesh_component.set_editor_property("static_mesh", cube)
            changed = True
    except Exception as error:
        _warning("Placeholder mesh was not assigned automatically: %s" % error)

    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)


def _ensure_widget_asset():
    """Create an empty native-parent widget only when it does not already exist."""
    existing = unreal.EditorAssetLibrary.load_asset(COMBAT_WIDGET)
    if existing:
        _log("Reusing " + COMBAT_WIDGET + "; existing layout was not changed.")
        return existing

    parent_class = _class("/Script/BrokenHorizon.BHCombatStatusWidget")
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    widget = _asset_tools().create_asset(
        "WBP_CombatStatus", UI_FOLDER, unreal.WidgetBlueprint, factory
    )
    if not widget:
        raise RuntimeError("Could not create " + COMBAT_WIDGET)
    unreal.EditorAssetLibrary.save_loaded_asset(widget)
    _log("Created " + COMBAT_WIDGET)
    return widget


def _try_assign_player_widget(widget):
    """Only assign the one known project player Blueprint; never search/guess."""
    player_blueprint = unreal.EditorAssetLibrary.load_asset(PLAYER_BLUEPRINT)
    if not player_blueprint:
        _warning("Player Blueprint was not found at %s. Set Combat Status Widget Class manually." % PLAYER_BLUEPRINT)
        return False
    cdo = unreal.get_default_object(player_blueprint.generated_class())
    if _try_set_property(cdo, "combat_status_widget_class", widget.generated_class()):
        unreal.EditorAssetLibrary.save_loaded_asset(player_blueprint)
        _log("Assigned WBP_CombatStatus to MyBHCharacter.Combat Status Widget Class.")
        return True
    _warning("Set Combat Status Widget Class on MyBHCharacter manually.")
    return False


def _widget_manual_steps():
    _warning("Combat-status widget layout is intentionally left untouched. In WBP_CombatStatus, add a Progress Bar named HealthBar and a Progress Bar named StaminaBar. Optional Text Blocks are HealthText and StaminaText. Compile and save the widget.")


def run_setup():
    """Create/reuse combat-support assets and safely configure class defaults."""
    _log("Starting safe combat-support setup. Maps and placed actors will not be modified.")
    ammo_parent = _class("/Script/BrokenHorizon.BHAmmoSupply")
    medical_parent = _class("/Script/BrokenHorizon.BHMedicalSupply")
    ammo_blueprint, _ = _create_or_reuse_actor_blueprint(AMMO_BLUEPRINT, ammo_parent)
    medical_blueprint, _ = _create_or_reuse_actor_blueprint(MEDICAL_BLUEPRINT, medical_parent)
    _configure_supply_defaults(ammo_blueprint, reserve_ammo=30)
    _configure_supply_defaults(
        medical_blueprint,
        heal_amount=0.0,
        medkit_amount=1,
        field_dressing_amount=2,
    )
    widget = _ensure_widget_asset()
    _try_assign_player_widget(widget)
    _widget_manual_steps()
    _log("Complete. Supply Persistent IDs are intentionally instance-only; assign a unique ID after placing each supply.")
    _notify("Combat-support setup complete. Check Output Log for the small widget-layout remainder.")


def _notify(message):
    try:
        unreal.EditorDialog.show_message(
            "Broken Horizon", message, unreal.AppMsgType.OK
        )
    except Exception:
        _log(message)


def place_demo_supplies():
    """Explicit opt-in: place one tagged ammo and medical supply near PlayerStart."""
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        _warning("No editor world is open; no supplies were placed.")
        return
    existing = unreal.GameplayStatics.get_all_actors_with_tag(world, AUTO_TAG)
    if existing:
        _warning("Auto-generated demo supplies already exist in this map; no duplicates were placed.")
        return
    player_starts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart)
    origin = player_starts[0].get_actor_location() if player_starts else unreal.Vector(0, 0, 100)
    ammo_class = unreal.EditorAssetLibrary.load_blueprint_class(AMMO_BLUEPRINT)
    medical_class = unreal.EditorAssetLibrary.load_blueprint_class(MEDICAL_BLUEPRINT)
    if not ammo_class or not medical_class:
        _warning("Run run_setup() successfully before placing demo supplies.")
        return
    for label, actor_class, offset in (
        ("Ammo", ammo_class, unreal.Vector(250, 100, 0)),
        ("Medical", medical_class, unreal.Vector(250, -100, 0)),
    ):
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, origin + offset)
        actor.tags = list(actor.tags) + [AUTO_TAG]
        actor.set_editor_property("persistence_id", unreal.Name("Auto_%s_%s" % (label, unreal.Guid.new_guid().to_string())))
        actor.set_actor_label("BH Auto %s Supply" % label)
    _log("Placed one tagged ammo and one tagged medical supply. Save the level when you are happy with their positions.")
    _notify("Combat-support demo supplies placed. They are tagged " + AUTO_TAG)


# Tools > Execute Python Script runs the file, so perform only the safe setup.
run_setup()
