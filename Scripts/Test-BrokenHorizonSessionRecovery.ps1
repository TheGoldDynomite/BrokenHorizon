[CmdletBinding()]
param(
    [ValidateSet('Editor','Packaged')][string]$Runtime = 'Editor',
    [string]$PackageRoot,
    [ValidateRange(1024,65535)][int]$Port = 8791,
    [ValidateRange(120,900)][int]$TimeoutSeconds = 600,
    [ValidatePattern('^[A-Za-z0-9_-]{1,64}$')][string]$LogPrefix = 'BH-SessionRecovery'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'Config\ProjectManifest.json') | ConvertFrom-Json
$resolvedPackageRoot = $null
if ($Runtime -eq 'Packaged') {
    if ([string]::IsNullOrWhiteSpace($PackageRoot)) { throw 'Packaged runtime requires an explicit Windows archive PackageRoot.' }
    $resolvedPackageRoot = if ([IO.Path]::IsPathRooted($PackageRoot)) { [IO.Path]::GetFullPath($PackageRoot) } else { [IO.Path]::GetFullPath((Join-Path $projectRoot $PackageRoot)) }
    $executable = Join-Path $resolvedPackageRoot 'BrokenHorizon\Binaries\Win64\BrokenHorizon.exe'
} else { $executable = Join-Path $manifest.engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe' }
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Missing runtime: $executable" }
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$runDirectory = Join-Path $projectRoot "Saved\Automation\SessionRecovery\$runId"
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$summaryPath = Join-Path $runDirectory "$LogPrefix-Summary.json"
$roles = [ordered]@{}
$active = @{}
$summary = [ordered]@{
    result = 'Failed'; runId = $runId; runtime = $Runtime; packageRoot = $resolvedPackageRoot
    executable = $executable; executableSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $executable).Hash
    port = $Port; deadlineUtc = $deadline.ToString('o'); roles = $roles; evidence = [ordered]@{}
    menuButtonInvocation = 'Actual displayed local NewGame/Join UButton.OnClicked broadcasts through real bindings'
    physicalInputProof = $false; renderedPixelProof = $false; continueRestorationProof = $false
    failure = $null; cleanupErrors = @(); runDirectory = $runDirectory
}
function Get-SessionRecoveryRecords {
    param([string]$Content, [string]$RunId, [string]$Role)
    foreach ($line in [regex]::Matches($Content, 'BH_TEST_SESSION_RECOVERY [^\r\n]+')) {
        $fields = [ordered]@{}
        foreach ($field in [regex]::Matches($line.Value, '(?<key>[a-z_]+)=(?:"(?<quoted>[^"]*)"|(?<plain>[^\s]+))')) {
            $key = $field.Groups['key'].Value
            if ($fields.Contains($key)) { throw "Duplicate fixture field: $key" }
            $fields[$key] = if ($field.Groups['quoted'].Success) { $field.Groups['quoted'].Value } else { $field.Groups['plain'].Value }
        }
        foreach ($key in @('run_id','role','phase','result','pid','state','pending','named_session','net_mode','players','remote_open',
            'local_possessed','connection_open','game_instance','connection','widget','actionable','host_enabled','join_enabled','status','control_dir','detail')) {
            if (-not $fields.Contains($key)) { throw "Missing fixture field: $key" }
        }
        if ($fields.run_id -cne $RunId -or $fields.role -cne $Role) { throw 'Foreign run/role marker in owned log.' }
        if ($fields.result -eq 'failure') { throw "Native fixture failed: $($fields.detail)" }
        if ($fields.result -notin @('observed','requested')) { throw 'Invalid fixture result.' }
        foreach ($key in @('pid','state','pending','named_session','players','remote_open','local_possessed','connection_open','widget','actionable','host_enabled','join_enabled')) {
            if ($fields[$key] -notmatch '^[0-9]+$') { throw "Invalid numeric fixture field: $key" }
        }
        [pscustomobject]$fields
    }
}
function Assert-SessionFixtureAlive {
    foreach ($role in @($active.Keys)) {
        $process = $active[$role]; $process.Refresh()
        if ($process.HasExited) { throw "Owned $role process exited unexpectedly; see $($roles[$role].log)" }
        if (Test-Path -LiteralPath $roles[$role].log) {
            $content = Get-Content -Raw -LiteralPath $roles[$role].log
            if ($content -match 'Fatal error:|Assertion failed:|Unhandled Exception:') { throw "Fatal error in owned $role log." }
            $null = @(Get-SessionRecoveryRecords -Content $content -RunId $runId -Role $role)
        }
    }
}
function Wait-SessionPhase {
    param([string]$Role, [string]$Phase, [int]$MaximumSeconds = 120)
    $phaseDeadline = [DateTime]::UtcNow.AddSeconds($MaximumSeconds)
    do {
        Assert-SessionFixtureAlive
        if (Test-Path -LiteralPath $roles[$Role].log) {
            $records = @(Get-SessionRecoveryRecords -Content (Get-Content -Raw -LiteralPath $roles[$Role].log) -RunId $runId -Role $Role | Where-Object phase -eq $Phase)
            if ($records.Count -gt 1) { throw "Duplicate $Role/$Phase evidence." }
            if ($records.Count -eq 1) {
                if ([int]$records[0].pid -ne $roles[$Role].processId) { throw 'Fixture PID does not match the launched owner.' }
                if ($records[0].result -ne 'observed') { throw "Expected an observation for $Role/$Phase." }
                return $records[0]
            }
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline -and [DateTime]::UtcNow -lt $phaseDeadline)
    throw "Timed out waiting for $Role/$Phase; see $($roles[$Role].log)"
}
function Assert-ActionableMenu {
    param($Record, [int]$ExpectedState)
    if ($Record.state -ne "$ExpectedState" -or $Record.pending -ne '0' -or $Record.widget -ne '1' -or
        $Record.actionable -ne '1' -or $Record.host_enabled -ne '1' -or $Record.join_enabled -ne '1' -or [string]::IsNullOrWhiteSpace($Record.status)) {
        throw "Menu is not actually actionable at $($Record.role)/$($Record.phase)."
    }
    if ($ExpectedState -eq 7 -and $Record.status -notmatch 'ACTION FAILED') { throw 'Actual menu does not display the Error heading.' }
}
function Start-SessionRole {
    param([ValidateSet('Host','Client','RestartHost')][string]$Role)
    $userDirectory = Join-Path $runDirectory "$Role\User"
    New-Item -ItemType Directory -Path $userDirectory -Force | Out-Null
    $log = Join-Path $runDirectory "$LogPrefix-$Role.log"
    $saveSuffix = "SessionRecovery_$($runId -replace '-', '_')_$Role"
    $arguments = @()
    if ($Runtime -eq 'Editor') { $arguments += (Join-Path $projectRoot $manifest.uproject) }
    $arguments += @($manifest.maps.mainMenu, '-game', '-nullrhi', '-unattended', '-nosound', '-NoSplash', '-DisablePython', '-DDC-ForceMemoryCache',
        "-Port=$Port", "-UserDir=$userDirectory", "-BHTestSaveSlotSuffix=$saveSuffix", "-abslog=$log",
        '-BHTestSessionRecovery', "-BHTestSessionRunId=$runId", "-BHTestSessionRole=$Role", "-BHTestSessionTimeout=$TimeoutSeconds")
    $quoted = foreach ($argument in $arguments) {
        if ($argument -match '["\r\n]' -or $argument.EndsWith('\')) { throw "Unsupported process argument: $argument" }
        '"' + $argument + '"'
    }
    $roles[$Role] = [ordered]@{log=$log; userDirectory=$userDirectory; saveSuffix=$saveSuffix; arguments=$arguments; processId=$null; controlDirectory=$null; stopped=$false}
    $process = Start-Process -FilePath $executable -ArgumentList $quoted -WindowStyle Hidden -PassThru
    $active[$Role] = $process; $roles[$Role].processId = $process.Id
    $ready = Wait-SessionPhase $Role 'ready'
    Assert-ActionableMenu $ready 0
    $control = [IO.Path]::GetFullPath($ready.control_dir)
    $ownedPrefix = [IO.Path]::GetFullPath($userDirectory).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $expectedEnd = [IO.Path]::DirectorySeparatorChar + ('Automation\SessionRecovery\' + $runId)
    if (-not $control.StartsWith($ownedPrefix,[StringComparison]::OrdinalIgnoreCase) -or -not $control.EndsWith($expectedEnd,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Native control directory is not the fixed path beneath owned UserDir: $control"
    }
    $roles[$Role].controlDirectory = $control
}
function Send-SessionSignal {
    param([string]$Role, [ValidateSet('search.ready','reject.ready','host.ready','join.ready','retry.ready')][string]$Signal)
    if (-not $roles[$Role].controlDirectory) { throw 'Role has no validated control directory.' }
    Set-Content -LiteralPath (Join-Path $roles[$Role].controlDirectory $Signal) -Value 'ready' -Encoding ASCII
}
function Stop-SessionRole {
    param([string]$Role)
    if (-not $active.ContainsKey($Role)) { return }
    $process = $active[$Role]; $process.Refresh()
    if (-not $process.HasExited) { Stop-Process -Id $process.Id; if (-not $process.WaitForExit(10000)) { throw "Owned $Role did not stop." } }
    $roles[$Role].stopped = $true
    $roles[$Role].exitCode = $process.ExitCode
    $active.Remove($Role)
}
function Assert-JoinedParticipant {
    param($Record)
    if ($Record.state -ne '5' -or $Record.pending -ne '0' -or $Record.net_mode -ne '3' -or
        $Record.local_possessed -ne '1' -or $Record.connection_open -ne '1' -or $Record.named_session -ne '1' -or $Record.connection -eq 'none') {
        throw "Missing actual joined participant evidence at $($Record.phase)."
    }
}
function Assert-HostParticipants {
    param($Record)
    if ($Record.state -ne '5' -or $Record.pending -ne '0' -or $Record.net_mode -ne '2' -or
        $Record.players -ne '2' -or $Record.remote_open -ne '1' -or $Record.local_possessed -ne '1' -or $Record.named_session -ne '1') {
        throw 'Host does not have exactly one local and one connected possessed remote.'
    }
}
try {
    Start-SessionRole Client
    Start-SessionRole Host
    Send-SessionSignal Client search.ready
    $noMatch = Wait-SessionPhase Client no_match
    Assert-ActionableMenu $noMatch 7
    if ($noMatch.named_session -ne '0') { throw 'Initial no-match search unexpectedly owns a named session.' }
    Send-SessionSignal Host reject.ready
    $rejected = Wait-SessionPhase Host rejected
    Assert-ActionableMenu $rejected 7
    $nativeRejected = Wait-SessionPhase Host server_travel_rejected
    if ($nativeRejected.detail -ne 'accepted=0' -or $rejected.named_session -ne '1') { throw 'Missing actual synchronous rejection and retained named session.' }
    Send-SessionSignal Host host.ready
    $hosted = Wait-SessionPhase Host hosted
    if ($hosted.state -ne '5' -or $hosted.net_mode -ne '2' -or $hosted.local_possessed -ne '1') { throw 'Actual menu host retry did not reach a possessed listen world.' }
    Send-SessionSignal Client join.ready
    $joined = Wait-SessionPhase Client joined
    Assert-JoinedParticipant $joined
    $initialParticipants = Wait-SessionPhase Host participants
    Assert-HostParticipants $initialParticipants
    $initialAddress = Wait-SessionPhase Client resolved_join
    Stop-SessionRole Host
    $disconnected = Wait-SessionPhase Client disconnected 120
    Assert-ActionableMenu $disconnected 7
    if ($disconnected.named_session -ne '1') { throw 'Expected retained named session before direct Join retry.' }
    Start-SessionRole RestartHost
    Send-SessionSignal RestartHost host.ready
    $null = Wait-SessionPhase RestartHost hosted
    Send-SessionSignal Client retry.ready
    $rejoined = Wait-SessionPhase Client rejoined
    Assert-JoinedParticipant $rejoined
    $retryParticipants = Wait-SessionPhase RestartHost participants
    Assert-HostParticipants $retryParticipants
    if ($joined.game_instance -cne $rejoined.game_instance -or $joined.pid -cne $rejoined.pid -or $joined.connection -ceq $rejoined.connection) {
        throw 'Retry did not retain the client GameInstance/process with a new server connection.'
    }
    $clientRecords = @(Get-SessionRecoveryRecords -Content (Get-Content -Raw -LiteralPath $roles.Client.log) -RunId $runId -Role Client)
    $resolved = @($clientRecords | Where-Object phase -eq resolved_join)
    if ($resolved.Count -ne 2 -or @($resolved | Where-Object { $_.detail -notmatch "address=.+:$Port(?:\D|$)" }).Count -gt 0) { throw 'LAN discovery did not resolve the expected owned game port on both joins.' }
    Assert-SessionFixtureAlive
    $summary.evidence = [ordered]@{noMatch=$noMatch; rejected=$rejected; hostRetry=$hosted; joined=$joined; initialHostParticipants=$initialParticipants;
        disconnected=$disconnected; rejoined=$rejoined; restartedHostParticipants=$retryParticipants; discoveredAddresses=@($resolved.detail);
        sameClientProcess=$true; sameGameInstance=$true; replacedServerConnection=$true; directJoinRetryWithoutLeave=$true}
    $summary.result = 'Passed'
} catch { $summary.failure = $_.Exception.Message; throw }
finally {
    foreach ($role in @($active.Keys)) { try { Stop-SessionRole $role } catch { $summary.cleanupErrors += $_.Exception.Message } }
    if ($summary.cleanupErrors.Count -gt 0) { $summary.result = 'Failed' }
    $summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-Host "Session recovery result=$($summary.result): $summaryPath"
    if ($summary.cleanupErrors.Count -gt 0) { throw ($summary.cleanupErrors -join '; ') }
}
