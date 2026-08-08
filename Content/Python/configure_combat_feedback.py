"""Configure the v0.12.0 combat-feedback presentation assets."""

import unreal


ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"

HIT_REACTIONS = (
    "/Game/Characters/Mannequins/Anims/Rifle/HitReact/"
    "MM_HitReact_Front_Lgt_01",
    "/Game/Characters/Mannequins/Anims/Rifle/HitReact/"
    "MM_HitReact_Front_Lgt_02",
    "/Game/Characters/Mannequins/Anims/Rifle/HitReact/"
    "MM_HitReact_Front_Med_01",
    "/Game/Characters/Mannequins/Anims/Rifle/HitReact/"
    "MM_HitReact_Back_Med_01",
)

DEATH_ANIMATION = (
    "/Game/Characters/Mannequins/Anims/Death/"
    "MM_Death_Back_01"
)


def _log(message):
    unreal.log("[BH Combat Feedback] " + message)


def _load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Required combat-feedback asset is missing: " + path)
    return asset


def _asset_path(asset):
    return asset.get_path_name() if asset else "None"


def _remove_legacy_damage_causer_hide(blueprint):
    """Remove the obsolete death node that hides the player's weapon."""
    graph = unreal.BlueprintEditorLibrary.find_event_graph(blueprint)
    if not graph:
        raise RuntimeError("BP_EnemySoldier has no event graph.")

    graph_editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
    nodes_to_remove = []

    for node in graph_editor.list_all_nodes():
        if node.get_node_title() != "Set Actor Hidden In Game":
            continue

        for pin in node.list_all_pins():
            if str(pin.get_pin_name()) != "self":
                continue

            for connected_pin in pin.list_connected_pins():
                source_node = connected_pin.get_owning_node()
                if (
                    str(connected_pin.get_pin_name()) == "DamageCauser"
                    and source_node.get_node_title()
                    == "Event OnEnemyDeathCosmetics"
                ):
                    nodes_to_remove.append(node)
                    break

            if node in nodes_to_remove:
                break

    if nodes_to_remove:
        graph_editor.remove_nodes(nodes_to_remove)

    return len(nodes_to_remove)


def main():
    blueprint = _load(ENEMY_BLUEPRINT)
    cdo = unreal.get_default_object(blueprint.generated_class())
    reactions = [_load(path) for path in HIT_REACTIONS]
    death_animation = _load(DEATH_ANIMATION)
    removed_node_count = _remove_legacy_damage_causer_hide(blueprint)

    mesh_components = cdo.get_components_by_class(
        unreal.SkeletalMeshComponent
    )
    if not mesh_components:
        raise RuntimeError("BP_EnemySoldier has no skeletal mesh component.")

    enemy_mesh = mesh_components[0].get_editor_property(
        "skeletal_mesh_asset"
    )
    enemy_skeleton = (
        enemy_mesh.get_editor_property("skeleton")
        if enemy_mesh
        else None
    )

    for reaction in reactions + [death_animation]:
        reaction_skeleton = reaction.get_editor_property("skeleton")
        if reaction_skeleton != enemy_skeleton:
            raise RuntimeError(
                "Animation skeleton mismatch: %s uses %s; enemy uses %s"
                % (
                    _asset_path(reaction),
                    _asset_path(reaction_skeleton),
                    _asset_path(enemy_skeleton),
                )
            )

    cdo.set_editor_property("hit_reaction_animations", reactions)
    cdo.set_editor_property("death_animation", death_animation)
    cdo.set_editor_property("enable_ragdoll_on_death", True)
    cdo.set_editor_property("enable_debug", False)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        raise RuntimeError("Failed to save BP_EnemySoldier.")

    _log(
        "Configured four compatible hit reactions, a death lead-in, "
        "ragdoll death, and disabled enemy debug presentation."
    )
    _log(
        "Removed %d legacy enemy-death node(s) that hid DamageCauser."
        % removed_node_count
    )


main()
