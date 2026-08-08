"""Validate Broken Horizon v0.12.0 combat feedback."""

import os
import unreal


ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
RIFLE_BLUEPRINT = "/Game/BP_Rifle"
ENEMY_HEADER = "Source/BrokenHorizon/Public/BHEnemySoldier.h"
ENEMY_SOURCE = "Source/BrokenHorizon/Private/BHEnemySoldier.cpp"

EXPECTED_REACTIONS = (
    "/Game/Characters/Mannequins/Anims/Rifle/HitReact/"
    "MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01",
    "/Game/Characters/Mannequins/Anims/Rifle/HitReact/"
    "MM_HitReact_Front_Lgt_02.MM_HitReact_Front_Lgt_02",
    "/Game/Characters/Mannequins/Anims/Rifle/HitReact/"
    "MM_HitReact_Front_Med_01.MM_HitReact_Front_Med_01",
    "/Game/Characters/Mannequins/Anims/Rifle/HitReact/"
    "MM_HitReact_Back_Med_01.MM_HitReact_Back_Med_01",
)

EXPECTED_DEATH = (
    "/Game/Characters/Mannequins/Anims/Death/"
    "MM_Death_Back_01.MM_Death_Back_01"
)


def _log(message):
    unreal.log("[BH Combat Feedback Validation] " + message)


def _load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset


def _path(value):
    return value.get_path_name() if value else "None"


def _component(cdo, component_name, expected_type):
    for component in cdo.get_components_by_class(unreal.ActorComponent):
        if component.get_name() == component_name:
            if not isinstance(component, expected_type):
                raise RuntimeError(
                    "%s has unexpected type %s"
                    % (component_name, component.get_class().get_name())
                )
            return component
    raise RuntimeError("Missing component: " + component_name)


def _validate_enemy_death_graph(blueprint):
    graph = unreal.BlueprintEditorLibrary.find_event_graph(blueprint)
    if not graph:
        raise RuntimeError("BP_EnemySoldier has no event graph.")

    graph_editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)

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
                    raise RuntimeError(
                        "Enemy death still hides DamageCauser; this hides "
                        "the player's rifle after its first kill."
                    )

    _log("PASS enemy death does not hide the damage-causing rifle.")


def _validate_enemy():
    blueprint = _load(ENEMY_BLUEPRINT)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    _validate_enemy_death_graph(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    mesh_component = _component(
        cdo,
        "CharacterMesh0",
        unreal.SkeletalMeshComponent,
    )
    mesh = mesh_component.get_editor_property("skeletal_mesh_asset")

    if not mesh:
        raise RuntimeError("Enemy skeletal mesh is not assigned.")

    physics_asset = mesh.get_editor_property("physics_asset")
    if not physics_asset:
        raise RuntimeError("Enemy mesh has no physics asset for ragdoll.")

    reaction_paths = tuple(
        _path(asset)
        for asset in cdo.get_editor_property("hit_reaction_animations")
    )
    if reaction_paths != EXPECTED_REACTIONS:
        raise RuntimeError(
            "Enemy hit reactions differ: %s" % (reaction_paths,)
        )

    if _path(cdo.get_editor_property("death_animation")) != EXPECTED_DEATH:
        raise RuntimeError("Enemy death animation is not assigned.")

    if not cdo.get_editor_property("enable_ragdoll_on_death"):
        raise RuntimeError("Enemy ragdoll death is disabled.")

    if cdo.get_editor_property("enable_debug"):
        raise RuntimeError("Enemy debug presentation is still enabled.")

    _log(
        "PASS enemy animation skeleton, physics asset, reactions, "
        "ragdoll, and debug defaults."
    )


def _validate_rifle():
    blueprint = _load(RIFLE_BLUEPRINT)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    impact_class = cdo.get_editor_property("impact_actor_class")

    if _path(impact_class) != "/Script/BrokenHorizon.BHImpactEffect":
        raise RuntimeError(
            "Unexpected rifle impact actor class: " + _path(impact_class)
        )

    _log("PASS native physical impact actor assignment.")


def _validate_no_trace_lines():
    project_dir = unreal.Paths.project_dir()
    source_files = (
        "Source/BrokenHorizon/BHCharacter.cpp",
        "Source/BrokenHorizon/Private/BHRifle.cpp",
        "Source/BrokenHorizon/Private/BHEnemySoldier.cpp",
    )

    for relative_path in source_files:
        full_path = os.path.join(project_dir, relative_path)
        with open(full_path, "r", encoding="utf-8") as source_file:
            contents = source_file.read()
        if "DrawDebugLine(" in contents:
            raise RuntimeError(
                "Visible debug trace remains in " + relative_path
            )

    _log("PASS player, rifle, and enemy visible trace-line removal.")


def _read(relative_path):
    path = os.path.join(unreal.Paths.project_dir(), relative_path)
    with open(path, "r", encoding="utf-8") as source_file:
        return source_file.read()


def _require_fragments(relative_path, fragments):
    source = _read(relative_path)

    for fragment in fragments:
        if fragment not in source:
            raise RuntimeError(
                "%s is missing: %s" % (relative_path, fragment)
            )


def _validate_multiplayer_presentations():
    _require_fragments(
        ENEMY_HEADER,
        (
            "UFUNCTION(NetMulticast, Unreliable)",
            "void MulticastFirePresentation(",
            "void MulticastHitPresentation(",
            "UFUNCTION(NetMulticast, Reliable)",
            "void MulticastDeathPresentation(",
        ),
    )
    _require_fragments(
        ENEMY_SOURCE,
        (
            "if (!HasAuthority() ||",
            "MulticastFirePresentation(",
            "MulticastHitPresentation(DamageApplied, DamageCauser);",
            "MulticastDeathPresentation(",
            "void ABHEnemySoldier::MulticastFirePresentation_Implementation(",
            "void ABHEnemySoldier::MulticastDeathPresentation_Implementation(",
        ),
    )
    _log(
        "PASS authoritative enemy fire, impact, hit, and death "
        "presentations multicast to joined players."
    )


def main():
    _validate_enemy()
    _validate_rifle()
    _validate_no_trace_lines()
    _validate_multiplayer_presentations()
    _log("ALL CHECKS PASSED")


main()
