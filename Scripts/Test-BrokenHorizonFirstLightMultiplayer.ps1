[CmdletBinding()]
param(
    [ValidateRange(0, 65535)][int]$Port = 0,
    [ValidateRange(30, 180)][int]$TimeoutSeconds = 150,
    [ValidatePattern('^[A-Za-z0-9_-]{1,80}$')][string]$LogPrefix = 'G1-FirstLightCompletion',
    [switch]$RequireInventoryTransfer,
    [switch]$RequireWeaponRoleRuntime,
    [ValidateSet('Editor', 'Packaged')][string]$Runtime = 'Editor',
    [ValidateSet('Dedicated', 'Listen')][string]$Topology = 'Dedicated',
    [string]$PackageRoot = 'Builds\FirstLight-Development\Windows'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Config\ProjectManifest.json') | ConvertFrom-Json
$uproject = Join-Path $projectRoot $manifest.uproject
$editor = Join-Path $manifest.engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if ($Topology -eq 'Listen' -and $Runtime -ne 'Packaged') { throw 'Listen topology requires -Runtime Packaged.' }
if ($Port -ne 0 -and $Port -lt 1024) { throw 'Port must be zero (automatic) or 1024..65535.' }
if ($Port -eq 0) { $Port = 8600 + (Get-Random -Minimum 0 -Maximum 500) }
$resolvedPackageRoot = if ([IO.Path]::IsPathRooted($PackageRoot)) {
    [IO.Path]::GetFullPath($PackageRoot)
} else { [IO.Path]::GetFullPath((Join-Path $projectRoot $PackageRoot)) }
# Launch the actual runtime executable, not the package bootstrapper and its child process.
$packaged = Join-Path $resolvedPackageRoot 'BrokenHorizon\Binaries\Win64\BrokenHorizon.exe'
if ($Runtime -eq 'Packaged' -and -not (Test-Path -LiteralPath $packaged -PathType Leaf)) { throw "Missing packaged runtime: $packaged" }
if ($Topology -eq 'Dedicated' -and -not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Missing Editor host: $editor" }
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$safeRunId = $runId -replace '-', '_'
$runDirectory = Join-Path $projectRoot "Saved\Automation\FirstLightMultiplayer\$runId"
$logDirectory = Join-Path $projectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $runDirectory, $logDirectory | Out-Null
$hostLog = Join-Path $logDirectory "$LogPrefix-$runId-Host.log"
$rejoinALog = Join-Path $logDirectory "$LogPrefix-$runId-RejoinA.log"
$summaryPath = Join-Path $logDirectory "$LogPrefix-$runId-Summary.json"
$remoteRoles = if ($Topology -eq 'Listen') { @('ClientA') } else { @('ClientA', 'ClientB') }
$mode = if ($Topology -eq 'Listen') { 'PackagedListenHostAndOnePackagedRemote' } elseif ($Runtime -eq 'Packaged') {
    'HybridEditorDedicatedHostAndTwoPackagedClients'
} else { 'EditorDedicatedHostAndTwoEditorClients' }
$summary = [ordered]@{
    result = 'Failed'; runId = $runId; mode = $mode; topology = $Topology; clientRuntime = $Runtime
    port = $Port; map = $manifest.maps.firstLight; playerCount = 2; remoteClientCount = $remoteRoles.Count
    expectedPlayerReadiness = @{ count = 2; attempts = 120; intervalSeconds = 0.5 }
    packageRoot = $(if ($Runtime -eq 'Packaged') { $resolvedPackageRoot } else { $null })
    roles = [ordered]@{}; evidence = [ordered]@{}; cleanupErrors = @(); failure = $null
    # Logs and isolated role directories are retained as evidence; no shared/user saves are removed.
    retainedRunDirectory = $runDirectory; realDebriefPresentationVerified = $false
}
$launchedProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
$runningProcesses = @{}
$commonArguments = @('-nullrhi', '-unattended', '-nosound', '-NoSplash', '-DDC-ForceMemoryCache', '-DisablePython')
$runtimeFailurePattern =
        "Fatal error:|Assertion failed:|Unhandled Exception:|" +
        "NetworkFailure|TravelFailure|PIE: Error:|" +
        "Checkpoint save failed|BH_(WAR|FIELD)_AUTOSAVE_FAILED|" +
        "BH_TEST_WEAPON_ROLE_RUNTIME_RESTORE result=failure|" +
        "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE result=failure|" +
        "Supply .*BHAmmoSupply_.* has no persistence ID"

function Start-BHProcess {
    param([string]$Role, [string]$RoleRuntime, [string]$Address, [string]$LogPath, [string[]]$ExtraArguments)
    $executable = if ($RoleRuntime -eq 'Editor') { $editor } else { $packaged }
    $roleDirectory = Join-Path $runDirectory $Role
    New-Item -ItemType Directory -Path $roleDirectory | Out-Null
    $arguments = @()
    if ($RoleRuntime -eq 'Editor') { $arguments += $uproject }
    $arguments += @($Address, "-abslog=$LogPath", "-UserDir=$roleDirectory", "-BHTestSaveSlotSuffix=${safeRunId}_$Role")
    $arguments += $ExtraArguments
    $arguments += $commonArguments
    $quotedArguments = foreach ($argument in $arguments) {
        if ($argument -match '["\r\n]' -or $argument.EndsWith('\')) { throw "Unsupported launch argument: $argument" }
        '"' + $argument + '"'
    }
    $summary.roles[$Role] = [ordered]@{
        runtime = $RoleRuntime; executable = $executable; arguments = $arguments
        log = $LogPath; userDirectory = $roleDirectory; saveSuffix = "${safeRunId}_$Role"
        processId = $null; stopped = $false
    }
    $process = Start-Process -FilePath $executable -ArgumentList $quotedArguments -WindowStyle Hidden -PassThru
    $launchedProcesses.Add($process)
    $runningProcesses[$Role] = $process
    $summary.roles[$Role].processId = $process.Id
    return $process
}
function Stop-OwnedProcess {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return }
    foreach ($role in @($runningProcesses.Keys)) {
        if ($runningProcesses[$role].Id -eq $Process.Id) { $runningProcesses.Remove($role); $summary.roles[$role].stopped = $true }
    }
    $Process.Refresh()
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id
        if (-not $Process.WaitForExit(10000)) { throw "Owned process $($Process.Id) did not exit." }
    }
}
function Assert-OwnedProcessesAlive {
    foreach ($role in @($runningProcesses.Keys)) {
        $process = $runningProcesses[$role]; $process.Refresh()
        if ($process.HasExited) { throw "Owned $role process exited unexpectedly ($($process.ExitCode)); see $($summary.roles[$role].log)" }
    }
}

function Wait-ForMarker {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $failurePattern = $runtimeFailurePattern
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Assert-OwnedProcessesAlive
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

function Find-OwnerAmmoObservation {
    param([string]$Content, [int]$PlayerId, [uint64]$AfterCycles)
    $pattern = '(?s)BH_TEST_FIRST_LIGHT_AMMO_OBSERVATION phase=begin player_id=(?<player>[0-9]+) magazine=30 reserve=180 qpc=(?<qpc>[0-9]+)' +
        '(?<body>.*?)BH_TEST_FIRST_LIGHT_AMMO_OBSERVATION phase=end player_id=\k<player> magazine=30 reserve=180 qpc=\k<qpc>(?:\s|$)'
    foreach ($block in [regex]::Matches($Content, $pattern)) {
        [uint64]$cycles = 0
        if ([int]$block.Groups['player'].Value -ne $PlayerId -or
            -not [uint64]::TryParse($block.Groups['qpc'].Value, [ref]$cycles) -or $cycles -le $AfterCycles) { continue }
        $body = $block.Groups['body'].Value
        if ($body -notmatch 'BH_TEST_FIRST_LIGHT_AMMO_OBSERVATION' -and
            $body -match 'BH_BATTLEFIELD_LOOT_HUD_UPDATED result=success magazine=30 reserve=180 text="[^"]*30 / 180') {
            return [pscustomobject]@{ PlayerId = $PlayerId; ObservationCycles = $cycles }
        }
    }
    return $null
}
function Wait-ForOwnerAmmoObservation {
    param([int]$PlayerId, [uint64]$AfterCycles)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Assert-OwnedProcessesAlive
        foreach ($role in $remoteRoles) {
            $content = if (Test-Path -LiteralPath $remoteLogs[$role]) { Get-Content -Raw -LiteralPath $remoteLogs[$role] } else { '' }
            if ($content -match $runtimeFailurePattern) { throw "Owner ammo observation encountered a failure: $($remoteLogs[$role])" }
            $observation = Find-OwnerAmmoObservation -Content $content -PlayerId $PlayerId -AfterCycles $AfterCycles
            if ($null -ne $observation) {
                return [pscustomobject]@{ Role = $role; LogPath = $remoteLogs[$role]; ObservationCycles = $observation.ObservationCycles }
            }
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Selected player $PlayerId has no post-transaction OnRep block containing the actual owner HUD update."
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


function Wait-ForExactLoot {
    param([string]$LogPath, [string[]]$Available, [AllowEmptyCollection()][string[]]$Consumed, [string]$Label)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Assert-OwnedProcessesAlive
        $content = if (Test-Path -LiteralPath $LogPath) { Get-Content -Raw -LiteralPath $LogPath } else { '' }
        if ($content -match $runtimeFailurePattern) {
            throw "$Label encountered a failure marker: $LogPath"
        }
        $state = Get-ReplicatedLootState -Content $content
        if (@($state.AvailableIds | Where-Object { $_ -notin $Available }).Count -gt 0 -or
            @($state.ConsumedIds | Where-Object { $_ -notin $Consumed }).Count -gt 0) { throw "$Label observed unexpected loot identities." }
        if (($state.AvailableIds -join ',') -ceq ($Available -join ',') -and
            ($state.ConsumedIds -join ',') -ceq ($Consumed -join ',')) { return $state }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "$Label timed out waiting for exact loot identities: $LogPath"
}

try {
    $hostRuntime = if ($Topology -eq 'Listen') { 'Packaged' } else { 'Editor' }
    $hostAddress = $manifest.maps.firstLight + $(if ($Topology -eq 'Listen') { '?listen' } else { '' })
    $hostFlags = @("-port=$Port", '-BHTestFirstLightPlayableRoute', '-BHTestFirstLightExpectedPlayers=2',
        '-BHTestFirstLightPlayableRouteAfterSeconds=30', '-BHTestBattlefieldLootHUD')
    $hostFlags += $(if ($Topology -eq 'Listen') { '-game' } else { '-server' })
    if ($RequireInventoryTransfer) { $hostFlags += '-BHTestInventoryTransferRuntime' }
    if ($RequireWeaponRoleRuntime) { $hostFlags += '-BHTestWeaponRoleRuntime' }
    $hostProcess = Start-BHProcess -Role Host -RoleRuntime $hostRuntime -Address $hostAddress -LogPath $hostLog -ExtraArguments $hostFlags
    $null = Wait-ForMarker $hostLog 'BH_WAR_GAME_STATE_READY' 'First Light host readiness'
    $remoteLogs = @{}
    $remoteProcesses = @{}
    foreach ($role in $remoteRoles) {
        $remoteLogs[$role] = Join-Path $logDirectory "$LogPrefix-$runId-$role.log"
        $remoteProcesses[$role] = Start-BHProcess -Role $role -RoleRuntime $Runtime -Address "127.0.0.1:$Port" -LogPath $remoteLogs[$role] `
            -ExtraArguments @('-game', '-BHTestRuntimeSupplyReplication', '-BHTestBattlefieldLootAmmoReplication', '-BHTestBattlefieldLootHUD', '-BHTestFirstLightAmmoObservation')
        $null = Wait-ForMarker $remoteLogs[$role] 'BH_WAR_SNAPSHOT_APPLIED' "$role connection"
    }
    $hostContent = Wait-ForMarker $hostLog 'BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=ammo_drop result=success' 'Authoritative loot transaction'
    if ($RequireInventoryTransfer) { $null = Wait-ForMarker $hostLog 'BH_TEST_INVENTORY_TRANSFER_RUNTIME result=success' 'Inventory transfer' }
    if ($RequireWeaponRoleRuntime) {
        $null = Wait-ForMarker $hostLog 'BH_TEST_WEAPON_ROLE_RUNTIME result=success' 'Weapon roles'
        $null = Wait-ForMarker $hostLog 'BH_TEST_WEAPON_ROLE_RUNTIME_RESTORE result=success' 'Weapon role restoration'
    }
    $hostContent = Wait-ForMarker $hostLog 'BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE_COMPLETE result=success objectives=4 players=2 completed=2' 'Exact two-player route completion'
    if ($hostContent -notmatch 'BH_TEST_FIRST_LIGHT_READINESS result=success expected=2 connected=2') { throw 'Missing exact pre-route player readiness.' }
    $authority = [regex]::Match($hostContent, 'BH_TEST_FIRST_LIGHT_LOOT_AUTHORITY result=success spawned=([0-9,]+) consumed=([0-9,]+) remaining=([0-9,]+)')
    if (-not $authority.Success) { throw 'Missing authoritative runtime loot identity inventory.' }
    $spawnedIds = @($authority.Groups[1].Value.Split(',') | Sort-Object)
    $consumedIds = @($authority.Groups[2].Value.Split(',') | Sort-Object)
    $remainingIds = @($authority.Groups[3].Value.Split(',') | Sort-Object)
    foreach ($id in @($spawnedIds + $consumedIds + $remainingIds)) {
        [uint64]$numeric = 0
        if (-not [uint64]::TryParse($id, [ref]$numeric) -or $numeric -le 1) { throw "Invalid authoritative loot ID: $id" }
    }
    if ($consumedIds.Count -ne 1 -or $remainingIds.Count -eq 0 -or @($spawnedIds | Select-Object -Unique).Count -ne $spawnedIds.Count) {
        throw 'Authoritative loot set is not a unique set with exactly one consumed pickup and remaining loot.'
    }
    Assert-LootIdentitySet -Expected $spawnedIds -Actual @(@($consumedIds + $remainingIds) | Sort-Object) -Label 'Authoritative loot partition'
    $initialLoot = @{}
    foreach ($role in $remoteRoles) {
        $null = Wait-ForMarker $remoteLogs[$role] 'BH_MISSION_STATE_REPLICATED .*completed=4 complete=1 failed=0' "$role replicated completion"
        $initialLoot[$role] = Wait-ForExactLoot -LogPath $remoteLogs[$role] -Available $spawnedIds -Consumed $consumedIds -Label "$role initial loot"
    }
    $owner = [regex]::Match($hostContent, 'BH_TEST_FIRST_LIGHT_AMMO_OWNER role=(local_authority|remote) player_id=(-?[0-9]+)')
    if (-not $owner.Success) { throw 'Missing selected route/ammo owner marker.' }
    $transaction = [regex]::Match($hostContent, 'BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=ammo_drop result=success before=([0-9]+) after=180 rounds=([0-9]+) qpc=([0-9]+)')
    [uint64]$transactionCycles = 0
    if (-not $transaction.Success -or -not [uint64]::TryParse($transaction.Groups[3].Value, [ref]$transactionCycles) -or $transactionCycles -eq 0) {
        throw 'Authoritative ammo transaction lacks the verified 180 reserve and QPC timestamp.'
    }
    $ownerObservationCycles = $null
    $ownerRole = $owner.Groups[1].Value
    if ($ownerRole -eq 'local_authority') {
        if ($Topology -ne 'Listen') { throw 'Dedicated host cannot own a local player HUD.' }
        $ownerLog = $hostLog
        $ownerParticipant = 'Host'
        $ownerContent = Wait-ForMarker $hostLog '(?s)BH_TEST_FIRST_LIGHT_AMMO_OWNER role=local_authority.*BH_BATTLEFIELD_LOOT_HUD_UPDATED result=success magazine=30 reserve=180 text="[^"]*30 / 180' 'Local authority owner HUD after loot owner selection'
        $ammoObservation = 'AuthoritativeTransactionAndLocalHUD'
    } else {
        $remoteEvidence = Wait-ForOwnerAmmoObservation -PlayerId ([int]$owner.Groups[2].Value) -AfterCycles $transactionCycles
        $ownerLog = $remoteEvidence.LogPath
        $ownerParticipant = $remoteEvidence.Role
        $ownerObservationCycles = $remoteEvidence.ObservationCycles
        # Preserve the previous diagnostic contract too; it is no longer the owner-correlation proof.
        $null = Wait-ForMarker $ownerLog 'BH_BATTLEFIELD_LOOT_AMMO_REPLICATED result=success .*magazine=30 reserve=180' 'Legacy owner ammo replication marker'
        $ammoObservation = 'SelectedOwnerPostTransactionOnRepWithSynchronousHUD'
    }
    $summary.evidence = [ordered]@{
        canonicalObjectiveCount = 4; authoritativeCompletedParticipants = 2; replicatedCompletionRoles = $remoteRoles
        runtimeLootSpawnedIds = $spawnedIds; consumedIds = $consumedIds; remainingIds = $remainingIds
        initialLootReplicatedRoles = $remoteRoles; ownerKind = $ownerRole; ammoOwnerParticipant = $ownerParticipant
        serverOwnerPlayerId = [int]$owner.Groups[2].Value; ammoObservation = $ammoObservation
        transactionQpc = $transactionCycles; ownerObservationQpc = $ownerObservationCycles
        ownerAmmoReserve = 180; ownerAmmoHUD = '30 / 180'; ownerLog = $ownerLog
        inventoryTransfer = [bool]$RequireInventoryTransfer; weaponRoleRuntime = [bool]$RequireWeaponRoleRuntime
    }
    Stop-OwnedProcess $remoteProcesses['ClientA']
    $rejoinA = Start-BHProcess -Role RejoinA -RoleRuntime $Runtime -Address "127.0.0.1:$Port" -LogPath $rejoinALog `
        -ExtraArguments @('-game', '-BHTestRuntimeSupplyReplication')
    $null = Wait-ForMarker $rejoinALog 'BH_MISSION_STATE_REPLICATED .*completed=4 complete=1 failed=0' 'Fresh remote inherited completion'
    $null = Wait-ForMarker $hostLog 'BH_SHARED_MISSION_ADOPTED .*completed=4 complete=1 result=success' 'Authoritative late-join shared completion'
    $null = Wait-ForMarker $rejoinALog 'BH_SALVAGE_STATE_REPLICATED id=FirstLightSalvageCache01 type=AMMO quantity=30' 'Rejoin authored salvage'
    $rejoinLoot = Wait-ForExactLoot -LogPath $rejoinALog -Available $remainingIds -Consumed @() -Label 'Fresh remote remaining loot'
    Start-Sleep -Seconds 2
    $rejoinLoot = Get-ReplicatedLootState -Content (Get-Content -Raw -LiteralPath $rejoinALog)
    Assert-LootIdentitySet -Expected $remainingIds -Actual $rejoinLoot.AvailableIds -Label 'Settled rejoin available loot'
    Assert-LootIdentitySet -Expected @() -Actual $rejoinLoot.ConsumedIds -Label 'Settled rejoin consumed loot'
    $summary.evidence.rejoin = [ordered]@{
        kind = 'FreshRemoteProcess'; originalRole = 'ClientA'; newRole = 'RejoinA'
        completionInherited = $true; authoredSalvageReplicated = $true
        availableIds = $rejoinLoot.AvailableIds; consumedAbsent = $true; retainedHostProcessId = $hostProcess.Id
    }
    Assert-OwnedProcessesAlive
    foreach ($role in @($runningProcesses.Keys)) {
        $finalContent = Get-Content -Raw -LiteralPath $summary.roles[$role].log
        if ($finalContent -match $runtimeFailurePattern) { throw "Final $role log contains a runtime failure: $($summary.roles[$role].log)" }
    }
    $summary.result = 'Passed'
}
catch {
    $summary.failure = $_.Exception.Message
    throw
}
finally {
    foreach ($process in $launchedProcesses) {
        try { Stop-OwnedProcess $process } catch { $summary.cleanupErrors += $_.Exception.Message }
    }
    if ($summary.cleanupErrors.Count -gt 0) { $summary.result = 'Failed' }
    $summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-Host "First Light $mode result=$($summary.result): $summaryPath"
    if ($summary.cleanupErrors.Count -gt 0) { throw ($summary.cleanupErrors -join '; ') }
}
