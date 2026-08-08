"""Validate Broken Horizon campaign save and reset lifecycle contracts."""

import os

import unreal


PREFIX = "[BH Campaign Persistence Validation] "


def _log(message):
    unreal.log(PREFIX + message)


def _read(relative_path):
    path = os.path.join(unreal.Paths.project_dir(), relative_path)
    with open(path, "r", encoding="utf-8") as source_file:
        return source_file.read()


def _require_ordered(source, fragments, contract_name):
    cursor = 0

    for fragment in fragments:
        position = source.find(fragment, cursor)

        if position < 0:
            raise RuntimeError(
                "%s is missing or out of order: %s"
                % (contract_name, fragment)
            )

        cursor = position + len(fragment)


def _validate_return_to_command():
    character_source = _read(
        "Source/BrokenHorizon/BHCharacter.cpp"
    )
    function_start = character_source.find(
        "bool ABHCharacter::ReturnToMainMenu()"
    )

    if function_start < 0:
        raise RuntimeError("ReturnToMainMenu implementation is missing.")

    function_source = character_source[function_start:]
    _require_ordered(
        function_source,
        (
            "!SaveSubsystem->SaveProgress()",
            "if (bWarMapOpen)",
            "if (bPauseMenuOpen)",
            "UGameplayStatics::OpenLevel(this, FName(*PackageName));",
        ),
        "return-to-command checkpoint contract",
    )

    _log("PASS return to command saves before closing the live session.")


def _validate_new_campaign_reset():
    save_source = _read(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp"
    )
    function_start = save_source.find(
        "bool UBHSaveSubsystem::DeleteSaveGame()"
    )
    function_end = save_source.find(
        "bool UBHSaveSubsystem::RecordConsumedWorldItem",
        function_start,
    )

    if function_start < 0 or function_end < 0:
        raise RuntimeError("DeleteSaveGame implementation is missing.")

    function_source = save_source[function_start:function_end]
    _require_ordered(
        function_source,
        (
            "{&SaveSlotName, &BackupSaveSlotName}",
            "!UGameplayStatics::DeleteGameInSlot(",
            "if (!bDeleteSucceeded)",
            "ClearPendingWarAutosave(World);",
            "PendingSaveData = nullptr;",
            "PendingPlayerDeathAttritionSectorID = NAME_None;",
            "RuntimeConsumedWorldItemIDs.Reset();",
            "TGuardValue<bool> SuppressAutosaveGuard(",
            "WarSubsystem->ResetCampaign();",
            '"BH_CAMPAIGN_RESET slots=%s,%s "',
            "return true;",
        ),
        "new-campaign atomic reset contract",
    )

    _log("PASS failed deletion cannot partially reset the live campaign.")
    _log("PASS new campaign clears pending load, casualty, and autosave state.")
    _log("PASS war reset cannot schedule an old-campaign autosave.")


def _validate_checkpoint_recovery():
    header_source = _read(
        "Source/BrokenHorizon/Public/BHSaveSubsystem.h"
    )
    save_source = _read(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp"
    )

    for fragment in (
        "static const FString BackupSaveSlotName;",
        "UBHSaveGame* LoadBestSaveGame(",
        "bool SavePrimaryWithBackup(UBHSaveGame* SaveData) const;",
    ):
        if fragment not in header_source:
            raise RuntimeError(
                "checkpoint recovery header is missing: " + fragment
            )

    for fragment in (
        'TEXT("BrokenHorizon_Checkpoint_Backup")',
        "LoadBestSaveGame(&bUsedBackup)",
        "BH_CHECKPOINT_RECOVERY initialized campaign",
        "BH_CHECKPOINT_RECOVERY loading backup",
        "BH_CHECKPOINT_BACKUP slot=%s source=%s",
        "return IsValid(LoadBestSaveGame());",
        "if (!HasValidSaveGame())",
    ):
        if fragment not in save_source:
            raise RuntimeError(
                "checkpoint recovery implementation is missing: "
                + fragment
            )

    if save_source.count("SavePrimaryWithBackup(SaveData)") < 3:
        raise RuntimeError(
            "not every campaign write path preserves a backup"
        )

    _log("PASS every campaign write preserves a last-known-good save.")
    _log("PASS continue and initialization fall back to the backup save.")
    _log("PASS new campaign deletes both primary and backup checkpoints.")


def _validate_multiplayer_save_authority():
    save_source = _read(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp"
    )

    for fragment in (
        "BH_CAMPAIGN_SAVE_REJECTED_CLIENT",
        "BH_CAMPAIGN_SAVE_REJECTED_AUTHORITY",
        "BH_CAMPAIGN_LOAD_REJECTED_CLIENT",
        "BH_CAMPAIGN_RESET_REJECTED_CLIENT",
        "BH_CONSUMED_ITEM_SAVE_REJECTED_CLIENT",
        "World->GetNetMode() == NM_ListenServer",
        "World->GetNetMode() == NM_DedicatedServer",
        "World->ServerTravel(TravelURL, true)",
        "BH_CAMPAIGN_SERVER_TRAVEL_FAILED",
        "BH_CAMPAIGN_SERVER_TRAVEL level=%s",
    ):
        if fragment not in save_source:
            raise RuntimeError(
                "multiplayer save authority contract is missing: "
                + fragment
            )

    load_start = save_source.find(
        "bool UBHSaveSubsystem::LoadProgress()"
    )
    load_end = save_source.find(
        "bool UBHSaveSubsystem::ReloadCheckpointAfterPlayerDeath",
        load_start,
    )

    if load_start < 0 or load_end < 0:
        raise RuntimeError("LoadProgress implementation is missing.")

    _require_ordered(
        save_source[load_start:load_end],
        (
            "if (IsClientCampaignWorld(World))",
            "PendingSaveData = SaveData;",
            "if (World->GetNetMode() == NM_ListenServer ||",
            "World->ServerTravel(TravelURL, true)",
            "return true;",
            "UGameplayStatics::OpenLevel(World, LevelToLoad);",
        ),
        "multiplayer-safe campaign load contract",
    )

    _log("PASS only authoritative worlds can mutate campaign saves.")
    _log("PASS host loads use server travel and retain connected clients.")


def _validate_deployment_checkpoint_transaction():
    save_source = _read(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp"
    )
    function_start = save_source.find(
        "bool UBHSaveSubsystem::DeployOperationForCharacter("
    )
    function_end = save_source.find(
        "bool UBHSaveSubsystem::HasSaveGame() const",
        function_start,
    )

    if function_start < 0 or function_end < 0:
        raise RuntimeError("DeployOperation implementation is missing.")

    function_source = save_source[function_start:function_end]
    _require_ordered(
        function_source,
        (
            "if (!SaveProgressForCharacter(RequestingCharacter))",
            '"BH_DEPLOYMENT_CHECKPOINT_BLOCKED sector=%s "',
            "RequestingCharacter->BeginOperationInWorld(",
            "const bool bDeploymentCheckpointSaved =",
            "SaveProgressForCharacter(RequestingCharacter);",
            "if (!bDeploymentCheckpointSaved)",
            '"BH_DEPLOYMENT_CHECKPOINT_RETRY_PENDING "',
            "return true;",
            '"BH_DEPLOYMENT_CHECKPOINT_COMMITTED sector=%s "',
        ),
        "deployment checkpoint transaction",
    )

    _log("PASS deployment checkpoints before committing war resources.")
    _log("PASS an active operation is not misreported as a failed deploy.")
    _log("PASS failed post-deploy saves retain a safe recovery checkpoint.")

def _validate_campaign_epilogue_persistence():
    save_header = _read(
        "Source/BrokenHorizon/Public/BHSaveGame.h"
    )
    war_types_header = _read(
        "Source/BrokenHorizon/Public/BHWarTypes.h"
    )
    save_source = _read(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp"
    )
    character_source = _read(
        "Source/BrokenHorizon/BHCharacter.cpp"
    )

    for fragment in (
        "CurrentSchemaVersion = 37;",
        "struct FBHConvoySalvageSaveState",
        "TArray<FBHConvoySalvageSaveState> ConvoySalvageStates;",
        "int32 SurvivingSecurityCount = 0;",
        "bool bCampaignEpilogueAcknowledged = false;",
        "bool bOperationDebriefAcknowledged = false;",
        "bool bFieldSquadHasCommandLocation = false;",
        "FVector FieldSquadCommandLocation = FVector::ZeroVector;",
        "float FieldSquadCommandYaw = 0.0f;",
        "bool bFieldSquadEmbarked = false;",
        "FName FieldSquadTransportPersistenceID = NAME_None;",
    ):
        if fragment not in save_header:
            raise RuntimeError(
                "campaign epilogue save contract is missing: "
                + fragment
            )

    for fragment in (
        "bool bFriendlySupportHasCommandLocation = false;",
        "FVector FriendlySupportCommandLocation = FVector::ZeroVector;",
        "float FriendlySupportCommandYaw = 0.0f;",
        "bool bEmbarked = false;",
    ):
        if fragment not in war_types_header:
            raise RuntimeError(
                "operation command save contract is missing: "
                + fragment
            )

    _require_ordered(
        save_source,
        (
            "SaveData->bCampaignEpilogueAcknowledged =",
            "Character->IsCampaignEpilogueAcknowledged();",
            "Character->RestoreCampaignEpilogueAcknowledgement(",
            "SaveData->bCampaignEpilogueAcknowledged",
        ),
        "campaign epilogue round-trip contract",
    )
    _require_ordered(
        save_source,
        (
            "SaveData->bOperationDebriefAcknowledged =",
            "Character->IsOperationDebriefAcknowledged();",
            "Character->RestoreOperationDebriefAcknowledgement(",
            "SaveData->bOperationDebriefAcknowledged",
        ),
        "post-operation free-roam round-trip contract",
    )
    _require_ordered(
        save_source,
        (
            "SaveData->bFieldSquadHasCommandLocation =",
            "Character->HasFieldSquadCommandLocation();",
            "SaveData->FieldSquadCommandLocation =",
            "Character->GetFieldSquadCommandLocation();",
            "SaveData->FieldSquadCommandYaw =",
            "Character->GetFieldSquadCommandYaw();",
            "SaveData->bFieldSquadEmbarked =",
            "Character->IsFieldSquadEmbarked();",
            "SaveData->FieldSquadTransportPersistenceID =",
            "Character->GetFieldSquadTransportPersistenceID();",
            "bSavedFieldSquadHasCommandLocation",
            "SaveData->FieldSquadCommandLocation",
            "SaveData->FieldSquadCommandYaw",
            "bSavedFieldSquadEmbarked",
            "SaveData->FieldSquadTransportPersistenceID",
            "Character->RestoreFieldSquadState(",
            "bRestoreFieldSquadPassengers",
        ),
        "field squad order and passenger round-trip contract",
    )
    _require_ordered(
        character_source[
            character_source.find(
                "void ABHCharacter::HandleMissionContinueRequested()"
            ):
        ],
        (
            "WarSubsystem->IsCampaignResolved();",
            "bCampaignEpilogueAcknowledged = bResolvedCampaign;",
            "bOperationDebriefAcknowledged = !bResolvedCampaign;",
            "!SaveSubsystem->SaveProgress()",
            "EnterCampaignEpilogueFreeRoam(true);",
        ),
        "campaign epilogue acknowledgement contract",
    )

    _log("PASS campaign epilogue acknowledgement persists across loads.")
    _log("PASS final debrief returns to playable strategic review.")
    _log("PASS ordinary debriefs return to persistent free roam.")


def _validate_deployment_resource_continuity():
    character_source = _read(
        "Source/BrokenHorizon/BHCharacter.cpp"
    )
    function_start = character_source.find(
        "bool ABHCharacter::BeginOperationInWorld("
    )
    function_end = character_source.find(
        "bool ABHCharacter::IsWarMapOpen() const",
        function_start,
    )

    if function_start < 0 or function_end < 0:
        raise RuntimeError(
            "BeginOperationInWorld implementation is missing."
        )

    deployment_source = character_source[
        function_start:function_end
    ]

    for forbidden in (
        "HealthComponent->ResetHealth();",
        "InjuryComponent->ResetInjuries();",
    ):
        if forbidden in deployment_source:
            raise RuntimeError(
                "deployment grants free field recovery: " + forbidden
            )

    for required in (
        "ApplyMovementSpeed();",
        "BH_DEPLOYMENT_FIELD_READINESS",
        "HealthComponent->GetCurrentHealth()",
        "WeaponComponent->GetMagazineAmmo()",
        "WeaponComponent->GetReserveAmmo()",
        "InjuryComponent->GetMedkitCount()",
        "InjuryComponent->GetFieldDressingCount()",
        "InjuryComponent->IsBleeding()",
        "InjuryComponent->IsArmInjured()",
        "InjuryComponent->IsLegInjured()",
    ):
        if required not in deployment_source:
            raise RuntimeError(
                "deployment resource continuity is missing: "
                + required
            )

    _log("PASS deployment preserves health, injuries, ammo, and supplies.")


def _validate_debrief_widget_fallback():
    character_source = _read(
        "Source/BrokenHorizon/BHCharacter.cpp"
    )
    widget_header = _read(
        "Source/BrokenHorizon/Public/BHMissionCompleteWidget.h"
    )
    widget_source = _read(
        "Source/BrokenHorizon/Private/BHMissionCompleteWidget.cpp"
    )

    function_start = character_source.find(
        "void ABHCharacter::EnterMissionCompleteState("
    )
    function_end = character_source.find(
        "void ABHCharacter::EnterCampaignEpilogueFreeRoam(",
        function_start,
    )

    if function_start < 0 or function_end < 0:
        raise RuntimeError(
            "EnterMissionCompleteState implementation is missing."
        )

    debrief_source = character_source[
        function_start:function_end
    ]

    for required in (
        "TSubclassOf<UBHMissionCompleteWidget> DebriefWidgetClass",
        "UBHMissionCompleteWidget::StaticClass()",
        "BH_OPERATION_DEBRIEF_WIDGET_FAILED",
    ):
        if required not in debrief_source:
            raise RuntimeError(
                "debrief widget fallback is missing: " + required
            )

    if "virtual void NativeOnInitialized() override;" not in widget_header:
        raise RuntimeError(
            "native debrief initialization contract is missing."
        )

    for required in (
        "void UBHMissionCompleteWidget::NativeOnInitialized()",
        "WidgetTree->ConstructWidget<UBorder>",
        "WidgetTree->ConstructWidget<UTextBlock>",
        "WidgetTree->RootWidget = DebriefBackdrop;",
        "DebriefBackdrop->SetContent(MissionCompleteText);",
    ):
        if required not in widget_source:
            raise RuntimeError(
                "native debrief presentation is missing: " + required
            )

    _log("PASS operation debrief has a visible native fallback.")


def _validate_safe_field_autosave():
    character_header = _read(
        "Source/BrokenHorizon/BHCharacter.h"
    )
    character_source = _read(
        "Source/BrokenHorizon/BHCharacter.cpp"
    )
    save_header = _read(
        "Source/BrokenHorizon/Public/BHSaveSubsystem.h"
    )
    save_source = _read(
        "Source/BrokenHorizon/Private/BHSaveSubsystem.cpp"
    )

    for required in (
        "bool CanCreateFieldAutosave() const;",
        "float LastPlayerDamageTimeSeconds = -BIG_NUMBER;",
    ):
        if required not in character_header:
            raise RuntimeError(
                "field autosave safety contract is missing: "
                + required
            )

    for required in (
        "bool ABHCharacter::CanCreateFieldAutosave() const",
        "LastPlayerDamageTimeSeconds > -BIG_NUMBER",
        "World->GetTimeSeconds() -",
        "LastPlayerDamageTimeSeconds < 20.0f",
        "!bIsHandlingDeath",
        "!bIsHandlingMissionComplete",
        "!bPauseMenuOpen",
        "!bWarMapOpen",
        "!bIsTraversing",
        "!bWaitingForInitialWorldStreaming",
        "HealthComponent->GetHealthPercentage() >= 0.5f",
        "!InjuryComponent->IsBleeding()",
        "!InjuryComponent->IsMedkitTreatmentActive()",
        "!WeaponComponent->IsFiring()",
        "!WeaponComponent->IsReloading()",
        "LastPlayerDamageTimeSeconds = World->GetTimeSeconds();",
    ):
        if required not in character_source:
            raise RuntimeError(
                "field autosave safety guard is missing: "
                + required
            )

    for required in (
        "void ScheduleFieldAutosave(UWorld* World);",
        "void PerformFieldAutosave();",
        "FTimerHandle FieldAutosaveTimerHandle;",
        "float FieldAutosaveIntervalSeconds = 90.0f;",
    ):
        if required not in save_header:
            raise RuntimeError(
                "field autosave schedule is missing: " + required
            )

    for required in (
        "ScheduleFieldAutosave(LoadedWorld);",
        "void UBHSaveSubsystem::ScheduleFieldAutosave(UWorld* World)",
        "&UBHSaveSubsystem::PerformFieldAutosave",
        "void UBHSaveSubsystem::PerformFieldAutosave()",
        "!Character->CanCreateFieldAutosave()",
        "BH_FIELD_AUTOSAVE_DEFERRED",
        "BH_FIELD_AUTOSAVE_COMPLETE",
        "SaveData->PlayerTransform = Character->GetActorTransform();",
        "SaveData->FieldTransportStates.Add(TransportState);",
        "SaveData->ConvoySalvageStates.Add(SalvageState);",
        "Director->GetSurvivingConvoySecurityCount(",
        "World->SpawnActor<ABHSupplyConvoyTarget>(",
        "RestoredWreck->RestoreSalvageWreck(",
        "RestoreSupplyConvoySalvageSecurity(",
        "CapturePlayerResourceState(SaveData, Character);",
        "CaptureWarState(SaveData, GetGameInstance());",
    ):
        if required not in save_source:
            raise RuntimeError(
                "field autosave implementation is missing: "
                + required
            )

    convoy_source = _read(
        "Source/BrokenHorizon/Private/BHSupplyConvoyTarget.cpp"
    )
    function_start = convoy_source.find(
        "void ABHSupplyConvoyTarget::HandleConvoyDestroyed("
    )
    function_end = convoy_source.find(
        "void ABHSupplyConvoyTarget::UpdateConvoyLabel()",
        function_start,
    )

    if function_start < 0 or function_end < 0:
        raise RuntimeError(
            "convoy destruction transaction is missing."
        )

    _require_ordered(
        convoy_source[function_start:function_end],
        (
            "RecoverableSupply =",
            "EnableWreckInteraction();",
            "SetLifeSpan(FMath::Max(1.0f, SalvageLifetime));",
            "SaveSubsystem->SaveProgress()",
        ),
        "convoy salvage checkpoint transaction",
    )

    _log(
        "PASS safe field autosave preserves travel and logistics progress."
    )
    _log(
        "PASS convoy checkpoints include the recoverable wreck "
        "before the save is written."
    )


def _validate_operation_mobilization_deadline():
    war_types_source = _read(
        "Source/BrokenHorizon/Public/BHWarTypes.h"
    )
    director_header = _read(
        "Source/BrokenHorizon/Public/BHOpenWorldOperationDirector.h"
    )
    director_source = _read(
        "Source/BrokenHorizon/Private/BHOpenWorldOperationDirector.cpp"
    )
    character_source = _read(
        "Source/BrokenHorizon/BHCharacter.cpp"
    )
    combat_header = _read(
        "Source/BrokenHorizon/Public/BHCombatStatusWidget.h"
    )
    combat_source = _read(
        "Source/BrokenHorizon/Private/BHCombatStatusWidget.cpp"
    )

    if (
        "float SecondsUntilApproachDeadline = -1.0f;"
        not in war_types_source
    ):
        raise RuntimeError(
            "operation approach deadline is not persisted"
        )

    for required in (
        "float DefenseApproachBaseSeconds = 120.0f;",
        "float OffensiveApproachBaseSeconds = 240.0f;",
        "float ExpectedApproachTravelSpeed = 1800.0f;",
        "float ApproachTravelTimeMultiplier = 2.0f;",
        "float MaximumApproachSeconds = 900.0f;",
        "float CalculateApproachWindowSeconds(",
        "float GetApproachSecondsRemaining() const;",
        "float ApproachDeadlineTime = -1.0f;",
    ):
        if required not in director_header:
            raise RuntimeError(
                "mobilization deadline declaration is missing: "
                + required
            )

    for required in (
        "CalculateApproachWindowSeconds(PlayerDistance)",
        "ApproachDeadlineTime = IsValid(World)",
        "BH_OPERATION_APPROACH_STARTED",
        "GetApproachSecondsRemaining()",
        "State.SecondsUntilApproachDeadline =",
        "SavedState.SecondsUntilApproachDeadline > 0.0f",
        "BH_OPERATION_APPROACH_EXPIRED",
        "OpenWorldDefenseApproachExpired",
        "OpenWorldOffensiveApproachExpired",
        "ApproachDeadlineTime = -1.0f;",
    ):
        if required not in director_source:
            raise RuntimeError(
                "mobilization deadline implementation is missing: "
                + required
            )

    _require_ordered(
        director_source,
        (
            "if (!bOperationActivated)",
            "GetApproachSecondsRemaining() <= 0.0f",
            "FailOperation(",
        ),
        "approach deadline strategic consequence",
    )

    for required in (
        "float GetApproachSecondsRemaining() const;",
    ):
        if required not in director_header:
            raise RuntimeError(
                "mobilization guidance API is missing: " + required
            )

    for required in (
        "void SetOperationArrivalDeadlineRisk(bool bAtRisk);",
        "bool bOperationArrivalDeadlineRisk = false;",
    ):
        if required not in combat_header:
            raise RuntimeError(
                "arrival-risk HUD declaration is missing: " + required
            )

    for required in (
        "void UBHCombatStatusWidget::SetOperationArrivalDeadlineRisk(",
        "LATE ARRIVAL RISK // ETA EXCEEDS WINDOW",
        "LATE ARRIVAL RISK // CHANGE VEHICLE OR ROUTE",
    ):
        if required not in combat_source:
            raise RuntimeError(
                "arrival-risk HUD implementation is missing: "
                + required
            )

    _require_ordered(
        character_source,
        (
            "ArrivalPreparationReserveSeconds",
            "ApproachSecondsRemaining =",
            "EstimatedTravelMinutes * 60.0f",
            ") > ApproachSecondsRemaining;",
            "CombatStatusWidget->SetOperationWaypoint(",
            "CombatStatusWidget->SetOperationArrivalDeadlineRisk(",
        ),
        "vehicle ETA versus mobilization deadline guidance",
    )

    _log(
        "PASS accepted operations cannot freeze the war indefinitely."
    )
    _log(
        "PASS mobilization timing scales with travel distance and survives saves."
    )
    _log(
        "PASS vehicle ETA and fuel range warn when arrival is at risk."
    )


def main():
    _validate_return_to_command()
    _validate_new_campaign_reset()
    _validate_checkpoint_recovery()
    _validate_multiplayer_save_authority()
    _validate_deployment_checkpoint_transaction()
    _validate_campaign_epilogue_persistence()
    _validate_deployment_resource_continuity()
    _validate_debrief_widget_fallback()
    _validate_safe_field_autosave()
    _validate_operation_mobilization_deadline()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
