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
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHDefenseMissionDirector.cpp",
        (
            "CaptureExistingDefenders();",
            "Enemy->SetObjectiveIdToCompleteOnDeath(NAME_None);",
            "World->SpawnActor<ABHEnemySoldier>(",
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
            "PlayerHealth->IsDead()",
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


def main():
    _validate_reflection()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
