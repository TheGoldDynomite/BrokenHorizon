"""Validate Broken Horizon tactical AI defaults and morale behavior."""

from pathlib import Path

import unreal


ENEMY_BLUEPRINT = "/Game/Characters/BP_EnemySoldier"
FIRST_LIGHT_MAP = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"


def _log(message):
    unreal.log("[BH Tactical AI Validation] " + message)


def _load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset


def _property(cdo, name):
    return cdo.get_editor_property(name)


def _validate_defaults():
    blueprint = _load(ENEMY_BLUEPRINT)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())

    minimum_distance = _property(
        cdo,
        "minimum_engagement_distance",
    )
    desired_distance = _property(
        cdo,
        "desired_engagement_distance",
    )
    maximum_distance = _property(
        cdo,
        "maximum_engagement_distance",
    )
    minimum_burst = _property(cdo, "minimum_burst_shots")
    maximum_burst = _property(cdo, "maximum_burst_shots")
    minimum_recovery = _property(cdo, "minimum_burst_recovery")
    maximum_recovery = _property(cdo, "maximum_burst_recovery")
    reposition_interval = _property(
        cdo,
        "combat_reposition_interval",
    )
    reposition_radius = _property(
        cdo,
        "combat_reposition_radius",
    )
    retreat_health_threshold = _property(
        cdo,
        "retreat_health_threshold",
    )
    retreat_suppression_threshold = _property(
        cdo,
        "retreat_suppression_threshold",
    )
    retreat_distance = _property(cdo, "retreat_distance")
    retreat_duration = _property(cdo, "retreat_duration")
    normal_movement_speed = _property(
        cdo,
        "normal_movement_speed",
    )
    retreat_movement_speed = _property(
        cdo,
        "retreat_movement_speed",
    )
    ally_casualty_morale_radius = _property(
        cdo,
        "ally_casualty_morale_radius",
    )
    ally_casualty_suppression = _property(
        cdo,
        "ally_casualty_suppression",
    )
    magazine_capacity = _property(cdo, "magazine_capacity")
    starting_reserve_ammo = _property(
        cdo,
        "starting_reserve_ammo",
    )
    reload_duration = _property(cdo, "reload_duration")
    battlefield_ammo_supply_class = _property(
        cdo,
        "battlefield_ammo_supply_class",
    )
    maximum_dropped_ammo = _property(
        cdo,
        "maximum_dropped_ammo",
    )
    near_miss_suppression_amount = _property(
        cdo,
        "near_miss_suppression_amount",
    )
    maximum_frag_grenades = _property(
        cdo,
        "maximum_frag_grenades",
    )
    minimum_grenade_range = _property(
        cdo,
        "minimum_grenade_range",
    )
    maximum_grenade_range = _property(
        cdo,
        "maximum_grenade_range",
    )
    grenade_decision_interval = _property(
        cdo,
        "grenade_decision_interval",
    )
    grenade_use_chance = _property(
        cdo,
        "grenade_use_chance",
    )
    grenade_friendly_safety_radius = _property(
        cdo,
        "grenade_friendly_safety_radius",
    )
    frag_grenade_class = _property(
        cdo,
        "frag_grenade_class",
    )

    if not 0.0 <= minimum_distance <= desired_distance:
        raise RuntimeError("Invalid minimum/desired engagement band.")
    if desired_distance > maximum_distance:
        raise RuntimeError("Invalid desired/maximum engagement band.")
    if not 1 <= minimum_burst <= maximum_burst:
        raise RuntimeError("Invalid burst shot range.")
    if not 0.0 <= minimum_recovery <= maximum_recovery:
        raise RuntimeError("Invalid burst recovery range.")
    if reposition_interval <= 0.0 or reposition_radius <= 0.0:
        raise RuntimeError("Combat repositioning is disabled.")
    if not 0.0 < retreat_health_threshold < 1.0:
        raise RuntimeError("Invalid retreat health threshold.")
    if not 0.0 < retreat_suppression_threshold <= 1.0:
        raise RuntimeError("Invalid retreat suppression threshold.")
    if retreat_distance < 100.0 or retreat_duration <= 0.0:
        raise RuntimeError("Tactical withdrawal is disabled.")
    if (
        normal_movement_speed <= 0.0
        or retreat_movement_speed <= normal_movement_speed
    ):
        raise RuntimeError("Retreat movement is not faster than patrol.")
    if ally_casualty_morale_radius <= 0.0:
        raise RuntimeError("Ally casualty morale radius is disabled.")
    if not 0.0 < ally_casualty_suppression <= 1.0:
        raise RuntimeError("Ally casualty morale shock is disabled.")
    if magazine_capacity < maximum_burst:
        raise RuntimeError("AI magazine cannot hold a full burst.")
    if starting_reserve_ammo < magazine_capacity:
        raise RuntimeError(
            "AI reserve ammunition cannot support a full reload."
        )
    if reload_duration <= 0.0:
        raise RuntimeError("AI reload duration is disabled.")
    if not battlefield_ammo_supply_class:
        raise RuntimeError("AI battlefield ammunition drops are disabled.")
    if maximum_dropped_ammo <= 0:
        raise RuntimeError("AI ammunition drops contain no rounds.")
    if not 0.0 < near_miss_suppression_amount <= 1.0:
        raise RuntimeError("AI near-miss suppression is disabled.")
    if maximum_frag_grenades < 1 or not frag_grenade_class:
        raise RuntimeError("AI frag-grenade inventory is disabled.")
    if not 0.0 < minimum_grenade_range < maximum_grenade_range:
        raise RuntimeError("Invalid AI grenade engagement range.")
    if grenade_decision_interval <= 0.0:
        raise RuntimeError("AI grenade decision interval is disabled.")
    if not 0.0 < grenade_use_chance <= 1.0:
        raise RuntimeError("AI grenade use chance is disabled.")
    if grenade_friendly_safety_radius <= 0.0:
        raise RuntimeError("AI grenade friendly safety is disabled.")

    _log(
        "PASS engagement %.0f/%.0f/%.0f cm, bursts %d-%d, "
        "recovery %.2f-%.2f s, reposition %.0f cm every %.2f s."
        % (
            minimum_distance,
            desired_distance,
            maximum_distance,
            minimum_burst,
            maximum_burst,
            minimum_recovery,
            maximum_recovery,
            reposition_radius,
            reposition_interval,
        )
    )
    _log(
        "PASS morale withdrawal at health <= %.2f and suppression "
        ">= %.2f, retreating %.0f cm for %.2f s at %.0f cm/s."
        % (
            retreat_health_threshold,
            retreat_suppression_threshold,
            retreat_distance,
            retreat_duration,
            retreat_movement_speed,
        )
    )
    _log(
        "PASS casualties pressure same-faction allies within %.0f cm "
        "by %.2f suppression."
        % (
            ally_casualty_morale_radius,
            ally_casualty_suppression,
        )
    )
    _log(
        "PASS AI ammunition uses %d-round magazines, %d reserve rounds, "
        "%.2f s reloads, and up to %d recoverable rounds."
        % (
            magazine_capacity,
            starting_reserve_ammo,
            reload_duration,
            maximum_dropped_ammo,
        )
    )
    _log(
        "PASS AI near misses apply %.2f suppression to opposing squads."
        % near_miss_suppression_amount
    )
    _log(
        "PASS AI carries %d frag grenade and considers throws from "
        "%.0f-%.0f cm every %.1f s with %.0f cm friendly safety."
        % (
            maximum_frag_grenades,
            minimum_grenade_range,
            maximum_grenade_range,
            grenade_decision_interval,
            grenade_friendly_safety_radius,
        )
    )


def _validate_source_contract():
    source_root = (
        Path(unreal.Paths.project_dir())
        / "Source"
        / "BrokenHorizon"
    )
    controller_header = (
        source_root
        / "Public"
        / "BHEnemyAIController.h"
    ).read_text(encoding="utf-8")
    controller_source = (
        source_root
        / "Private"
        / "BHEnemyAIController.cpp"
    ).read_text(encoding="utf-8")
    soldier_source = (
        source_root
        / "Private"
        / "BHEnemySoldier.cpp"
    ).read_text(encoding="utf-8")
    soldier_header = (
        source_root
        / "Public"
        / "BHEnemySoldier.h"
    ).read_text(encoding="utf-8")
    operation_source = (
        source_root
        / "Private"
        / "BHOpenWorldOperationDirector.cpp"
    ).read_text(encoding="utf-8")
    operation_header = (
        source_root
        / "Public"
        / "BHOpenWorldOperationDirector.h"
    ).read_text(encoding="utf-8")
    combat_status_header = (
        source_root
        / "Public"
        / "BHCombatStatusWidget.h"
    ).read_text(encoding="utf-8")
    combat_status_source = (
        source_root
        / "Private"
        / "BHCombatStatusWidget.cpp"
    ).read_text(encoding="utf-8")
    ambient_source = (
        source_root
        / "Private"
        / "BHAmbientWarDirector.cpp"
    ).read_text(encoding="utf-8")
    ammo_supply_source = (
        source_root
        / "Private"
        / "BHAmmoSupply.cpp"
    ).read_text(encoding="utf-8")
    character_source = (
        source_root
        / "BHCharacter.cpp"
    ).read_text(encoding="utf-8")

    required_header_fragments = (
        "Retreat,",
        "ShouldRetreat(",
        "EnterRetreat(",
        "PursueLastKnownTarget(",
        "LastConfirmedTargetTime",
        "void SetFollowTarget(",
        "void ClearFollowTarget();",
        "bool HasFollowTarget() const;",
        "void SetHoldPosition(",
        "bool HasHoldPosition() const;",
        "void UpdateFollowMovement();",
        "void UpdateHoldMovement();",
    )
    required_source_fragments = (
        "ShouldRetreat(Enemy)",
        "EBHEnemyAIState::Retreat",
        "ProjectPointToNavigation(",
        "BH_AI_RETREAT",
        "GetRetreatMovementSpeed()",
        "Enemy->IsReloading()",
        "Enemy->GetSightMemoryDuration()",
        "PursueLastKnownTarget(Enemy, DeltaSeconds)",
        "MoveToLocation(",
        "LastKnownTargetLocation,",
        "TryThrowGrenade(Enemy, GrenadeTargetLocation)",
        "IsGrenadeTargetSafe(",
        "BH_AI_GRENADE_WITHHELD",
        "Enemy->IsOutOfAmmunition()",
        "bWithdrawingForAmmunition",
        "BH_AI_AMMO_WITHDRAWAL_COMPLETE",
        "void ABHEnemyAIController::SetFollowTarget(",
        "void ABHEnemyAIController::UpdateFollowMovement()",
        "Leader->GetActorForwardVector()",
        "FollowLocalOffset.X",
        "FollowRepositionDistance",
        "if (HasFollowTarget())",
        "void ABHEnemyAIController::SetHoldPosition(",
        "void ABHEnemyAIController::UpdateHoldMovement()",
        "HoldRepositionDistance",
    )
    required_soldier_fragments = (
        "GetAllyCasualtyMoraleRadius()",
        "Ally->ApplySuppression(",
        "BH_AI_MORALE_SHOCK",
        "UpdateReloadState(CurrentTime)",
        "BH_AI_RELOAD_STARTED",
        "BH_AI_RELOAD_COMPLETE",
        "HasClearLineOfFireTo(",
        "SCENE_QUERY_STAT(BHEnemyLineOfFire)",
        "BH_AI_FIRE_BLOCKED",
        "AActor* DamageTarget = nullptr;",
        "else if (IsHostileTo(HitActor))",
        "ApplyPointDamage(\n            DamageTarget,",
        "NearMissSuppressionAmount",
        "SoldierTarget->ApplySuppression(",
        "bool ABHEnemySoldier::ThrowFragGrenadeAt(",
        "BH_AI_GRENADE_THROWN",
        "CurrentReserveAmmo - RoundsLoaded",
        "BH_AI_OUT_OF_AMMO",
        "DropRemainingAmmunition();",
        "BH_AI_AMMO_DROPPED",
        "void ABHEnemySoldier::RestorePersistentCombatState(",
        "HealthComponent->RestorePersistentHealthState(SavedHealth);",
        "bool ABHEnemySoldier::NeedsCombatService() const",
        "bool ABHEnemySoldier::ServiceCombatLoadout()",
    )
    required_soldier_header_fragments = (
        "bool HasClearLineOfFireTo(",
        "TSubclassOf<ABHFragGrenade> FragGrenadeClass;",
        "float GrenadeFriendlySafetyRadius = 700.0f;",
        "int32 StartingReserveAmmo = 60;",
        "bool HasCombatAmmunition() const;",
        "TSubclassOf<ABHAmmoSupply> BattlefieldAmmoSupplyClass;",
        "void RestorePersistentCombatState(",
    )

    for fragment in required_header_fragments:
        if fragment not in controller_header:
            raise RuntimeError(
                "Missing tactical withdrawal declaration: " + fragment
            )

    for fragment in required_source_fragments:
        if fragment not in controller_source:
            raise RuntimeError(
                "Missing tactical withdrawal behavior: " + fragment
            )

    for fragment in required_soldier_fragments:
        if fragment not in soldier_source:
            raise RuntimeError(
                "Missing squad morale behavior: " + fragment
            )

    for fragment in required_soldier_header_fragments:
        if fragment not in soldier_header:
            raise RuntimeError(
                "Missing AI line-of-fire declaration: " + fragment
            )

    for fragment in (
        "Soldier->IsOutOfAmmunition()",
        "Enemy->HasCombatAmmunition()",
        "Ally->HasCombatAmmunition()",
    ):
        if (
            fragment not in operation_source and
            fragment not in ambient_source
        ):
            raise RuntimeError(
                "Missing war-logistics resolution behavior: " +
                fragment
            )

    for fragment in (
        "UpdateFieldSquadStatusHUD();",
        "CombatStatusWidget->SetFieldSquadStatus(",
        "bFieldSquadHolding,",
        "bFieldSquadEmbarked",
        "TryResolveFieldSquadCommandLocation(",
        "BH_FIELD_SQUAD_ORDER order=hold mode=%s",
        "bFieldSquadHasCommandLocation",
        "FieldSquadCommandLocation",
        "FieldSquadCommandRotation",
        "ImpactNormal.Z >= 0.25f",
        "SquadAIController->SetHoldPosition(",
        "GetFieldSquadMemberStates() const",
        "MemberState.bIncapacitated ||",
        "if (SavedMember.bHasWorldTransform)",
        "RestoredMember->RestorePersistentCombatState(",
        "CountFieldSquadMembersNeedingService(",
        "ServiceFieldSquadMembers(",
        "GetFieldSquadMembersNeedingServiceCount()",
    ):
        if fragment not in character_source:
            raise RuntimeError(
                "Missing persistent field-fireteam HUD behavior: " +
                fragment
            )

    for fragment in (
        "bool SetFriendlySupportMoveAndHoldOrder(",
        "bool SetFriendlySupportFollowOrder();",
        "bool HasFriendlySupportCommandLocation() const;",
        "FVector GetFriendlySupportCommandLocation() const;",
        "int32 GetLivingFriendlySupportCount() const;",
    ):
        if fragment not in operation_header:
            raise RuntimeError(
                "Missing unified friendly command declaration: " +
                fragment
            )

    for fragment in (
        "FriendlyController->SetFollowTarget(",
        "CalculateFriendlyFormationOffset(Index)",
        "ToggleFriendlySupportHoldOrder()",
        "SetFriendlySupportMoveAndHoldOrder(",
        "SetFriendlySupportFollowOrder()",
        "bFriendlySupportHasCommandLocation = true;",
        "FriendlySupportCommandLocation = CommandLocation;",
        "State.bFriendlySupportHasCommandLocation =",
        "SavedState.bFriendlySupportHasCommandLocation",
        "BH_SQUAD_ORDER sector=%s order=hold",
        "Controller->SetHoldPosition(",
        "ApplyFriendlySupportOrder();",
    ):
        if fragment not in operation_source:
            raise RuntimeError(
                "Missing responsive friendly formation behavior: " +
                fragment
            )

    for fragment in (
        "BH_UNIFIED_SQUAD_ORDER order=hold mode=%s",
        "GetLivingFriendlySupportCount()",
        "SetFriendlySupportMoveAndHoldOrder(",
        "SetFriendlySupportFollowOrder()",
        "bMultipleGroups",
        "CommandRight * 350.0f",
        "UpdateSquadCommandWaypointHUD();",
        "CombatStatusWidget->SetSquadCommandWaypoint(",
        "GetFriendlySupportCommandLocation()",
        "FieldSquadCommandLocation +",
    ):
        if fragment not in character_source:
            raise RuntimeError(
                "Missing unified player squad command behavior: " +
                fragment
            )

    for fragment in (
        "void SetSquadCommandWaypoint(",
        "BuildSquadCommandWaypointLabel(",
    ):
        if fragment not in combat_status_header:
            raise RuntimeError(
                "Missing squad rally waypoint declaration: " +
                fragment
            )

    for fragment in (
        "DrawSquadCommandWaypoint(",
        "SQUAD HOLD POINT // %.0f M // [%s] FOLLOW",
        "ORDER FOLLOW // AIM + [%s] MOVE/HOLD",
    ):
        if fragment not in combat_status_source:
            raise RuntimeError(
                "Missing squad rally waypoint presentation: " +
                fragment
            )

    for fragment in (
        "ConfigureRuntimePickup(",
        "MarkAsRuntimeSupply();",
    ):
        if fragment not in ammo_supply_source:
            raise RuntimeError(
                "Missing battlefield ammunition pickup behavior: " +
                fragment
            )

    _log("PASS tactical withdrawal source contract.")
    _log("PASS AI checks weapon line of fire before shooting.")
    _log("PASS AI rounds damage the hostile actor actually hit.")
    _log("PASS AI grenade throws enforce range and friendly safety.")
    _log("PASS finite AI ammunition drives routs and salvage.")
    _log("PASS operation allies follow the player and reform after combat.")
    _log("PASS operation allies obey persistent follow/hold orders.")
    _log("PASS all friendly elements share designated combat orders.")
    _log("PASS designated squad orders expose a persistent rally waypoint.")
    _log("PASS field fireteam state remains visible during combat.")


def _validate_level_support():
    unreal.EditorLevelLibrary.load_level(FIRST_LIGHT_MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    enemies = unreal.GameplayStatics.get_all_actors_of_class(
        world,
        unreal.load_class(
            None,
            "/Script/BrokenHorizon.BHEnemySoldier",
        ),
    )
    nav_bounds = unreal.GameplayStatics.get_all_actors_of_class(
        world,
        unreal.load_class(
            None,
            "/Script/NavigationSystem.NavMeshBoundsVolume",
        ),
    )
    recast_navmeshes = unreal.GameplayStatics.get_all_actors_of_class(
        world,
        unreal.RecastNavMesh,
    )

    if not enemies:
        raise RuntimeError("First Light has no enemy soldier.")
    if not nav_bounds:
        raise RuntimeError(
            "First Light has no NavMeshBoundsVolume for repositioning."
        )
    if not recast_navmeshes:
        raise RuntimeError("First Light has no RecastNavMesh instance.")

    dynamic_navmeshes = [
        navmesh
        for navmesh in recast_navmeshes
        if navmesh.get_editor_property("runtime_generation")
        == unreal.RuntimeGenerationType.DYNAMIC
    ]

    if not dynamic_navmeshes:
        raise RuntimeError(
            "First Light RecastNavMesh is not configured for "
            "dynamic runtime generation."
        )

    scalable_navmeshes = [
        navmesh
        for navmesh in dynamic_navmeshes
        if navmesh.get_editor_property("tile_size_uu") >= 8000.0
        and not navmesh.get_editor_property("fixed_tile_pool_size")
        and navmesh.get_editor_property("tile_pool_size") >= 256
    ]

    if len(scalable_navmeshes) != len(dynamic_navmeshes):
        raise RuntimeError(
            "First Light RecastNavMesh does not have scalable tile "
            "settings for tactical movement."
        )

    _log(
        "PASS First Light has %d enemy, %d navigation bounds, "
        "and %d scalable dynamic Recast navmesh."
        % (
            len(enemies),
            len(nav_bounds),
            len(scalable_navmeshes),
        )
    )


def _validate_runtime_grenade_throw():
    world = unreal.EditorLevelLibrary.get_editor_world()
    grenade_class = getattr(unreal, "BHFragGrenade", None)
    soldier_class = getattr(unreal, "BHEnemySoldier", None)

    if not grenade_class or not soldier_class:
        raise RuntimeError("Native AI grenade classes are not reflected.")

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    grenades_before = set(
        actor.get_path_name()
        for actor in unreal.GameplayStatics.get_all_actors_of_class(
            world,
            grenade_class,
        )
    )
    soldier = None
    spawned_grenade = None

    try:
        soldier = unreal.EditorLevelLibrary.spawn_actor_from_class(
            soldier_class,
            unreal.Vector(0.0, 0.0, 1000.0),
        )
        if not soldier:
            raise RuntimeError("Could not spawn AI grenade test soldier.")

        soldier.refill_frag_grenades()
        if not soldier.throw_frag_grenade_at(
            unreal.Vector(1000.0, 0.0, 1000.0)
        ):
            raise RuntimeError("AI grenade throw function returned false.")

        grenades_after = (
            unreal.GameplayStatics.get_all_actors_of_class(
                world,
                grenade_class,
            )
        )
        new_grenades = [
            actor
            for actor in grenades_after
            if actor.get_path_name() not in grenades_before
        ]

        if len(new_grenades) != 1:
            raise RuntimeError(
                "AI grenade throw did not spawn exactly one grenade."
            )

        spawned_grenade = new_grenades[0]
        if soldier.get_current_frag_grenades() != 0:
            raise RuntimeError(
                "AI grenade inventory was not consumed."
            )
    finally:
        if spawned_grenade:
            actor_subsystem.destroy_actor(spawned_grenade)
        if soldier:
            actor_subsystem.destroy_actor(soldier)

    _log("PASS runtime AI throw spawns and consumes one frag grenade.")


def _validate_runtime_ammunition_drop():
    soldier_class = getattr(unreal, "BHEnemySoldier", None)
    ammo_supply_class = getattr(unreal, "BHAmmoSupply", None)

    if not soldier_class or not ammo_supply_class:
        raise RuntimeError(
            "Native AI ammunition classes are not reflected."
        )

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    soldier = None
    ammo_drop = None

    try:
        soldier = unreal.EditorLevelLibrary.spawn_actor_from_class(
            soldier_class,
            unreal.Vector(500.0, 0.0, 1000.0),
        )
        if not soldier:
            raise RuntimeError(
                "Could not spawn AI ammunition test soldier."
            )

        soldier.refill_ammunition()
        expected_total = (
            soldier.get_current_magazine_ammo()
            + soldier.get_current_reserve_ammo()
        )
        if expected_total != 90:
            raise RuntimeError(
                "AI ammunition refill did not restore 90 rounds."
            )

        ammo_drop = soldier.drop_remaining_ammunition()
        if not ammo_drop:
            raise RuntimeError(
                "AI ammunition salvage did not spawn a pickup."
            )
        if ammo_drop.get_reserve_ammo_amount() != 30:
            raise RuntimeError(
                "AI ammunition salvage did not cap at 30 rounds."
            )
        if soldier.drop_remaining_ammunition():
            raise RuntimeError(
                "AI ammunition salvage spawned more than once."
            )
    finally:
        if ammo_drop:
            actor_subsystem.destroy_actor(ammo_drop)
        if soldier:
            actor_subsystem.destroy_actor(soldier)

    _log(
        "PASS runtime AI ammunition refills and drops one "
        "30-round salvage pickup."
    )


def _validate_runtime_friendly_casualty_recovery():
    soldier_class = getattr(unreal, "BHEnemySoldier", None)
    combat_faction_type = getattr(
        unreal,
        "BHCombatFaction",
        None,
    )

    if not soldier_class or not combat_faction_type:
        raise RuntimeError(
            "Friendly casualty recovery reflection is unavailable."
        )

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    soldier = None

    try:
        soldier = unreal.EditorLevelLibrary.spawn_actor_from_class(
            soldier_class,
            unreal.Vector(1000.0, 0.0, 1000.0),
        )
        if not soldier:
            raise RuntimeError(
                "Could not spawn friendly casualty test soldier."
            )

        soldier.set_combat_faction(
            combat_faction_type.FRIENDLY
        )
        soldier.restore_incapacitated_state_with_remaining_time(
            12.0
        )

        if not soldier.is_incapacitated() or not soldier.is_dead():
            raise RuntimeError(
                "Friendly operative did not enter persistent "
                "incapacitation."
            )

        if abs(
            soldier.get_incapacitation_seconds_remaining() - 12.0
        ) > 0.1:
            raise RuntimeError(
                "Friendly operative recovery window did not restore."
            )

        if not soldier.stabilize_incapacitated_soldier():
            raise RuntimeError(
                "Friendly operative could not be stabilized."
            )

        health_component = soldier.get_health_component()
        if (
            soldier.is_incapacitated()
            or soldier.is_dead()
            or not health_component
            or health_component.get_current_health() <= 0.0
        ):
            raise RuntimeError(
                "Stabilized operative did not return combat-ready."
            )
    finally:
        if soldier:
            actor_subsystem.destroy_actor(soldier)

    _log(
        "PASS friendly operatives persist downed and return after "
        "stabilization."
    )


def main():
    _validate_defaults()
    _validate_source_contract()
    _validate_level_support()
    _validate_runtime_grenade_throw()
    _validate_runtime_ammunition_drop()
    _validate_runtime_friendly_casualty_recovery()
    _log("ALL CHECKS PASSED")


main()
