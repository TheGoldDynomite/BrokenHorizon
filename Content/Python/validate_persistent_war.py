"""Validate Broken Horizon Persistent War v1."""

import os

import unreal


def _log(message):
    unreal.log("[BH Persistent War Validation] " + message)


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

def _reject_fragments(relative_path, fragments):
    source = _read(relative_path)

    for fragment in fragments:
        if fragment in source:
            raise RuntimeError(
                "%s must not contain: %s" %
                (relative_path, fragment)
            )


def _validate_reflection():
    war_subsystem_class = getattr(unreal, "BHWarSubsystem", None)
    save_game_class = getattr(unreal, "BHSaveGame", None)
    ambient_director_class = getattr(
        unreal,
        "BHAmbientWarDirector",
        None,
    )
    convoy_target_class = getattr(
        unreal,
        "BHSupplyConvoyTarget",
        None,
    )
    war_game_state_class = getattr(
        unreal,
        "BHWarGameState",
        None,
    )

    if not war_subsystem_class:
        raise RuntimeError("BHWarSubsystem is not reflected.")

    if not save_game_class:
        raise RuntimeError("BHSaveGame is not reflected.")

    if not ambient_director_class:
        raise RuntimeError("BHAmbientWarDirector is not reflected.")

    if not convoy_target_class:
        raise RuntimeError("BHSupplyConvoyTarget is not reflected.")

    if not war_game_state_class:
        raise RuntimeError("BHWarGameState is not reflected.")

    war_defaults = unreal.get_default_object(war_subsystem_class)
    save_defaults = unreal.get_default_object(save_game_class)
    ambient_defaults = unreal.get_default_object(
        ambient_director_class
    )
    convoy_target_defaults = unreal.get_default_object(
        convoy_target_class
    )

    interval = war_defaults.get_editor_property(
        "simulation_interval_seconds"
    )
    engaged_interval = war_defaults.get_editor_property(
        "committed_operation_simulation_interval_seconds"
    )

    if abs(float(interval) - 120.0) > 0.01:
        raise RuntimeError(
            "War simulation interval expected 120 but found %.2f"
            % float(interval)
        )

    if abs(float(engaged_interval) - 180.0) > 0.01:
        raise RuntimeError(
            "Engaged war simulation interval expected 180 but "
            "found %.2f" % float(engaged_interval)
        )

    if int(save_defaults.get_editor_property("schema_version")) != 37:
        raise RuntimeError("Persistent-war save schema is not version 37.")

    if bool(
        save_defaults.get_editor_property(
            "operation_debrief_acknowledged"
        )
    ):
        raise RuntimeError(
            "New saves should not begin with an acknowledged debrief."
        )

    save_defaults.get_editor_property("war_sector_states")
    save_defaults.get_editor_property("war_supply_convoys")
    save_defaults.get_editor_property("war_friendly_manpower_reserve")
    save_defaults.get_editor_property("war_enemy_manpower_reserve")
    save_defaults.get_editor_property(
        "war_friendly_recruitment_progress"
    )
    save_defaults.get_editor_property(
        "war_enemy_recruitment_progress"
    )
    save_defaults.get_editor_property("war_event_history")
    save_defaults.get_editor_property("war_turn_number")
    save_defaults.get_editor_property("war_simulation_accumulator")
    save_defaults.get_editor_property(
        "assigned_war_supply_source_sector_id"
    )
    if bool(save_defaults.get_editor_property("mission_failed")):
        raise RuntimeError(
            "New save objects should not begin in a failed state."
        )

    operation_state = save_defaults.get_editor_property(
        "open_world_operation_state"
    )
    if abs(
        float(
            operation_state.get_editor_property(
                "defense_breach_progress"
            )
        )
    ) > 0.01:
        raise RuntimeError(
            "New operation snapshots should have no breach progress."
        )
    save_defaults.get_editor_property("saved_health")
    save_defaults.get_editor_property("has_saved_injury_state")
    save_defaults.get_editor_property("saved_bleed_rate")
    save_defaults.get_editor_property("saved_frag_grenade_count")
    patrol_separation = float(
        ambient_defaults.get_editor_property(
            "opposing_patrol_separation"
        )
    )
    formation_spacing = float(
        ambient_defaults.get_editor_property("formation_spacing")
    )
    sector_contact_cooldown = float(
        ambient_defaults.get_editor_property(
            "sector_contact_cooldown"
        )
    )
    patrol_supply_cost = float(
        ambient_defaults.get_editor_property(
            "patrol_supply_cost_per_member"
        )
    )
    projection_supply_cost = float(
        ambient_defaults.get_editor_property(
            "patrol_projection_supply_cost_per_hop"
        )
    )
    convoy_spawn_distance = float(
        ambient_defaults.get_editor_property(
            "convoy_opportunity_spawn_distance"
        )
    )
    convoy_escort_count = int(
        ambient_defaults.get_editor_property(
            "convoy_escort_count"
        )
    )
    configured_convoy_target_class = (
        ambient_defaults.get_editor_property(
            "supply_convoy_target_class"
        )
    )
    convoy_health_component = (
        convoy_target_defaults.get_editor_property(
            "health_component"
        )
    )
    convoy_movement_speed = float(
        convoy_target_defaults.get_editor_property(
            "movement_speed"
        )
    )

    if patrol_separation < 1000.0:
        raise RuntimeError(
            "Opposing ambient patrols deploy too close together."
        )

    if formation_spacing < 100.0:
        raise RuntimeError(
            "Ambient patrol formation spacing is disabled."
        )

    if sector_contact_cooldown < 30.0:
        raise RuntimeError(
            "Ambient sector contact cooldown is too short."
        )

    if patrol_supply_cost <= 0.0:
        raise RuntimeError(
            "Ambient patrols have no strategic supply cost."
        )

    if projection_supply_cost <= 0.0:
        raise RuntimeError(
            "Projected patrols have no additional logistics cost."
        )

    if abs(convoy_spawn_distance - 6000.0) > 0.01:
        raise RuntimeError(
            "Enemy convoy opportunity distance is not 6000 cm."
        )

    if convoy_escort_count < 2:
        raise RuntimeError(
            "Enemy convoy opportunities lack an escort detail."
        )

    if not configured_convoy_target_class:
        raise RuntimeError(
            "Ambient war has no physical convoy target class."
        )

    if not convoy_health_component:
        raise RuntimeError(
            "Physical convoy target has no health component."
        )

    if convoy_movement_speed <= 0.0:
        raise RuntimeError(
            "Physical convoy target movement is disabled."
        )

    _log("PASS war subsystem and schema-v37 save fields are reflected.")
    _log(
        "PASS ambient squads deploy %.0f cm apart with %.0f cm "
        "formation spacing and a %.0f s sector cooldown."
        % (
            patrol_separation,
            formation_spacing,
            sector_contact_cooldown,
        )
    )
    _log(
        "PASS ambient patrol supply costs are %.2f per member plus "
        "%.2f per projection hop."
        % (patrol_supply_cost, projection_supply_cost)
    )
    _log(
        "PASS enemy convoy opportunities spawn %.0f cm ahead and "
        "travel at %.0f cm/s with %d escorts."
        % (
            convoy_spawn_distance,
            convoy_movement_speed,
            convoy_escort_count,
        )
    )


def _validate_simulation():
    game_instance = unreal.new_object(unreal.GameInstance)
    war = unreal.new_object(
        unreal.BHWarSubsystem,
        outer=game_instance,
    )
    war.reset_campaign()

    sectors = war.get_sector_states()

    if len(sectors) != 6:
        raise RuntimeError(
            "Default campaign expected 6 sectors but found %d"
            % len(sectors)
        )

    initial_crossroads = war.get_sector_state(
        "KoronaCrossroads"
    )
    initial_western_fob = war.get_sector_state("WesternFOB")

    if (
        int(initial_crossroads.garrison_capacity) != 12
        or int(initial_crossroads.enemy_garrison) != 9
        or int(initial_western_fob.garrison_capacity) != 14
        or int(initial_western_fob.friendly_garrison) != 10
    ):
        raise RuntimeError(
            "Default campaign garrisons are not initialized."
        )

    if int(war.get_sector_garrison_count(
        "KoronaCrossroads",
        unreal.BHWarFaction.ENEMY,
    )) != 9:
        raise RuntimeError(
            "Enemy garrison query returned the wrong count."
        )

    militia_count = int(
        war.get_sector_militia_mobilization_count(
            "WesternFOB"
        )
    )
    militia_supply_cost = float(
        war.get_sector_militia_mobilization_supply_cost(
            "WesternFOB"
        )
    )

    if militia_count != 2 or abs(militia_supply_cost - 8.0) > 0.01:
        raise RuntimeError(
            "Civilian support produced the wrong militia package."
        )

    if not war.can_mobilize_sector_militia("WesternFOB"):
        raise RuntimeError(
            "Supplied friendly headquarters cannot rally militia."
        )

    if not war.mobilize_sector_militia("WesternFOB"):
        raise RuntimeError("Militia mobilization was rejected.")

    mobilized_headquarters = war.get_sector_state("WesternFOB")

    if (
        int(mobilized_headquarters.friendly_garrison) != 12
        or abs(float(mobilized_headquarters.supply) - 72.0) > 0.01
        or abs(
            float(mobilized_headquarters.civilian_support) - 72.0
        ) > 0.01
        or abs(
            float(mobilized_headquarters.friendly_strength) - 84.0
        ) > 0.01
        or abs(
            float(
                mobilized_headquarters.enemy_response_pressure
            ) - 25.0
        ) > 0.01
    ):
        raise RuntimeError(
            "Militia did not persist in garrison, strength, supply, "
            "civilian support, and enemy response."
        )

    if war.mobilize_sector_militia("KoronaCrossroads"):
        raise RuntimeError(
            "Enemy-controlled sector mobilized friendly militia."
        )

    war.reset_campaign()

    aid_target_id = "KoronaCrossroads"
    aid_source_id = str(
        war.get_operation_supply_source(
            aid_target_id,
            unreal.BHWarPriorityType.ATTACK,
        )
    )
    aid_target_before = war.get_sector_state(aid_target_id)
    aid_source_before = war.get_sector_state(aid_source_id)
    aid_recruitment_before = float(
        war.get_faction_recruitment_per_turn(
            unreal.BHWarFaction.FRIENDLY
        )
    )

    if not war.can_deliver_civilian_aid(
        aid_target_id,
        unreal.BHWarPriorityType.ATTACK,
    ):
        raise RuntimeError(
            "Connected hostile community cannot receive aid."
        )

    if not war.deliver_civilian_aid(
        aid_target_id,
        unreal.BHWarPriorityType.ATTACK,
    ):
        raise RuntimeError("Civilian aid delivery was rejected.")

    aid_target_dispatched = war.get_sector_state(aid_target_id)
    aid_source_after = war.get_sector_state(aid_source_id)
    aid_convoys = [
        convoy
        for convoy in war.get_supply_convoys()
        if str(convoy.destination_sector_id) == aid_target_id
        and "CIVILIAN_AID" in str(convoy.cargo_type).upper()
    ]

    if (
        abs(
            float(aid_source_after.supply)
            - (
                float(aid_source_before.supply)
                - float(war.get_civilian_aid_supply_cost())
            )
        )
        > 0.01
        or abs(
            float(aid_target_dispatched.civilian_support)
            - float(aid_target_before.civilian_support)
        )
        > 0.01
        or not aid_convoys
    ):
        raise RuntimeError(
            "Aid dispatch did not consume supply and create an "
            "in-transit civilian cargo convoy without applying "
            "its destination effects early."
        )

    if war.can_deliver_civilian_aid(
        aid_target_id,
        unreal.BHWarPriorityType.ATTACK,
    ):
        raise RuntimeError(
            "A second aid shipment was allowed to stack in transit."
        )

    war.advance_war_turn()
    aid_target_after = war.get_sector_state(aid_target_id)

    if (
        float(aid_target_after.civilian_support)
        <= float(aid_target_before.civilian_support)
        or float(aid_target_after.intel_confidence)
        <= float(aid_target_before.intel_confidence)
        or float(
            war.get_faction_recruitment_per_turn(
                unreal.BHWarFaction.FRIENDLY
            )
        )
        <= aid_recruitment_before
        or any(
            str(convoy.destination_sector_id) == aid_target_id
            and "CIVILIAN_AID" in str(convoy.cargo_type).upper()
            for convoy in war.get_supply_convoys()
        )
    ):
        raise RuntimeError(
            "Aid convoy arrival did not improve its destination "
            "network and clear the active route."
        )

    war.reset_campaign()

    recovery_sector_id = "DovrenVillage"
    recovery_before = war.get_sector_state(recovery_sector_id)
    ambient_recovery = float(
        war.recover_battlefield_materiel(
            recovery_sector_id,
            4,
            1,
            False,
        )
    )
    recovery_after = war.get_sector_state(recovery_sector_id)

    if (
        abs(ambient_recovery - 4.0) > 0.01
        or abs(
            float(recovery_after.supply)
            - (
                float(recovery_before.supply)
                + ambient_recovery
            )
        ) > 0.01
        or float(
            war.recover_battlefield_materiel(
                "KoronaCrossroads",
                6,
                0,
                False,
            )
        ) > 0.01
    ):
        raise RuntimeError(
            "Battlefield recovery did not reward bounded salvage "
            "only in secured territory."
        )

    war.reset_campaign()

    raid_sector_id = "KoronaCrossroads"
    raid_before = war.get_sector_state(raid_sector_id)

    if not war.is_viable_operation(
        raid_sector_id,
        unreal.BHWarPriorityType.RAID,
    ):
        raise RuntimeError(
            "Enemy logistics sector did not expose a raid operation."
        )

    if war.is_viable_operation(
        "WesternFOB",
        unreal.BHWarPriorityType.RAID,
    ):
        raise RuntimeError(
            "Friendly territory exposed an invalid raid operation."
        )

    if not war.apply_mission_result(
        raid_sector_id,
        unreal.BHWarPriorityType.RAID,
        True,
    ):
        raise RuntimeError("Successful logistics raid was rejected.")

    raid_after = war.get_sector_state(raid_sector_id)

    if (
        raid_after.owner != unreal.BHWarFaction.ENEMY
        or float(raid_after.supply) >= float(raid_before.supply)
        or float(raid_after.enemy_strength)
        >= float(raid_before.enemy_strength)
        or float(raid_after.enemy_response_pressure)
        <= float(raid_before.enemy_response_pressure)
    ):
        raise RuntimeError(
            "Raid did not disrupt enemy logistics while preserving "
            "territorial ownership and increasing response pressure."
        )

    war.reset_campaign()

    if abs(float(
        war.get_sector_intel_confidence(
            "KoronaCrossroads"
        )
    ) - 20.0) > 0.01:
        raise RuntimeError(
            "Hostile town begins with incorrect intelligence confidence."
        )

    if not war.report_sector_recon(
        "KoronaCrossroads",
        45.0,
    ):
        raise RuntimeError("Sector reconnaissance report was rejected.")

    if "EST ENEMY" not in str(
        war.get_sector_enemy_intel_summary(
            "KoronaCrossroads"
        )
    ):
        raise RuntimeError(
            "Moderate intelligence did not produce an estimate."
        )

    war.advance_war_turn()

    if abs(float(
        war.get_sector_intel_confidence(
            "KoronaCrossroads"
        )
    ) - 59.0) > 0.01:
        raise RuntimeError(
            "Hostile-sector intelligence did not become stale."
        )

    war.reset_campaign()

    if not war.apply_ambient_battle_result(
        "KoronaCrossroads",
        0,
        2,
    ):
        raise RuntimeError(
            "Garrison casualty report was rejected."
        )

    if int(
        war.get_sector_state(
            "KoronaCrossroads"
        ).enemy_garrison
    ) != 7:
        raise RuntimeError(
            "Enemy casualties did not reduce the persistent garrison."
        )

    war.reset_campaign()

    if str(war.get_priority_sector_id()) != "KoronaCrossroads":
        raise RuntimeError(
            "Initial priority is not KoronaCrossroads."
        )

    if war.get_priority_type() != unreal.BHWarPriorityType.ATTACK:
        raise RuntimeError("Initial priority is not an attack.")

    if abs(float(
        war.get_sector_supply_change_per_turn("WesternFOB")
    ) - 3.0) > 0.01:
        raise RuntimeError(
            "Western FOB is not producing logistics-hub supply."
        )

    if abs(float(
        war.get_sector_supply_change_per_turn("EasternDepot")
    ) - 3.0) > 0.01:
        raise RuntimeError(
            "Eastern Depot is not producing logistics-hub supply."
        )

    if abs(float(
        war.get_sector_supply_change_per_turn("DovrenVillage")
    ) + 2.0) > 0.01:
        raise RuntimeError(
            "Friendly frontline supply consumption is incorrect."
        )

    if abs(float(
        war.get_sector_supply_change_per_turn("NorthPass")
    )) > 0.01:
        raise RuntimeError(
            "Neutral sectors should not produce strategic supply."
        )

    if abs(float(
        war.get_sector_reinforcement_per_turn("WesternFOB")
    ) - 4.2) > 0.01:
        raise RuntimeError(
            "Western FOB effective reinforcement rate is incorrect."
        )

    supplied_effective_strength = float(
        war.get_sector_effective_strength(
            "DovrenVillage",
            unreal.BHWarFaction.FRIENDLY,
        )
    )

    if str(
        war.get_priority_operation_supply_source()
    ) != "WesternFOB":
        raise RuntimeError(
            "Default attack is not using the strongest routed hub."
        )

    route = [
        str(sector_id)
        for sector_id in
        war.get_priority_operation_supply_route()
    ]

    if route != [
        "WesternFOB",
        "DovrenVillage",
        "KoronaCrossroads",
    ]:
        raise RuntimeError(
            "Default priority supply route is invalid: %s"
            % route
        )

    assigned_route = [
        str(sector_id)
        for sector_id in war.get_operation_supply_route(
            "KoronaCrossroads",
            unreal.BHWarPriorityType.ATTACK,
        )
    ]

    if assigned_route != route:
        raise RuntimeError(
            "Assigned-operation route does not match priority route."
        )

    if str(war.get_operation_supply_source(
        "KoronaCrossroads",
        unreal.BHWarPriorityType.ATTACK,
    )) != "WesternFOB":
        raise RuntimeError(
            "Assigned operation cannot recover its staging source."
        )

    if abs(float(
        war.get_priority_operation_supply_cost()
    ) - 10.0) > 0.01:
        raise RuntimeError(
            "Priority-operation supply cost is not 10."
        )

    if not war.can_fund_priority_operation():
        raise RuntimeError(
            "Default campaign cannot fund its priority operation."
        )

    committed_before = war.get_sector_state("KoronaCrossroads")
    western_garrison_before = int(
        war.get_sector_state("WesternFOB").friendly_garrison
    )

    if not war.set_committed_operation(
        "KoronaCrossroads",
        unreal.BHWarPriorityType.ATTACK,
    ):
        raise RuntimeError(
            "The default priority could not be committed."
        )

    if not war.has_committed_operation():
        raise RuntimeError(
            "Committed operation state was not retained."
        )

    if not war.set_committed_operation(
        "KoronaCrossroads",
        unreal.BHWarPriorityType.ATTACK,
    ):
        raise RuntimeError(
            "Restoring the same committed operation is not idempotent."
        )

    if war.set_committed_operation(
        "DovrenVillage",
        unreal.BHWarPriorityType.DEFEND,
    ):
        raise RuntimeError(
            "Conflicting operation replaced the active deployment."
        )

    if war.is_viable_operation(
        "DovrenVillage",
        unreal.BHWarPriorityType.DEFEND,
    ):
        raise RuntimeError(
            "Conflicting operation remained viable during deployment."
        )

    war.advance_war_turn()
    committed_after = war.get_sector_state("KoronaCrossroads")
    western_garrison_after = int(
        war.get_sector_state("WesternFOB").friendly_garrison
    )

    if western_garrison_after != western_garrison_before:
        raise RuntimeError(
            "Committed staging garrison changed during deployment."
        )

    if (
        str(war.get_priority_sector_id()) != "KoronaCrossroads"
        or war.get_priority_type()
        != unreal.BHWarPriorityType.ATTACK
        or str(war.get_priority_reason_text()).upper()
        != "OPERATION IN PROGRESS"
    ):
        raise RuntimeError(
            "Background simulation retargeted the committed operation."
        )

    if (
        committed_after.owner != committed_before.owner
        or abs(
            float(committed_after.friendly_strength)
            - float(committed_before.friendly_strength)
        ) > 0.01
        or abs(
            float(committed_after.enemy_strength)
            - float(committed_before.enemy_strength)
        ) > 0.01
        or abs(
            float(committed_after.supply)
            - float(committed_before.supply)
        ) > 0.01
    ):
        raise RuntimeError(
            "Background simulation changed the committed sector."
        )

    war.clear_committed_operation()

    if war.has_committed_operation():
        raise RuntimeError(
            "Committed operation state did not clear."
        )

    war.advance_war_turn()
    western_garrison_released = int(
        war.get_sector_state("WesternFOB").friendly_garrison
    )

    if western_garrison_released <= western_garrison_after:
        raise RuntimeError(
            "Released staging garrison did not resume reinforcement."
        )

    war.reset_campaign()

    if not war.is_sector_connected_to_faction_logistics(
        "DovrenVillage"
    ):
        raise RuntimeError(
            "Default friendly frontline is cut off from logistics."
        )

    if abs(float(
        war.get_sector_isolation_attrition_per_turn(
            "DovrenVillage"
        )
    )) > 0.01:
        raise RuntimeError(
            "Connected Dovren Village is suffering isolation attrition."
        )

    if not war.is_sector_connected_to_faction_logistics(
        "KoronaCrossroads"
    ):
        raise RuntimeError(
            "Default enemy frontline is cut off from logistics."
        )

    war.apply_mission_result(
        "DovrenVillage",
        unreal.BHWarPriorityType.DEFEND,
        False,
    )
    war.apply_mission_result(
        "NorthPass",
        unreal.BHWarPriorityType.ATTACK,
        True,
    )

    if war.is_sector_connected_to_faction_logistics("NorthPass"):
        raise RuntimeError(
            "Isolated friendly North Pass found a false supply route."
        )

    if abs(float(
        war.get_sector_reinforcement_per_turn("NorthPass")
    )) > 0.01:
        raise RuntimeError(
            "Cut-off North Pass still receives reinforcements."
        )

    if float(
        war.get_sector_supply_change_per_turn("NorthPass")
    ) >= 0.0:
        raise RuntimeError(
            "Cut-off North Pass is not losing local supply."
        )

    isolation_attrition = float(
        war.get_sector_isolation_attrition_per_turn("NorthPass")
    )

    if abs(isolation_attrition - (2.0 / 3.0)) > 0.01:
        raise RuntimeError(
            "Cut-off North Pass has the wrong strength attrition rate."
        )

    if str(war.get_priority_sector_id()) != "DovrenVillage":
        raise RuntimeError(
            "Command did not prioritize the Dovren reconnection attack."
        )

    if war.get_priority_type() != unreal.BHWarPriorityType.ATTACK:
        raise RuntimeError(
            "The isolated-sector recovery priority is not an attack."
        )

    if str(war.get_priority_reason_text()).upper() != (
        "RESTORE ISOLATED SECTORS"
    ):
        raise RuntimeError(
            "The reconnection priority does not explain command intent."
        )

    if not war.can_fund_priority_operation():
        raise RuntimeError(
            "The reconnection attack cannot use the supplied western hub."
        )

    war.reset_campaign()
    patrol_supply_before = float(
        war.get_sector_state("WesternFOB").supply
    )
    patrol_supply_committed = float(
        war.commit_ambient_patrol_supply(
            "WesternFOB",
            unreal.BHWarFaction.FRIENDLY,
            1.05,
        )
    )
    patrol_supply_after = float(
        war.get_sector_state("WesternFOB").supply
    )

    if abs(patrol_supply_committed - 1.05) > 0.01:
        raise RuntimeError(
            "Friendly patrol committed the wrong supply amount."
        )

    if abs(
        (patrol_supply_before - patrol_supply_after) - 1.05
    ) > 0.01:
        raise RuntimeError(
            "Friendly patrol did not consume source-sector supply."
        )

    enemy_supply_before = float(
        war.get_sector_state("EasternDepot").supply
    )
    enemy_supply_committed = float(
        war.commit_ambient_patrol_supply(
            "EasternDepot",
            unreal.BHWarFaction.ENEMY,
            0.50,
        )
    )
    enemy_supply_after = float(
        war.get_sector_state("EasternDepot").supply
    )

    if abs(enemy_supply_committed - 0.50) > 0.01:
        raise RuntimeError(
            "Enemy patrol committed the wrong supply amount."
        )

    if abs(
        (enemy_supply_before - enemy_supply_after) - 0.50
    ) > 0.01:
        raise RuntimeError(
            "Enemy patrol did not consume source-sector supply."
        )

    war.reset_campaign()
    starved_strength_before = float(
        war.get_sector_state("WesternFOB").friendly_strength
    )
    drained_supply = float(
        war.commit_ambient_patrol_supply(
            "WesternFOB",
            unreal.BHWarFaction.FRIENDLY,
            1000.0,
        )
    )

    if abs(drained_supply - 80.0) > 0.01:
        raise RuntimeError(
            "Starvation setup did not drain Western FOB supply."
        )

    if abs(float(
        war.get_sector_reinforcement_per_turn("WesternFOB")
    )) > 0.01:
        raise RuntimeError(
            "Starved sector still reports incoming reinforcements."
        )

    war.advance_war_turn()
    starved_strength_after = float(
        war.get_sector_state("WesternFOB").friendly_strength
    )

    if abs(
        starved_strength_after - starved_strength_before
    ) > 0.01:
        raise RuntimeError(
            "Zero-supply sector still generated reinforcements."
        )

    war.reset_campaign()
    supplied_combat_factor = float(
        war.get_sector_combat_supply_factor("DovrenVillage")
    )
    supplied_crossroads_before = float(
        war.get_sector_state(
            "KoronaCrossroads"
        ).friendly_strength
    )
    war.advance_war_turn()
    supplied_crossroads_after = float(
        war.get_sector_state(
            "KoronaCrossroads"
        ).friendly_strength
    )
    supplied_frontline_pressure = (
        supplied_crossroads_after -
        supplied_crossroads_before
    )

    war.reset_campaign()
    war.commit_ambient_patrol_supply(
        "DovrenVillage",
        unreal.BHWarFaction.FRIENDLY,
        1000.0,
    )
    starved_combat_factor = float(
        war.get_sector_combat_supply_factor("DovrenVillage")
    )
    starved_effective_strength = float(
        war.get_sector_effective_strength(
            "DovrenVillage",
            unreal.BHWarFaction.FRIENDLY,
        )
    )
    starved_dovren = war.get_sector_state("DovrenVillage")
    expected_starved_effective_strength = (
        float(starved_dovren.friendly_strength)
        + (float(starved_dovren.friendly_garrison) * 4.0)
    ) * starved_combat_factor
    starved_crossroads_before = float(
        war.get_sector_state(
            "KoronaCrossroads"
        ).friendly_strength
    )
    war.advance_war_turn()
    starved_crossroads_after = float(
        war.get_sector_state(
            "KoronaCrossroads"
        ).friendly_strength
    )
    starved_frontline_pressure = (
        starved_crossroads_after -
        starved_crossroads_before
    )

    if supplied_combat_factor <= starved_combat_factor:
        raise RuntimeError(
            "Supply does not improve frontline combat effectiveness."
        )

    if abs(starved_combat_factor - 0.35) > 0.01:
        raise RuntimeError(
            "Starved frontline combat factor is not 0.35."
        )

    if supplied_effective_strength <= starved_effective_strength:
        raise RuntimeError(
            "Strategic strength ignores source-sector supply."
        )

    if abs(
        starved_effective_strength
        - expected_starved_effective_strength
    ) > 0.01:
        raise RuntimeError(
            "Starved Dovren effective strength does not include "
            "its persistent garrison and combat supply factor."
        )

    if (
        supplied_frontline_pressure <=
        starved_frontline_pressure
    ):
        raise RuntimeError(
            "Starved forces project normal frontline pressure."
        )

    war.reset_campaign()
    staging_supply_before = float(
        war.get_sector_state("WesternFOB").supply
    )

    if not war.consume_priority_operation_supply():
        raise RuntimeError(
            "Priority operation did not consume staging supply."
        )

    staging_supply_after = float(
        war.get_sector_state("WesternFOB").supply
    )

    if abs(
        (staging_supply_before - staging_supply_after) - 10.0
    ) > 0.01:
        raise RuntimeError(
            "Priority operation consumed the wrong supply amount."
        )

    war.reset_campaign()

    if not war.resolve_priority_mission(True):
        raise RuntimeError("Priority mission could not be resolved.")

    crossroads = war.get_sector_state("KoronaCrossroads")

    if crossroads.owner != unreal.BHWarFaction.FRIENDLY:
        raise RuntimeError(
            "Successful attack did not capture KoronaCrossroads."
        )

    if (
        int(crossroads.enemy_garrison) != 0
        or int(crossroads.friendly_garrison) < 2
    ):
        raise RuntimeError(
            "Captured sector did not establish an occupation garrison."
        )

    if int(war.get_turn_number()) != 1:
        raise RuntimeError("Mission result did not advance the war turn.")

    operation_events = war.get_recent_war_events()

    if (
        not operation_events
        or str(operation_events[-1].event_type)
        != "OperationSucceeded"
        or "KORONA CROSSROADS"
        not in str(operation_events[-1].summary).upper()
    ):
        raise RuntimeError(
            "Successful operation was not recorded in campaign history."
        )

    war.advance_war_turn()

    if int(war.get_turn_number()) != 2:
        raise RuntimeError("Simulation did not advance the war turn.")

    if len(war.get_sector_states()) != 6:
        raise RuntimeError("Simulation damaged the sector graph.")

    war.reset_campaign()
    rout_before = war.get_sector_state("KoronaCrossroads")

    if not war.apply_ambient_rout_result(
        "KoronaCrossroads",
        0,
        2,
    ):
        raise RuntimeError("Ambient enemy rout was rejected.")

    rout_after = war.get_sector_state("KoronaCrossroads")

    if abs(
        float(rout_after.enemy_strength)
        - (float(rout_before.enemy_strength) - 1.5)
    ) > 0.01:
        raise RuntimeError(
            "Ambient rout applied the wrong enemy-strength loss."
        )

    if abs(
        float(rout_after.friendly_strength)
        - float(rout_before.friendly_strength)
    ) > 0.01:
        raise RuntimeError(
            "Enemy rout incorrectly damaged friendly strength."
        )

    if abs(
        float(rout_after.supply)
        - (float(rout_before.supply) - 0.5)
    ) > 0.01:
        raise RuntimeError(
            "Ambient rout applied the wrong supply cost."
        )

    war.reset_campaign()
    war.advance_war_turn()
    western_fob = war.get_sector_state("WesternFOB")
    dovren_village = war.get_sector_state("DovrenVillage")
    eastern_depot = war.get_sector_state("EasternDepot")
    south_bridge = war.get_sector_state("SouthBridge")
    dispatched_convoys = war.get_supply_convoys()

    if float(western_fob.supply) >= 81.0:
        raise RuntimeError(
            "Western FOB did not dispatch rear-area supply."
        )

    if float(dovren_village.supply) > 53.01:
        raise RuntimeError(
            "Dovren Village received supply without convoy travel."
        )

    if float(eastern_depot.supply) >= 91.0:
        raise RuntimeError(
            "Eastern Depot did not dispatch enemy supply."
        )

    if len(dispatched_convoys) < 2:
        raise RuntimeError(
            "Strategic logistics did not create supply convoys."
        )

    if float(
        war.get_incoming_convoy_supply("DovrenVillage")
    ) <= 0.0:
        raise RuntimeError(
            "Dovren Village has no incoming convoy cargo."
        )

    if float(
        war.get_outgoing_convoy_supply("WesternFOB")
    ) <= 0.0:
        raise RuntimeError(
            "Western FOB has no outgoing convoy cargo."
        )

    initial_convoy_ids = {
        str(convoy.convoy_id)
        for convoy in dispatched_convoys
    }
    dovren_supply_before_arrival = float(dovren_village.supply)
    south_supply_before_arrival = float(south_bridge.supply)

    war.advance_war_turn()
    dovren_after_arrival = war.get_sector_state("DovrenVillage")
    south_after_arrival = war.get_sector_state("SouthBridge")
    active_convoy_ids = {
        str(convoy.convoy_id)
        for convoy in war.get_supply_convoys()
    }

    if float(dovren_after_arrival.supply) <= dovren_supply_before_arrival:
        raise RuntimeError(
            "Dovren Village did not receive its delayed convoy."
        )

    if float(south_after_arrival.supply) <= south_supply_before_arrival:
        raise RuntimeError(
            "South Bridge did not receive its delayed convoy."
        )

    if initial_convoy_ids & active_convoy_ids:
        raise RuntimeError(
            "Arrived convoys remained active after delivery."
        )

    arrival_events = war.get_recent_war_events()

    if not any(
        str(event.event_type) == "ConvoyArrived"
        for event in arrival_events
    ):
        raise RuntimeError(
            "Convoy delivery was not recorded in campaign history."
        )

    war.reset_campaign()
    war.advance_war_turn()
    friendly_convoy = next(
        (
            convoy
            for convoy in war.get_supply_convoys()
            if convoy.owner == unreal.BHWarFaction.FRIENDLY
        ),
        None,
    )

    if not friendly_convoy:
        raise RuntimeError(
            "Interdiction test found no friendly supply convoy."
        )

    convoy_id = friendly_convoy.convoy_id
    destination_id = friendly_convoy.destination_sector_id
    destination_supply_before_interdiction = float(
        war.get_sector_state(destination_id).supply
    )

    if not war.has_supply_convoy(convoy_id):
        raise RuntimeError(
            "Dispatched convoy cannot be queried by identifier."
        )

    queried_convoy = war.get_supply_convoy_state(convoy_id)

    if str(queried_convoy.destination_sector_id) != str(
        destination_id
    ):
        raise RuntimeError(
            "Convoy lookup returned the wrong strategic route."
        )

    if not war.interdict_supply_convoy(convoy_id):
        raise RuntimeError(
            "Active supply convoy could not be interdicted."
        )

    if war.has_supply_convoy(convoy_id):
        raise RuntimeError(
            "Interdicted convoy remained in the active manifest."
        )

    if float(
        war.get_incoming_convoy_supply(destination_id)
    ) > 0.01:
        raise RuntimeError(
            "Interdicted cargo still reports as incoming."
        )

    interdiction_events = war.get_recent_war_events()

    if (
        not interdiction_events
        or str(interdiction_events[-1].event_type)
        != "ConvoyInterdicted"
    ):
        raise RuntimeError(
            "Convoy interdiction was not recorded in campaign history."
        )

    if int(
        war.get_recent_convoy_interdiction_count(
            destination_id,
            3,
        )
    ) != 1:
        raise RuntimeError(
            "Recent convoy route risk did not track interdiction."
        )

    war.advance_war_turn()
    destination_after_interdiction = war.get_sector_state(
        destination_id
    )

    if float(
        destination_after_interdiction.supply
    ) >= destination_supply_before_interdiction:
        raise RuntimeError(
            "Interdicted cargo reached its destination."
        )

    war.reset_campaign()
    soak_turn_count = 80

    for soak_turn in range(1, soak_turn_count + 1):
        war.advance_war_turn()
        soak_sectors = war.get_sector_states()

        if len(soak_sectors) != 6:
            raise RuntimeError(
                "Campaign soak changed the sector graph on turn %d."
                % soak_turn
            )

        for sector in soak_sectors:
            capacity = int(sector.garrison_capacity)
            friendly_garrison = int(sector.friendly_garrison)
            enemy_garrison = int(sector.enemy_garrison)

            if (
                capacity < 0
                or friendly_garrison < 0
                or enemy_garrison < 0
                or friendly_garrison > capacity
                or enemy_garrison > capacity
                or not 0.0 <= float(sector.supply) <= 100.0
                or not 0.0
                <= float(sector.civilian_support)
                <= 100.0
                or not 0.0
                <= float(sector.intel_confidence)
                <= 100.0
            ):
                raise RuntimeError(
                    "Campaign soak produced invalid state in %s "
                    "on turn %d."
                    % (str(sector.sector_id), soak_turn)
                )

        for faction in (
            unreal.BHWarFaction.FRIENDLY,
            unreal.BHWarFaction.ENEMY,
        ):
            reserve = int(
                war.get_faction_manpower_reserve(faction)
            )
            progress = float(
                war.get_faction_recruitment_progress(faction)
            )

            if (
                reserve < 0
                or reserve > 200
                or progress < 0.0
                or progress >= 1.0
            ):
                raise RuntimeError(
                    "Campaign soak produced invalid manpower state "
                    "on turn %d." % soak_turn
                )

        if (
            war.get_campaign_outcome()
            != unreal.BHWarCampaignOutcome.ONGOING
        ):
            raise RuntimeError(
                "Unattended campaign resolved after only %d turns; "
                "a 40-minute free-roam session must remain playable."
                % soak_turn
            )

    _log("PASS mission outcome captures a sector and advances the war.")
    _log("PASS each strategic location has a finite garrison.")
    _log("PASS battlefield casualties reduce persistent garrisons.")
    _log("PASS supplied rear garrisons reinforce over time.")
    _log("PASS captured sites establish an occupation garrison.")
    _log("PASS reconnaissance reveals estimates that become stale.")
    _log("PASS deterministic frontline simulation advances independently.")
    _log("PASS connected rear sectors resupply both frontlines.")
    _log("PASS cut-off sectors lose supply and reinforcements.")
    _log("PASS low-supply pockets suffer escalating force attrition.")
    _log("PASS command prioritizes a supplied reconnection attack.")
    _log("PASS command priorities explain their strategic intent.")
    _log("PASS active operations stay locked to their sector.")
    _log("PASS background war turns preserve committed battles.")
    _log("PASS controlled logistics hubs produce extra supply.")
    _log("PASS rear-area supply dispatches as in-transit convoys.")
    _log("PASS convoy cargo arrives on the following war turn.")
    _log("PASS convoy interdiction prevents strategic delivery.")
    _log("PASS recent convoy losses increase route-risk history.")
    _log("PASS major war outcomes enter the campaign history.")
    _log("PASS priority operations consume routed staging supply.")
    _log("PASS ambient patrols consume faction source-sector supply.")
    _log("PASS zero-supply sectors cannot generate reinforcements.")
    _log("PASS reported reinforcement rates reflect live supply.")
    _log("PASS low supply reduces projected frontline combat power.")
    _log("PASS strategic priorities use supply-effective strength.")
    _log("PASS multi-sector friendly routes reach rear supply hubs.")
    _log("PASS assigned operations retain target-specific supply routes.")
    _log("PASS routed ambient squads weaken their strategic force.")
    _log("PASS the unattended campaign remains stable for 80 turns.")


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarTypes.h",
        (
            "enum class EBHWarFaction",
            "enum class EBHWarPriorityType",
            'Raid UMETA(DisplayName = "Raid")',
            "enum class EBHRaidOperationalSignature",
            "Clean UMETA(DisplayName = \"Clean\")",
            "Contested UMETA(DisplayName = \"Contested\")",
            "Loud UMETA(DisplayName = \"Loud\")",
            "enum class EBHWarCampaignOutcome",
            "FriendlyVictory",
            "EnemyVictory",
            "enum class EBHWarSiteType",
            "struct BROKENHORIZON_API FBHFieldSquadMemberState",
            "int32 FragGrenades = -1;",
            "bool bIncapacitated = false;",
            "bool bEmbarked = false;",
            "bool bHasWorldTransform = false;",
            "FTransform WorldTransform = FTransform::Identity;",
            "struct BROKENHORIZON_API FBHOpenWorldOperationState",
            "bool bRaidTargetSabotaged = false;",
            "bool bRaidDetectedBeforeSabotage = false;",
            "bool bHasSnapshot = false;",
            "bool bSecuringObjective = false;",
            "bool bFriendlySupportHasCommandLocation = false;",
            "FVector FriendlySupportCommandLocation = FVector::ZeroVector;",
            "float FriendlySupportCommandYaw = 0.0f;",
            "float ObjectiveSecureProgress = 0.0f;",
            "float DefenseBreachProgress = 0.0f;",
            "float SecondsUntilNextWave = 0.0f;",
            "int32 LivingEnemyCount = 0;",
            "int32 FriendlySupportCasualties = 0;",
            "int32 EnemyRoutedCount = 0;",
            "struct BROKENHORIZON_API FBHWarSectorState",
            "EBHWarSiteType SiteType",
            "int32 GarrisonCapacity = 0;",
            "int32 FriendlyGarrison = 0;",
            "int32 EnemyGarrison = 0;",
            "float IntelConfidence = 0.0f;",
            "float CivilianSupport = 50.0f;",
            "EBHWarPriorityType AnticipatedOperationType",
            "int32 RepeatedOperationCount = 0;",
            "TArray<FName> ConnectedSectorIDs;",
            "float ReinforcementRate = 3.0f;",
            "struct BROKENHORIZON_API FBHWarSupplyConvoyState",
            "enum class EBHWarConvoyCargoType : uint8",
            "EBHWarConvoyCargoType CargoType =",
            "EBHWarConvoyCargoType::MilitarySupply;",
            "FName ConvoyID = NAME_None;",
            "FName SourceSectorID = NAME_None;",
            "FName DestinationSectorID = NAME_None;",
            "float SupplyPayload = 0.0f;",
            "int32 TurnsRemaining = 1;",
            "int32 DispatchTurn = 0;",
            "struct BROKENHORIZON_API FBHWarGarrisonTransferState",
            "FName TransferID = NAME_None;",
            "int32 TroopCount = 0;",
            "struct BROKENHORIZON_API FBHWarEventRecord",
            "FName EventType = NAME_None;",
            "FString Summary;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarSubsystem.h",
        (
            "public FTickableGameObject",
            "struct FBHWarStateSnapshot;",
            "FBHWarStateSnapshot CaptureReplicatedSnapshot(",
            "bool ApplyReplicatedSnapshot(",
            "void AdvanceWarTurn();",
            "float SimulationIntervalSeconds = 120.0f;",
            "float CommittedOperationSimulationIntervalSeconds = 180.0f;",
            "float GetCurrentSimulationIntervalSeconds() const;",
            "float GetSecondsUntilNextWarTurn() const;",
            "bool ResolvePriorityMission(bool bFriendlySucceeded);",
            "bool SetCommittedOperation(",
            "void ClearCommittedOperation();",
            "bool HasCommittedOperation() const;",
            "int32 GetSectorGarrisonCount(",
            "bool CanRecruitFieldOperative(FName SectorID) const;",
            "bool RecruitFieldOperative(FName SectorID);",
            "float GetFieldOperativeSupplyCost() const;",
            "int32 GetSectorGarrisonCapacity(",
            "float RecoverBattlefieldMateriel(",
            "float GetSectorIntelConfidence(",
            "float GetSectorCivilianSupport(",
            "bool CanDeliverCivilianAid(",
            "bool DeliverCivilianAid(",
            "float GetCivilianAidSupplyCost() const;",
            "float GetCivilianAidSupportGain() const;",
            "EBHWarCampaignOutcome GetCampaignOutcome() const;",
            "bool IsCampaignResolved() const;",
            "FText GetCampaignOutcomeText() const;",
            "FText GetSectorEnemyIntelSummary(",
            "bool ReportSectorRecon(",
            "bool ReportCivilianSecurityOutcome(",
            "bool CanMobilizeSectorMilitia(",
            "bool MobilizeSectorMilitia(",
            "int32 GetSectorMilitiaMobilizationCount(",
            "float GetSectorMilitiaMobilizationSupplyCost(",
            "FName GetSectorGarrisonRedeploymentSource(",
            "int32 GetSectorGarrisonRedeploymentCount(",
            "float GetSectorGarrisonRedeploymentSupplyCost(",
            "int32 GetSectorGarrisonRedeploymentTurns(",
            "bool CanRedeploySectorGarrison(",
            "bool RedeploySectorGarrison(",
            "GetIncomingGarrisonTransferCount(",
            "GetIncomingGarrisonTransferTurns(",
            "FName GetCommittedOperationSectorID() const;",
            "GetCommittedOperationSupplySourceSectorID() const;",
            "GetCommittedOperationEnemySourceSectorID() const;",
            "bool IsOperationSectorLocked(FName SectorID) const;",
            "GetCommittedOperationType() const;",
            "bool ApplyMissionResult(",
            "bool ApplyOperationCasualtyResult(",
            "EBHRaidOperationalSignature ApplyRaidOperationalSignature(",
            "bool bRaidSucceeded = true",
            "bool bDetectedBeforeSabotage = false",
            "bool ApplyAmbientRoutResult(",
            "bool ApplyOperationRoutResult(",
            "bool ConsumeSectorSupply(",
            "float WithdrawFieldLogisticsSupply(",
            "float DeliverFieldLogisticsSupply(",
            "FName GetRecommendedFieldLogisticsDestination(",
            "FName GetRecommendedFieldCivilianAidDestination(",
            "float WithdrawFieldCivilianAidSupply(",
            "bool DeliverFieldCivilianAidSupply(",
            "float CommitAmbientPatrolSupply(",
            "bool ConsumePriorityOperationSupply();",
            "bool CanFundPriorityOperation() const;",
            "FName GetPriorityOperationSupplySource() const;",
            "FName GetOperationSupplySource(",
            "TArray<FName> GetPriorityOperationSupplyRoute() const;",
            "TArray<FName> GetOperationSupplyRoute(",
            "float GetPriorityOperationSupplyCost() const;",
            "FText GetPriorityReasonText() const;",
            "float GetSectorSupplyChangePerTurn(",
            "float GetSectorIsolationAttritionPerTurn(",
            "bool IsSectorConnectedToFactionLogistics(",
            "float GetSectorCombatSupplyFactor(",
            "float GetSectorEffectiveStrength(",
            "float GetSectorReinforcementPerTurn(",
            "TArray<FBHWarSupplyConvoyState> GetSupplyConvoys() const;",
            "GetGarrisonTransfers() const;",
            "TArray<FBHWarEventRecord> GetRecentWarEvents() const;",
            "int32 GetRecentConvoyInterdictionCount(",
            "bool HasSupplyConvoy(FName ConvoyID) const;",
            "FBHWarSupplyConvoyState GetSupplyConvoyState(",
            "bool InterdictSupplyConvoy(FName ConvoyID);",
            "float GetIncomingConvoySupply(FName SectorID) const;",
            "float GetOutgoingConvoySupply(FName SectorID) const;",
            "bool RestoreWarState(",
            "SavedSupplyConvoys",
            "SavedWarEvents",
            "TArray<FBHWarSupplyConvoyState> SupplyConvoys;",
            "TArray<FBHWarGarrisonTransferState> GarrisonTransfers;",
            "TArray<FBHWarEventRecord> RecentWarEvents;",
            "void AdvanceSupplyConvoys();",
            "void AdvanceGarrisonTransfers();",
            "void ReinforceGarrisons();",
            "void UpdateCivilianSupport();",
            "void EvaluateCampaignOutcome();",
            "void DecaySectorIntelligence();",
            "bool ValidateSupplyConvoy(",
            "bool ValidateGarrisonTransfer(",
            "FBHOnWarStateChanged OnWarStateChanged;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWarSubsystem.cpp",
        (
            '#include "BHWarGameState.h"',
            "GetNetMode() == NM_Client",
            "CaptureReplicatedSnapshot(",
            "ApplyReplicatedSnapshot(",
            'TEXT("WesternFOB")',
            'TEXT("DovrenVillage")',
            'TEXT("NorthPass")',
            'TEXT("KoronaCrossroads")',
            'TEXT("SouthBridge")',
            'TEXT("EasternDepot")',
            "const float SafeDeltaTime = FMath::Max",
            "SimulationAccumulator += SafeDeltaTime;",
            "SimulateFrontlines();",
            "AdvanceSupplyConvoys();",
            "AdvanceGarrisonTransfers();",
            "RedistributeSupplies();",
            "ReinforceGarrisons();",
            "GarrisonSupplyCostPerUnit",
            "MinimumGarrisonReinforcementSupply",
            "SalvageSupplyPerEnemyCasualty",
            "SalvageLossPerFriendlyCasualty",
            "MaximumAmbientSalvageSupply",
            "MaximumOperationSalvageSupply",
            "RaidSuccessCivilianSupport",
            "RaidSuccessEnemyStrengthLoss",
            "RaidSuccessSupplyLoss",
            "RaidSuccessResponsePressure",
            "CleanRaidResponseReduction",
            "CleanRaidCivilianSupportBonus",
            "LoudRaidResponseIncrease",
            "LoudRaidCivilianSupportPenalty",
            "Sector.AnticipatedOperationType = MissionType;",
            "Sector.RepeatedOperationCount = 1;",
            'TEXT("EnemyPatternAdapted")',
            "UBHWarSubsystem::GetSectorEnemyAdaptationSummary(",
            "UBHWarSubsystem::ApplyRaidOperationalSignature(",
            "BH_RAID_OPERATIONAL_SIGNATURE",
            '"signature=%s succeeded=%d "',
            'TEXT("CleanRaidSignature")',
            'TEXT("ContestedRaidSignature")',
            'TEXT("LoudRaidSignature")',
            "BH_GARRISON_REINFORCED",
            "BH_BATTLEFIELD_MATERIEL_RECOVERED",
            "BH_FIELD_LOGISTICS_LOADED",
            "BH_FIELD_LOGISTICS_DELIVERED",
            "UBHWarSubsystem::GetRecommendedFieldLogisticsDestination(",
            "GetRecommendedFieldCivilianAidDestination(",
            "BH_FIELD_CIVILIAN_AID_LOADED",
            "BH_FIELD_CIVILIAN_AID_DELIVERED",
            "IsFrontlineSector(SectorIndex)",
            'TEXT("FieldLogisticsDelivered")',
            'TEXT("BattlefieldMaterielRecovered")',
            'TEXT("LogisticsRaidSucceeded")',
            "BH_LOGISTICS_RAID_SUCCEEDED",
            '"OPERATION BLACKOUT // {0}"',
            '"RaidMissionBriefing"',
            '"unnecessary casualties: a clean raid earns local "',
            '"support and reduces the enemy response; a loud "',
            "BH_GARRISON_SAVE_MIGRATED",
            "BH_INTEL_SAVE_MIGRATED",
            "BH_CIVILIAN_SUPPORT_SAVE_MIGRATED",
            "BH_SECTOR_RECON_UPDATED",
            "UpdateCivilianSupport();",
            "DecaySectorIntelligence();",
            "CivilianIntelContribution",
            "CivilianAidSupplyCost",
            "CivilianAidSupportGain",
            "CivilianAidIntelGain",
            "CivilianAidHostileExposurePressure",
            "UndergroundRecruitmentFactor",
            "NeutralRecruitmentFactor",
            "BH_CIVILIAN_AID_DELIVERED",
            "BH_CIVILIAN_AID_DISPATCHED",
            'TEXT("CivilianAidDispatched")',
            'TEXT("CivilianAidDelivered")',
            "EBHWarConvoyCargoType::CivilianAid",
            "BH_CIVILIAN_SECURITY_OUTCOME",
            "BH_MILITIA_MOBILIZED",
            'TEXT("MilitiaMobilized")',
            "MilitiaSupportCostPerUnit",
            "MilitiaSupplyCostPerUnit",
            "MilitiaStrengthPerUnit",
            "MilitiaResponsePressurePerUnit",
            "GarrisonRedeploymentSupplyCostPerUnit",
            "BH_GARRISON_REDEPLOYED",
            'TEXT("GarrisonTransferDispatched")',
            "BH_GARRISON_TRANSFER_ARRIVED",
            'TEXT("GarrisonTransferArrived")',
            "CounterinsurgencyResponseThreshold",
            "CounterinsurgencySuccessResponseReduction",
            "CounterinsurgencyDefenseSector",
            "bCounterinsurgencyThreat",
            '"ENEMY SWEEP DETECTED"',
            '"OPERATION SAFEHOUSE // {0}"',
            '"Enemy security forces are sweeping {0} for militia "',
            'TEXT("CounterinsurgencyBroken")',
            "BH_COUNTERINSURGENCY_BROKEN",
            "BH_CAMPAIGN_RESOLVED",
            "The strategic simulation must not end the campaign while the",
            "if (HasCommittedOperation())",
            'TEXT("CampaignVictory")',
            'TEXT("CampaignDefeat")',
            "bAllSectorsFriendly",
            "bFriendlyHeadquartersLost",
            'TEXT("CivilianSupportShifted")',
            "Sector.FriendlyGarrison -= SafeFriendlyCasualties;",
            "Sector.EnemyGarrison -= SafeEnemyCasualties;",
            "BH_WAR_CONVOY_DISPATCHED",
            "BH_WAR_CONVOY_ARRIVED",
            "BH_WAR_CONVOY_LOST",
            "BH_WAR_CONVOY_INTERDICTED",
            "GetRecentConvoyInterdictionCount(",
            "BH_WAR_EVENT",
            'TEXT("SectorCaptured")',
            'TEXT("SectorLost")',
            'TEXT("OperationSucceeded")',
            'TEXT("OperationFailed")',
            "BH_WAR_CONVOY_SAVE_DROPPED",
            "SupplyConvoys.RemoveAtSwap(ConvoyIndex);",
            "Convoy.TurnsRemaining",
            "SupplyConvoys.Add(Convoy);",
            "BH_WAR_SUPPLY_PRODUCTION",
            "BH_OPERATION_SUPPLY_COMMITTED",
            "BH_SECTOR_RESUPPLY_CONSUMED",
            "AmbientStrengthLossPerRoutedUnit",
            "AmbientSupplyCostPerRoutedUnit",
            "BH_AMBIENT_ROUT_RESOLVED",
            "BH_OPERATION_CASUALTIES_APPLIED",
            'TEXT("OperationCasualtiesApplied")',
            "BH_OPERATION_ROUT_APPLIED",
            'TEXT("EnemyOperationForcesRouted")',
            "BH_AMBIENT_PATROL_SUPPLY_COMMITTED",
            "SafeRequestedSupply",
            "ForceStrength",
            "MinimumSuppliedReinforcementFactor",
            "MaximumReinforcementSupplyFactor",
            "IsolatedSupplyAttritionPerTurn",
            "IsolatedAttritionSupplyThreshold",
            "MinimumIsolationStrengthAttrition",
            "MaximumIsolationStrengthAttrition",
            "BH_WAR_ISOLATION_ATTRITION",
            "BH_WAR_OPERATION_COMMITTED",
            "BH_WAR_OPERATION_COMMIT_REJECTED",
            "BH_WAR_OPERATION_RELEASED",
            "const bool bMatchesCurrentOperation =",
            "BH_OCCUPATION_GARRISON_TRANSFERRED",
            "BH_ENEMY_OCCUPATION_GARRISON_TRANSFERRED",
            "BH_CAPTURE_UNGARRISONED",
            "BH_ENEMY_CAPTURE_UNGARRISONED",
            'TEXT("OccupationForceDeployed")',
            'TEXT("EnemyOccupationForceDeployed")',
            "SupplySource.FriendlyGarrison -=",
            "EnemySource.EnemyGarrison -=",
            '"OPERATION IN PROGRESS"',
            "IsCommittedOperationSector(",
            "CommittedOperationSectorID",
            "CommittedOperationSupplySourceSectorID",
            "CommittedOperationEnemySourceSectorID",
            "CommittedOperationType",
            "ReconnectionAttackBonus",
            "bTouchesConnectedFriendly",
            "bTouchesIsolatedFriendly",
            "BestReconnectionAttackSector",
            '"RESTORE ISOLATED SECTORS"',
            '"BREAKTHROUGH THREAT"',
            '"FRONTLINE UNDER PRESSURE"',
            '"EXPAND FRIENDLY CONTROL"',
            "MinimumCombatSupplyFactor",
            "MaximumCombatSupplyFactor",
            "GarrisonStrengthPerUnit",
            "InitialFriendlyManpowerReserve",
            "InitialEnemyManpowerReserve",
            "MaximumFactionManpowerReserve",
            "FieldOperativeManpowerCost",
            "FieldOperativeSupplyCost",
            "BH_FIELD_OPERATIVE_RECRUITED",
            'TEXT("FieldOperativeRecruited")',
            "GetSiteRecruitmentWeight(",
            "RecruitFactionManpower();",
            "void UBHWarSubsystem::RecruitFactionManpower()",
            "BH_MANPOWER_RECRUITED",
            "ManpowerReserve -= ReinforcedTroops;",
            "CalculateCombatSupplyFactor(",
            "CalculateReinforcementAmount(",
            "VisitedIndices",
            "GetSectorEffectiveStrength(",
            "Garrison * GarrisonStrengthPerUnit",
            "MinimumOccupationGarrison",
            "DesiredOccupationGarrison",
            "OccupationGarrison",
            "SupplySource.FriendlyGarrison",
            "EnemySource.EnemyGarrison",
            "EffectiveFriendlyStrength",
            "EffectiveEnemyStrength",
            "NormalizedSupply",
            "Sector.Supply <= KINDA_SMALL_NUMBER",
            "BH_WAR_REINFORCEMENT_STARVED",
            "FindPriorityOperationSupplySourceIndex() const",
            "BuildOperationSupplyRouteIndices(",
            "ResolveContestedSectors(RandomStream);",
            "RecalculatePriority();",
            "BH_WAR_SAVE_MIGRATED",
            "Sector.Owner = EBHWarFaction::Friendly;",
            "Sector.Owner = EBHWarFaction::Enemy;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarGameState.h",
        (
            "struct BROKENHORIZON_API FBHWarStateSnapshot",
            "bool bInitialized = false;",
            "TArray<FBHWarSectorState> SectorStates;",
            "TArray<FBHWarSupplyConvoyState> SupplyConvoys;",
            "TArray<FBHWarGarrisonTransferState> GarrisonTransfers;",
            "TArray<FBHWarEventRecord> RecentWarEvents;",
            "class BROKENHORIZON_API ABHWarGameState",
            "GetLifetimeReplicatedProps(",
            "ReplicatedUsing = OnRep_WarStateSnapshot",
            "void PublishAuthoritativeSnapshot();",
            "void ApplyReplicatedSnapshot();",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWarGameState.cpp",
        (
            '#include "Net/UnrealNetwork.h"',
            "DOREPLIFETIME_CONDITION_NOTIFY(",
            "WarSubsystem->CaptureReplicatedSnapshot(",
            "WarSubsystem->ApplyReplicatedSnapshot(",
            "ForceNetUpdate();",
            '"BH_WAR_GAME_STATE_READY revision=%d sectors=%d"',
            '"BH_WAR_SNAPSHOT_PUBLISHED revision=%d turn=%d "',
            '"BH_WAR_SNAPSHOT_APPLIED revision=%d turn=%d "',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHGameMode.cpp",
        (
            '#include "BHWarGameState.h"',
            "GameStateClass = ABHWarGameState::StaticClass();",
            "BH_WAR_GAME_STATE_MISSING",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSaveSubsystem.h",
        (
            "bool IsClientCampaignWorld(const UWorld* World) const;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp",
        (
            "World->GetNetMode() == NM_Client",
            "BH_CAMPAIGN_BOOTSTRAP_SKIPPED_CLIENT",
            "BH_CAMPAIGN_SAVE_REJECTED_CLIENT",
            "BH_PLAYER_RESOURCE_SAVE_REJECTED_CLIENT",
            "BH_CAMPAIGN_LOAD_REJECTED_CLIENT",
            "BH_CHECKPOINT_RELOAD_REJECTED_CLIENT",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSaveGame.h",
        (
            "CurrentSchemaVersion = 37;",
            "FName CargoDestinationSectorID = NAME_None;",
            "bool bCampaignEpilogueAcknowledged = false;",
            "bool bOperationDebriefAcknowledged = false;",
            "bool bMissionFailed = false;",
            "FName AssignedWarSupplySourceSectorID = NAME_None;",
            "int32 SavedFragGrenadeCount = -1;",
            "FBHOpenWorldOperationState OpenWorldOperationState;",
            "int32 LivingFieldSquadCount = 0;",
            "TArray<FBHFieldSquadMemberState> FieldSquadMemberStates;",
            "bool bFieldSquadHolding = false;",
            "bool bFieldSquadHasCommandLocation = false;",
            "FVector FieldSquadCommandLocation = FVector::ZeroVector;",
            "float FieldSquadCommandYaw = 0.0f;",
            "bool bFieldSquadEmbarked = false;",
            "FName FieldSquadTransportPersistenceID = NAME_None;",
            "float SavedHealth = -1.0f;",
            "bool bHasSavedInjuryState = false;",
            "float SavedBleedRate = 0.0f;",
            "TArray<FBHWarSectorState> WarSectorStates;",
            "TArray<FBHWarSupplyConvoyState> WarSupplyConvoys;",
            "TArray<FBHWarGarrisonTransferState> WarGarrisonTransfers;",
            "int32 WarFriendlyManpowerReserve = 0;",
            "int32 WarEnemyManpowerReserve = 0;",
            "float WarFriendlyRecruitmentProgress = 0.0f;",
            "float WarEnemyRecruitmentProgress = 0.0f;",
            "TArray<FBHWarEventRecord> WarEventHistory;",
            "int32 WarTurnNumber = 0;",
            "float WarSimulationAccumulator = 0.0f;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHObjectiveComponent.h",
        (
            "bool FailMission();",
            "void ClearMissionState();",
            "bool IsMissionFailed() const;",
            "bool bMissionFailed = false;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHObjectiveComponent.cpp",
        (
            "bool UBHObjectiveComponent::FailMission()",
            "bMissionFailed = true;",
            "if (bSavedMissionFailed)",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSaveSubsystem.h",
        (
            "bool ReloadCheckpointAfterPlayerDeath(",
            "FName PendingPlayerDeathAttritionSectorID = NAME_None;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp",
        (
            "Collection.InitializeDependency<UBHWarSubsystem>();",
            "CaptureWarState(SaveData, GetGameInstance());",
            "Character->GetOpenWorldOperationState();",
            "Character->IsMissionFailed();",
            "SaveData->bMissionFailed,",
            "SaveData->OpenWorldOperationState,",
            "Character->GetLivingFieldSquadCount();",
            "Character->GetFieldSquadMemberStates();",
            "Character->IsFieldSquadHolding();",
            "Character->HasFieldSquadCommandLocation();",
            "Character->GetFieldSquadCommandLocation();",
            "Character->GetFieldSquadCommandYaw();",
            "Character->IsFieldSquadEmbarked();",
            "Character->GetFieldSquadTransportPersistenceID();",
            "SaveData->FieldSquadMemberStates,",
            "SaveData->bFieldSquadHasCommandLocation",
            "SaveData->FieldSquadCommandLocation",
            "SaveData->FieldSquadCommandYaw",
            "SaveData->bFieldSquadEmbarked",
            "SaveData->FieldSquadTransportPersistenceID",
            "bRestoreFieldSquadPassengers",
            "ShouldRestoreFieldSquadPassengers(",
            "Character->RestoreFieldSquadState(",
            "HealthComponent->GetCurrentHealth();",
            "HealthComponent->RestorePersistentHealthState(",
            "InjuryComponent->RestorePersistentInjuryState(",
            "ClearPendingWarAutosave(World);",
            "BH_WAR_AUTOSAVE_COALESCED",
            "BH_INITIAL_ITEM_CHECKPOINT",
            "WarSubsystem->RestoreWarState(",
            "SaveData->WarSupplyConvoys",
            "ExistingSave->WarSupplyConvoys",
            "SaveData->WarGarrisonTransfers",
            "ExistingSave->WarGarrisonTransfers",
            "RestoreGarrisonTransfers(",
            "SaveData->WarFriendlyManpowerReserve",
            "SaveData->WarEnemyManpowerReserve",
            "GetFactionManpowerReserve(",
            "GetFactionRecruitmentProgress(",
            "RestoreManpowerState(",
            "SaveData->WarEventHistory",
            "ExistingSave->WarEventHistory",
            "WarSubsystem->ResetCampaign();",
            "bool UBHSaveSubsystem::ReloadCheckpointAfterPlayerDeath(",
            "PendingPlayerDeathAttritionSectorID = CasualtySectorID;",
            "WarSubsystem->ApplyAmbientBattleResult(",
            "BH_PLAYER_DEATH_ATTRITION sector=%s",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        (
            "SetReplicates(true);",
            "bAlwaysRelevant = true;",
            "GetLivingPlayerParticipants() const",
            "GetPlayerParticipants() const",
            "FindLivingPlayerParticipant() const",
            "GetClosestParticipantDistanceToOperation() const",
            "AreAllLivingParticipantsOutsideRadius(",
            "BH_OPERATION_COMMAND_PLAYER_REASSIGNED",
            "AdoptSharedWarOperationAuthority(",
            "for (ABHCharacter* Participant : GetLivingPlayerParticipants())",
            "BH_OPERATION_COMMAND_HANDOFF_REJECTED",
            "BH_OPERATION_COMPLETION_REJECTED_NO_COMMAND_PLAYER",
            "BH_OPERATION_COMPLETION_OBJECTIVE_REJECTED",
            "BH_OPERATION_FAILURE_REJECTED_NO_COMMAND_PLAYER",
            "Candidate->IsPlayerControlled()",
            "if (!HasAuthority() ||",
            "CaptureSaveState() const",
            "RestoreOperationState(",
            "State.LivingEnemyCount = GetLivingEnemyCount();",
            "State.LivingAllyCount = GetLivingAllyCount();",
            "State.EnemyRoutedCount = EnemyRoutedCount;",
            "State.bSecuringObjective = bSecuringObjective;",
            "State.bRaidTargetSabotaged = bRaidTargetSabotaged;",
            "State.bRaidDetectedBeforeSabotage =",
            "State.ObjectiveSecureProgress =",
            "State.DefenseBreachProgress =",
            "SavedState.SecondsUntilNextWave",
            "SavedState.bSecuringObjective",
            "SavedState.bRaidTargetSabotaged",
            "SavedState.bRaidDetectedBeforeSabotage",
            "SavedState.ObjectiveSecureProgress",
            "SavedState.DefenseBreachProgress",
            "SavedState.EnemyRoutedCount",
            "SpawnFriendlySupport(LivingAllyCount);",
            "SpawnEnemies(LivingEnemyCount);",
            "FriendlyController->SetFollowTarget(",
            "CalculateFriendlyFormationOffset(Index)",
            "SavedState.bFriendlySupportHolding",
            "SavedState.bFriendlySupportHasCommandLocation",
            "SavedState.FriendlySupportCommandLocation",
            "SavedState.FriendlySupportCommandYaw",
            "ToggleFriendlySupportHoldOrder()",
            "SetFriendlySupportMoveAndHoldOrder(",
            "SetFriendlySupportFollowOrder()",
            "State.bFriendlySupportHasCommandLocation =",
            "Controller->SetHoldPosition(",
            "ApplyFriendlySupportOrder();",
            "BH_OPERATION_RESTORED",
            "RequestOperationCheckpoint();",
            "RequestOperationCheckpoint(CasualtyCheckpointDelay);",
            "OperationCheckpointTimerHandle",
            "SaveSubsystem->SaveProgress()",
            "BH_OPERATION_CHECKPOINT",
            '"OpenWorldOperationRaidLabel"',
            "ClassifyRaidOperationalSignature(",
            "UpdateRaidDetectionState();",
            "UpdateEnemyRouts();",
            "BH_OPERATION_ENEMY_ROUTED",
            "BH_OPERATION_UNIT_CONTROLLER_RECOVERED",
            "BH_OPERATION_UNIT_REJECTED",
            "BH_OPERATION_UNIT_REPOSITIONED",
            "BH_OPERATION_UNIT_SPAWN_EXHAUSTED",
            "BH_OPERATION_FORCE_SHORTFALL",
            "MaximumOperationSpawnAttempts",
            "BuildSpawnTransform(Index, AttemptIndex)",
            "BuildFriendlySpawnTransform(",
            "AdjustIfPossibleButDontSpawnIfColliding",
            "Enemy->SpawnDefaultController();",
            "CalculateRaidReactionForceCount(",
            "IsEnemyRoutedFromOperation(",
            "WasRaidDetectedBeforeSabotage() const",
            '"OpenWorldRaidCleanSignatureLabel"',
            '"OpenWorldRaidContestedSignatureLabel"',
            '"OpenWorldRaidLoudSignatureLabel"',
            '"SIGNATURE {1} // EXFIL {2} M // "',
            '"SIGNATURE {1} // HOSTILES {2} // SUPPORT {3}/{4}"',
            '"OPEN-WORLD RAID ACTIVE\\n\\n"',
            "SABOTAGE LOGISTICS CACHE",
            '"OpenWorldRaidDisruptObjective"',
            "SpawnRaidSabotageTarget();",
            "ABHRaidSabotageTarget::StaticClass()",
            "BH_RAID_TARGET_SPAWNED",
            "BH_RAID_EXFILTRATION_STARTED",
            "BH_RAID_EXFILTRATION_COMPLETE",
            "BH_RAID_REACTION_FORCE_ALERTED",
            "EnemyController->NotifyAllyAlert(PlayerCharacter);",
            "SpawnEnemies(ReactionForceCount);",
            "RaidExfiltrationRadius",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHRaidSabotageTarget.h",
        (
            "public IBHInteractable",
            "void ConfigureTarget(",
            "bool IsSabotaged() const;",
            "TObjectPtr<ABHOpenWorldOperationDirector>",
            "bool bSabotaged = false;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHRaidSabotageTarget.cpp",
        (
            "Plant Demolition Charges",
            "BH_RAID_TARGET_SABOTAGED",
            "HandleRaidTargetSabotaged(this)",
            "SetActorEnableCollision(false)",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "SetReplicates(true);",
            "SetReplicateMovement(true);",
            "!ObjectiveComponent->IsMissionComplete()",
            "BH_OBJECTIVE_CHECKPOINT objective=%s",
            "OnTreatmentCompleted.AddDynamic(",
            "SavePlayerConditionCheckpoint(TEXT(\"FieldDressing\"));",
            "BH_PLAYER_CONDITION_CHECKPOINT reason=%s",
            "SaveSubsystem->SaveProgress()",
            "SaveSubsystem->ReloadCheckpointAfterPlayerDeath(",
            "World->GetNetMode() != NM_Standalone",
            "BH_MULTIPLAYER_FIELD_RESPAWN sector=%s",
            "ClientCompleteFieldRespawn();",
            "SaveSubsystem->SaveProgress();",
            "SynchronizeReplicatedOperationPresentation();",
            "GetActiveOperationSnapshot();",
            "SHARED OPERATION ASSIGNED",
            "SHARED OPERATION ACTIVE",
            "SHARED OPERATION COMPLETE",
            "SHARED OPERATION FAILED",
            "AssignedWarSupplySourceSectorID.IsNone()",
            "bool ABHCharacter::TryRecruitFieldSquadMember()",
            "bool ABHCharacter::SpawnFieldSquadMember(",
            "void ABHCharacter::ApplyFieldSquadOrder()",
            "bool ABHCharacter::RestoreFieldSquadState(",
            "bool ABHCharacter::HasFieldSquadCommandLocation() const",
            "FVector ABHCharacter::GetFieldSquadCommandLocation() const",
            "float ABHCharacter::GetFieldSquadCommandYaw() const",
            "FName ABHCharacter::GetFieldSquadTransportPersistenceID() const",
            "GetFieldSquadMemberStates() const",
            "RestorePersistentCombatState(",
            "MemberState.FragGrenades =",
            "MemberState.bIncapacitated =",
            "MemberState.bEmbarked = bMemberEmbarked;",
            "MemberState.bHasWorldTransform =",
            "MemberState.WorldTransform =",
            "MemberState.IncapacitationSecondsRemaining =",
            "SavedMember.FragGrenades",
            "SavedMember.bIncapacitated",
            "SavedMember.bEmbarked",
            "SavedMember.bHasWorldTransform",
            "PendingFieldSquadTransportPassengers",
            "bUseSavedPassengerManifest",
            "SavedMember.WorldTransform",
            "SavedMember.IncapacitationSecondsRemaining",
            "RestoreIncapacitatedStateWithRemainingTime(",
            "SetCasualtyWaypoint(",
            "TryStabilizeFieldSquadMember(",
            "IsSharedFieldSquadMember(",
            "FindFieldSquadOwner(",
            "Candidate->FieldSquadMembers.Contains(SquadMember)",
            "SquadOwner->ApplyFieldSquadOrder();",
            "ConsumeFieldDressingForSquadAid()",
            "BH_FIELD_OPERATIVE_AID",
            "HandleFieldSquadMemberCasualtyExpired(",
            "BH_FIELD_OPERATIVE_LOST",
            "IsFieldSquadMemberTransportEligible(",
            "Member->StabilizeIncapacitatedSoldier()",
            "bStabilizedCasualty",
            "SquadAIController->SetFollowTarget(",
            "SquadAIController->SetHoldPosition(",
            "ServerInteract_Implementation(",
            "ResolveInteractionTarget(",
            "ExecuteInteraction(",
            "ClientShowStatusNotification_Implementation(",
            "ClientShowHitConfirmation_Implementation(",
            "if (HasAuthority())",
            "ServerInteract(TargetActor);",
        ),
    )
    _reject_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "SaveSubsystem->SaveProgressForCharacter(this);",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.h",
        (
            "void ServerInteract(AActor* RequestedTarget);",
            "void ClientShowStatusNotification(const FText& Message);",
            "void ClientShowHitConfirmation(",
            "bool ResolveInteractionTarget(AActor*& OutTarget);",
            "void ExecuteInteraction(AActor* TargetActor);",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHHealthComponent.h",
        (
            "GetLifetimeReplicatedProps(",
            "ReplicatedUsing = OnRep_CurrentHealth",
            "void OnRep_CurrentHealth(float PreviousHealth);",
            "bool HasMutationAuthority() const;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHHealthComponent.cpp",
        (
            '#include "Net/UnrealNetwork.h"',
            "SetIsReplicatedByDefault(true);",
            "DOREPLIFETIME(",
            "if (!HasMutationAuthority())",
            "Owner->HasAuthority()",
            "OnRep_CurrentHealth(",
            "RestorePersistentHealthState(",
            "SavedHealth,",
            "bIsDead = false;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWeaponComponent.h",
        (
            "GetLifetimeReplicatedProps(",
            "ReplicatedUsing = OnRep_Ammo",
            "ReplicatedUsing = OnRep_WeaponState",
            "ReplicatedUsing = OnRep_IsAiming",
            "void ServerStartFiring();",
            "void ServerStopFiring();",
            "void ServerStartReload();",
            "void ServerToggleFireMode();",
            "void ServerSetAiming(bool bNewIsAiming);",
            "void ServerStopAllActions();",
            "void MulticastFirePresentation(",
            "void ClientDryFirePresentation();",
            "bool HasWeaponAuthority() const;",
            "void TryPredictedFire();",
            'FName ThirdPersonWeaponSocketName = TEXT("hand_r");',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWeaponComponent.cpp",
        (
            '#include "Net/UnrealNetwork.h"',
            "SetIsReplicatedByDefault(true);",
            "DOREPLIFETIME_CONDITION_NOTIFY(",
            "COND_OwnerOnly",
            "REPNOTIFY_Always",
            "ServerStartFiring_Implementation()",
            "ServerStartReload_Implementation()",
            "ServerSetAiming_Implementation(",
            "ServerStopAllActions_Implementation()",
            "MulticastFirePresentation_Implementation(",
            "Owner->HasAuthority()",
            "if (!HasWeaponAuthority())",
            "MulticastFirePresentation(ShotHit, bHadBlockingHit);",
            "void UBHWeaponComponent::TryPredictedFire()",
            "PlayPredictedFirePresentation();",
            "PlayReplicatedImpactPresentation(",
            "CharacterOwner->GetMesh();",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHRifle.cpp",
        (
            "GetNetMode() != NM_DedicatedServer",
            "WeaponOwner->HasAuthority()",
            "PlayReplicatedFirePresentation(",
            "PlayPredictedFirePresentation()",
            "PlayReplicatedImpactPresentation(",
            "SetFirstPersonPresentation(bool bFirstPerson)",
            "PlayImpactPresentation(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHDoor.cpp",
        (
            '#include "Net/UnrealNetwork.h"',
            "SetReplicates(true);",
            "DOREPLIFETIME(ABHDoor, bIsOpen);",
            "DOREPLIFETIME(ABHDoor, bLocked);",
            "DOREPLIFETIME(ABHDoor, TargetOpenRotation);",
            "if (!HasAuthority())",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHKeycard.cpp",
        (
            "SetReplicates(true);",
            "if (!HasAuthority())",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSupplyBase.cpp",
        (
            '#include "Net/UnrealNetwork.h"',
            "SetReplicates(true);",
            "DOREPLIFETIME(ABHSupplyBase, bConsumed);",
            "if (!HasAuthority())",
            "void ABHSupplyBase::OnRep_Consumed()",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHFieldTransport.cpp",
        (
            "SetReplicates(true);",
            "SetReplicateMovement(true);",
            "if (!HasAuthority())",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHInjuryComponent.cpp",
        (
            "RestorePersistentInjuryState(",
            "CurrentBleedRate = FMath::Max",
            "bArmInjured = bSavedArmInjured;",
            "bLegInjured = bSavedLegInjured;",
            "OnTreatmentCompleted.Broadcast();",
            "ConsumeFieldDressingForSquadAid()",
            "--FieldDressingCount;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSupplyConvoyTarget.h",
        (
            "class BROKENHORIZON_API ABHSupplyConvoyTarget",
            "void ConfigureConvoy(",
            "void SetTravelDestination(",
            "void SetTravelRoute(",
            "FBHWarSupplyConvoyState",
            "TObjectPtr<UBHHealthComponent> HealthComponent;",
            "TObjectPtr<ABHWorldRoute> TravelRoute;",
            "float MovementSpeed = 450.0f;",
            "float ArrivalRadius = 300.0f;",
            "float CurrentRouteDistance = 0.0f;",
            "float DestinationRouteDistance = 0.0f;",
            "float RouteTravelDirection = 1.0f;",
            "float GetHealthPercentage() const;",
            "EBHWarFaction GetConvoyOwner() const;",
            "bool IsResolved() const;",
            "void HandleConvoyDestroyed(AActor* DamageCauser);",
            "void MoveAlongRoute(float DeltaSeconds);",
            "void ResolveLocalRouteExit();",
            "void NotifyPlayer(const FText& Message) const;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSupplyConvoyTarget.cpp",
        (
            "CreateDefaultSubobject<UBHHealthComponent>",
            '#include "BHWorldRoute.h"',
            "WarSubsystem->HasSupplyConvoy(ConvoyID)",
            "WarSubsystem->InterdictSupplyConvoy(ConvoyID)",
            "SaveSubsystem->SaveProgress()",
            "MoveAlongRoute(DeltaSeconds);",
            "BH_CONVOY_ROUTE_ASSIGNED",
            "TravelRoute->GetWorldLocationAtDistance(",
            "TravelRoute->GetWorldDirectionAtDistance(",
            "ResolveLocalRouteExit();",
            "ProjectPointToNavigation(",
            "BH_CONVOY_TARGET_CLEARED",
            '"CONVOY ESCAPED\\n\\n"',
            '"CONVOY CONTACT ENDED\\n\\n"',
            "PlayerCharacter->ShowStatusNotification(Message);",
            "PlayerCharacter->ShowStatusNotification(",
            "BH_CONVOY_TARGET_EXPIRED",
            "BH_CONVOY_TARGET_DESTROYED",
            '"CONVOY INTERDICTED\\n\\n"',
            '"FRIENDLY CONVOY LOST\\n\\n"',
            '"FRIENDLY SUPPLY CONVOY\\n{0} SUPPLY"',
            '"FRIENDLY CONVOY SECURED\\n\\n"',
            'TEXT("BH_Friendly")',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHEnemySoldier.cpp",
        (
            "SetReplicates(true);",
            "SetReplicateMovement(true);",
            "DOREPLIFETIME(ABHEnemySoldier, CombatFaction);",
            "DOREPLIFETIME(ABHEnemySoldier, bIncapacitated);",
            "IncapacitationSecondsRemaining",
            "void ABHEnemySoldier::OnRep_CombatFaction()",
            "void ABHEnemySoldier::OnRep_Incapacitated()",
            "MulticastRestoreFromIncapacitation();",
            '#include "BHSupplyConvoyTarget.h"',
            "Cast<ABHSupplyConvoyTarget>(OtherActor)",
            "ConvoyTarget->GetConvoyOwner() ==",
            "EBHWarFaction::Friendly",
            "EBHWarFaction::Enemy",
            "ConvoyTarget->IsResolved()",
            "EnterFriendlyIncapacitation(",
            "StabilizeIncapacitatedSoldier()",
            "RestoreIncapacitatedState()",
            "RestoreIncapacitatedStateWithRemainingTime(",
            "StartIncapacitationTimer()",
            "BH_FIELD_OPERATIVE_INCAPACITATED",
            "BH_FIELD_OPERATIVE_STABILIZED",
            "BH_FIELD_OPERATIVE_EXPIRED",
            "OnFriendlyCasualtyExpired.Broadcast(this)",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHEnemyAIController.cpp",
        (
            '#include "BHSupplyConvoyTarget.h"',
            "Cast<ABHSupplyConvoyTarget>(Candidate)",
            "TActorIterator<ABHSupplyConvoyTarget>",
            "NewTarget->IsA<ABHSupplyConvoyTarget>()",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHCombatStatusWidget.h",
        (
            "void SetCasualtyWaypoint(",
            "bool bCasualtyWaypointVisible = false;",
            "CasualtyWaypointRecoverySecondsRemaining",
            "void SetConvoyWaypoint(",
            "bool bConvoyWaypointVisible = false;",
            "float ConvoySupplyPayload = 0.0f;",
            "void SetStrategicSituation(",
            "void SetCivilianSupport(",
            "void SetFieldReconStatus(",
            "bool bStrategicSituationVisible = false;",
            "bool bFieldReconActive = false;",
            "EBHWarFaction StrategicSectorOwner",
            "float StrategicCivilianSupport = 50.0f;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp",
        (
            "int32 DrawConvoyWaypoint(",
            "FRIENDLY CONVOY // %.0f SUPPLY //",
            "HOSTILE CONVOY // %.0f SUPPLY //",
            "%d%% INTEGRITY // %s // DEFEND",
            "%d%% INTEGRITY // %s // INTERDICT",
            "ConvoyWaypointOwner",
            "ConvoyIntegrityPercentage",
            "ConvoyDirectionAngleRadians",
            "ConvoyDistanceCentimeters",
            "void UBHCombatStatusWidget::SetStrategicSituation(",
            "void UBHCombatStatusWidget::SetCivilianSupport(",
            "void UBHCombatStatusWidget::SetFieldReconStatus(",
            '"LOCAL AO // %s // T%03d\\n"',
            '"INTEL %.0f%% // RECON M %.0f/%.0fM "',
            '"// OBS %.0f/%.0fS"',
            '"INTEL %.0f%% // REPORT READY %.0fS"',
            "StrategicSupplyFlowPerTurn",
            "StrategicSectorSupply / 100.0f",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            '#include "BHSupplyConvoyTarget.h"',
            "UpdateConvoyWaypointHUD(DeltaTime);",
            "TActorIterator<ABHSupplyConvoyTarget>",
            "NearestTarget->GetSupplyPayload()",
            "NearestTarget->GetHealthPercentage()",
            "NearestTarget->GetConvoyOwner()",
            "CombatStatusWidget->SetConvoyWaypoint(",
            "UpdateStrategicSituationHUD(DeltaTime);",
            "AmbientWarDirector->GetPlayerSectorID()",
            "CombatStatusWidget->SetStrategicSituation(",
            "AmbientWarDirector->GetFieldReconStatus(",
            "CombatStatusWidget->SetFieldReconStatus(",
            '"BH_FIELD_SITUATION sector=%s owner=%d "',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarSubsystem.h",
        (
            "FName GetOperationEnemySource(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWarSubsystem.cpp",
        (
            "FName UBHWarSubsystem::GetOperationEnemySource(",
            "CurrentSector.Owner == EBHWarFaction::Enemy",
            "BestSourceIndex",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHAmbientWarDirector.h",
        (
            "bool ResolveForceSourceSector(",
            "FName GetPlayerSectorID() const;",
            "bool GetFieldReconStatus(",
            "EBHWarFaction ForceFaction,",
            "int32* OutHopCount = nullptr",
            "int32 MaxForceProjectionHops = 3;",
            "float PatrolSupplyCostPerMember = 0.35f;",
            "float PatrolProjectionSupplyCostPerHop = 0.15f;",
            "float SectorEntryIntelGain = 8.0f;",
            "float FieldReconIntelGain = 35.0f;",
            "float FieldReconObservationDuration = 18.0f;",
            "float FieldReconMovementRequired = 2500.0f;",
            "float FieldReconReportCooldown = 45.0f;",
            "void UpdateFieldRecon(",
            "float CalculatePatrolSupplyCost(",
            "TSubclassOf<ABHSupplyConvoyTarget>",
            "float ConvoyOpportunitySpawnDistance = 6000.0f;",
            "float ConvoyRouteConnectionTolerance = 150000.0f;",
            "int32 ConvoyEscortCount = 2;",
            "int32 ConvoyRouteSecurityTurnWindow = 3;",
            "int32 MaximumConvoyRouteSecurityBonus = 2;",
            "float ConvoyEscortSpacing = 350.0f;",
            "float ConvoyEscortWithdrawalDelay = 12.0f;",
            "void SpawnSupplyConvoyEscorts(",
            "void SpawnFriendlySupplyConvoyDefenders(",
            "int32 RequestedCombatantCount",
            "int32 CalculateConvoyCombatantCount(",
            "EBHWarFaction CombatantFaction",
            "void UpdateSupplyConvoyEscorts();",
            "void CleanupSupplyConvoyEscorts();",
            "void ReportSupplyConvoyEscortCasualties(",
            "void ReportSupplyConvoyDefenderCasualties(",
            "void UpdateSupplyConvoyOpportunity(",
            "ABHWorldRoute* FindBestWorldRoute(",
            "ActiveSupplyConvoyTarget",
            "TSet<FName> PresentedSupplyConvoyIDs;",
            "ConvoyEscorts;",
            "ConvoyDefenders;",
            "ConvoyEscortPatrolPoints;",
            "FName ActiveConvoySourceSectorID = NAME_None;",
            "FName ActiveConvoyDefenderSourceSectorID = NAME_None;",
            "FName ActiveFriendlyForceSectorID = NAME_None;",
            "FName ActiveEnemyForceSectorID = NAME_None;",
            "int32 ActiveFriendlySourceHops = INDEX_NONE;",
            "int32 ActiveEnemySourceHops = INDEX_NONE;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSectorAnchor.h",
        (
            "TObjectPtr<UTextRenderComponent> SectorStatusLabel;",
            "float SectorStatusCullDistance = 30000.0f;",
            "void HandleWarStateChanged(",
            "void RefreshWarStatus();",
            "void FaceStatusTowardPlayer();",
            "EBHWarFaction CachedOwner",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSectorAnchor.cpp",
        (
            '#include "BHWarSubsystem.h"',
            "CreateDefaultSubobject<UTextRenderComponent>",
            "SectorStatusLabel->SetCullDistance(",
            "WarSubsystem->OnWarStateChanged.AddDynamic(",
            "WarSubsystem->OnWarStateChanged.RemoveDynamic(",
            "WarSubsystem->GetPrioritySectorID() == SectorID",
            '"F-GARRISON %d / %d\\n%s"',
            "GetSectorEnemyIntelSummary(",
            '"BH_SECTOR_WORLD_STATUS sector=%s owner=%d "',
            "FaceStatusTowardPlayer();",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWarMapWidget.cpp",
        (
            "GetSectorIntelConfidence(",
            "GetSectorEnemyIntelSummary(",
            "WarSubsystem->GetCampaignOutcome()",
            "WarSubsystem->GetCampaignOutcomeText()",
            "!WarSubsystem->IsCampaignResolved()",
            "OnMilitiaRequested.Broadcast(SectorID);",
            "OnGarrisonRedeployRequested.Broadcast(",
            "GetIncomingGarrisonTransferCount(",
            "GetSectorMilitiaMobilizationCount(",
            '" // [R] RALLY %s +%d MILITIA (%.0f SUPPLY)"',
            '" // [T] MOVE %d FROM %s "',
            '"INTEL %.0f%% // HOSTILES UNKNOWN // RECON "',
            "GetSectorEnemyAdaptationSummary(",
            '" // COUNTER +%d"',
            'bConfirmedIntel ? TEXT("CONFIRMED") : TEXT("ESTIMATED")',
            "Key == EKeys::H",
            "OnCivilianAidRequested.Broadcast(",
            '" // [H] AID NETWORK"',
            "EBHWarPriorityType::Raid",
            '"[A/D] SELECT OPERATION // "',
            '? TEXT("RAID")',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "ServerRequestMobilizeMilitia_Implementation(",
            "ServerRequestRedeployGarrison_Implementation(",
            "ServerRequestCivilianAid_Implementation(",
            "ServerRequestToggleFriendlySquadOrder_Implementation()",
            "ServerRequestToggleFriendlySquadOrder();",
            "PresentSharedOperationDebrief(",
            "ClientPresentOperationDebrief(Message);",
            "WarSubsystem->MobilizeSectorMilitia(SectorID)",
            "WarSubsystem->RedeploySectorGarrison(",
            "WarSubsystem->DeliverCivilianAid(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHAmbientWarDirector.cpp",
        (
            "SetReplicates(true);",
            "bAlwaysRelevant = true;",
            "if (!HasAuthority())",
            "SetActorTickEnabled(false);",
            "ResolveEnemyClass();",
            "BH_AMBIENT_ENEMY_CLASS_RESOLVED",
            "UpdatePlayerSectorAwareness(PlayerCharacter);",
            "UpdateFieldRecon(PlayerCharacter, DeltaSeconds);",
            "UpdateSupplyConvoyOpportunity(PlayerCharacter);",
            "ABHSupplyConvoyTarget::StaticClass();",
            "Convoy.Owner != EBHWarFaction::Neutral",
            "bTouchesPlayerSector",
            "SpawnActorDeferred<ABHSupplyConvoyTarget>",
            "ConfigureConvoy(LocalConvoy);",
            "FindBestWorldRoute(",
            "ConvoyRouteConnectionTolerance",
            "bSourceConnected",
            "bDestinationConnected",
            "SetTravelRoute(",
            "ConvoyTarget->SetTravelDestination(",
            "PresentedSupplyConvoyIDs.Add(",
            "SpawnSupplyConvoyEscorts(",
            "SpawnFriendlySupplyConvoyDefenders(",
            "CalculateConvoyCombatantCount(",
            "RecentRouteInterdictions",
            "MaximumConvoyRouteSecurityBonus",
            "RequestedCombatantCount",
            "ConvoyCombatSourceSectorID",
            "WarSubsystem->CommitAmbientPatrolSupply(",
            "BH_CONVOY_FORCE_PACKAGE",
            "source_strength=%.1f source_supply=%.1f",
            "BH_CONVOY_ESCORTS_DEPLOYED",
            "BH_CONVOY_DEFENDERS_DEPLOYED",
            "BH_CONVOY_DEFENDER_PACKAGE",
            "BH_CONVOY_ESCORTS_WITHDREW",
            "BH_CONVOY_ESCORT_CASUALTIES",
            "BH_CONVOY_DEFENDER_CASUALTIES",
            "ReportSupplyConvoyEscortCasualties(EscortCasualties);",
            "ReportSupplyConvoyDefenderCasualties(",
            "WarSubsystem->ApplyAmbientBattleResult(",
            "WarSubsystem->RecoverBattlefieldMateriel(",
            '"BATTLEFIELD SALVAGE SECURED\\n\\n"',
            '"recovered_supply=%.1f"',
            "SetCombatFaction(EBHCombatFaction::Hostile)",
            "EBHCombatFaction::Friendly",
            "AttachToActor(",
            "BH_CONVOY_OPPORTUNITY_SPAWNED",
            '"distance=%.0f route=%s mode=%s"',
            'TEXT("spline")',
            '"ENEMY SUPPLY CONVOY DETECTED\\n\\n"',
            '"FRIENDLY SUPPLY CONVOY UNDER ATTACK\\n\\n"',
            '"CIVILIAN AID CONVOY UNDER ATTACK\\n\\n"',
            '"Defend the marked cargo until it clears "',
            '"{4} friendly guards on station.\\n"',
            "LocalConvoy.Owner == EBHWarFaction::Friendly",
            "ResolveForceSourceSector(",
            "EBHWarFaction::Enemy",
            '"SectorEntryNotification"',
            "BH_PLAYER_ENTERED_SECTOR",
            '"LogisticsHubSectorRole"',
            '"FrontlineSectorRole"',
            '"RearAreaSectorRole"',
            "WarSubsystem->ReportSectorRecon(",
            "FieldReconIntelGain",
            "ReconMovementAccumulated",
            "ReconObservationAccumulated",
            "bool ABHAmbientWarDirector::GetFieldReconStatus(",
            "NextReconReportTime - World->GetTimeSeconds()",
            "BH_FIELD_RECON_REPORTED",
            '"FIELD RECON FILED // +{0} INTEL\\n\\n"',
            "RECON // MOVE AND OBSERVE TO CONFIRM HOSTILES",
            "WarSubsystem->GetSectorEnemyIntelSummary(",
            '"GARRISON F {6} // CAP {7}\\n"',
            '"SUPPLY FLOW {8} / TURN{9}"',
            "WarSubsystem->GetSectorSupplyChangePerTurn(",
            "AdjustControlledPatrolCount(",
            "SectorState.CivilianSupport <= 25.0f",
            "SectorState.CivilianSupport >= 70.0f",
            "LowSupplyThreshold",
            "HighSupplyThreshold",
            "friendly_size=%d supply=%.1f",
            "OpposingPatrolSeparation",
            "FormationSpacing",
            "FriendlyPatrolCenter",
            "HostilePatrolCenter",
            "AssignedPatrolPoints",
            "separation=%.0f formation_spacing=%.0f",
            '#include "BHEnemyAIController.h"',
            "RetreatingFriendlyCount",
            "RetreatingEnemyCount",
            "EBHEnemyAIState::Retreat",
            'TEXT("rout")',
            "resolution=%s routed=%d",
            "ReportAmbientRout(",
            "WarSubsystem->ApplyAmbientRoutResult(",
            "WarSubsystem->ReportCivilianSecurityOutcome(",
            "SectorContactCooldown",
            "GetRemainingSectorContactCooldown(",
            "BeginSectorContactCooldown(",
            "SectorContactReadyTimes",
            "BH_AMBIENT_CONTACT_COOLDOWN",
            "RoutedCombatants",
            "BH_AMBIENT_ROUT_CASUALTY_IGNORED",
            '"AmbientSkirmishContactReport"',
            '"CONTACT REPORT // {0}\\n{1}\\n{2}\\n\\n"',
            '"LOCAL SUPPORT {5}%"',
            "BH_AMBIENT_CONTACT_REPORT",
            "PlayerCharacter->ShowStatusNotification(",
            "MinimumPatrolStrength",
            "PatrolStrengthPerMember",
            "LimitPatrolCountByStrength(",
            "LimitPatrolCountBySupply(",
            "SupplyCostPerMember",
            "FMath::Max(0.0f, SourceSupply)",
            "CalculatePatrolSupplyCost(1, SourceHops)",
            "ResolveForceSourceSector(",
            "VisitedSectorIDs",
            "ActiveFriendlyForceSectorID",
            "ActiveEnemyForceSectorID",
            "enemy_source=%s enemy_hops=%d",
            "friendly_source=%s friendly_hops=%d",
            "BH_AMBIENT_CASUALTY_SOURCES",
            "BH_AMBIENT_ROUT_SOURCES",
            '"AmbientContactAlert"',
            '"CONTACT // {0}\\n"',
            "BH_AMBIENT_CONTACT_ALERT",
            "SafeMaximumHops",
            "Candidate.Owner != OpposingFaction",
            "CandidateGarrison > 0",
            "SourceSector.EnemyGarrison",
            "SourceSector.FriendlyGarrison",
            "bMayProjectThroughSector",
            "friendly_hops=%d",
            "enemy_hops=%d",
            "WarSubsystem->CommitAmbientPatrolSupply(",
            "BH_AMBIENT_DEPLOYMENT_SUPPLY",
            "FriendlySupplyRequested",
            "EnemySupplyRequested",
            "CurrentContactSupply",
        ),
    )
    _reject_fragments(
        "Source/BrokenHorizon/Private/BHAmbientWarDirector.cpp",
        (
            "FClassFinder<ABHEnemySoldier>",
            'TEXT("/Game/Characters/BP_EnemySoldier")',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarOperationRules.h",
        (
            "ClassifyRaidOperationalSignature(",
            "CalculateRaidReactionForceCount(",
            "IsOperationCombatantReady(",
            "IsFieldSquadMemberTransportEligible(",
            "CalculateFriendlyFormationOffset(",
            "float IntelConfidence = 0.0f;",
            "float CivilianSupport = 50.0f;",
            "int32 EnemyPatternPreparationLevel = 0;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWarOperationRules.cpp",
        (
            "BHWarOperationRules::ClassifyRaidOperationalSignature(",
            "BHWarOperationRules::CalculateRaidReactionForceCount(",
            "BHWarOperationRules::IsEnemyRoutedFromOperation(",
            "BHWarOperationRules::IsOperationCombatantReady(",
            "BHWarOperationRules::\n    IsFieldSquadMemberTransportEligible(",
            "BHWarOperationRules::CalculateFriendlyFormationOffset(",
            "return EBHRaidOperationalSignature::Loud;",
            "return EBHRaidOperationalSignature::Clean;",
            "return EBHRaidOperationalSignature::Contested;",
            "Package.IntelConfidence = TargetSector.IntelConfidence;",
            "Package.CivilianSupport = TargetSector.CivilianSupport;",
            "bLowConfidenceIntel",
            "bConfirmedIntel",
            "IntelligenceForceAdjustment",
            "IntelligenceWaveAdjustment",
            "PopulationForceAdjustment",
            "PopulationWaveAdjustment",
            "MaximumPatternPreparationLevel",
            "TargetSector.AnticipatedOperationType == OperationType",
            "Package.EnemyPatternPreparationLevel",
            "CounterinsurgencyWavePressureThreshold",
            "CounterinsurgencyWaveAdjustment",
            "bCounterinsurgencySweep",
            "MaximumRaidForce",
            "MaximumRaidReinforcementWaves",
            "MaximumRaidSupport",
            "OperationType == EBHWarPriorityType::Raid",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHSectorResupplyStation.h",
        (
            "public IBHInteractable",
            "void ConfigureStation(FName NewSectorID);",
            "float StrategicSupplyCost = 5.0f;",
            "int32 ReserveAmmoAmount = 90;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSectorResupplyStation.cpp",
        (
            '#include "BHSaveSubsystem.h"',
            "WarSubsystem->ConsumeSectorSupply(",
            "WeaponComponent->AddReserveAmmo(AmmoRequest)",
            "InjuryComponent->AddMedicalSupplies(",
            "InjuryComponent->RepairArmor(",
            "SaveSubsystem->SaveProgress()",
            '"CHECKPOINT UPDATED\\n\\n"',
            '"[F] Resupply / Vehicle Support // [C] Recruit Fireteam"',
            '"SECTOR RESUPPLY COMPLETE\\n\\n"',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "void ABHCharacter::OnMissionCompleted()",
            "bool ABHCharacter::FailCurrentWarOperation(",
            "WarSubsystem->ApplyOperationRoutResult(",
            "WarSubsystem->ApplyOperationCasualtyResult(",
            "void ABHCharacter::HandleWarStateChanged(",
            "void ABHCharacter::CacheObservedWarState(",
            "WarSubsystem->OnWarStateChanged.AddDynamic(",
            '"FrontlineOwnershipUpdate"',
            '"StrategicPriorityUpdate"',
            "LastObservedSectorLogisticsConnected",
            "IsSectorConnectedToFactionLogistics(",
            '"SectorLogisticsSevered"',
            '"LOGISTICS SEVERED\\n\\n"',
            '"SectorLogisticsRestored"',
            '"LOGISTICS RESTORED\\n\\n"',
            "LastObservedSectorSupplyReadiness",
            "GetSectorSupplyReadiness(",
            "bEnteredSupplyCrisis",
            "bRecoveredFromSupplyCrisis",
            '"SectorSupplyCrisis"',
            '"SUPPLY CRISIS\\n\\n"',
            '"SectorSupplyRecovered"',
            '"SUPPLY RECOVERED\\n\\n"',
            "LastObservedWarEventTurn",
            "WarSubsystem->GetRecentWarEvents()",
            "bHasNewWarEvent",
            '"BH_FIELD_CAMPAIGN_UPDATE "',
            '"CampaignEventFieldUpdate"',
            '"CAMPAIGN UPDATE\\n\\n{0}\\nWar turn {1}."',
            "CurrentCampaignOutcome",
            '"FieldCampaignVictory"',
            '"CAMPAIGN VICTORY\\n\\n"',
            '"FieldCampaignDefeat"',
            '"CAMPAIGN DEFEAT\\n\\n"',
            "WarSubsystem->GetPrioritySectorID();",
            "WarSubsystem->GetPriorityType();",
            "WarSubsystem->ResolvePriorityMission(true)",
            "OpenWorldOperationDirector->RestoreOperationState(",
            "StartOpenWorldOperationDirector(true);",
            '"WarMissionCompleteMessage"',
            '"WarMissionFailedMessage"',
            "WarSubsystem->ApplyMissionResult(",
            "WarSubsystem->ApplyRaidOperationalSignature(",
            "FriendlySupportLosses,\n                false",
            "GetEnemyRoutedCount()",
            '"ROUTED {HostileRouted}\\n"',
            "WarSubsystem->RecoverBattlefieldMateriel(",
            '"BATTLEFIELD RECOVERY "',
            "recovered_supply=%.1f.",
            '"WarRaidSuccess"',
            '"LOGISTICS DISRUPTED // {0} SIGNATURE"',
            '"RAID REPULSED // {0} SIGNATURE"',
            '"WarRaidFailure"',
            "ObjectiveComponent->FailMission()",
            "SaveSubsystem->SaveProgress();",
            "BH_OPERATION_RESULT_CHECKPOINT_FAILED",
            "BH_OPERATION_RESULT_CONTINUE_BLOCKED",
            "BH_OPERATION_RESULT_CHECKPOINT_CONFIRMED",
            '"MissionCompleteCheckpointPending"',
            "bCampaignEpilogueAcknowledged = bResolvedCampaign;",
            "bOperationDebriefAcknowledged = !bResolvedCampaign;",
            "EnterCampaignEpilogueFreeRoam(true);",
            "BH_CAMPAIGN_EPILOGUE_ENTERED",
            "EnterPostOperationFreeRoam(true);",
            "BH_POST_OPERATION_FREE_ROAM_ENTERED",
            "BH_POST_OPERATION_FREE_ROAM_CHECKPOINT_FAILED",
        ),
    )

    _log("PASS six connected sectors seed the first campaign.")
    _log("PASS strength, supply, reinforcements, and battles simulate.")
    _log("PASS attack/defend priorities and mission results are exposed.")
    _log("PASS field victory resolves the priority before checkpointing.")
    _log("PASS sector graph, turn, and timer persist in checkpoints.")
    _log("PASS in-transit supply convoys persist in checkpoints.")
    _log("PASS active operation phase, forces, and losses persist.")
    _log("PASS health, bleeding, and limb injuries persist.")
    _log("PASS explicit checkpoints coalesce pending war autosaves.")
    _log("PASS first consumed item establishes a checkpoint.")
    _log("PASS non-final objective progress checkpoints automatically.")
    _log("PASS successful field treatment checkpoints player condition.")
    _log("PASS free-roam sector transitions report live war state.")
    _log("PASS sector entry reports strategic role and supply flow.")
    _log("PASS controlling-faction patrol strength follows supply.")
    _log("PASS local enemy logistics create field convoy targets.")
    _log("PASS friendly logistics create convoy-defense contacts.")
    _log("PASS hostile raiders can target friendly supply convoys.")
    _log("PASS convoy HUD markers communicate defend or interdict.")
    _log("PASS live convoy integrity remains visible on the field HUD.")
    _log("PASS convoy combat packages scale and consume source supply.")
    _log("PASS enemy convoy security adapts to recent route losses.")
    _log("PASS friendly convoy guards defend strategic shipments.")
    _log("PASS field supply convoys follow connected world routes.")
    _log("PASS operation units cannot deadlock combat without controllers.")
    _log("PASS blocked combat spawns retry without reducing force silently.")
    _log("PASS live field convoy targets expose a tracking waypoint.")
    _log("PASS destroyed convoy targets checkpoint interdiction.")
    _log("PASS strategic frontline changes reach the free-roam HUD.")
    _log("PASS friendly supply crises and recoveries reach the HUD.")
    _log("PASS recent campaign outcomes reach the free-roam HUD.")
    _log("PASS total liberation or headquarters loss resolves the campaign.")
    _log("PASS active player operations defer terminal campaign outcomes.")
    _log("PASS operation debriefs gate strategic return on a durable save.")
    _log("PASS resolved campaigns enter a persistent playable epilogue.")
    _log("PASS resolved campaigns freeze turns and block new deployments.")
    _log("PASS civilian support can rally persistent local militia.")
    _log("PASS strategic aid builds support and underground networks.")
    _log("PASS hostile aid networks trade supply for exposure risk.")
    _log("PASS player victories recover bounded battlefield materiel.")
    _log("PASS raids disrupt logistics without capturing territory.")
    _log("PASS raids use a physical sabotage target.")
    _log("PASS raid casualties create persistent operational signatures.")
    _log("PASS live raid guidance exposes the current signature.")
    _log("PASS failed and withdrawn raids retain their signature.")
    _log("PASS militia mobilization trades support and supply for defense.")
    _log("PASS arming local militia provokes stronger enemy response.")
    _log("PASS local sector control and supply remain visible in the HUD.")
    _log("PASS sector hubs display live ownership and force balance.")
    _log("PASS friendly sectors can fund player field resupply.")
    _log("PASS friendly resupply stations update the respawn checkpoint.")
    _log("PASS player deaths consume persistent campaign manpower.")


def main():
    _validate_reflection()
    _validate_simulation()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
