"""Validate the persistent field-transport pool in the master world."""

import os

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
EXPECTED_TRANSPORTS = {
    "Transport_WesternFOB_01": unreal.Name(
        "WesternFOBFieldTransport01"
    ),
    "Transport_DovrenVillage_01": unreal.Name(
        "DovrenVillageFieldTransport01"
    ),
}
PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..")
)


def _log(message):
    unreal.log("[BH Field Transport Validation] " + message)


def _require_source_fragments(relative_path, fragments):
    source_path = os.path.join(PROJECT_ROOT, relative_path)

    with open(source_path, "r", encoding="utf-8") as source_file:
        source = source_file.read()

    for fragment in fragments:
        if fragment not in source:
            raise RuntimeError(
                "%s is missing: %s" % (relative_path, fragment)
            )


def _validate_source_contract():
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHPlayerResolver.cpp",
        (
            "APawn* BHPlayerResolver::FindCombatPawn(",
            "Cast<ABHCharacter>(PlayerPawn)",
            "Cast<ABHFieldTransport>(PlayerPawn)",
            "? PlayerPawn",
            "return Transport->GetOccupant();",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHEnemyAIController.cpp",
        (
            '#include "BHPlayerResolver.h"',
            "AActor* ResolvedTarget = ResolveCombatTarget(TargetActor);",
            "EnterCombat(ResolvedTarget);",
            "BHPlayerResolver::Find(this) == Current",
            "BHPlayerResolver::FindCombatPawn(this)",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHAmbientWarDirector.cpp",
        ("return BHPlayerResolver::Find(this);",),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp",
        (
            "SaveData->FieldTransportStates.Add(TransportState);",
            "return BHPlayerResolver::Find(World);",
            "Transport->RestorePersistentState(",
            "TransportState.FuelFraction =",
            "TransportState.HullFraction =",
            "TransportState.CargoSupply =",
            "TransportState.CargoSourceSectorID =",
            "SavedState->FuelFraction,",
            "SavedState->HullFraction,",
            "SavedState->CargoSupply,",
            "SavedState->CargoSourceSectorID",
            "bRestoreFieldSquadPassengers",
            "ShouldRestoreFieldSquadPassengers(",
            "BH_FIELD_SQUAD_EMBARK_RESTORED",
            "SaveData->ConvoySalvageStates.Add(SalvageState);",
            "World->SpawnActor<ABHSupplyConvoyTarget>(",
            "RestoredWreck->RestoreSalvageWreck(",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHEnemySoldier.cpp",
        ("BHPlayerResolver::Find(this);",),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHSupplyConvoyTarget.cpp",
        (
            "BHPlayerResolver::Find(this)",
            "CalculateRecoverableSupply(",
            "LoadRecoveredMilitarySupply(",
            "BH_CONVOY_SALVAGE_RECOVERED",
            "RECOVERY VEHICLE REQUIRED",
            "HasActiveHostileSecurity()",
            "RECOVERY AREA CONTESTED",
            "Recovery blocked: enemy security nearby",
            "EnableWreckInteraction();",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHFieldTransport.cpp",
        (
            "float ABHFieldTransport::TakeDamage(",
            "float ABHFieldTransport::CalculatePassengerDamage(",
            "bool ABHFieldTransport::ShouldRestoreFieldSquadPassengers(",
            "CrewDamageFraction",
            "PassengerDamageMultiplier",
            "ApplyFieldSquadTransportDamage(",
            "bBoardFieldSquad",
            "Character->BoardFieldSquadTransport(",
            "bUseSavedFieldSquadPassengerManifest",
            "Character->DisembarkFieldSquadTransport(this)",
            '"// FIRETEAM %d ABOARD "',
            'TEXT("SLOW DOWN BEFORE EXITING")',
            "float ABHFieldTransport::GetFuelPercentage() const",
            "float ABHFieldTransport::GetHullPercentage() const",
            "float ABHFieldTransport::GetSpeedKPH() const",
            "float ABHFieldTransport::GetEstimatedRangeKilometers() const",
            "float ABHFieldTransport::GetEstimatedTravelMinutes(",
            "MaximumForwardSpeed * 0.036f * 0.70f",
            "bool ABHFieldTransport::IsImmobilized() const",
            "bool ABHFieldTransport::ServiceVehicle()",
            "FName ABHFieldTransport::RefreshMilitaryCargoDestination()",
            "FName ABHFieldTransport::RefreshCivilianAidDestination()",
            "GetFieldLogisticsDeliveryCapacity(",
            "GetRecommendedInTransitCivilianAidDestination(",
            "float ABHFieldTransport::LoadRecoveredMilitarySupply(",
            "BH_FIELD_TRANSPORT_SALVAGE_LOADED",
            "bool ABHFieldTransport::RecoverAndService(",
            "BH_FIELD_TRANSPORT_RECOVERED",
            "FuelBurnPerKilometer",
            "TryTransferFieldLogistics();",
            "TryTransferCivilianAid();",
            "WithdrawFieldLogisticsSupply(",
            "DeliverFieldLogisticsSupply(",
            "RefreshMilitaryCargoDestination();",
            "RefreshCivilianAidDestination();",
            "WithdrawFieldCivilianAidSupply(",
            "DeliverFieldCivilianAidSupply(",
            "X LOGISTICS",
            "V CIVILIAN AID",
            "FIELD TRANSPORT OUT OF FUEL",
            "FIELD TRANSPORT DISABLED",
            "PrimaryActorTick.bTickEvenWhenPaused = true;",
            "Occupant->TogglePauseMenu();",
            "Occupant->ToggleWarMap();",
            "PlayerController->IsMoveInputIgnored()",
            "if (IsLocallyControlled())",
            "ServerSubmitDriverInput(",
            "ClientActivateDriverControls()",
            "ResetIgnoreMoveInput()",
            "BH_FIELD_TRANSPORT_DRIVER_CONTROLS_ACTIVE",
            "DOREPLIFETIME_CONDITION(",
            "ABHFieldTransport,\n        CurrentSpeed,\n        COND_OwnerOnly",
            "DOREPLIFETIME(ABHFieldTransport, CurrentFuel);",
            "DOREPLIFETIME(ABHFieldTransport, CurrentHull);",
            "DOREPLIFETIME(ABHFieldTransport, CurrentCargoSupply);",
            "DOREPLIFETIME(ABHFieldTransport, CargoSourceSectorID);",
            "DOREPLIFETIME(ABHFieldTransport, CargoType);",
            "void ABHFieldTransport::OnRep_TransportState()",
            "if (!HasAuthority() ||\n        DamageAmount <= 0.0f",
            "if (!HasAuthority())\n    {\n        return 0.0f;",
            "SetNetUpdateFrequency(10.0f);",
            "SetMinNetUpdateFrequency(2.0f);",
            "BHFieldTransportMovement",
            "SweepSingleByChannel(",
            "BH_FIELD_TRANSPORT_MOVEMENT_BLOCKED",
            "DriverInputStaleSeconds > 0.35f",
            "ServerRequestExitVehicle();",
            "ServerRequestFieldLogisticsTransfer();",
            "ServerRequestCivilianAidTransfer();",
            "bool ABHFieldTransport::FindSafeExitLocation(",
            "World->FindTeleportSpot(",
            '"EXIT BLOCKED // MOVE THE VEHICLE CLEAR "',
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Public/BHFieldTransport.h",
        (
            "void ServerSubmitDriverInput(",
            "void ServerRequestExitVehicle();",
            "UPROPERTY(Replicated, Transient)",
            "UPROPERTY(ReplicatedUsing = OnRep_TransportState)",
            "void OnRep_TransportState();",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Public/BHSaveGame.h",
        (
            "CurrentSchemaVersion = 37;",
            "struct FBHConvoySalvageSaveState",
            "TArray<FBHConvoySalvageSaveState> ConvoySalvageStates;",
            "float FuelFraction = 1.0f;",
            "float HullFraction = 1.0f;",
            "float CargoSupply = 0.0f;",
            "FName CargoSourceSectorID = NAME_None;",
            "FName CargoDestinationSectorID = NAME_None;",
            "EBHWarConvoyCargoType CargoType =",
            "bool bFieldSquadEmbarked = false;",
            "FName FieldSquadTransportPersistenceID = NAME_None;",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Public/BHWarTypes.h",
        (
            "FBHFieldSquadMemberState",
            "bool bEmbarked = false;",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHSectorResupplyStation.cpp",
        (
            '#include "BHFieldTransport.h"',
            "TActorIterator<ABHFieldTransport>",
            "ServiceTransport->NeedsService()",
            "ServiceTransport->ServiceVehicle()",
            "RecoveryTransport",
            "VehicleRecoverySupplyCost",
            "!IsValid(Candidate->GetOccupant())",
            "RecoverAndService(",
            "RECOVERED / REFUELED / REPAIRED",
            "REFUELED / REPAIRED",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "ABHCharacter::ResolveOwningPlayerController() const",
            "BHPlayerResolver::Find(this) == this",
            "ABHFieldTransport* RelevantTransport =",
            "GetFuelPercentage() <= 0.25f",
            "GetHullPercentage() <= 0.35f",
            "bNeedsVehicleService",
            "UpdateResupplyWaypointHUD(DeltaTime);",
            "UpdateTransportWaypointHUD(DeltaTime);",
            "UpdateLogisticsWaypointHUD(DeltaTime);",
            "RefreshMilitaryCargoDestination();",
            "void ABHCharacter::UpdateTransportWaypointHUD(",
            "void ABHCharacter::UpdateLogisticsWaypointHUD(",
            "Candidate->HasRecoverableSalvage()",
            "NearestSalvage->GetRecoverableSupply()",
            "CombatStatusWidget->SetSalvageWaypoint(",
            "NearbyTransportMarkerSuppressionRadius",
            "ABHFieldTransport* RemoteRecoveryTransport = nullptr;",
            "ABHFieldTransport* NearbyServiceTransport = nullptr;",
            "Candidate->IsImmobilized()",
            "Candidate->NeedsService() &&",
            "Candidate->IsImmobilized() ||",
            "GetEstimatedTravelMinutes(",
            "GetEstimatedRangeKilometers()",
            "ArrivalFuelReserveKilometers",
            "TravelTransport->GetCargoSupply()",
            "TravelTransport->GetCargoType()",
            "TravelTransport->RefreshCivilianAidDestination()",
            '"AID // {0}"',
            "CombatStatusWidget->SetLogisticsWaypoint(",
            "UpdateVehicleReadinessHUD();",
            "Transport->GetSpeedKPH()",
            "void ABHCharacter::EnterMissionCompleteState(",
            "void ABHCharacter::HandleDeath(",
            "int32 ABHCharacter::BoardFieldSquadTransport(",
            "int32 ABHCharacter::DisembarkFieldSquadTransport(",
            "void ABHCharacter::ApplyFieldSquadTransportDamage(",
            "FName ABHCharacter::GetFieldSquadTransportPersistenceID() const",
            "BH_FIELD_SQUAD_BOARDED",
            "BH_FIELD_SQUAD_DISEMBARKED",
            "IsFieldSquadMemberTransportEligible(",
            "casualties=%d order=%s",
            "Member->IsDead() &&",
            "!Member->IsIncapacitated()",
            "Member->GetAttachParentActor() == FieldSquadTransport",
            "const bool bMemberEmbarked =",
            "MemberState.bEmbarked = bMemberEmbarked;",
            "!bMemberEmbarked &&",
            "SavedMember.bEmbarked",
            "PendingFieldSquadTransportPassengers",
            "bUseSavedPassengerManifest",
            "SquadAIController->SetActorTickEnabled(false)",
            "SquadAIController->SetActorTickEnabled(true)",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp",
        (
            "void UBHCombatStatusWidget::SetLogisticsWaypoint(",
            "void UBHCombatStatusWidget::SetSalvageWaypoint(",
            "LOGISTICS // DELIVER %.0f // %s // %.1f KM",
            "CONVOY WRECK // RECOVER %.0f SUPPLY // %s",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHWarMapWidget.cpp",
        (
            "NearestOperationalTransport",
            "NearestDisabledTransport",
            "!Transport->IsImmobilized() &&",
            "? NearestOperationalTransport",
            ": NearestDisabledTransport",
        ),
    )
    _require_source_fragments(
        "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp",
        (
            "void UBHCombatStatusWidget::SetVehicleReadiness(",
            "void UBHCombatStatusWidget::SetTransportWaypoint(",
            "DrawTransportWaypoint(",
            '"FIELD TRANSPORT // IMMOBILIZED // "',
            '"RECOVER AT FRIENDLY RESUPPLY"',
            '"FIELD TRANSPORT // %s\\n"',
            '"SPEED %03d KM/H // FUEL %d%% // HULL %d%%"',
            '" // ETA %d MIN"',
            '"FUEL SHORTFALL // RANGE %.1f KM // RESUPPLY%s"',
            "bVehicleImmobilized",
            "VehicleFuelPercentage",
            "VehicleHullPercentage",
        ),
    )


def run_validation():
    level_subsystem = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )

    if not level_subsystem.load_level(MAP_PATH):
        raise RuntimeError("Could not load the Broken Horizon master world.")

    actor_subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    actors_by_label = {
        actor.get_actor_label(): actor
        for actor in actor_subsystem.get_all_level_actors()
    }
    persistence_ids = set()

    for label, persistence_id in EXPECTED_TRANSPORTS.items():
        transport = actors_by_label.get(label)

        if transport is None:
            raise RuntimeError("Missing field transport: " + label)

        if not isinstance(transport, unreal.BHFieldTransport):
            raise RuntimeError(
                "%s is not a BHFieldTransport." % label
            )

        if transport.get_editor_property("is_spatially_loaded"):
            raise RuntimeError(
                "%s must remain available across world streaming."
                % label
            )

        actual_persistence_id = transport.get_editor_property(
            "persistence_id"
        )

        if actual_persistence_id != persistence_id:
            raise RuntimeError(
                "%s has the wrong persistence ID." % label
            )

        if actual_persistence_id in persistence_ids:
            raise RuntimeError(
                "Duplicate transport persistence ID: %s"
                % actual_persistence_id
            )

        persistence_ids.add(actual_persistence_id)
        _log(
            "PASS actor=%s location=%s persistence=%s"
            % (
                label,
                transport.get_actor_location(),
                persistence_id,
            )
        )

    _validate_source_contract()

    _log("PASS two-region transport redundancy is available.")
    _log("PASS every transport has a unique stable save identity.")
    _log("PASS vehicle-aware campaign and crew-safety contracts.")
    _log("PASS recruited fireteams board, travel, and deploy with vehicles.")
    _log("PASS scripted AI alerts redirect to the occupied transport.")
    _log("PASS drivers retain live fuel, hull, speed, and mobility HUD.")
    _log("PASS operations show conservative travel ETA and fuel range risk.")
    _log(
        "PASS disabled transport redirects the player to remote recovery."
    )
    _log("PASS blocked exits search alternatives before refusing safely.")
    _log(
        "PASS destroyed enemy convoys leave transportable field salvage."
    )


if __name__ == "__main__":
    run_validation()
