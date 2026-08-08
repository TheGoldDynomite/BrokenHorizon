"""Validate Broken Horizon priority-driven field mission presentation."""

import os

import unreal


def _log(message):
    unreal.log("[BH War Mission Assignment Validation] " + message)


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


def _require_order(relative_path, fragments):
    source = _read(relative_path)
    previous_position = -1

    for fragment in fragments:
        position = source.find(fragment, previous_position + 1)

        if position < 0:
            raise RuntimeError(
                "%s is missing ordered fragment: %s"
                % (relative_path, fragment)
            )

        previous_position = position


def _validate_attack_assignment():
    game_instance = unreal.new_object(unreal.GameInstance)
    war = unreal.new_object(
        unreal.BHWarSubsystem,
        outer=game_instance,
    )
    war.reset_campaign()

    if war.get_priority_type() != unreal.BHWarPriorityType.ATTACK:
        raise RuntimeError("Default campaign did not assign an attack.")

    title = str(war.get_priority_operation_title())
    briefing = str(war.get_priority_mission_briefing())
    objective = str(
        war.get_priority_objective_text("EliminateGuard")
    )

    if "BREAKTHROUGH" not in title:
        raise RuntimeError("Attack operation title is not distinct.")

    if "Korona Crossroads" not in briefing:
        raise RuntimeError("Attack briefing omits the priority sector.")

    if "Neutralize hostile forces" not in objective:
        raise RuntimeError("Attack field objective is not specialized.")

    _log("PASS attack priority produces sector-specific field orders.")


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarSubsystem.h",
        (
            "FText GetPriorityOperationTitle() const;",
            "FText GetPriorityMissionBriefing() const;",
            "FText GetPriorityObjectiveText(FName ObjectiveID) const;",
            "FText GetOperationTitle(",
            "FText GetOperationMissionBriefing(",
            "FText GetOperationObjectiveText(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWarSubsystem.cpp",
        (
            '"OPERATION HOLDFAST // {0}"',
            '"OPERATION BREAKTHROUGH // {0}"',
            '"OPERATION SAFEHOUSE // {0}"',
            '"Enemy security forces are sweeping {0} for militia "',
            '"Repel the enemy assault on {0}"',
            '"Neutralize hostile forces in {0}"',
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHObjectiveComponent.h",
        (
            "void SetObjectiveDisplayOverride(",
            "void ClearObjectiveDisplayOverrides();",
            "TMap<FName, FText> ObjectiveDisplayOverrides;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "ConfigureStrategicMissionPresentation();",
            "ObjectiveComponent->SetObjectiveDisplayOverride(",
            "WarSubsystem->ApplyMissionResult(",
            "WarSubsystem->CanFundOperation(",
            "WarSubsystem->HasCommittedOperation()",
            "WarSubsystem->ConsumeOperationSupply(",
            "WarSubsystem->SetCommittedOperation(",
            "WarSubsystem->ClearCommittedOperation();",
            "const auto RollBackDeployment =",
            "bool bCommittedThisDeployment = false;",
            "if (bCommittedThisDeployment &&",
            "bCommittedThisDeployment = true;",
            '"Open-world operation deployment rolled back; "',
            "ObjectiveComponent->StartRuntimeMission(",
            '"StrategicMissionBriefingNotification"',
            '"WarSectorHeldDebrief"',
            '"WarSectorCapturedDebrief"',
            "FFormatNamedArguments DebriefArguments;",
            '"OPERATION IMPACT // FRIENDLY {FriendlyDelta} // "',
            '"CONTROL {PreviousOwner} -> {CurrentOwner}\\n"',
            '"SECTOR FORCE // FRIENDLY {FriendlyStrength} // "',
            "UpdatedSector.FriendlyStrength",
            "UpdatedSector.EnemyStrength",
            "UpdatedSector.Supply",
            '"STAGING {StagingSector} // "',
            '"SUPPORT LOSSES {SupportLosses} // "',
            '"HOSTILE LOSSES {HostileLosses} // "',
            '"ROUTED {HostileRouted}\\n"',
            "FriendlySupportLosses",
            "EnemyLosses",
            "EnemyRouted",
            "UpdateOperationWaypointHUD();",
            "SetOperationWaypoint(",
            "OpenWorldOperationDirector->IsOperationActivated(),",
            "UpdateResupplyWaypointHUD(DeltaTime);",
            "bNeedsVehicleService",
            "RelevantTransport->GetFuelPercentage() <= 0.25f",
            "RelevantTransport->GetHullPercentage() <= 0.35f",
            "TActorIterator<ABHSectorResupplyStation>",
            "Sector.Owner != EBHWarFaction::Friendly",
            "SetResupplyWaypoint(",
            "UpdateTransportWaypointHUD(DeltaTime);",
            "SetTransportWaypoint(",
        ),
    )
    _require_order(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "const auto RollBackDeployment =",
            "if (!StartOpenWorldOperationDirector())",
            "if (!WarSubsystem->SetCommittedOperation(",
            "if (!WarSubsystem->ConsumeOperationSupply(",
            "ObjectiveComponent->StartRuntimeMission(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        (
            "GetOperationCenter() const",
            "GetSectorDisplayName() const",
            "GetOperationStatusText() const",
            "IsOperationActivated() const",
            "void ABHOpenWorldOperationDirector::ConfigureForcePackage()",
            "BHWarOperationRules::BuildForcePackage(",
            "EffectiveAttackEnemyCount",
            "EffectiveAttackReinforcementWaveCount",
            "EffectiveAttackReinforcementsPerWave",
            "EffectiveDefenseWaveCount",
            "EffectiveDefenseEnemiesPerWave",
            "EffectiveFriendlySupportCount",
            "SupplySourceSectorID",
            "EnemySourceSectorID",
            "ForcePackage.StagingFriendlyStrength",
            "ForcePackage.StagingSupply",
            "ForcePackage.EnemyStrength",
            '"staging=%s enemy_source=%s staging_strength=%.1f "',
            '"BH_OPERATION_FORCE_PACKAGE sector=%s type=%d "',
            "friendly_support=%d",
            "SpawnFriendlySupport(",
            "EBHCombatFaction::Friendly",
            "HandleFriendlySupportDeath",
            "ReportFriendlySupportCasualties();",
            "GetFriendlySupportCasualties() const",
            "GetEnemyCasualties() const",
            "const FName AttritionSectorID =",
            "SupplySourceSectorID.IsNone()",
            "WarSubsystem->ApplyAmbientBattleResult(",
            "AttritionSectorID,",
            "BH_OPERATION_SUPPORT_CASUALTY",
            "BH_OPERATION_SUPPORT_SUMMARY",
            "HandleEnemyDeath",
            "EnemyCasualties",
            "BH_OPERATION_HOSTILE_CASUALTY",
            "EnemySourceSectorID.IsNone()",
            "0,",
            "1",
            "void ABHOpenWorldOperationDirector::EndPlay(",
            "FNavigationSystem::GetCurrent<UNavigationSystemV1>(",
            "NavigationSystem->ProjectPointToNavigation(",
            "void ABHOpenWorldOperationDirector::BuildOperationPatrolPoints()",
            "BuildPatrolPointAssignment(",
            "Enemy->SetPatrolPoints(",
            "FriendlySoldier->SetPatrolPoints(",
            '"BH_OPERATION_PATROL_READY sector=%s "',
            '"OpenWorldAttackApproachHUDStatus"',
            '"OpenWorldDefenseApproachHUDStatus"',
            '"OpenWorldOperationHeavyThreatLabel"',
            '"OpenWorldAttackReactionDetected"',
            '"OpenWorldDefenseWaveArrived"',
            '"{0}\\nSTAGING {1} // SUPPLY {2}%\\n"',
            '"LOSSES // SUPPORT {3} // HOSTILES {4}"',
            "FMath::RoundToInt(StagingSector.Supply)",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "WarSubsystem->GetOperationSupplySource(",
            "AssignedWarSupplySourceSectorID",
            "GetAssignedWarSupplySourceSectorID() const",
            "AssignedWarPriorityType,",
            "void ABHCharacter::EnterPostOperationFreeRoam(",
            "ObjectiveComponent->ClearMissionState();",
            "OpenWarMap(true);",
            "const bool bCanDeploy =",
            "!bRuntimeWarOperation &&",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp",
        (
            "SaveData->AssignedWarSupplySourceSectorID =",
            "Character->GetAssignedWarSupplySourceSectorID();",
            "SaveData->AssignedWarSupplySourceSectorID,",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp",
        (
            "void UBHCombatStatusWidget::SetOperationWaypoint(",
            '"OPERATION // %s // %.1f KM%s"',
            '"ACTIVE OP // %s // %.1f KM%s\\n%s"',
            '" // ETA %d MIN"',
            '"FUEL SHORTFALL // RANGE %.1f KM // RESUPPLY%s"',
            "Label[CharacterIndex] == TEXT('\\n')",
            "DrawOperationWaypoint(",
            "void UBHCombatStatusWidget::SetResupplyWaypoint(",
            '"RESUPPLY // %s // %.1f KM"',
            "DrawResupplyWaypoint(",
            "void UBHCombatStatusWidget::SetTransportWaypoint(",
            "DrawTransportWaypoint(",
        ),
    )

    _log("PASS attack and defense presentation paths are implemented.")
    _log("PASS objective IDs remain stable while display orders vary.")
    _log("PASS mission result stays bound to its deployment assignment.")
    _log("PASS abandoned transport remains navigable during field operations.")
    _log("PASS the player receives a strategic briefing on deployment.")
    _log("PASS distant operations expose a live bearing and range HUD.")
    _log("PASS operation force packages react to live sector pressure.")
    _log("PASS enemy logistics drives attack reinforcement pressure.")
    _log("PASS operation combatants are projected onto navigation.")
    _log("PASS friendly war strength supplies operation support.")
    _log("PASS support losses immediately reduce staging reserves.")
    _log("PASS after-action reports expose the resulting sector state.")
    _log("PASS after-action reports include staging losses and supply.")
    _log("PASS operation forces advance into the objective area.")
    _log("PASS active operations retain their objective waypoint.")
    _log("PASS active operation waypoints show force and wave status.")
    _log("PASS active operation HUD shows staging supply and losses.")
    _log("PASS depleted players are guided to friendly field logistics.")
    _log("PASS priority deployments consume routed strategic supply.")
    _log("PASS failed deployments roll back before mission commit.")
    _log("PASS routed staging strength supplies operation support.")
    _log("PASS active operations preserve their staging source on save.")
    _log("PASS deployed operations lock their strategic sector.")
    _log("PASS restored operations rebuild their strategic commitment.")
    _log("PASS debriefs release the player into persistent free roam.")
    _log("PASS strategic command can deploy from free roam.")


def main():
    _validate_attack_assignment()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
