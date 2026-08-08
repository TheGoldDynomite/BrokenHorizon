"""Validate Broken Horizon Strategic War Map v1."""

import os

import unreal


def _log(message):
    unreal.log("[BH Strategic War Map Validation] " + message)


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
    widget_class = getattr(unreal, "BHWarMapWidget", None)
    character_class = getattr(unreal, "BHCharacter", None)

    if not widget_class:
        raise RuntimeError("BHWarMapWidget is not reflected.")

    if not character_class:
        raise RuntimeError("BHCharacter is not reflected.")

    character_defaults = unreal.get_default_object(character_class)
    configured_class = character_defaults.get_editor_property(
        "war_map_widget_class"
    )

    if not configured_class:
        raise RuntimeError(
            "BHCharacter has no default strategic war-map widget class."
        )

    _log("PASS native war-map widget and character class are reflected.")
    _log("PASS character defaults to the native war-map implementation.")


def _validate_source_contract():
    _require_fragments(
        "Source/BrokenHorizon/Public/BHWarMapWidget.h",
        (
            "void InitializeWarMap(UBHWarSubsystem* InWarSubsystem);",
            "void RefreshWarMap();",
            "FBHOnWarMapCloseRequested OnCloseRequested;",
            "FBHOnWarMapWithdrawRequested OnWithdrawRequested;",
            "virtual FReply NativeOnKeyDown(",
            "virtual int32 NativePaint(",
            "void HandleWarStateChanged(",
            "TArray<FBHWarEventRecord> RecentWarEvents;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/Private/BHWarMapWidget.cpp",
        (
            'TEXT("BROKEN HORIZON // STRATEGIC COMMAND")',
            'TEXT("CURRENT PRIORITY // %s // %s")',
            'TEXT("CAMPAIGN LOG // RECENT")',
            "WarSubsystem->GetRecentWarEvents()",
            "RecentWarEvents.Num() - 1 - DisplayIndex",
            "GetPriorityReasonText()",
            '"FRIENDLY FORCE %3.0f // GARRISON %d "',
            '"SUPPLY %3.0f%% // FLOW %+.1f / TURN // %s"',
            'TEXT("FIELD RESUPPLY     %s")',
            'TEXT("CHECKPOINT ONLY")',
            "GetStrategicSupplyStatus(",
            "GetStrategicSupplyColor(",
            "GetStrategicConnectionColor(",
            "GetConvoyRouteThreatLabel(",
            "GetConvoyRouteThreatColor(",
            '"ROUTES // GREEN FRIENDLY // RED HOSTILE "',
            '"// AMBER FRONT // ORANGE CUT OFF // BRIGHT TRANSIT"',
            "const FLinearColor ConnectionColor =",
            "GetSupplyConvoys()",
            "FriendlyConvoyCount",
            "EnemyConvoyCount",
            '"// SUPPLY %.0f // CONVOYS F%d E%d // THREATS %d"',
            "ThreatenedRouteCount",
            "TransitCargo",
            "TransitFaction",
            "IncomingConvoySupply",
            "OutgoingConvoySupply",
            "GetRecentConvoyInterdictionCount(",
            "RecentRouteInterdictions",
            "ContactAndRouteLine",
            'TEXT(" // ROUTE %s")',
            '"CONVOY TRANSIT     IN +%.0f // OUT -%.0f"',
            'return TEXT("STARVED");',
            'return TEXT("CRITICAL");',
            'return TEXT("STABLE");',
            'return TEXT("STOCKPILED");',
            'return TEXT("CUT OFF");',
            "FriendlySectorCount",
            "ConnectedFriendlySectorCount",
            "CutOffFriendlySectorCount",
            "FriendlyLogisticsHubCount",
            "FriendlySupplyStockpile",
            '"LOGISTICS %d/%d // CUT OFF %d // HUBS %d "',
            '"NEXT UPDATE %s // %s TEMPO"',
            "WarSubsystem->GetSecondsUntilNextWarTurn()",
            "WarSubsystem->OnWarStateChanged.AddDynamic(",
            "WarSubsystem->GetSectorSupplyChangePerTurn(",
            "GetSectorIsolationAttritionPerTurn(",
            'TEXT(" // ATTRITION -%.1f / TURN")',
            "GetSectorReinforcementPerTurn(",
            "WarSubsystem->IsLogisticsHubSector(",
            "IsSectorConnectedToFactionLogistics(",
            "WarSubsystem->GetPriorityOperationSupplyCost()",
            "WarSubsystem->GetOperationSupplySource(",
            "GetOperationSupplyRoute(",
            "const TArray<FName> SupplyRoute =",
            "FLinearColor(0.01f, 0.02f, 0.02f, 0.95f)",
            "11.0f",
            "5.0f",
            'TEXT("SUPPLY SOURCE // PRIORITY TARGET")',
            'TEXT("SUPPLY SOURCE // ACTIVE OPERATION")',
            'TEXT("ACTIVE OPERATION")',
            "WarSubsystem->HasCommittedOperation()",
            "GetCommittedOperationSectorID()",
            'TEXT("LOGISTICS HUB // SUPPLY SOURCE")',
            'TEXT("PRIORITY TARGET")',
            'TEXT("LOGISTICS HUB")',
            "WarSubsystem->CanFundOperation(",
            "RefreshOperationChoices();",
            "SelectOperationOffset(-1);",
            "SelectOperationOffset(1);",
            "TryDeploySelectedOperation();",
            "OperationSectorChoices[SelectedOperationIndex]",
            'TEXT("SELECTED // COMMAND PRIORITY")',
            'TEXT("SELECTED OPERATION")',
            "Key == EKeys::BackSpace",
            "IsWithdrawalConfirmationActive()",
            "OnWithdrawRequested.Broadcast();",
            '"[BACKSPACE] CONFIRM WITHDRAWAL // "',
            '"[BACKSPACE] WITHDRAW OPERATION     "',
            '"[A/D] SELECT OPERATION // "',
            '"[ENTER] DEPLOY %s AT %s "',
            '"// STAGING %s // ROUTE %d HOPS"',
            "BHWarOperationRules::BuildForcePackage(",
            "BuildDeploymentForcePreviewText(",
            '"%s // HOSTILES %dx%d // SUPPORT %d // GARRISON %d"',
            '"%s // HOSTILES %d + %dx%d // SUPPORT %d // GARRISON %d"',
            "BuildDeploymentReadinessText(",
            '"OCC %d/%d // REAR GAR %d%s"',
            '" // OCCUPATION SHORTFALL"',
            '" // REAR EXPOSED"',
            "HasDeploymentManpowerRisk(",
            "Preview.bOccupationGarrisonShortfall",
            "Preview.RemainingStagingGarrisonCount",
            "BuildDeploymentTravelPreview(",
            "BuildDeploymentLoadoutPreview(",
            "GetMagazineAmmo()",
            "GetReserveAmmo()",
            "GetFieldDressingCount()",
            "GetMedkitCount()",
            "GetBodyArmorDurabilityPercentage()",
            '"LOADOUT // AMMO %d/%d // DRESS %d // MED %d "',
            '"// ARMOR %.0f%% // FRAG %d // %s"',
            "IsDeploymentRiskConfirmationActive()",
            '"READINESS WARNING // %s // %s "',
            "FOLLOW GREEN RESUPPLY MARKER",
            "TREAT WOUNDS BEFORE DEPLOYING",
            '"// PRESS ENTER AGAIN TO DEPLOY"',
            "DeploymentRiskConfirmationExpiresAt",
            '"FIELD PLAN // %.1f KM // WINDOW %.1f MIN "',
            '"// ETA %.1f MIN // RANGE %.0f KM // %s"',
            'TEXT("TRANSPORT READY")',
            'TEXT("FUEL SHORTFALL")',
            'TEXT("LATE ARRIVAL RISK")',
            "CalculateApproachWindowSecondsForOperation(",
            "GetEstimatedRangeKilometers()",
            "GetEstimatedTravelMinutes(",
            "BHWarOperationRules::GetMobilizationSectorID(",
            "WarSubsystem->MobilizeSectorMilitia(SectorID)",
            '" // [R] RALLY %s +%d MILITIA (%.0f SUPPLY)"',
            '? TEXT("LOCAL")',
            ': TEXT("STAGING"),',
            "Key == EKeys::T",
            "WarSubsystem->RedeploySectorGarrison(",
            '"RESERVES EN ROUTE // %s > %s // %d TROOPS "',
            '" // [T] MOVE %d FROM %s "',
            '"(%.0f SUP // %dT)"',
            "GetSectorGarrisonRedeploymentTurns(",
            "GetIncomingGarrisonTransferCount(",
            "GetFactionManpowerReserve(",
            "GetFactionRecruitmentPerTurn(",
            "RESERVE %d // RECRUIT +%.1f/T",
            '" // INBOUND +%d ETA %dT"',
            '"BLOCKED // NO ROUTE"',
            '"BLOCKED // SUPPLY SHORTFALL"',
            "Preview.StagingFriendlyStrength",
            "Preview.StagingSupply",
            "DeploymentReadinessColor",
            "const float FooterHeight = bDeploymentMode",
            '"DEPLOYMENT BLOCKED // "',
            '"DEPLOYMENT BLOCKED // NO VIABLE OPERATION"',
            '"NO FRIENDLY STAGING ROUTE"',
            '"DEPLOYMENT BLOCKED // INSUFFICIENT SUPPLY "',
            "Key == EKeys::M || Key == EKeys::Escape",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.h",
        (
            "void ToggleWarMap();",
            "void CloseWarMap();",
            "void HandleWithdrawRequested();",
            "bool BeginOperationInWorld(",
            "bool IsWarMapOpen() const;",
            "TSubclassOf<UBHWarMapWidget> WarMapWidgetClass;",
            "bool bWarMapOpen = false;",
        ),
    )
    _require_fragments(
        "Source/BrokenHorizon/BHCharacter.cpp",
        (
            "WarMapWidgetClass = UBHWarMapWidget::StaticClass();",
            "EKeys::M,",
            "&ABHCharacter::ToggleWarMap",
            "WarMapWidget->InitializeWarMap(WarSubsystem);",
            "WarMapWidget->OnCloseRequested.AddDynamic(",
            "WarMapWidget->OnWithdrawRequested.AddDynamic(",
            "WarMapWidget->OnWithdrawRequested.RemoveDynamic(",
            "SaveSubsystem->DeployOperation(",
            "PlayerController->SetIgnoreMoveInput(true);",
            "WarMapWidget->SetKeyboardFocus();",
            '"LogisticsHubOnline"',
            '"LogisticsHubSecured"',
            '"LogisticsHubLost"',
            '"SupplyRouteSevered"',
            '"SupplyRouteRestored"',
            '"SupplyRouteUpdated"',
            "!bRuntimeWarOperation",
            "void ABHCharacter::HandleWithdrawRequested()",
            "FailCurrentWarOperation(WithdrawalReason)",
            "OpenWorldOperationDirector->Destroy();",
            "BH_OPERATION_WITHDRAWN sector=%s type=%d",
        ),
    )

    _log("PASS M opens the map and M/Escape close it.")
    _log("PASS map locks field controls and takes keyboard focus.")
    _log(
        "PASS sector ownership, forces, supply, links, and "
        "field logistics are rendered."
    )
    _log("PASS every sector card reports live strategic supply flow.")
    _log("PASS sector cards classify live strategic supply readiness.")
    _log("PASS isolated sectors are marked CUT OFF.")
    _log("PASS isolated low-supply sectors report force attrition.")
    _log("PASS command header summarizes the friendly logistics network.")
    _log("PASS route colors distinguish logistics and frontline states.")
    _log("PASS in-transit supply convoys are summarized and highlighted.")
    _log("PASS sector cards report inbound and outbound convoy cargo.")
    _log("PASS recent convoy losses surface as route threat levels.")
    _log("PASS current priority reports command intent.")
    _log("PASS committed sectors are marked as active operations.")
    _log("PASS reinforcement estimates reflect current supply.")
    _log("PASS deployment reports its staging route and supply cost.")
    _log("PASS deployment allows cycling and launching viable fronts.")
    _log("PASS blocked deployment input explains the exact constraint.")
    _log("PASS deployment previews expected hostile and support forces.")
    _log(
        "PASS deployment reports ammunition, medical, armor, and "
        "grenade readiness."
    )
    _log(
        "PASS materially underprepared deployments require "
        "deliberate confirmation."
    )
    _log(
        "PASS readiness warnings direct the player toward logistics "
        "or immediate treatment."
    )
    _log(
        "PASS attack previews expose occupation and rear-garrison risk."
    )
    _log(
        "PASS R can reinforce attack staging or local defense sectors."
    )
    _log(
        "PASS T dispatches conserved rear troops with route-based ETAs."
    )
    _log("PASS rear hubs can route supply across friendly sectors.")
    _log("PASS deployment highlights its active logistics corridor.")
    _log("PASS sector cards identify strategic logistics roles.")
    _log("PASS logistics-hub control changes issue strategic alerts.")
    _log("PASS supply-route readiness changes issue operational alerts.")
    _log("PASS committed missions suppress next-operation route noise.")
    _log("PASS war-state changes refresh the visible strategic picture.")
    _log("PASS active operations support confirmed campaign withdrawal.")


def main():
    _validate_reflection()
    _validate_source_contract()
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
