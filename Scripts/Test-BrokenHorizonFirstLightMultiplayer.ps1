[CmdletBinding()]
param(
    [ValidateRange(1024, 65535)]
    [int]$Port = 0,
    [ValidateRange(30, 180)]
    [int]$TimeoutSeconds = 150,
    [string]$LogPrefix = "G1-FirstLightCompletion",
    [switch]$RequireInventoryTransfer,
    [switch]$RequireWeaponRoleRuntime
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw `
    -LiteralPath (Join-Path $projectRoot "Config\ProjectManifest.json") |
    ConvertFrom-Json
$uproject = Join-Path $projectRoot $manifest.uproject
$editor = Join-Path `
    $manifest.engineRoot `
    "Engine\Binaries\Win64\UnrealEditor.exe"
$logDirectory = Join-Path $projectRoot "Saved\Logs"
$runId = Get-Date -Format "yyyyMMdd-HHmmss"
$safeRunId = $runId -replace '-', '_'
if ($Port -eq 0) {
    $Port = 8600 + (Get-Random -Minimum 0 -Maximum 500)
}

New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$hostLog = Join-Path $logDirectory "$LogPrefix-$runId-Host.log"
$clientALog = Join-Path $logDirectory "$LogPrefix-$runId-ClientA.log"
$clientBLog = Join-Path $logDirectory "$LogPrefix-$runId-ClientB.log"
$rejoinALog = Join-Path $logDirectory "$LogPrefix-$runId-RejoinA.log"
$summaryPath = Join-Path $logDirectory "$LogPrefix-$runId-Summary.json"
$launchedProcesses =
    [System.Collections.Generic.List[System.Diagnostics.Process]]::new()

function Start-BHProcess {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)

    $process = Start-Process `
        -FilePath $editor `
        -ArgumentList $Arguments `
        -WindowStyle Hidden `
        -PassThru
    $launchedProcesses.Add($process)
    return $process
}

function Stop-OwnedProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -ne $Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id
        $Process.WaitForExit(5000) | Out-Null
    }
}

function Wait-ForMarker {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $failurePattern =
        "Fatal error:|Assertion failed:|Unhandled Exception:|" +
        "NetworkFailure|TravelFailure|PIE: Error:|" +
        "Checkpoint save failed|BH_(WAR|FIELD)_AUTOSAVE_FAILED|" +
        "BH_TEST_WEAPON_ROLE_RUNTIME_RESTORE result=failure|" +
        "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE result=failure|" +
        "Supply .*BHAmmoSupply_.* has no persistence ID"
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if (Test-Path -LiteralPath $LogPath) {
            $content = Get-Content -Raw -LiteralPath $LogPath
            if ($content -match $failurePattern) {
                throw "$Label encountered a failure marker. See $LogPath"
            }
            if ($content -match $Pattern) {
                return $content
            }
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "$Label timed out waiting for '$Pattern'. See $LogPath"
}

function Wait-ForAnyMarker {
    param(
        [Parameter(Mandatory = $true)][string[]]$LogPaths,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $failurePattern =
        "Fatal error:|Assertion failed:|Unhandled Exception:|" +
        "NetworkFailure|TravelFailure|PIE: Error:|" +
        "Checkpoint save failed|BH_(WAR|FIELD)_AUTOSAVE_FAILED|" +
        "BH_TEST_WEAPON_ROLE_RUNTIME_RESTORE result=failure|" +
        "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE result=failure|" +
        "Supply .*BHAmmoSupply_.* has no persistence ID"
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        foreach ($logPath in $LogPaths) {
            if (Test-Path -LiteralPath $logPath) {
                $content = Get-Content -Raw -LiteralPath $logPath
                if ($content -match $failurePattern) {
                    throw "$Label encountered a failure marker. See $logPath"
                }
                if ($content -match $Pattern) {
                    return [pscustomobject]@{
                        LogPath = $logPath
                        Content = $content
                    }
                }
            }
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "$Label timed out waiting for '$Pattern'. See $($LogPaths -join ', ')"
}

function Get-ReplicatedLootState {
    param([Parameter(Mandatory = $true)][string]$Content)

    # Actor names change across reconnects; network IDs identify server actors.
    $available = [System.Collections.Generic.HashSet[string]]::new()
    $consumed = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($marker in [regex]::Matches($Content,
        'BH_RUNTIME_SUPPLY_(AVAILABLE|CONSUMED)_REPLICATED result=success[^\r\n]*')) {
        $identity = [regex]::Match($marker.Value,
            '(?:^|\s)network_id=([0-9]+)(?:\s|$)')
        [uint64]$networkId = 0
        if (-not $identity.Success -or
            -not [uint64]::TryParse($identity.Groups[1].Value, [ref]$networkId) -or
            $networkId -le 1) {
            throw "Runtime loot marker has no valid assigned network ID: $($marker.Value)"
        }
        if ($marker.Groups[1].Value -eq 'AVAILABLE') {
            [void]$available.Add($networkId.ToString())
        } else {
            [void]$consumed.Add($networkId.ToString())
        }
    }
    return [pscustomobject]@{
        AvailableIds = @($available | Sort-Object)
        ConsumedIds = @($consumed | Sort-Object)
        RemainingIds = @($available | Where-Object { -not $consumed.Contains($_) } | Sort-Object)
    }
}

function Assert-LootIdentitySet {
    param(
        [AllowEmptyCollection()][string[]]$Expected,
        [AllowEmptyCollection()][string[]]$Actual,
        [Parameter(Mandatory = $true)][string]$Label
    )
    if (($Expected -join ',') -cne ($Actual -join ',')) {
        throw "$Label has incorrect runtime loot identities (expected=$($Expected -join ',') actual=$($Actual -join ','))."
    }
}

try {
    $commonArguments = @(
        "-nullrhi",
        "-unattended",
        "-nosound",
        "-NoSplash",
        "-DDC-ForceMemoryCache"
    )
    $hostArguments = @(
        $uproject,
        $manifest.maps.firstLight,
        "-server",
        "-port=$Port",
        "-abslog=$hostLog",
        "-BHTestSaveSlotSuffix=$runId",
        "-BHTestFirstLightPlayableRoute",
        "-BHTestFirstLightPlayableRouteAfterSeconds=30"
    )
    if ($RequireInventoryTransfer) {
        $hostArguments += "-BHTestInventoryTransferRuntime"
    }
    if ($RequireWeaponRoleRuntime) {
        $hostArguments += "-BHTestWeaponRoleRuntime"
    }
    $hostProcess = Start-BHProcess -Arguments ($hostArguments + $commonArguments)
    $hostContent = Wait-ForMarker `
        -LogPath $hostLog `
        -Pattern "BH_WAR_GAME_STATE_READY" `
        -Label "First Light host readiness"

    $clientA = Start-BHProcess -Arguments (@(
        $uproject,
        "127.0.0.1:$Port",
        "-game",
        "-abslog=$clientALog",
        "-BHTestRuntimeSupplyReplication",
        "-BHTestBattlefieldLootAmmoReplication",
        "-BHTestBattlefieldLootHUD"
    ) + $commonArguments)
    Start-Sleep -Seconds 2
    $clientB = Start-BHProcess -Arguments (@(
        $uproject,
        "127.0.0.1:$Port",
        "-game",
        "-abslog=$clientBLog",
        "-BHTestRuntimeSupplyReplication",
        "-BHTestBattlefieldLootAmmoReplication",
        "-BHTestBattlefieldLootHUD"
    ) + $commonArguments)

    $clientAContent = Wait-ForMarker `
        -LogPath $clientALog `
        -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
        -Label "First Light Client A connection"
    $clientBContent = Wait-ForMarker `
        -LogPath $clientBLog `
        -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
        -Label "First Light Client B connection"
    $hostContent = Wait-ForMarker `
        -LogPath $hostLog `
        -Pattern "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=ammo_drop result=success" `
        -Label "Authoritative First Light battlefield-loot interaction"
    if ($RequireInventoryTransfer) {
        $hostContent = Wait-ForMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_INVENTORY_TRANSFER_RUNTIME result=success" `
            -Label "Authoritative multiplayer inventory transfer"
    }
    if ($RequireWeaponRoleRuntime) {
        $hostContent = Wait-ForMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_WEAPON_ROLE_RUNTIME result=success" `
            -Label "Authoritative weapon role runtime contract"
        $hostContent = Wait-ForMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_WEAPON_ROLE_RUNTIME_RESTORE result=success" `
            -Label "Authoritative weapon role fixture restoration"
    }
    $hostContent = Wait-ForMarker `
        -LogPath $hostLog `
        -Pattern "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE_COMPLETE result=success objectives=4 players=2 completed=2" `
        -Label "Authoritative two-player production First Light route"
    $clientAContent = Wait-ForMarker `
        -LogPath $clientALog `
        -Pattern "BH_MISSION_STATE_REPLICATED .*completed=4 complete=1 failed=0" `
        -Label "Client A replicated mission completion"
    $clientBContent = Wait-ForMarker `
        -LogPath $clientBLog `
        -Pattern "BH_MISSION_STATE_REPLICATED .*completed=4 complete=1 failed=0" `
        -Label "Client B replicated mission completion"
    $clientAContent = Wait-ForMarker `
        -LogPath $clientALog `
        -Pattern "BH_RUNTIME_SUPPLY_CONSUMED_REPLICATED result=success" `
        -Label "Client A replicated consumed battlefield loot"
    $clientBContent = Wait-ForMarker `
        -LogPath $clientBLog `
        -Pattern "BH_RUNTIME_SUPPLY_CONSUMED_REPLICATED result=success" `
        -Label "Client B replicated consumed battlefield loot"
    $ammoOwnerEvidence = Wait-ForAnyMarker `
        -LogPaths @($clientALog, $clientBLog) `
        -Pattern "BH_BATTLEFIELD_LOOT_AMMO_REPLICATED result=success .*reserve=180" `
        -Label "Owning client replicated battlefield-loot ammunition"
    $ammoOwnerClient = if (
        $ammoOwnerEvidence.LogPath -eq $clientALog
    ) { "ClientA" } else { "ClientB" }
    $ammoOwnerContent = Wait-ForMarker `
        -LogPath $ammoOwnerEvidence.LogPath `
        -Pattern 'BH_BATTLEFIELD_LOOT_HUD_UPDATED result=success magazine=30 reserve=180' `
        -Label "Owning client updated battlefield-loot ammo HUD"

    $clientALoot = Get-ReplicatedLootState -Content (
        Get-Content -Raw -LiteralPath $clientALog
    )
    $clientBLoot = Get-ReplicatedLootState -Content (
        Get-Content -Raw -LiteralPath $clientBLog
    )
    Assert-LootIdentitySet -Expected $clientALoot.AvailableIds `
        -Actual $clientBLoot.AvailableIds -Label 'Initial clients available loot'
    Assert-LootIdentitySet -Expected $clientALoot.ConsumedIds `
        -Actual $clientBLoot.ConsumedIds -Label 'Initial clients consumed loot'
    if ($clientALoot.ConsumedIds.Count -ne 1 -or
        $clientALoot.RemainingIds.Count -eq 0 -or
        @($clientALoot.ConsumedIds | Where-Object {
            $_ -notin $clientALoot.AvailableIds
        }).Count -ne 0) {
        throw "Initial clients did not observe exactly one consumed pickup from the available loot set. See $clientALog and $clientBLog"
    }
    $expectedRemainingLootCount = $clientALoot.RemainingIds.Count

    Stop-OwnedProcess -Process $clientA
    $rejoinA = Start-BHProcess -Arguments (@(
        $uproject,
        "127.0.0.1:$Port",
        "-game",
        "-abslog=$rejoinALog",
        "-BHTestRuntimeSupplyReplication"
    ) + $commonArguments)
    $rejoinContent = Wait-ForMarker `
        -LogPath $rejoinALog `
        -Pattern "BH_MISSION_STATE_REPLICATED .*completed=4 complete=1 failed=0" `
        -Label "Rejoining Client A inherited shared completion"
    $hostContent = Wait-ForMarker `
        -LogPath $hostLog `
        -Pattern "BH_SHARED_MISSION_ADOPTED .*completed=4 complete=1 result=success" `
        -Label "Authoritative late-join shared mission adoption"
    $rejoinContent = Wait-ForMarker `
        -LogPath $rejoinALog `
        -Pattern "BH_SALVAGE_STATE_REPLICATED id=FirstLightSalvageCache01 type=AMMO quantity=30" `
        -Label "Rejoining Client A received authored salvage state"
    $rejoinContent = Wait-ForMarker `
        -LogPath $rejoinALog `
        -Pattern "(?s)(BH_RUNTIME_SUPPLY_AVAILABLE_REPLICATED result=success[^\r\n]* network_id=[0-9]+.*){$expectedRemainingLootCount}" `
        -Label "Rejoining Client A received remaining battlefield loot"
    Start-Sleep -Seconds 2
    $rejoinContent = Get-Content -Raw -LiteralPath $rejoinALog
    $rejoinLoot = Get-ReplicatedLootState -Content $rejoinContent
    $rejoinAvailableLootCount = $rejoinLoot.AvailableIds.Count
    Assert-LootIdentitySet -Expected $clientALoot.RemainingIds `
        -Actual $rejoinLoot.AvailableIds -Label 'Rejoining Client A available loot'
    Assert-LootIdentitySet -Expected @() `
        -Actual $rejoinLoot.ConsumedIds -Label 'Rejoining Client A consumed loot'

    $summary = [ordered]@{
        result = "Passed"
        runId = $runId
        port = $Port
        map = $manifest.maps.firstLight
        playerCount = 2
        canonicalObjectiveCount = 4
        authoritativeCompletion = $true
        productionActorRoute = $true
        battlefieldLootConsumed = $true
        inventoryTransfer = [bool]$RequireInventoryTransfer
        weaponRoleRuntime = [bool]$RequireWeaponRoleRuntime
        ammoOwnerClient = $ammoOwnerClient
        ownerAmmoReserveReplicated = 180
        ownerAmmoHUDText = "30 / 180"
        clientAConsumedLootReplicated = $true
        clientBConsumedLootReplicated = $true
        rejoinAuthoredSalvageStateReplicated = $true
        rejoinAvailableLootCount = $rejoinAvailableLootCount
        initialAvailableLootNetworkIds = $clientALoot.AvailableIds
        consumedLootNetworkIds = $clientALoot.ConsumedIds
        expectedRemainingLootNetworkIds = $clientALoot.RemainingIds
        rejoinAvailableLootNetworkIds = $rejoinLoot.AvailableIds
        rejoinConsumedLootAbsent = $true
        clientACompletionReplicated = $true
        clientBCompletionReplicated = $true
        rejoinCompletionInherited = $true
        hostLog = $hostLog
        clientALog = $clientALog
        clientBLog = $clientBLog
        rejoinALog = $rejoinALog
    }
    $summary | ConvertTo-Json | Set-Content -LiteralPath $summaryPath
    Write-Host "Two-player First Light completion passed: $summaryPath"
}
finally {
    foreach ($process in $launchedProcesses) {
        Stop-OwnedProcess -Process $process
    }

    $saveDirectory = Join-Path $projectRoot "Saved\SaveGames"
    foreach ($saveName in @(
        "BrokenHorizon_Checkpoint_$safeRunId.sav",
        "BrokenHorizon_Checkpoint_Backup_$safeRunId.sav"
    )) {
        $savePath = Join-Path $saveDirectory $saveName
        if (Test-Path -LiteralPath $savePath) {
            Remove-Item -LiteralPath $savePath -Force
        }
    }
}
