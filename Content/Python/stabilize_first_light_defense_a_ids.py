"""Persist stable Defense A enemy identities in the First Light map."""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
TAG = "BH_Auto_DefenseA_Garrison"
LABEL_TO_ID = {
    "FL_DefenseA_Garrison_01_West":
        "DefenseA_FL_DefenseA_Garrison_01_West",
    "FL_DefenseA_Garrison_02_East":
        "DefenseA_FL_DefenseA_Garrison_02_East",
    "FL_DefenseA_Garrison_03_NorthWest":
        "DefenseA_FL_DefenseA_Garrison_03_NorthWest",
    "FL_DefenseA_Garrison_04_NorthEast":
        "DefenseA_FL_DefenseA_Garrison_04_NorthEast",
    "FL_DefenseA_Garrison_05_OuterWest":
        "DefenseA_FL_DefenseA_Garrison_05_OuterWest",
    "FL_DefenseA_Garrison_06_OuterEast":
        "DefenseA_FL_DefenseA_Garrison_06_OuterEast",
}


def author():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Missing First Light map: " + MAP_PATH)

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()
    enemy_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHEnemySoldier",
    )
    if not enemy_class:
        raise RuntimeError("BHEnemySoldier class is unavailable")

    enemies = unreal.GameplayStatics.get_all_actors_of_class(
        world,
        enemy_class,
    )
    garrison = [
        enemy for enemy in enemies
        if TAG in [str(tag) for tag in enemy.tags]
    ]
    if len(garrison) != len(LABEL_TO_ID):
        raise RuntimeError(
            "Expected %d Defense A garrison actors, found %d"
            % (len(LABEL_TO_ID), len(garrison))
        )

    assigned_ids = set()
    for enemy in garrison:
        label = str(enemy.get_actor_label())
        field_operative_id = LABEL_TO_ID.get(label)
        if not field_operative_id:
            raise RuntimeError(
                "Unexpected Defense A garrison label: " + label
            )
        enemy.set_editor_property(
            "field_operative_id",
            unreal.Name(field_operative_id),
        )
        assigned_ids.add(field_operative_id)

    if assigned_ids != set(LABEL_TO_ID.values()):
        raise RuntimeError(
            "Defense A identity assignment did not cover the canonical set"
        )

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log(
        "[FirstLight Defense A IDs] PASS: assigned %d stable identities"
        % len(assigned_ids)
    )
    return True


if __name__ == "__main__":
    author()
