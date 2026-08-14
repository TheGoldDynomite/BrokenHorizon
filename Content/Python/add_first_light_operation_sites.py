"""Idempotently author the eight operation-matrix site contracts in First Light."""

import unreal

MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
MARKER_CLASS = "/Script/BrokenHorizon.BHOperationSiteMarker"
TAG = "BH_Auto_FirstLight_OperationSite"

SITES = (
    ("FirstLightAttackA", "Attack", "A_Baseline", "Checkpoint", "DIRECT CHECKPOINT APPROACH", (6100.0, 0.0, 40.0)),
    ("FirstLightAttackB", "Attack", "B_Offset", "WaterEdgeCheckpoint", "RURAL / WATER-EDGE FLANK", (-4700.0, -1800.0, 40.0)),
    ("FirstLightDefenseA", "Defense", "A_Hold", "ResistanceFacility", "HOLD THE FACILITY", (-9500.0, -4500.0, 40.0)),
    ("FirstLightDefenseB", "Defense", "B_Breach", "OuterPerimeter", "RECOVER BROKEN PERIMETER", (-8750.0, -3900.0, 40.0)),
    ("FirstLightRaidA", "Raid", "A_Clean", "LogisticsTarget", "QUIET DEPOT SABOTAGE", (9000.0, 4500.0, 40.0)),
    ("FirstLightRaidB", "Raid", "B_Contested", "ReactionForce", "CONTESTED DEPOT SABOTAGE", (8400.0, 5200.0, 40.0)),
    ("FirstLightResupplyA", "Resupply", "A_Convoy", "CargoRoute", "SECURE RURAL CONVOY", (1000.0, 1000.0, 40.0)),
    ("FirstLightResupplyB", "Resupply", "B_Water", "WaterRoute", "WATER CROSSING REQUIRED", (5000.0, 300.0, 120.0)),
)


def find_marker(persistence_id, label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
        try:
            if actor.get_editor_property("persistence_id") == unreal.Name(persistence_id):
                return actor
        except Exception:
            pass
    return None


def main():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    marker_class = unreal.load_class(None, MARKER_CLASS)
    if not marker_class:
        unreal.log_error("[FirstLight OperationSites] Native marker class unavailable")
        return False
    authored = []
    for persistence_id, family, variation, purpose, approach, location in SITES:
        label = persistence_id
        actor = find_marker(persistence_id, label)
        if actor is None:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                marker_class, unreal.Vector(*location), unreal.Rotator(0.0, 0.0, 0.0)
            )
            if not actor:
                unreal.log_error("[FirstLight OperationSites] Spawn failed for %s" % label)
                return False
            actor.set_actor_label(label)
            actor.tags = list(actor.tags) + [unreal.Name(TAG)]
        actor.set_actor_location(unreal.Vector(*location), False, True)
        actor.configure_operation_site(
            unreal.Name(persistence_id), unreal.Name(family), unreal.Name(variation),
            unreal.Name(purpose), unreal.Text(approach)
        )
        authored.append(persistence_id)
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[FirstLight OperationSites] Authored sites=%d ids=%s" % (len(authored), ",".join(authored)))
    return True


main()
