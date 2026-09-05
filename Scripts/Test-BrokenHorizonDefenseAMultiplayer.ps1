[CmdletBinding()]
param(
    [ValidateRange(0, 65535)][int]$Port = 0,
    [ValidateRange(5, 120)][int]$ConnectionTimeoutSeconds = 120,
    [ValidateRange(15, 300)][int]$PhaseTimeoutSeconds = 180,
    [ValidateRange(120, 900)][int]$TotalTimeoutSeconds = 900,
    [ValidatePattern('^[A-Za-z0-9_-]{1,80}$')]
    [string]$LogPrefix = 'BH-Alpha-DefenseA-Multiplayer',
    [switch]$Rendered
)

# Windows PowerShell 5.1. Uses the real director/session/HUD paths; controls only
# fixture deployment, damage and physical positioning, never operation clocks.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Read-BHDefenseLog {
    param([string]$LogFile)
    if (Test-Path -LiteralPath $LogFile) {
        return [string](Get-Content -LiteralPath $LogFile -Raw)
    }
    return ''
}

function Get-BHDefenseRows {
    param([string]$LogText, [ValidateSet('ready', 'hud', 'host', 'debrief', 'terminal', 'capture', 'presentation', 'continueRequested', 'continueAcknowledged', 'continued')][string]$Kind)
    $identity = ' run_id=(?<runId>\S+) role=(?<role>\S+)'
    $state = ' operation_id=(?<operationId>\S+) revision=(?<revision>\d+) phase=(?<phase>\d+) wave=(?<wave>\d+) total=(?<total>\d+) hostiles=(?<hostiles>\d+) defeated=(?<defeated>\d+)'
    $pattern = switch ($Kind) {
        'presentation' { 'BH_TEST_DEFENSE_A_PRESENTATION' + $identity + ' result=observed operation_id=(?<operationId>\S+) revision=(?<revision>\d+) phase=(?<phase>\d+) objective_visible=(?<objectiveVisible>[01]) briefing_present=(?<briefingPresent>[01]) notifications_suppressed=(?<notificationsSuppressed>[01])' }
        'continueRequested' { 'BH_TEST_DEFENSE_A_CONTINUE' + $identity + ' result=requested operation_id=(?<operationId>\S+)' }
        'continueAcknowledged' { 'BH_TEST_DEFENSE_A_CONTINUE' + $identity + ' result=acknowledged operation_id=(?<operationId>\S+)' }
        'continued' { 'BH_TEST_DEFENSE_A_CONTINUE' + $identity + ' result=success operation_id=(?<operationId>\S+) acknowledged=(?<acknowledged>[01]) war_map_open=(?<warMapOpen>[01]) current_objective=(?<currentObjective>\S+) objective_visible=(?<objectiveVisible>[01]) notifications_suppressed=(?<notificationsSuppressed>[01]) debrief_viewport=(?<debriefViewport>[01]) snapshot_phase=(?<snapshotPhase>\d+)' }
        'ready' { 'BH_TEST_DEFENSE_A_HUD_READY' + $identity + ' result=success' }
        'hud' { 'BH_TEST_DEFENSE_A_HUD' + $identity + ' result=observed' + $state + ' visible=(?<visible>[01]) active=(?<active>[01]) text=(?<text>[^\r\n]*)' }
        'host' { 'BH_TEST_DEFENSE_A_HOST' + $identity + ' step=(?<step>\S+) result=observed' + $state + ' authored=(?<authored>\d+) active=(?<active>\d+) runtime=(?<runtime>\d+) participants=(?<participants>\d+)' }
        'debrief' { 'BH_TEST_DEFENSE_A_DEBRIEF' + $identity + ' result=observed operation_id=(?<operationId>\S+) viewport=(?<viewport>[01]) mission_complete=(?<missionComplete>[01]) text=(?<text>[^\r\n]*)' }
        'terminal' { 'BH_TEST_DEFENSE_A_MULTIPLAYER' + $identity + ' result=success operation_id=(?<operationId>\S+) revision=(?<revision>\d+) phase=(?<phase>\d+) participants=(?<participants>\d+) completed=(?<completed>\d+) wave=(?<wave>\d+) total=(?<total>\d+) defeated=(?<defeated>\d+)' }
        'capture' { 'BH_TEST_DEFENSE_A_CAPTURE' + $identity + ' stage=(?<stage>\S+) result=(?<result>requested|written) path=(?<path>[^\r\n]*)' }
    }
    $expression = [regex]::new($pattern)
    $numericFields = @('revision', 'phase', 'wave', 'total', 'hostiles', 'defeated', 'visible', 'active', 'authored', 'runtime', 'participants', 'viewport', 'missionComplete', 'completed', 'objectiveVisible', 'briefingPresent', 'notificationsSuppressed', 'acknowledged', 'warMapOpen', 'debriefViewport', 'snapshotPhase')
    foreach ($recordMatch in $expression.Matches($LogText)) {
        $record = [ordered]@{ kind = $Kind; offset = $recordMatch.Index }
        foreach ($groupName in $expression.GetGroupNames()) {
            if ($groupName -eq '0') { continue }
            $record[$groupName] = if ($groupName -in $numericFields) {
                [int]$recordMatch.Groups[$groupName].Value
            } else { $recordMatch.Groups[$groupName].Value }
        }
        [pscustomobject]$record
    }
}

function Assert-BHDefense {
    param([bool]$Condition, [string]$Name, [string]$Failure)
    $script:report.assertions[$Name] = $Condition
    if (-not $Condition) { throw $Failure }
}

function Wait-BHDefense {
    param([string]$Label, [int]$TimeoutSeconds, [scriptblock]$Probe, [object]$ProbeContext)
    $script:report.phase = $Label
    Write-Host "Defense A multiplayer: $Label"
    $phaseDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    if ($phaseDeadline -gt $script:totalDeadline) { $phaseDeadline = $script:totalDeadline }
    do {
        if ([DateTime]::UtcNow -ge $script:totalDeadline) { throw "Total $TotalTimeoutSeconds second deadline expired." }
        foreach ($requiredHandle in $script:ownedProcesses) {
            $requiredHandle.Refresh()
            if ($requiredHandle.HasExited) { throw "Owned process $($requiredHandle.Id) exited during '$Label' with code $($requiredHandle.ExitCode)." }
        }
        foreach ($scanLogFile in $script:allLogs) {
            $scanText = Read-BHDefenseLog $scanLogFile
            if ($scanText -match $script:failurePattern) { throw "Failure marker '$($Matches[0])' in $scanLogFile" }
        }
        # Probe inputs are explicit: no reliance on caller variables captured by
        # PowerShell's dynamic scope (including the former path/Path collision).
        $probeResult = & $Probe $ProbeContext
        if ($null -ne $probeResult -and $probeResult -ne $false) { return $probeResult }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $phaseDeadline)
    throw "Timed out during '$Label' (phase limit $TimeoutSeconds seconds; total limit $TotalTimeoutSeconds seconds)."
}

function Start-BHDefense {
    param([string]$Role, [string[]]$NativeArguments)
    $quoted = foreach ($nativeArgument in $NativeArguments) {
        if ($nativeArgument.Contains('"') -or $nativeArgument.EndsWith('\')) { throw "Invalid launch argument: $nativeArgument" }
        '"' + $nativeArgument + '"'
    }
    $script:report.commands[$Role] = [ordered]@{ executable = $editor; arguments = $NativeArguments; commandLine = '"' + $editor + '" ' + ($quoted -join ' ') }
    Write-Host $script:report.commands[$Role].commandLine
    $newHandle = Start-Process -FilePath $editor -ArgumentList $quoted -WindowStyle Hidden -PassThru
    $script:ownedProcesses.Add($newHandle)
    $script:report.processes[$Role] = $newHandle.Id
}

function Stop-BHDefense {
    param([System.Diagnostics.Process]$OwnedHandle)
    $OwnedHandle.Refresh()
    if (-not $OwnedHandle.HasExited) {
        Stop-Process -InputObject $OwnedHandle -ErrorAction Stop
        if (-not $OwnedHandle.WaitForExit(5000)) {
            Stop-Process -InputObject $OwnedHandle -Force -ErrorAction Stop
            if (-not $OwnedHandle.WaitForExit(5000)) { throw "Owned process $($OwnedHandle.Id) did not stop." }
        }
    }
}

function Get-BHDefenseStage {
    param([object]$StageContext)
    $hostRows = @(Get-BHDefenseRows (Read-BHDefenseLog $StageContext.hostLog) 'host' | Where-Object {
        $_.runId -eq $StageContext.runId -and $_.role -eq 'Host' -and $_.step -eq $StageContext.hostStep -and
        $_.phase -eq $StageContext.phase -and $_.wave -eq $StageContext.wave
    })
    if ($hostRows.Count -eq 0) { return $null }
    $allClientRows = @{}
    foreach ($clientRole in @('ClientA', 'ClientB')) {
        $allClientRows[$clientRole] = @(Get-BHDefenseRows (Read-BHDefenseLog $StageContext.clientLogs[$clientRole]) 'hud' | Where-Object {
            $_.runId -eq $StageContext.runId -and $_.role -eq $clientRole -and
            $_.phase -eq $StageContext.phase -and $_.wave -eq $StageContext.wave -and
            $_.visible -eq $StageContext.visible -and $_.active -eq $StageContext.active
        })
        if ($allClientRows[$clientRole].Count -eq 0) { return $null }
    }
    foreach ($authoritativeRow in $hostRows) {
        $matchingClients = [ordered]@{}
        $matchingPresentations = [ordered]@{}
        foreach ($clientRole in @('ClientA', 'ClientB')) {
            $matchesState = @($allClientRows[$clientRole] | Where-Object {
                $_.operationId -eq $authoritativeRow.operationId -and $_.total -eq $authoritativeRow.total -and
                $_.hostiles -eq $authoritativeRow.hostiles -and $_.defeated -eq $authoritativeRow.defeated -and
                -not [string]::IsNullOrWhiteSpace($_.text)
            })
            if ($matchesState.Count -eq 0) { break }
            $presentationRows = @(Get-BHDefenseRows (Read-BHDefenseLog $StageContext.clientLogs[$clientRole]) 'presentation' | Where-Object {
                $_.runId -eq $StageContext.runId -and $_.role -eq $clientRole
            })
            if ($presentationRows.Count -eq 0) { break }
            $presentation = $presentationRows[-1]
            $expectedSuppression = if ($StageContext.phase -eq 6) { 1 } else { 0 }
            if ($presentation.operationId -ne $authoritativeRow.operationId -or
                $presentation.phase -ne $StageContext.phase -or
                $presentation.objectiveVisible -ne 0 -or $presentation.briefingPresent -ne 0 -or
                $presentation.notificationsSuppressed -ne $expectedSuppression) { break }
            $matchingClients[$clientRole] = $matchesState[-1]
            $matchingPresentations[$clientRole] = $presentation
        }
        if ($matchingClients.Count -eq 2) {
            return [pscustomobject]@{ host = $authoritativeRow; clients = $matchingClients; presentation = $matchingPresentations }
        }
    }
    return $null
}

function Wait-BHDefenseStage {
    param([string]$StageName, [int]$ExpectedPhase, [int]$ExpectedWave, [int]$ExpectedVisible = 1, [int]$ExpectedActive = 1)
    $stageContext = [pscustomobject]@{
        hostLog = $hostLog; clientLogs = $clientLogs; runId = $runId; hostStep = $StageName
        phase = $ExpectedPhase; wave = $ExpectedWave; visible = $ExpectedVisible; active = $ExpectedActive
    }
    $stageEvidence = Wait-BHDefense "$StageName on host and both actual local HUDs" $PhaseTimeoutSeconds {
        param($currentStage)
        Get-BHDefenseStage $currentStage
    } $stageContext
    Assert-BHDefense ($stageEvidence.host.operationId -ne 'None' -and $stageEvidence.host.operationId -ne $null -and $stageEvidence.host.participants -ge 2) "$StageName-operation-and-participants" 'Phase has no operation or fewer than two participants.'
    foreach ($statusRole in @('ClientA', 'ClientB')) {
        $actualStatus = $stageEvidence.clients[$statusRole].text
        $statusMatches = switch ($ExpectedPhase) {
            2 { $actualStatus -match ('WAVE_' + $ExpectedWave + '/' + $stageEvidence.host.total + '(?:_|$)') -and $actualStatus -match ('HOSTILES_' + $stageEvidence.clients[$statusRole].hostiles + '(?:_|$)') -and $actualStatus -match ('LOSSES_' + $stageEvidence.clients[$statusRole].defeated + '(?:_|$)') }
            3 { $actualStatus.Contains("WAVE_$ExpectedWave/$($stageEvidence.host.total)_CLEAR") -and $actualStatus.Contains('REINFORCEMENTS_') }
            4 { $actualStatus.Contains('SECURE_AND_HOLD') }
            6 { $actualStatus -eq 'none' }
            default { $false }
        }
        Assert-BHDefense $statusMatches "$StageName-$statusRole-actual-status" 'Actual widget status text does not match the observed phase.'
    }
    $script:report.stages[$StageName] = $stageEvidence
    return $stageEvidence
}

function Get-BHDefensePng {
    param([string]$ImageFile)
    if (-not (Test-Path -LiteralPath $ImageFile -PathType Leaf)) { return $null }
    try {
        $imageBytes = [IO.File]::ReadAllBytes($ImageFile)
        if ($imageBytes.Length -lt 33 -or [BitConverter]::ToString($imageBytes, 0, 8) -ne '89-50-4E-47-0D-0A-1A-0A') { return $null }
        $decodedImage = [System.Drawing.Image]::FromFile($ImageFile)
        try {
            if ($decodedImage.Width -ne 1280 -or $decodedImage.Height -ne 720) { throw "Capture has dimensions $($decodedImage.Width)x$($decodedImage.Height), expected 1280x720: $ImageFile" }
            return [pscustomobject]@{ path = $ImageFile; width = $decodedImage.Width; height = $decodedImage.Height; bytes = $imageBytes.Length; sha256 = (Get-FileHash -LiteralPath $ImageFile -Algorithm SHA256).Hash }
        } finally { $decodedImage.Dispose() }
    } catch {
        # An asynchronous image write may not be decodable yet. The bounded wait
        # reports failure if a complete frame never arrives.
        return $null
    }
}

function Wait-BHDefenseCaptures {
    param([string]$CaptureStage)
    if (-not $Rendered) { return }
    $captureContext = [pscustomobject]@{ root = $sharedRoot; stage = $CaptureStage; runId = $runId; clientLogs = $clientLogs }
    $captureEvidence = Wait-BHDefense "$CaptureStage real PNG frames from both clients" $PhaseTimeoutSeconds {
        param($currentCapture)
        $frames = [ordered]@{}
        foreach ($captureRole in @('ClientA', 'ClientB')) {
            $frameFile = Join-Path (Join-Path $currentCapture.root $captureRole) ($currentCapture.stage + '.png')
            $records = @(Get-BHDefenseRows (Read-BHDefenseLog $currentCapture.clientLogs[$captureRole]) 'capture' | Where-Object {
                $_.runId -eq $currentCapture.runId -and $_.role -eq $captureRole -and $_.stage -eq $currentCapture.stage -and $_.result -eq 'written'
            })
            if ($records.Count -eq 0) { return $null }
            $frame = Get-BHDefensePng $frameFile
            if ($null -eq $frame) { return $null }
            $frames[$captureRole] = $frame
        }
        return [pscustomobject]$frames
    } $captureContext
    $script:report.captures[$CaptureStage] = $captureEvidence
}

function Write-BHDefenseControl {
    param([ValidatePattern('^(start|clear_wave[1-9][0-9]*|secure|continue)\.ready$')][string]$ControlName)
    [IO.File]::WriteAllText((Join-Path $sharedRoot $ControlName), '')
    $script:report.controls += [pscustomobject]@{ name = $ControlName; issuedAt = (Get-Date).ToString('o') }
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'Config\ProjectManifest.json') -Raw | ConvertFrom-Json
$uproject = Join-Path $projectRoot $manifest.uproject
$editor = Join-Path $manifest.engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor not found: $editor" }
if ($Port -gt 0 -and $Port -lt 1024) { throw 'Port must be zero or 1024..65535.' }
$portReservation = [Net.Sockets.UdpClient]::new()
try {
    $portReservation.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback, $Port))
    $Port = $portReservation.Client.LocalEndPoint.Port
} finally { $portReservation.Dispose() }
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$sharedRoot = Join-Path $projectRoot "Saved\Automation\DefenseAMultiplayer\$runId"
$logRoot = Join-Path $projectRoot 'Saved\Logs\Codex'
New-Item -ItemType Directory -Path $sharedRoot, $logRoot -Force | Out-Null
$hostLog = Join-Path $logRoot "$LogPrefix-$runId-Host.log"
$clientLogs = @{ ClientA = (Join-Path $logRoot "$LogPrefix-$runId-ClientA.log"); ClientB = (Join-Path $logRoot "$LogPrefix-$runId-ClientB.log") }
$summaryFile = Join-Path $logRoot "$LogPrefix-$runId-Summary.json"
$script:allLogs = @($hostLog, $clientLogs.ClientA, $clientLogs.ClientB)
$script:ownedProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
$script:totalDeadline = [DateTime]::UtcNow.AddSeconds($TotalTimeoutSeconds)
$script:failurePattern = 'Fatal error:|Assertion failed:|Unhandled Exception:|NetworkFailure|TravelFailure|PIE: Error:|BH_SESSION_ERROR|BH_WAR_SNAPSHOT_APPLY_FAILED|BH_TEST_DEFENSE_A_MULTIPLAYER[^\r\n]*result=failure|BH_TEST_DEFENSE_A_GARRISON[^\r\n]*result=failure|Checkpoint save failed|BH_(WAR|FIELD)_AUTOSAVE_FAILED|BH_OPERATION_RESULT_CONTINUE_BLOCKED|BH_OPERATION_RESULT_CHECKPOINT_FAILED|BH_POST_OPERATION_FREE_ROAM_CHECKPOINT_FAILED'
$script:report = [ordered]@{
    success = $false; runId = $runId; generatedAt = (Get-Date).ToString('o'); phase = 'setup'
    port = $Port; map = $manifest.maps.firstLight; renderedMode = [bool]$Rendered
    connectionTimeoutSeconds = $ConnectionTimeoutSeconds; phaseTimeoutSeconds = $PhaseTimeoutSeconds; totalTimeoutSeconds = $TotalTimeoutSeconds
    commands = [ordered]@{}; processes = [ordered]@{}; userDirectories = [ordered]@{}; saveSlotSuffixes = [ordered]@{}
    logs = [ordered]@{ Host = $hostLog; ClientA = $clientLogs.ClientA; ClientB = $clientLogs.ClientB }
    sharedRoot = $sharedRoot; stages = [ordered]@{}; assertions = [ordered]@{}; captures = [ordered]@{}; controls = @()
    postOperationContinueVerified = $false; presentationStateVerified = $false; widgetDataVerified = $false; pngFramesDecoded = $false; visualInspectionComplete = $false
    combatInputOrFeelValidated = $false; saveRecoveryValidated = $false; reconnectValidated = $false
    limitations = @('Dedicated loopback host and two clients with controlled fixture damage and positioning.', 'Director wave and securing clocks run naturally; player combat input and feel are not exercised.', 'Decoded PNGs require separate visual inspection; markers alone do not prove image appearance.', 'No checkpoint recovery, reconnect, packaged-build, or remote-network claim.')
    failureMessage = $null; cleanupErrors = @()
}
# Runtime fixtures use isolated user data and never require editor Python startup.
$commonFlags = @('-DisablePython', '-culture=en', '-unattended', '-nosound', '-NoSplash', '-DDC-ForceMemoryCache', '-BHTestDefenseAMultiplayer', "-BHTestDefenseARunId=$runId", "-BHTestDefenseATimeout=$TotalTimeoutSeconds")
$exitCode = 1
try {
    if ($Rendered) { Add-Type -AssemblyName System.Drawing }
    foreach ($launchRole in @('Host', 'ClientA', 'ClientB')) {
        $userDirectory = Join-Path $sharedRoot "UserData\$launchRole"
        New-Item -ItemType Directory -Path $userDirectory -Force | Out-Null
        $script:report.userDirectories[$launchRole] = $userDirectory
        $script:report.saveSlotSuffixes[$launchRole] = "${runId}_$launchRole"
        $roleFlags = @("-BHTestDefenseARole=$launchRole", "-UserDir=$userDirectory", "-BHTestSaveSlotSuffix=${runId}_$launchRole") + $commonFlags
        if ($launchRole -eq 'Host') {
            Start-BHDefense $launchRole (@($uproject, $manifest.maps.firstLight, '-server', "-port=$Port", '-nullrhi', "-abslog=$hostLog", '-BHTestDefenseAGarrisonPersistence', '-LogCmds=LogTemp VeryVerbose') + $roleFlags)
            Wait-BHDefense 'Dedicated host listening' $ConnectionTimeoutSeconds {
                param($hostReadiness)
                $hostText = Read-BHDefenseLog $hostReadiness.logFile
                if ($hostText -match ('GameNetDriver[^\r\n]*listening on port ' + $hostReadiness.port + '\b') -and $hostText -match 'BH_WAR_GAME_STATE_READY') { return $true }
                return $null
            } ([pscustomobject]@{ logFile = $hostLog; port = $Port }) | Out-Null
        } else {
            $presentationFlags = if ($Rendered) { @('-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720', '-BHTestDefenseACapture') } else { @('-nullrhi') }
            Start-BHDefense $launchRole (@($uproject, "127.0.0.1:$Port", '-game', "-abslog=$($clientLogs[$launchRole])") + $presentationFlags + $roleFlags)
        }
    }
    $readyEvidence = Wait-BHDefense 'Both real local HUDs ready before deployment' $ConnectionTimeoutSeconds {
        param($readyContext)
        $readyRows = [ordered]@{}
        foreach ($readyRole in @('ClientA', 'ClientB')) {
            $localReady = @(Get-BHDefenseRows (Read-BHDefenseLog $readyContext.logs[$readyRole]) 'ready' | Where-Object { $_.runId -eq $readyContext.runId -and $_.role -eq $readyRole })
            if ($localReady.Count -eq 0) { return $null }
            if ($localReady.Count -gt 1) { throw "Duplicate local HUD readiness marker for $readyRole." }
            $readyRows[$readyRole] = $localReady[0]
        }
        return [pscustomobject]$readyRows
    } ([pscustomobject]@{ logs = $clientLogs; runId = $runId })
    $script:report.stages.localHudReady = $readyEvidence
    Write-BHDefenseControl 'start.ready'

    $wave1 = Wait-BHDefenseStage 'combat_wave1' 2 1
    $operationId = $wave1.host.operationId
    $totalWaves = $wave1.host.total
    Assert-BHDefense ($totalWaves -ge 2 -and $totalWaves -le 32) 'multipleDefenseWaves' 'Defense A did not advertise a bounded multiwave operation.'
    Assert-BHDefense ($wave1.host.authored -eq 6 -and $wave1.host.active -eq 6 -and $wave1.host.hostiles -eq 6 -and $wave1.host.runtime -eq 0) 'authoredWave1Activated' 'Wave 1 did not activate all six authored garrison actors exclusively.'
    $dormantRows = @(Get-BHDefenseRows (Read-BHDefenseLog $hostLog) 'host' | Where-Object { $_.runId -eq $runId -and $_.role -eq 'Host' -and $_.step -eq 'dormant' })
    Assert-BHDefense ($dormantRows.Count -gt 0) 'dormancyObserved' 'Host did not report real authored garrison dormancy.'
    $dormant = $dormantRows[0]
    Assert-BHDefense ($dormant.authored -eq 6 -and $dormant.active -eq 0 -and $dormant.offset -lt $wave1.host.offset) 'sixAuthoredActorsInitiallyDormant' 'Initial authored garrison count or dormancy/order was wrong.'
    $script:report.stages.dormant = $dormant
    Wait-BHDefenseCaptures 'combat_wave1'
    Write-BHDefenseControl 'clear_wave1.ready'

    $awaiting = Wait-BHDefenseStage 'awaiting_wave' 3 1
    Assert-BHDefense ($awaiting.host.hostiles -eq 0 -and $awaiting.host.active -eq 0 -and $awaiting.host.operationId -eq $operationId) 'naturalAwaitingWave' 'AwaitingWave did not clear the same operation authored wave.'
    Wait-BHDefenseCaptures 'awaiting_wave'
    for ($waveNumber = 2; $waveNumber -le $totalWaves; ++$waveNumber) {
        $combatStage = 'combat_wave' + $waveNumber
        $laterWave = Wait-BHDefenseStage $combatStage 2 $waveNumber
        Assert-BHDefense ($laterWave.host.operationId -eq $operationId -and $laterWave.host.runtime -gt 0 -and $laterWave.host.hostiles -eq $laterWave.host.runtime -and $laterWave.host.active -eq 0) "$combatStage-runtime-hostiles" 'Later wave did not use runtime hostiles in the same operation.'
        if ($waveNumber -eq 2) { Wait-BHDefenseCaptures 'combat_wave2' }
        Write-BHDefenseControl ("clear_wave$waveNumber.ready")
    }
    $securing = Wait-BHDefenseStage 'securing' 4 $totalWaves
    Assert-BHDefense ($securing.host.operationId -eq $operationId -and $securing.host.hostiles -eq 0 -and $securing.host.runtime -eq 0) 'physicalSecuringPhase' 'Final securing did not follow the cleared final wave.'
    Wait-BHDefenseCaptures 'securing'
    Write-BHDefenseControl 'secure.ready'

    $terminal = Wait-BHDefenseStage 'debrief' 6 $totalWaves 0 0
    Assert-BHDefense ($terminal.host.operationId -eq $operationId -and $terminal.host.hostiles -eq 0) 'terminalWaypointHiddenOnBothClients' 'Terminal hidden/inactive waypoints are not tied to the completed operation.'
    $completeEvidence = Wait-BHDefense 'Both actual mission-complete debrief widgets and authoritative success' $PhaseTimeoutSeconds {
        param($completionContext)
        $serverTerminal = @(Get-BHDefenseRows (Read-BHDefenseLog $completionContext.hostLog) 'terminal' | Where-Object { $_.runId -eq $completionContext.runId -and $_.role -eq 'Host' -and $_.operationId -eq $completionContext.operationId -and $_.phase -eq 6 -and $_.completed -ge 2 -and $_.participants -ge 2 })
        if ($serverTerminal.Count -eq 0) { return $null }
        $debriefRows = [ordered]@{}
        foreach ($debriefRole in @('ClientA', 'ClientB')) {
            $localDebrief = @(Get-BHDefenseRows (Read-BHDefenseLog $completionContext.logs[$debriefRole]) 'debrief' | Where-Object { $_.runId -eq $completionContext.runId -and $_.role -eq $debriefRole -and $_.operationId -eq $completionContext.operationId -and $_.viewport -eq 1 -and $_.missionComplete -eq 1 -and $_.text.Contains('MISSION_COMPLETE') -and $_.text.Contains('SUCCESS') })
            if ($localDebrief.Count -eq 0) { return $null }
            $debriefRows[$debriefRole] = $localDebrief[-1]
        }
        return [pscustomobject]@{ host = $serverTerminal[-1]; clients = $debriefRows }
    } ([pscustomobject]@{ hostLog = $hostLog; logs = $clientLogs; runId = $runId; operationId = $operationId })
    $script:report.stages.completed = $completeEvidence
    Wait-BHDefenseCaptures 'debrief'

    # Continue only after both original debrief widgets and their rendered captures.
    # Each owner must acknowledge its own real continue RPC; shared snapshot clearing
    # by the first owner cannot substitute for the second client's transition.
    $hostCheckpointBaseline = [regex]::Matches((Read-BHDefenseLog $hostLog), 'BH_OPERATION_RESULT_CHECKPOINT_CONFIRMED').Count
    Write-BHDefenseControl 'continue.ready'
    $continueEvidence = Wait-BHDefense 'Both clients continued through their actual debrief delegate and own server acknowledgment' $PhaseTimeoutSeconds {
        param($continueContext)
        $continuedClients = [ordered]@{}
        foreach ($continueRole in @('ClientA', 'ClientB')) {
            $continueText = Read-BHDefenseLog $continueContext.logs[$continueRole]
            $records = @{}
            foreach ($continueKind in @('continueRequested', 'continueAcknowledged', 'continued')) {
                $phaseRecords = @(Get-BHDefenseRows $continueText $continueKind)
                if ($phaseRecords.Count -eq 0) { return $null }
                if ($phaseRecords.Count -ne 1 -or $phaseRecords[0].runId -ne $continueContext.runId -or
                    $phaseRecords[0].role -ne $continueRole -or $phaseRecords[0].operationId -ne $continueContext.operationId) {
                    throw "Invalid or duplicate $continueKind identity for $continueRole."
                }
                $records[$continueKind] = $phaseRecords[0]
            }
            $requested = $records.continueRequested
            $acknowledged = $records.continueAcknowledged
            $settled = $records.continued
            if ($requested.offset -ge $acknowledged.offset -or $acknowledged.offset -ge $settled.offset) {
                throw "Out-of-order debrief continue evidence for $continueRole."
            }
            if ($settled.acknowledged -ne 1 -or $settled.warMapOpen -ne 1 -or
                $settled.currentObjective -ne 'None' -or $settled.objectiveVisible -ne 0 -or
                $settled.notificationsSuppressed -ne 0 -or $settled.debriefViewport -ne 0 -or
                $settled.snapshotPhase -ne 0) {
                throw "Post-operation UI state is incorrect after $continueRole acknowledged continue."
            }
            $continuedClients[$continueRole] = [pscustomobject]@{
                requested = $requested; acknowledged = $acknowledged; settled = $settled
            }
        }
        $checkpointConfirmations = [regex]::Matches((Read-BHDefenseLog $continueContext.hostLog), 'BH_OPERATION_RESULT_CHECKPOINT_CONFIRMED').Count - $continueContext.checkpointBaseline
        if ($checkpointConfirmations -lt 2) { return $null }
        if ($checkpointConfirmations -ne 2) { throw 'Unexpected number of authoritative continue checkpoint confirmations.' }
        $continuedClients['hostCheckpointConfirmations'] = $checkpointConfirmations
        $continuedClients['hostCheckpointMarkerHasOwnerIdentity'] = $false
        return [pscustomobject]$continuedClients
    } ([pscustomobject]@{ logs = $clientLogs; runId = $runId; operationId = $operationId; hostLog = $hostLog; checkpointBaseline = $hostCheckpointBaseline })
    $script:report.stages.continued = $continueEvidence
    Assert-BHDefense ($null -ne $continueEvidence.ClientA -and $null -ne $continueEvidence.ClientB) 'bothOwnersAcknowledgedContinue' 'Both clients did not independently complete their own debrief continue transition.'
    $script:report.postOperationContinueVerified = $true

    # Audit every captured state, while permitting ordinary countdown/count
    # updates and either ordering of replicated snapshot and completion RPC.
    foreach ($auditRole in @('Host', 'ClientA', 'ClientB')) {
        $auditFile = if ($auditRole -eq 'Host') { $hostLog } else { $clientLogs[$auditRole] }
        $auditKind = if ($auditRole -eq 'Host') { 'host' } else { 'hud' }
        $auditRows = @(Get-BHDefenseRows (Read-BHDefenseLog $auditFile) $auditKind)
        foreach ($auditRow in $auditRows) {
            Assert-BHDefense ($auditRow.runId -eq $runId -and $auditRow.role -eq $auditRole) "$auditRole-marker-identity" 'Unexpected fixture run or role in the owned process log.'
            if ($auditRow.phase -gt 0) { Assert-BHDefense ($auditRow.operationId -eq $operationId) "$auditRole-one-operation" 'Incompatible operation identities were observed in one fixture run.' }
        }
        $revisionGroups = @($auditRows | Group-Object revision)
        foreach ($revisionGroup in $revisionGroups) {
            $identities = @($revisionGroup.Group | ForEach-Object { '{0}:{1}:{2}:{3}:{4}:{5}' -f $_.operationId, $_.phase, $_.wave, $_.total, $_.hostiles, $_.defeated } | Select-Object -Unique)
            Assert-BHDefense ($identities.Count -le 1) "$auditRole-compatible-revision-$($revisionGroup.Name)" 'One snapshot revision carried incompatible operation state evidence.'
        }
    }
    $script:report.operationId = $operationId
    $script:report.totalWaves = $totalWaves
    $script:report.presentationStateVerified = $true
    $script:report.widgetDataVerified = $true
    $script:report.pngFramesDecoded = [bool]$Rendered
    $script:report.success = $true
    $script:report.phase = 'complete'
    $exitCode = 0
} catch {
    $script:report.failureMessage = $_.Exception.Message
    Write-Warning "Defense A multiplayer failed: $($_.Exception.Message)"
} finally {
    # Clients first; all handles were created by this invocation. No process-name
    # enumeration, unrelated shutdown, user-save deletion, or cache cleanup.
    for ($cleanupIndex = $script:ownedProcesses.Count - 1; $cleanupIndex -ge 0; --$cleanupIndex) {
        $cleanupHandle = $script:ownedProcesses[$cleanupIndex]
        try { Stop-BHDefense $cleanupHandle }
        catch { $script:report.cleanupErrors += $_.Exception.Message; $script:report.success = $false; $exitCode = 1 }
        finally { $cleanupHandle.Dispose() }
    }
    $script:report.completedAt = (Get-Date).ToString('o')
    $script:report | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $summaryFile -Encoding UTF8
    Write-Host "Defense A multiplayer summary: $summaryFile"
}
exit $exitCode
