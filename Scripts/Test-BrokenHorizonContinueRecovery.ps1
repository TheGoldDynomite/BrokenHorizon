[CmdletBinding()]
param(
    [ValidateSet('Editor','Packaged')][string]$Runtime = 'Editor',
    [string]$PackageRoot,
    [ValidateRange(1024,65535)][int]$Port = 8793,
    [ValidateRange(120,600)][int]$TimeoutSeconds = 600,
    [ValidatePattern('^[A-Za-z0-9_-]{1,64}$')][string]$LogPrefix = 'BH-ContinueRecovery'
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
$runDirectory = Join-Path $projectRoot "Saved\Automation\ContinueRecovery\$runId"
$userDirectory = Join-Path $runDirectory 'User'
New-Item -ItemType Directory -Path $userDirectory -Force | Out-Null
$log = Join-Path $runDirectory "$LogPrefix.log"
$summaryPath = Join-Path $runDirectory "$LogPrefix-Summary.json"
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$ownedProcess = $null
$summary = [ordered]@{
    result = 'Failed'; runId = $runId; runtime = $Runtime; packageRoot = $resolvedPackageRoot
    executable = $executable; executableSha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
    processId = $null; runDirectory = $runDirectory; userDirectory = $userDirectory; log = $log
    port = $Port; deadlineUtc = $deadline.ToString('o'); nativeDeadlineSeconds = 600; arguments = @()
    controlDirectory = $null; checkpointBefore = $null; checkpointFailed = $null; checkpointAfter = $null
    evidence = [ordered]@{}; failure = $null; cleanupError = $null
    proof = 'Actual bound Continue button delegate, real deferred checkpoint apply failure, protected slots, and same-process retry restoration'
    fault = 'Duplicate runtime door PersistenceID; real pre-apply validation failure; removed by world teardown'
    consumedEvidence = 'Fixture-owned consumed-item tracking; no physical pickup/removal claim'
    partialApplyRollbackProof = $false; physicalInputProof = $false; renderedPixelProof = $false
}
function Get-ContinueRecoveryRecords {
    param([string]$Content, [string]$RunId)
    foreach ($line in [regex]::Matches($Content, 'BH_TEST_CONTINUE_RECOVERY [^\r\n]+')) {
        $fields = [ordered]@{}
        foreach ($field in [regex]::Matches($line.Value, '(?<key>[a-z_]+)=(?:"(?<quoted>[^"]*)"|(?<plain>[^\s]+))')) {
            $key = $field.Groups['key'].Value
            if ($fields.Contains($key)) { throw "Duplicate fixture field: $key" }
            $fields[$key] = if ($field.Groups['quoted'].Success) { $field.Groups['quoted'].Value } else { $field.Groups['plain'].Value }
        }
        foreach ($key in @('run_id','phase','pid','state','pending','protected','actionable','net_mode','control_dir','primary','backup','status','detail')) {
            if (-not $fields.Contains($key)) { throw "Missing fixture field: $key" }
        }
        if ($fields.run_id -cne $RunId) { throw 'Foreign run marker in owned log.' }
        if ($fields.phase -eq 'failure') { throw "Native fixture failed: $($fields.detail)" }
        foreach ($key in @('pid','state','pending','protected','actionable','net_mode')) {
            if ($fields[$key] -notmatch '^[0-9]+$') { throw "Invalid numeric fixture field: $key" }
        }
        $fields['index'] = $line.Index
        [pscustomobject]$fields
    }
}
function Assert-ContinueRecord {
    param($Record, [int]$State, [int]$Pending, [int]$Protected, [int]$Actionable = -1)
    if ($Record.state -ne "$State" -or $Record.pending -ne "$Pending" -or $Record.protected -ne "$Protected" -or
        ($Actionable -ge 0 -and $Record.actionable -ne "$Actionable")) { throw "Unexpected state at $($Record.phase)." }
}
function Assert-ContinueWriteProtection {
    param($Record)
    Assert-ContinueRecord $Record 4 1 1
    $expected = 'full=0 character=0 resource=0 item=0 central=0 tracking_unchanged=1 hashes_unchanged=1 valid_data=1 live_character=1'
    if ($Record.detail -cne $expected) { throw "Incomplete real write protection proof at $($Record.phase): $($Record.detail)" }
}
function Assert-ContinueOwnedPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path) -or -not [IO.Path]::IsPathRooted($Path)) { throw 'Missing absolute owned evidence path.' }
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith([IO.Path]::GetFullPath($userDirectory).TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Evidence path escaped isolated UserDir.' }
    return $full
}
function Read-ContinueSlotHashes {
    param($Record)
    $primary = Assert-ContinueOwnedPath $Record.primary
    $backup = Assert-ContinueOwnedPath $Record.backup
    if ($primary -eq $backup) { throw 'Primary and backup paths must differ.' }
    foreach ($path in @($primary,$backup)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) { throw "Missing checkpoint bytes: $path" }
    }
    [pscustomobject]@{ primaryPath=$primary; backupPath=$backup; primary=(Get-FileHash -LiteralPath $primary -Algorithm SHA256).Hash; backup=(Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash }
}
function Assert-ContinueHashes {
    param($Before,$After)
    if ($Before.primaryPath -cne $After.primaryPath -or $Before.backupPath -cne $After.backupPath -or
        $Before.primary -cne $After.primary -or $Before.backup -cne $After.backup) { throw 'Checkpoint bytes changed during failed restoration/retry.' }
}
function Read-ContinueLog {
    $ownedProcess.Refresh()
    if ($ownedProcess.HasExited) { throw "Owned game process exited unexpectedly: $($ownedProcess.ExitCode)." }
    if (-not (Test-Path -LiteralPath $log)) { return '' }
    $content = Get-Content -Raw -LiteralPath $log
    if ($content -match 'Fatal error!|Fatal error:|Assertion failed:|Unhandled Exception:') { throw 'Native crash/fatal error.' }
    $null = @(Get-ContinueRecoveryRecords $content $runId)
    return $content
}
function Wait-ContinuePhase {
    param([string]$Phase)
    do {
        $content = Read-ContinueLog
        $records = @(Get-ContinueRecoveryRecords $content $runId | Where-Object phase -eq $Phase)
        if ($records.Count -gt 1) { throw "Duplicate phase: $Phase" }
        if ($records.Count -eq 1) {
            if ([int]$records[0].pid -ne $ownedProcess.Id) { throw 'Fixture PID does not match owned process.' }
            return $records[0]
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out awaiting $Phase."
}
try {
    $saveSuffix = 'ContinueRecovery_' + ($runId -replace '-', '_')
    $arguments = @()
    if ($Runtime -eq 'Editor') { $arguments += (Join-Path $projectRoot $manifest.uproject) }
    $arguments += @('/Game/BrokenHorizon/Maps/L_FirstLight_Graybox?listen','-game','-nullrhi','-unattended','-nosound','-NoSplash','-DisablePython','-DDC-ForceMemoryCache',
        "-port=$Port","-UserDir=$userDirectory","-BHTestSaveSlotSuffix=$saveSuffix","-abslog=$log",'-BHTestContinueRecovery',"-BHTestContinueRunId=$runId")
    foreach ($argument in $arguments) { if ($argument -match '["\r\n]' -or $argument.EndsWith('\')) { throw 'Unsafe launch argument.' } }
    $summary.arguments = $arguments
    $ownedProcess = Start-Process -FilePath $executable -ArgumentList (($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ') -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $summary.processId = $ownedProcess.Id
    $seeded = Wait-ContinuePhase 'seeded'
    $summary.evidence.seeded = $seeded
    $control = Assert-ContinueOwnedPath $seeded.control_dir
    if (-not $control.EndsWith("\Automation\ContinueRecovery\$runId", [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected control directory.' }
    $summary.controlDirectory = $control
    $summary.checkpointBefore = Read-ContinueSlotHashes $seeded
    [IO.File]::WriteAllText((Join-Path $control 'fail.ready'), 'ready')
    $failedMenu = Wait-ContinuePhase 'failed_menu'
    Assert-ContinueRecord $failedMenu 7 0 1 1
    if ($failedMenu.status -notmatch 'ACTION FAILED' -or $failedMenu.status -notmatch 'checkpoint is protected') { throw 'Actual menu did not retain the restoration error.' }
    $summary.checkpointFailed = Read-ContinueSlotHashes $failedMenu
    Assert-ContinueHashes $summary.checkpointBefore $summary.checkpointFailed
    [IO.File]::WriteAllText((Join-Path $control 'retry.ready'), 'ready')
    $success = Wait-ContinuePhase 'success'
    Assert-ContinueRecord $success 5 0 0
    if ($success.net_mode -ne '2' -or $success.detail -notmatch '^health=73\.0 magazine=17 reserve=91 turn=37 objective=UnlockSecurityDoor consumed=1$') { throw 'Incomplete restored gameplay evidence.' }
    $content = Read-ContinueLog
    $expectedPhases = @('seeded','continue_requested','fault_installed','pending_writes','apply_delete','negative_before_failure','failed_writes','load_failed','failed_menu','retry_requested','negative_before_retry','load_applied','success')
    $records = @(Get-ContinueRecoveryRecords $content $runId)
    if ($records.Count -ne $expectedPhases.Count) { throw 'Unexpected or duplicate native fixture phase.' }
    for ($i=0; $i -lt $expectedPhases.Count; ++$i) {
        if ($records[$i].phase -cne $expectedPhases[$i] -or [int]$records[$i].pid -ne $ownedProcess.Id) { throw 'Native phases/PIDs do not match the required ordered lifecycle.' }
        $summary.evidence[$records[$i].phase] = $records[$i]
    }
    Assert-ContinueWriteProtection $summary.evidence.pending_writes
    Assert-ContinueWriteProtection $summary.evidence.failed_writes
    Assert-ContinueRecord $summary.evidence.apply_delete 4 1 1
    if ($summary.evidence.apply_delete.detail -cne 'delete=0 hashes_unchanged=1 mutation=1') { throw 'Missing protected DeleteSaveGame during actual Apply operation.' }
    foreach ($phase in @('negative_before_failure','negative_before_retry')) {
        Assert-ContinueRecord $summary.evidence[$phase] 4 1 1
        if ($summary.evidence[$phase].detail -cne 'turn=9 supply=11.0 consumed=0 checkpoint_turn=37 checkpoint_supply=63.0 tracking_differs=1') { throw 'Missing live negative control before actual apply.' }
    }
    Assert-ContinueRecord $summary.evidence.load_failed 4 1 1
    Assert-ContinueRecord $summary.evidence.load_applied 4 1 0
    if ($content -notmatch 'Duplicate door persistence ID' -or $content -notmatch 'BH_SESSION_CONTINUE_FAILED[^\r\n]+reason=apply_validation_failed' -or
        ([regex]::Matches($content,'BH_SESSION_CONTINUE_APPLIED ')).Count -ne 1) { throw 'Missing real validation failure or applied Session completion.' }
    $summary.checkpointAfter = Read-ContinueSlotHashes $success
    Assert-ContinueHashes $summary.checkpointBefore $summary.checkpointAfter
    $null = Read-ContinueLog
    $summary.result = 'Passed'
} catch { $summary.failure = $_.Exception.Message }
finally {
    if ($null -ne $ownedProcess) {
        try {
            $ownedProcess.Refresh()
            if (-not $ownedProcess.HasExited) { Stop-Process -Id $ownedProcess.Id; if (-not $ownedProcess.WaitForExit(10000)) { throw 'Owned process did not exit during cleanup.' } }
        } catch { $summary.cleanupError = $_.Exception.Message; $summary.result = 'Failed' }
    }
    $summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
}
Write-Output "Continue Recovery $($summary.result): $summaryPath"
if ($summary.result -ne 'Passed') { throw "Continue Recovery failed: $($summary.failure) $($summary.cleanupError)" }
