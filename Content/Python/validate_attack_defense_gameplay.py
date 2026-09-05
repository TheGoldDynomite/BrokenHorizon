"""Validate distinct attack and defense mission gameplay."""

import os

import unreal


def _log(message):
    unreal.log("[BH Attack Defense Validation] " + message)


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


def _require_ordered_fragments(relative_path, contract_name, fragments):
    source = _read(relative_path)
    search_start = 0

    for fragment in fragments:
        fragment_index = source.find(fragment, search_start)
        if fragment_index < 0:
            raise RuntimeError(
                "%s contract is missing or out of order in %s: %s"
                % (contract_name, relative_path, fragment)
            )
        search_start = fragment_index + len(fragment)


def _validate_reflection():
    director_class = getattr(
        unreal,
        "BHDefenseMissionDirector",
        None,
    )

    if not director_class:
        raise RuntimeError(
            "BHDefenseMissionDirector is not reflected."
        )

    defaults = unreal.get_default_object(director_class)

    if int(defaults.get_editor_property("total_waves")) != 3:
        raise RuntimeError("Defense should contain three waves.")

    if int(
        defaults.get_editor_property("reinforcements_per_wave")
    ) != 2:
        raise RuntimeError(
            "Defense reinforcement wave size should be two."
        )

    operation_class = getattr(
        unreal,
        "BHOpenWorldOperationDirector",
        None,
    )

    if not operation_class:
        raise RuntimeError(
            "BHOpenWorldOperationDirector is not reflected."
        )

    operation_defaults = unreal.get_default_object(
        operation_class
    )

    if abs(
        float(
            operation_defaults.get_editor_property(
                "objective_secure_radius"
            )
        ) - 650.0
    ) > 0.01:
        raise RuntimeError(
            "Attack objective secure radius should be 650 cm."
        )

    if abs(
        float(
            operation_defaults.get_editor_property(
                "objective_secure_duration"
            )
        ) - 8.0
    ) > 0.01:
        raise RuntimeError(
            "Attack objectives should require an eight-second hold."
        )

    if abs(
        float(
            operation_defaults.get_editor_property(
                "defense_breach_duration"
            )
        ) - 20.0
    ) > 0.01:
        raise RuntimeError(
            "Defense breaches should allow twenty seconds to recover."
        )

    if abs(
        float(
            operation_defaults.get_editor_property(
                "defense_breach_checkpoint_interval"
            )
        ) - 5.0
    ) > 0.01:
        raise RuntimeError(
            "Defense breach checkpoints should update every five seconds."
        )

    _log("PASS defense director and wave defaults are reflected.")
    _log("PASS attack objective securing defaults are reflected.")
    _log("PASS defense breach timing is reflected.")


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/Public/BHEnemySoldier.h",
        (
            "void SetObjectiveIdToCompleteOnDeath(FName ObjectiveID);",
            "FName GetObjectiveIdToCompleteOnDeath() const;",
            "EditInstanceOnly,",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHDefenseMissionDirector.cpp",
        (
            "CaptureExistingDefenders();",
            "Enemy->SetObjectiveIdToCompleteOnDeath(NAME_None);",
            "World->SpawnActorDeferred<ABHEnemySoldier>(",
            '"DefenseReinforcementWave"',
            "PlayerCharacter->CompleteObjective(",
            "BHObjectiveIds::EliminateGuard",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "AssignedWarPriorityType == EBHWarPriorityType::Defend",
            "SpawnActor<ABHDefenseMissionDirector>()",
            "DefenseMissionDirector->InitializeDefenseMission(this);",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHOpenWorldOperationDirector.h",
        (
            "int32 AttackReinforcementWaveCount = 1;",
            "int32 AttackReinforcementsPerWave = 1;",
            "float AttackInterWaveDelay = 8.0f;",
            "float ObjectiveSecureRadius = 650.0f;",
            "float ObjectiveSecureDuration = 8.0f;",
            "float ObjectiveSecureDecayMultiplier = 2.0f;",
            "float DefenseBreachDuration = 20.0f;",
            "float DefenseBreachRecoveryMultiplier = 2.0f;",
            "float DefenseBreachCheckpointInterval = 5.0f;",
            "int32 EffectiveAttackReinforcementWaveCount = 1;",
            "int32 EffectiveAttackReinforcementsPerWave = 1;",
            "void UpdateAttackObjectiveSecuring(float DeltaSeconds);",
            "void UpdateDefenseObjectiveHolding(float DeltaSeconds);",
            "void UpdateDefenseBreach(float DeltaSeconds);",
            "bool IsPlayerInsideSecureArea() const;",
            "bool IsSecureAreaContested() const;",
            "bool HasLivingDefenderInSecureArea() const;",
            "void FailOperation(const FText& FailureReason);",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarOperationRules.h",
        (
            "struct BROKENHORIZON_API FBHWarOperationForcePackage",
            "BuildForcePackage(",
            "int32 AttackEnemyCount = 3;",
            "int32 DefenseWaveCount = 3;",
            "int32 FriendlySupportCount = 0;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWarOperationRules.cpp",
        (
            "MaximumAttackReinforcementWaves",
            "MaximumAttackReinforcementWaveSize",
            "Package.AttackEnemyCount = FMath::Clamp(",
            "Package.DefenseWaveCount = FMath::Clamp(",
            "Package.FriendlySupportCount = FMath::Clamp(",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        (
            "BHWarOperationRules::BuildForcePackage(",
            "ForcePackage.AttackEnemyCount",
            "ForcePackage.DefenseWaveCount",
            "ForcePackage.FriendlySupportCount",
            "1 + EffectiveAttackReinforcementWaveCount",
            "GetSecondsUntilNextWave()",
            '"OpenWorldAttackApproachHUDStatus"',
            '"OpenWorldDefenseApproachHUDStatus"',
            '"OpenWorldOperationHeavyThreatLabel"',
            '"{0} // {1} THREAT // WAVE {2}/{3} CLEAR\\n"',
            '"OpenWorldDefenseWaveCleared"',
            '"OpenWorldAttackReactionDetected"',
            '"OpenWorldDefenseWaveArrived"',
            '"OpenWorldAttackReactionArrived"',
            "const int32 IncomingEnemyCount =",
            "? DefenseInterWaveDelay",
            ": AttackInterWaveDelay",
            "? EffectiveDefenseEnemiesPerWave",
            ": EffectiveAttackReinforcementsPerWave",
            '"Expected enemy waves: {2}.\\n"',
            "attack_reinforcement_waves=%d",
            '"OpenWorldAttackSecureHUDStatus"',
            '"OpenWorldAttackSecureAreaContested"',
            '"CONTESTED // CLEAR HOSTILES"',
            "UpdateAttackObjectiveSecuring(DeltaSeconds);",
            "UpdateDefenseObjectiveHolding(DeltaSeconds);",
            "UpdateDefenseBreach(DeltaSeconds);",
            '"OpenWorldDefenseHoldHUDStatus"',
            '"RETURN TO DEFENSIVE LINE"',
            '"BH_OPERATION_DEFENSE_HOLD_STARTED sector=%s "',
            '"BH_OPERATION_DEFENSE_HOLD_CONFIRMED sector=%s "',
            "bPlayerInsideSecureArea && !bSecureAreaContested",
            "TActorIterator<ABHEnemySoldier>",
            "EBHCombatFaction::Hostile",
            '"BH_OPERATION_SECURING_STARTED sector=%s "',
            '"BH_OPERATION_OBJECTIVE_SECURED sector=%s "',
            "HasPlayerOrFieldOperativeInSecureArea()",
            "State.bSecuringObjective = bSecuringObjective;",
            "State.ObjectiveSecureProgress =",
            "SavedState.bSecuringObjective",
            "SavedState.ObjectiveSecureProgress",
            "State.DefenseBreachProgress =",
            "SavedState.DefenseBreachProgress",
            '"OpenWorldDefenseBreachHUDStatus"',
            '"OpenWorldDefenseBreachRecoveringHUDStatus"',
            '"OpenWorldDefenseBreachWarning"',
            '"OpenWorldDefenseBreachRecovered"',
            '"BH_OPERATION_DEFENSE_BREACH_STARTED "',
            '"BH_OPERATION_DEFENSE_BREACH_RECOVERED "',
            "LastCheckpointedDefenseBreachProgress",
            "DefenseBreachCheckpointInterval",
            "PlayerCharacter->FailCurrentWarOperation(FailureReason)",
            '"OpenWorldDefenseBreachFailureReason"',
        ),
    )
    _require_ordered_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        "Shared operation event notification fan-out",
        (
            "void ABHOpenWorldOperationDirector::ShowOperationNotification(",
            "for (ABHCharacter* Participant : GetPlayerParticipants())",
            "Participant->ShowStatusNotification(Message);",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        (
            '"OpenWorldDefenseStarted"',
            '"OpenWorldDefenseWaveCleared"',
            '"OpenWorldDefenseWaveArrived"',
            '"OpenWorldHostileCasualty"',
        ),
    )
    _log(
        "PASS operation activation, wave, and casualty notifications fan out "
        "to authoritative player participants."
    )
    _require_ordered_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        "Operation completion idempotence",
        (
            "void ABHOpenWorldOperationDirector::CompleteOperation()",
            "if (!HasAuthority() || bOperationComplete)",
            "bOperationComplete = true;",
        ),
    )
    _log("PASS operation completion is guarded against duplicate finalization.")
    _require_ordered_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        "Shared operation completion",
        (
            "void ABHOpenWorldOperationDirector::CompleteOperation()",
            "if (!PlayerCharacter->CompleteSharedObjective(",
            "BHObjectiveIds::EliminateGuard",
            "for (ABHCharacter* Participant : GetPlayerParticipants())",
        ),
    )
    _log(
        "PASS operation completion propagates the final objective to "
        "all authoritative player participants."
    )
    _require_ordered_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        "Debrief objective HUD suppression",
        (
            "void ABHCharacter::EnterMissionCompleteState(",
            "if (IsValid(ObjectiveWidget))",
            "ObjectiveWidget->SetVisibility(",
            "ESlateVisibility::Collapsed",
        ),
    )
    _log("PASS the active objective HUD is hidden during debrief lockout.")
    _require_ordered_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        "Shared operation failure propagation",
        (
            "void ABHOpenWorldOperationDirector::FailOperation(",
            "if (!PlayerCharacter->FailCurrentWarOperation(FailureReason))",
            "PlayerCharacter->PropagateSharedOperationFailure();",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "void ABHCharacter::FailSharedOperationObjectives()",
            '"BH_SHARED_OPERATION_FAILURE_PROPAGATED "',
            "void ABHCharacter::PropagateSharedOperationFailure()",
            '"BH_SHARED_OPERATION_FAILURE_DEBRIEF "',
        ),
    )
    _log(
        "PASS operation failure propagates objective failure without "
        "reapplying the strategic war result and presents one shared debrief."
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHSupplyConvoyTarget.cpp",
        (
            "AssignedCharacter->FailCurrentWarOperation(",
            "AssignedCharacter->PropagateSharedOperationFailure();",
        ),
    )
    _log(
        "PASS escort convoy failure uses the shared objective/debrief path."
    )
    _require_ordered_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        "Replicated terminal mission handoff",
        (
            "void ABHCharacter::RefreshReplicatedMissionPresentation()",
            "const bool bTerminalMissionState =",
            "IsMissionComplete() || IsMissionFailed()",
            "EnterMissionCompleteState(false);",
        ),
    )
    _log(
        "PASS replicated terminal mission state re-enters the debrief "
        "presentation on reconnecting clients."
    )
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarTypes.h",
        (
            "bool bSecuringObjective = false;",
            "float ObjectiveSecureProgress = 0.0f;",
            "float DefenseBreachProgress = 0.0f;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp",
        (
            "if (OperationStatusText.IsEmpty())",
            '"OPERATION // %s // %.1f KM%s\\n"',
            '"ACTIVE OP // %s // %.1f KM%s\\n%s"',
        ),
    )

    _log("PASS attack leaves the original clear operation intact.")
    _log("PASS attack gains supply-driven reinforcement waves.")
    _log("PASS attack victory requires securing the objective area.")
    _log("PASS attack securing progress persists in checkpoints.")
    _log("PASS defense victory requires confirming control of the held area.")
    _log("PASS an undefended objective can fail the defense operation.")
    _log("PASS defense breach progress persists in checkpoints.")
    _log("PASS defense takes ownership of combat objective completion.")
    _log("PASS defense launches reinforcement waves from level anchors.")


def _validate_defense_a_garrison_lifecycle():
    _require_fragments(
        "Source/BrokenHorizon/Public/BHEnemySoldier.h",
        (
            "void SetOperationGarrisonActive(bool bNewOperationGarrisonActive);",
            "bool IsOperationGarrisonActive() const;",
            "void OnRep_OperationGarrisonActive();",
            "UPROPERTY(ReplicatedUsing = OnRep_OperationGarrisonActive)",
            "bool bOperationGarrisonActive = true;",
            "bool bIsOperationGarrison = false;",
        ),
    )
    _require_ordered_fragments(
        "Source/BrokenHorizon/Private/BHEnemySoldier.cpp",
        "Defense A soldier replication gate",
        (
            "DOREPLIFETIME(ABHEnemySoldier, bOperationGarrisonActive);",
            "bIsOperationGarrison = ActorHasTag(",
            'FName(TEXT("BH_Auto_DefenseA_Garrison"))',
            "if (bIsOperationGarrison)",
            "SetObjectiveIdToCompleteOnDeath(NAME_None);",
            "if (bIsOperationGarrison && HasAuthority() && FieldOperativeID.IsNone())",
            "const FString GarrisonIdentitySource = GetFName().ToString();",
            "SetFieldOperativeID(FName(*FString::Printf(",
            'TEXT("DefenseA_%s")',
            "SaveSubsystem->ApplyPendingSurrenderState(this);",
            "void ABHEnemySoldier::OnRep_OperationGarrisonActive()",
            "ApplyOperationGarrisonState();",
            "const bool bShouldBeActive = bOperationGarrisonActive && !IsDead();",
            "EnemyController->SetOperationGarrisonActive(bShouldBeActive);",
        ),
    )
    _require_ordered_fragments(
        "Source/BrokenHorizon/Private/BHEnemyAIController.cpp",
        "Defense A controller pause and resume",
        (
            "void ABHEnemyAIController::SetOperationGarrisonActive(bool bActive)",
            "if (!bOperationGarrisonActive)",
            "CombatTarget.Reset();",
            "ClearFollowTarget();",
            "ClearHoldPosition();",
            "ClearFocus(EAIFocusPriority::Gameplay);",
            "StopMovement();",
            "ClearTimer(PatrolWaitTimerHandle);",
            "AIPerception->ForgetAll();",
            "AIPerception->SetActive(false);",
            "SetActorTickEnabled(false);",
            "AIPerception->SetActive(true);",
            "AIPerception->RequestStimuliListenerUpdate();",
            "SetActorTickEnabled(true);",
        ),
    )
    _require_ordered_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        "Defense A authored garrison lifecycle",
        (
            "bool ABHOpenWorldOperationDirector::RestoreOperationState(",
            "ConfigureAuthoredDefenseAGarrison(!bWaitingForWave);",
            "void ABHOpenWorldOperationDirector::ActivateOperation()",
            "ConfigureAuthoredDefenseAGarrison(true);",
            "bool ABHOpenWorldOperationDirector::ConfigureAuthoredDefenseAGarrison(",
            "if (!IsValid(World))",
            'FName(TEXT("BH_Auto_DefenseA_Garrison"))',
            "AuthoredEnemy->GetFieldOperativeID().IsNone()",
            "const FString GarrisonIdentitySource =",
            "AuthoredEnemy->GetFName().ToString();",
            "AuthoredEnemy->SetFieldOperativeID(FName(*FString::Printf(",
            'TEXT("DefenseA_%s")',
            "SaveSubsystem->ApplyPendingSurrenderState(AuthoredEnemy);",
            "if (!bActivateGarrison || AuthoredEnemy->IsDead())",
            "AuthoredEnemy->SetOperationGarrisonActive(false);",
            "AuthoredEnemy->SetOperationGarrisonActive(true);",
            "EnemyController->NotifyAllyAlert(PlayerCharacter);",
            "HealthComponent->OnDeath.AddUniqueDynamic(",
            "&ABHOpenWorldOperationDirector::HandleEnemyDeath",
            "TrackedEnemies.AddUnique(AuthoredEnemy);",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp",
        (
            "void ABHOpenWorldOperationDirector::DestroyTrackedUnits()",
            'Enemy->ActorHasTag(FName(TEXT("BH_Auto_DefenseA_Garrison")))',
            "HealthComponent->OnDeath.RemoveDynamic(",
            "Enemy->SetOperationGarrisonActive(false);",
        ),
    )
    _require_ordered_fragments(
        "Source/BrokenHorizon/Private/BHWarGameState.cpp",
        "First Light route excludes operation-only Defense A garrison",
        (
            "TArray<ABHEnemySoldier*> ObjectiveGuards;",
            "It->GetObjectiveIdToCompleteOnDeath() ==",
            "BHObjectiveIds::EliminateGuard",
            "!It->ActorHasTag(",
            'FName(TEXT("BH_Auto_DefenseA_Garrison"))',
        ),
    )

    _log("PASS Defense A soldiers use a replicated operation-garrison gate.")
    _log("PASS Defense A controllers clear stale state while paused and resume perception.")
    _log("PASS Defense A lifecycle restores, activates, and tracks authored garrison members.")
    _log("PASS Defense A teardown preserves tagged garrison actors.")
    _log("PASS First Light route excludes operation-only Defense A garrison actors.")


def _validate_defense_a_authored_id_contract():
    map_path = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
        raise RuntimeError("Missing First Light map: " + map_path)

    unreal.EditorLevelLibrary.load_level(map_path)
    world = unreal.EditorLevelLibrary.get_editor_world()
    enemy_class = unreal.load_class(
        None,
        "/Script/BrokenHorizon.BHEnemySoldier",
    )
    if not enemy_class:
        raise RuntimeError("BHEnemySoldier class is unavailable")

    expected_ids = {
        "DefenseA_FL_DefenseA_Garrison_01_West",
        "DefenseA_FL_DefenseA_Garrison_02_East",
        "DefenseA_FL_DefenseA_Garrison_03_NorthWest",
        "DefenseA_FL_DefenseA_Garrison_04_NorthEast",
        "DefenseA_FL_DefenseA_Garrison_05_OuterWest",
        "DefenseA_FL_DefenseA_Garrison_06_OuterEast",
    }
    garrison = [
        enemy for enemy in unreal.GameplayStatics.get_all_actors_of_class(
            world,
            enemy_class,
        )
        if "BH_Auto_DefenseA_Garrison" in [
            str(tag) for tag in enemy.tags
        ]
    ]
    actual_ids = {
        str(enemy.get_editor_property("field_operative_id"))
        for enemy in garrison
    }
    if len(garrison) != len(expected_ids) or actual_ids != expected_ids:
        raise RuntimeError(
            "Defense A authored identity contract mismatch: %s"
            % sorted(actual_ids)
        )
    _log("PASS Defense A authored garrison IDs are explicit and stable.")


def main():
    _validate_reflection()
    _validate_source_contract()
    _validate_defense_a_garrison_lifecycle()
    _validate_defense_a_authored_id_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
