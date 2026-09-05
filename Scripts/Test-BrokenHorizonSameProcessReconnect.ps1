[CmdletBinding()]
param(
    [ValidateRange(0, 65535)]
    [int]$Port = 0,
    [ValidateRange(5, 120)]
    [int]$ConnectionTimeoutSeconds = 120,
    [ValidatePattern('^[A-Za-z0-9_-]{1,80}$')]
    [string]$LogPrefix = 'BH-Alpha-SameProcessReconnect'
)

# Windows PowerShell 5.1. This exercises real session leave/menu/client travel.
# It does not validate rendered HUD, tactical operation play, or save recovery.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Read-BHReconnectLog {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path) {
        return [string](Get-Content -Raw -LiteralPath $Path)
    }
    return ''
}

function Get-BHReconnectSnapshots {
    param([string]$Content, [string]$Marker = 'BH_WAR_SNAPSHOT_APPLIED')
    $pattern = [regex]::Escape($Marker) + ' revision=(?<revision>\d+) turn=(?<turn>\d+) sectors=(?<sectors>\d+) operation_id=(?<operation>\S+)'
    foreach ($match in [regex]::Matches($Content, $pattern)) {
        [pscustomobject]@{
            revision = [int]$match.Groups['revision'].Value
            turn = [int]$match.Groups['turn'].Value
            sectors = [int]$match.Groups['sectors'].Value
            operationId = $match.Groups['operation'].Value
            offset = $match.Index
            signature = '{0}:{1}:{2}:{3}' -f $match.Groups['revision'].Value,
                $match.Groups['turn'].Value, $match.Groups['sectors'].Value,
                $match.Groups['operation'].Value
        }
    }
}

function Get-BHReconnectLifecycle {
    param([string]$Content, [string]$RunId, [string]$Phase)
    $pattern = 'BH_TEST_SAME_PROCESS_RECONNECT phase=' + [regex]::Escape($Phase) +
        ' result=observed run_id=' + [regex]::Escape($RunId) +
        ' pid=(?<pid>\d+) game_instance=(?<game>\S+) war_subsystem=(?<war>\S+) connection=(?<connection>\S+) revision=(?<revision>-?\d+) turn=(?<turn>\d+) sectors=(?<sectors>\d+) operation_id=(?<operation>\S+)'
    $phaseMatches = [regex]::Matches($Content, $pattern)
    if ($phaseMatches.Count -eq 0) { return $null }
    if ($phaseMatches.Count -ne 1) { throw "Duplicate lifecycle phase '$Phase'." }
    $match = $phaseMatches[0]
    return [pscustomobject]@{
        phase = $Phase
        pid = [int]$match.Groups['pid'].Value
        gameInstance = $match.Groups['game'].Value
        warSubsystem = $match.Groups['war'].Value
        connection = $match.Groups['connection'].Value
        revision = [int]$match.Groups['revision'].Value
        turn = [int]$match.Groups['turn'].Value
        sectors = [int]$match.Groups['sectors'].Value
        operationId = $match.Groups['operation'].Value
        offset = $match.Index
        endOffset = $match.Index + $match.Length
        signature = '{0}:{1}:{2}:{3}' -f $match.Groups['revision'].Value,
            $match.Groups['turn'].Value, $match.Groups['sectors'].Value,
            $match.Groups['operation'].Value
    }
}

function Assert-BHReconnect {
    param([bool]$Condition, [string]$Name, [string]$Message)
    $script:summary.assertions[$Name] = $Condition
    if (-not $Condition) { throw $Message }
}

function Wait-BHReconnectPhase {
    param([string]$Name, [scriptblock]$Probe)
    $script:summary.phase = $Name
    Write-Host "Same-process reconnect: $Name"
    $deadline = [DateTime]::UtcNow.AddSeconds($ConnectionTimeoutSeconds)
    do {
        foreach ($ownedProcess in $script:requiredProcesses) {
            $ownedProcess.Refresh()
            if ($ownedProcess.HasExited) {
                throw "Owned process $($ownedProcess.Id) exited during '$Name' with code $($ownedProcess.ExitCode)."
            }
        }
        foreach ($reconnectLogPath in $script:logPaths) {
            $logText = Read-BHReconnectLog $reconnectLogPath
            if ($logText -match $script:failurePattern) {
                throw "Failure marker '$($Matches[0])' during '$Name'. See $reconnectLogPath"
            }
        }
        $result = & $Probe
        if ($null -ne $result -and $result -ne $false) { return $result }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out after $ConnectionTimeoutSeconds seconds during '$Name'."
}

function Start-BHReconnectProcess {
    param([string]$Role, [string[]]$Arguments)
    # Start-Process joins ArgumentList; quote each argument for paths with spaces.
    $quotedArguments = foreach ($argument in $Arguments) {
        if ($argument.Contains('"') -or $argument.EndsWith('\')) {
            throw "Unsupported quote or trailing slash in launch argument: $argument"
        }
        '"' + $argument + '"'
    }
    $script:summary.commands[$Role] = [ordered]@{
        executable = $editor
        arguments = $Arguments
        commandLine = '"' + $editor + '" ' + ($quotedArguments -join ' ')
    }
    Write-Host $script:summary.commands[$Role].commandLine
    $ownedProcess = Start-Process -FilePath $editor -ArgumentList $quotedArguments `
        -WindowStyle Hidden -PassThru
    $script:launchedProcesses.Add($ownedProcess)
    $script:summary.processes[$Role] = $ownedProcess.Id
    return $ownedProcess
}

function Stop-BHReconnectProcess {
    param([System.Diagnostics.Process]$OwnedProcess)
    if ($null -eq $OwnedProcess) { return }
    $OwnedProcess.Refresh()
    if (-not $OwnedProcess.HasExited) {
        # Operate on our retained process object, never enumerate or kill by name.
        Stop-Process -InputObject $OwnedProcess -ErrorAction Stop
        if (-not $OwnedProcess.WaitForExit(5000)) {
            Stop-Process -InputObject $OwnedProcess -Force -ErrorAction Stop
            if (-not $OwnedProcess.WaitForExit(5000)) {
                throw "Owned process $($OwnedProcess.Id) did not stop."
            }
        }
    }
}

function Wait-BHReconnectHost {
    param([string]$Path, [string]$Label)
    return Wait-BHReconnectPhase $Label {
        $content = Read-BHReconnectLog $Path
        $listenPattern = 'GameNetDriver[^\r\n]*listening on port ' + $Port + '\b'
        $commitPattern = 'BH_TEST_OPERATION_COMMIT result=success id=(?<operation>\S+) sector=(?<sector>\S+) type=2\b'
        if ($content -match $listenPattern -and
            $content -match 'BH_WAR_GAME_STATE_READY revision=\d+ sectors=[1-9]\d*' -and
            $content -match $commitPattern) {
            return [pscustomobject]@{
                operationId = $Matches['operation']
                sector = $Matches['sector']
                operationType = 2
            }
        }
        return $null
    }
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Config\ProjectManifest.json') | ConvertFrom-Json
$uproject = Join-Path $projectRoot $manifest.uproject
$editor = Join-Path $manifest.engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor not found: $editor" }
if ($Port -gt 0 -and $Port -lt 1024) { throw 'Port must be zero (automatic) or 1024..65535.' }

# Confirm the requested loopback UDP port is available; automatic mode asks the OS.
$portProbe = [System.Net.Sockets.UdpClient]::new()
try {
    $portProbe.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Loopback, $Port))
    $Port = $portProbe.Client.LocalEndPoint.Port
} finally {
    $portProbe.Dispose()
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$logDirectory = Join-Path $projectRoot 'Saved\Logs\Codex'
$controlDirectory = Join-Path $projectRoot "Saved\Automation\SameProcessReconnect\$runId"
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $controlDirectory | Out-Null
$initialHostLog = Join-Path $logDirectory "$LogPrefix-$runId-InitialHost.log"
$restartedHostLog = Join-Path $logDirectory "$LogPrefix-$runId-RestartedHost.log"
$clientLog = Join-Path $logDirectory "$LogPrefix-$runId-RetainedClient.log"
$summaryPath = Join-Path $logDirectory "$LogPrefix-$runId-Summary.json"
$leaveSignal = Join-Path $controlDirectory 'leave.ready'
$reconnectSignal = Join-Path $controlDirectory 'reconnect.ready'
$script:logPaths = @($initialHostLog, $restartedHostLog, $clientLog)
$script:launchedProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
$script:requiredProcesses = @()
$script:failurePattern = 'Fatal error:|Assertion failed:|Unhandled Exception:|NetworkFailure|TravelFailure|PIE: Error:|BH_SESSION_ERROR|BH_WAR_SNAPSHOT_APPLY_FAILED|BH_TEST_SAME_PROCESS_RECONNECT phase=failed|BH_TEST_OPERATION_COMMIT result=failure|Checkpoint save failed|BH_(WAR|FIELD)_AUTOSAVE_FAILED'
$script:summary = [ordered]@{
    success = $false
    generatedAt = (Get-Date).ToString('o')
    runId = $runId
    phase = 'setup'
    port = $Port
    connectionTimeoutSeconds = $ConnectionTimeoutSeconds
    map = $manifest.maps.firstLight
    configuredMainMenu = $manifest.maps.mainMenu
    initialRevisionFloor = 4096
    assertions = [ordered]@{}
    processes = [ordered]@{}
    commands = [ordered]@{}
    logs = [ordered]@{ initialHost = $initialHostLog; restartedHost = $restartedHostLog; retainedClient = $clientLog }
    controlDirectory = $controlDirectory
    saveSlotSuffixes = @("${runId}_initialhost", "${runId}_restartedhost", "${runId}_client")
    evidence = [ordered]@{}
    renderedValidated = $false
    hudValidated = $false
    tacticalDefenseValidated = $false
    campaignSaveRecoveryValidated = $false
    limitations = @('NullRHI loopback acceptance only.', 'Defend is a strategic committed operation; no tactical Defense A completion is asserted.', 'Restarted host starts a fresh isolated campaign; this is not save recovery.', 'No rendered UI, HUD, or remote-network validation.')
    cleanupErrors = @()
    failureMessage = $null
}
$commonArguments = @('-nullrhi', '-unattended', '-nosound', '-NoSplash', '-DDC-ForceMemoryCache')
$hostCommonArguments = @('-server', "-port=$Port", '-BHTestCommitPriorityOperation', '-BHTestOperationType=Defend', '-LogCmds=LogTemp VeryVerbose') + $commonArguments
$exitCode = 1
try {
    $initialHost = Start-BHReconnectProcess 'initialHost' (@($uproject, $manifest.maps.firstLight, "-abslog=$initialHostLog", "-BHTestSaveSlotSuffix=${runId}_initialhost", '-BHTestSnapshotRevisionFloor=4096') + $hostCommonArguments)
    $script:requiredProcesses = @($initialHost)
    $initialOperation = Wait-BHReconnectHost $initialHostLog 'Initial dedicated host listening and committed'
    $script:summary.evidence.initialHostOperation = $initialOperation

    $fixtureTimeout = [Math]::Min(600, [Math]::Max(30, $ConnectionTimeoutSeconds * 5))
    $client = Start-BHReconnectProcess 'retainedClient' (@($uproject, "127.0.0.1:$Port", '-game', "-abslog=$clientLog", "-BHTestSaveSlotSuffix=${runId}_client", '-BHTestSameProcessReconnect', "-BHTestReconnectRunId=$runId", "-BHTestReconnectPort=$Port", "-BHTestReconnectTimeout=$fixtureTimeout") + $commonArguments)
    $script:requiredProcesses = @($initialHost, $client)
    $initialAccepted = Wait-BHReconnectPhase 'Initial client accepted committed campaign' {
        $snapshots = @(Get-BHReconnectSnapshots (Read-BHReconnectLog $clientLog) | Where-Object { $_.operationId -eq $initialOperation.operationId -and $_.sectors -gt 0 })
        if ($snapshots.Count -gt 0) { return $snapshots[-1] }
        return $null
    }
    Assert-BHReconnect ($initialAccepted.revision -ge 4096) 'initialRevisionFloorApplied' 'Initial host did not exercise the high revision watermark.'
    [System.IO.File]::WriteAllText($leaveSignal, '')

    $menu = Wait-BHReconnectPhase 'Retained client completed LeaveSession into configured main menu' {
        Get-BHReconnectLifecycle (Read-BHReconnectLog $clientLog) $runId 'menu'
    }
    $clientBeforeRestart = Read-BHReconnectLog $clientLog
    $before = Get-BHReconnectLifecycle $clientBeforeRestart $runId 'before'
    Assert-BHReconnect ($null -ne $before -and $before.offset -lt $menu.offset) 'leavePhaseOrder' 'Missing or out-of-order before/menu lifecycle evidence.'
    $acceptedBeforeLeave = @(Get-BHReconnectSnapshots $clientBeforeRestart.Substring(0, $before.offset))
    Assert-BHReconnect ($acceptedBeforeLeave.Count -gt 0) 'acceptedBeforeLeave' 'No actual applied snapshot precedes LeaveSession.'
    $baseline = $acceptedBeforeLeave[-1]
    Assert-BHReconnect ($baseline.signature -eq $before.signature) 'beforeIdentityMatchesAcceptedState' 'Before-leave state does not match the last successfully applied snapshot.'
    Assert-BHReconnect ($before.pid -eq $client.Id -and $menu.pid -eq $client.Id -and $before.gameInstance -eq $menu.gameInstance -and $before.warSubsystem -eq $menu.warSubsystem) 'identitiesRetainedIntoMenu' 'The client process or persistent subsystem changed before reaching the menu.'
    Assert-BHReconnect ($before.connection -match '^\d+:\d+$' -and $menu.connection -eq 'none') 'menuDisconnected' 'The menu did not confirm an absent live server connection.'
    $script:summary.evidence.before = $before
    $script:summary.evidence.menu = $menu
    $script:summary.evidence.acceptedBeforeLeave = $baseline

    # The lifecycle menu marker requires actual configured menu BeginPlay and no driver.
    # Only now stop the owned host; the same client stays alive throughout.
    $script:requiredProcesses = @($client)
    Stop-BHReconnectProcess $initialHost
    Assert-BHReconnect $initialHost.HasExited 'initialHostStoppedAfterMenu' 'Initial owned host did not stop after the menu phase.'
    $restartedHost = Start-BHReconnectProcess 'restartedHost' (@($uproject, $manifest.maps.firstLight, "-abslog=$restartedHostLog", "-BHTestSaveSlotSuffix=${runId}_restartedhost") + $hostCommonArguments)
    $script:requiredProcesses = @($client, $restartedHost)
    $restartedOperation = Wait-BHReconnectHost $restartedHostLog 'Restarted dedicated host listening and committed'
    Assert-BHReconnect ($restartedOperation.operationId -ne $initialOperation.operationId) 'freshHostOperationIdentity' 'Restarted host did not create a distinct fresh committed operation.'
    $script:summary.evidence.restartedHostOperation = $restartedOperation
    [System.IO.File]::WriteAllText($reconnectSignal, '')

    $reconnected = Wait-BHReconnectPhase 'Same client returned to restarted host' {
        Get-BHReconnectLifecycle (Read-BHReconnectLog $clientLog) $runId 'reconnected'
    }
    $clientAfterReconnect = Read-BHReconnectLog $clientLog
    $travelRequested = Get-BHReconnectLifecycle $clientAfterReconnect $runId 'travel_requested'
    Assert-BHReconnect ($null -ne $travelRequested -and $travelRequested.offset -gt $menu.offset -and $reconnected.offset -gt $travelRequested.offset) 'reconnectPhaseOrder' 'Reconnect lifecycle markers are missing or out of order.'
    $afterTravel = $clientAfterReconnect.Substring($travelRequested.endOffset, $reconnected.offset - $travelRequested.endOffset)
    $reconnectedApplied = @(Get-BHReconnectSnapshots $afterTravel)
    Assert-BHReconnect ($reconnectedApplied.Count -gt 0) 'newSnapshotActuallyApplied' 'No successful snapshot application occurred after the actual reconnect travel request.'
    $recovered = $reconnectedApplied[-1]
    Assert-BHReconnect ($recovered.signature -eq $reconnected.signature) 'reconnectedIdentityMatchesAcceptedState' 'Reconnected identity does not match the new successfully applied snapshot.'
    Assert-BHReconnect ($recovered.revision -lt $baseline.revision) 'lowerRevisionAccepted' 'The retained client did not accept a lower revision from the restarted host.'
    Assert-BHReconnect ($reconnected.pid -eq $client.Id -and $reconnected.pid -eq $before.pid) 'sameClientProcess' 'Client PID changed across reconnect.'
    Assert-BHReconnect ($before.gameInstance -match '^\d+:\d+$' -and $reconnected.gameInstance -eq $before.gameInstance) 'sameGameInstance' 'GameInstance identity changed across reconnect.'
    Assert-BHReconnect ($before.warSubsystem -match '^\d+:\d+$' -and $reconnected.warSubsystem -eq $before.warSubsystem) 'sameWarSubsystem' 'WarSubsystem identity changed across reconnect.'
    Assert-BHReconnect ($reconnected.connection -match '^\d+:\d+$' -and $reconnected.connection -ne $before.connection) 'newLiveServerConnection' 'Reconnect did not use a different live server connection identity.'
    Assert-BHReconnect ($recovered.operationId -eq $restartedOperation.operationId -and $recovered.sectors -gt 0) 'restartedHostOperationAccepted' 'Accepted state does not contain the restarted host committed operation.'
    $hostPublished = Wait-BHReconnectPhase 'Accepted revision matches restarted host published campaign state' {
        $published = @(Get-BHReconnectSnapshots (Read-BHReconnectLog $restartedHostLog) 'BH_WAR_SNAPSHOT_PUBLISHED' | Where-Object { $_.signature -eq $recovered.signature })
        if ($published.Count -gt 0) { return $published[-1] }
        return $null
    }
    Assert-BHReconnect ($hostPublished.signature -eq $recovered.signature) 'hostAndClientSnapshotConverged' 'Restarted host publication does not match client accepted revision, turn, sectors, and operation.'
    $script:summary.evidence.travelRequested = $travelRequested
    $script:summary.evidence.reconnected = $reconnected
    $script:summary.evidence.acceptedAfterReconnect = $recovered
    $script:summary.evidence.matchingHostPublication = $hostPublished
    $script:summary.phase = 'complete'
    $script:summary.success = $true
    $exitCode = 0
} catch {
    $script:summary.failureMessage = $_.Exception.Message
    Write-Warning "Same-process reconnect failed: $($_.Exception.Message)"
} finally {
    foreach ($ownedProcess in $script:launchedProcesses) {
        try { Stop-BHReconnectProcess $ownedProcess }
        catch {
            $script:summary.cleanupErrors += $_.Exception.Message
            $script:summary.success = $false
            $exitCode = 1
        }
        finally { $ownedProcess.Dispose() }
    }
    $script:summary.completedAt = (Get-Date).ToString('o')
    $script:summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-Host "Same-process reconnect summary: $summaryPath"
}
exit $exitCode
