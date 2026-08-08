"""Read-only validation for Broken Horizon v0.14 cover combat."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
RIFLE_BLUEPRINT = "/Game/BP_Rifle"


def _log(message):
    unreal.log("[BH Cover Combat Validation] " + message)


def _load_blueprint(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Missing Blueprint: " + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    return unreal.get_default_object(asset.generated_class())


def _actors(world, class_path):
    actor_class = unreal.load_class(None, class_path)
    if not actor_class:
        raise RuntimeError("Missing native class: " + class_path)
    return unreal.GameplayStatics.get_all_actors_of_class(
        world,
        actor_class,
    )


def validate():
    enemy_cdo = _load_blueprint(ENEMY_BLUEPRINT)
    rifle_cdo = _load_blueprint(RIFLE_BLUEPRINT)

    if enemy_cdo.get_editor_property("cover_search_radius") <= 0.0:
        raise RuntimeError("Enemy cover search is disabled.")
    if enemy_cdo.get_editor_property(
        "suppression_cover_threshold"
    ) <= 0.0:
        raise RuntimeError("Enemy suppression response is disabled.")
    if enemy_cdo.get_editor_property("squad_alert_radius") <= 0.0:
        raise RuntimeError("Enemy squad coordination is disabled.")

    rifle_config = rifle_cdo.get_editor_property("rifle_config")
    if rifle_config.get_editor_property("suppression_radius") <= 0.0:
        raise RuntimeError("Rifle suppression radius is disabled.")
    if rifle_config.get_editor_property("suppression_amount") <= 0.0:
        raise RuntimeError("Rifle suppression amount is disabled.")

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    enemies = _actors(
        world,
        "/Script/BrokenHorizon.BHEnemySoldier",
    )
    cover_points = _actors(
        world,
        "/Script/BrokenHorizon.BHCoverPoint",
    )

    if len(enemies) != 3:
        raise RuntimeError(
            "Expected three First Light guards; found %d."
            % len(enemies)
        )
    if len(cover_points) < 6:
        raise RuntimeError(
            "Expected at least six cover points; found %d."
            % len(cover_points)
        )

    objective_ids = {
        str(enemy.get_editor_property(
            "objective_id_to_complete_on_death"
        ))
        for enemy in enemies
    }
    if objective_ids != {"EliminateGuard"}:
        raise RuntimeError(
            "Guards do not share EliminateGuard: %s"
            % sorted(objective_ids)
        )

    _log(
        "PASS: %d coordinated guards, %d cover points, "
        "suppression radius %.0f cm."
        % (
            len(enemies),
            len(cover_points),
            rifle_config.get_editor_property(
                "suppression_radius"
            ),
        )
    )
    return True


validate()
