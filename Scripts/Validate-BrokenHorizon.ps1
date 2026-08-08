[CmdletBinding()]
param(
    [switch]$Build,
    [switch]$Tests,
    [switch]$Assets,
    [switch]$AudioFX,
    [switch]$Localization,
    [switch]$Performance,
    [switch]$RenderedPerformance,
    [switch]$RenderedTraversalPerformance,
    [switch]$RenderedWorldPerformance,
    [switch]$NetworkBudget,
    [switch]$NetworkScale,
    [switch]$NetworkImpairment,
    [switch]$RenderedMultiplayer,
    [switch]$RenderedMultiplayerScale,
    [switch]$RenderedMultiplayerSoak,
    [switch]$RenderedUI,
    [switch]$RenderedPseudoLocalization,
    [switch]$Smoke,
    [switch]$FirstLight,
    [switch]$Packaged,
    [string]$EngineRoot,
    [string]$LogPrefix = "BHValidation"
)

$ErrorActionPreference = "Stop"

# NOTE: Avoid rewriting PATH/Path in this validator because some host shells
# expose duplicate path keys that can interfere with Unreal build tool startup.

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $projectRoot "Config\ProjectManifest.json"
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json

if (-not $EngineRoot) {
    $EngineRoot = $manifest.engineRoot
}

$uproject = Join-Path $projectRoot $manifest.uproject
$buildScript = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
$editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$editorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$logDirectory = Join-Path $projectRoot "Saved\Logs"
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

if (-not ($Build -or $Tests -or $Assets -or $AudioFX -or $Localization -or $Performance -or $RenderedPerformance -or $RenderedTraversalPerformance -or $RenderedWorldPerformance -or $NetworkBudget -or $NetworkScale -or $NetworkImpairment -or $RenderedMultiplayer -or $RenderedMultiplayerScale -or $RenderedMultiplayerSoak -or $RenderedUI -or $RenderedPseudoLocalization -or $Smoke -or $FirstLight -or $Packaged)) {
    $Build = $true
    $Tests = $true
    $Smoke = $true
}

function Invoke-CheckedProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $FilePath)) {
        throw "$Label executable was not found: $FilePath"
    }

    Write-Host "[$Label] Starting"
    $processName = [IO.Path]::GetFileNameWithoutExtension($FilePath)
    $existingProcessIds = @(
        Get-Process -Name $processName -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty Id
    )

    $global:LASTEXITCODE = $null
    & $FilePath @Arguments
    $exitCode = $LASTEXITCODE

    # GUI-subsystem executables can return control to Windows PowerShell
    # before the process exits and without setting LASTEXITCODE. Locate only
    # the process launched by this invocation and wait for its real result.
    if ([string]::IsNullOrWhiteSpace([string]$exitCode) -and
        [IO.Path]::GetExtension($FilePath) -ieq ".exe") {
        $processDeadline = [DateTime]::UtcNow.AddSeconds(10)
        $launchedProcess = $null
        while (-not $launchedProcess -and
            [DateTime]::UtcNow -lt $processDeadline) {
            $launchedProcess = Get-Process `
                -Name $processName `
                -ErrorAction SilentlyContinue |
                Where-Object { $_.Id -notin $existingProcessIds } |
                Sort-Object StartTime -Descending |
                Select-Object -First 1
            if (-not $launchedProcess) {
                Start-Sleep -Milliseconds 100
            }
        }

        if (-not $launchedProcess) {
            throw "$Label did not expose a process or exit code."
        }

        $launchedProcess.WaitForExit()
        $launchedProcess.Refresh()
        $exitCode = $launchedProcess.ExitCode
        if ([string]::IsNullOrWhiteSpace([string]$exitCode)) {
            # Windows PowerShell can omit ExitCode for GUI subsystem
            # processes even after WaitForExit. The mandatory clean-log
            # assertion that follows remains the authoritative result.
            $exitCode = 0
        }
    }

    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode."
    }
    Write-Host "[$Label] Passed"
}

function Assert-CleanLog {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Label,
        [switch]$RequireAutomationSuccess,
        [string[]]$AdditionalFailurePatterns = @()
    )

    $logWaitDeadline = [DateTime]::UtcNow.AddSeconds(10)
    while (-not (Test-Path -LiteralPath $LogPath) -and
        [DateTime]::UtcNow -lt $logWaitDeadline) {
        Start-Sleep -Milliseconds 100
    }

    if (-not (Test-Path -LiteralPath $LogPath)) {
        throw "$Label did not produce its expected log: $LogPath"
    }

    $content = Get-Content -Raw -LiteralPath $LogPath
    $failurePatterns = @(
        "Fatal error:",
        "Assertion failed:",
        "Unhandled Exception:",
        "Automation Test Failed",
        "LogAutomationController: Error:",
        "Test Completed. Result={Fail}",
        "Failed to load package",
        "CreateExport: Failed to load"
    )

    $failures = @(
        $failurePatterns |
            Where-Object { $content -match [regex]::Escape($_) }
        $AdditionalFailurePatterns |
            Where-Object { $content -match $_ }
    )
    if ($failures.Count -gt 0) {
        throw "$Label log contains failure markers: $($failures -join ', '). See $LogPath"
    }

    if ($RequireAutomationSuccess -and
        ($content -notmatch "Test Completed\. Result=\{Success\}" -or
         $content -notmatch "Automation Test Queue Empty")) {
        throw "$Label did not complete a successful automation test queue. See $LogPath"
    }

    Write-Host "[$Label] Log passed: $LogPath"
}

if ($Build) {
    $buildLog = Join-Path $logDirectory "$LogPrefix-Build.log"
    $unrealBuildToolLog = Join-Path $env:LOCALAPPDATA "UnrealBuildTool\Log.txt"
    Remove-Item -LiteralPath $buildLog -Force -ErrorAction SilentlyContinue
    Invoke-CheckedProcess -FilePath $buildScript -Label "Build" -Arguments @(
        $manifest.editorTarget,
        $manifest.platform,
        $manifest.configuration,
        "-Project=$uproject",
        "-WaitMutex",
        "-NoHotReloadFromIDE"
    )
    if (-not (Test-Path -LiteralPath $unrealBuildToolLog)) {
        throw "Build did not produce the UnrealBuildTool log: $unrealBuildToolLog"
    }
    Copy-Item -LiteralPath $unrealBuildToolLog -Destination $buildLog -Force
    Assert-CleanLog -LogPath $buildLog -Label "Build"
}

if ($Tests) {
    $testLogName = "$LogPrefix-Tests.log"
    $testLog = Join-Path $logDirectory $testLogName
    Remove-Item -LiteralPath $testLog -Force -ErrorAction SilentlyContinue
    Invoke-CheckedProcess -FilePath $editorCmd -Label "Automation" -Arguments @(
        $uproject,
        "-nullrhi",
        "-unattended",
        "-nosound",
        "-NoSplash",
        "-DDC-ForceMemoryCache",
        '-TestExit="Automation Test Queue Empty"',
        "-ExecCmds=`"Automation RunTests $($manifest.automationTestFilter)`"",
        "-abslog=$testLog"
    )
    Assert-CleanLog -LogPath $testLog -Label "Automation" -RequireAutomationSuccess
}

if ($Assets) {
    $assetAuditScript = Join-Path $projectRoot "Content\Python\validate_asset_readiness.py"
    $assetAuditLog = Join-Path $logDirectory "$LogPrefix-Assets.log"
    $assetAuditReport = Join-Path $projectRoot "Saved\Reports\BHAssetReadiness.json"
    Remove-Item -LiteralPath $assetAuditLog -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $assetAuditReport -Force -ErrorAction SilentlyContinue
    Invoke-CheckedProcess -FilePath $editorCmd -Label "Asset readiness audit" -Arguments @(
        $uproject,
        "-run=pythonscript",
        "-script=$assetAuditScript",
        "-unattended",
        "-nop4",
        "-nosplash",
        "-nullrhi",
        "-DDC-ForceMemoryCache",
        "-abslog=$assetAuditLog"
    )
    Assert-CleanLog -LogPath $assetAuditLog -Label "Asset readiness audit" `
        -AdditionalFailurePatterns @("BH_ASSET_READINESS_ITEM_ERROR")

    $assetAuditLogContent = Get-Content -Raw -LiteralPath $assetAuditLog
    if ($assetAuditLogContent -notmatch "BH_ASSET_READINESS_AUDIT result=complete") {
        throw "Asset readiness audit did not emit its completion marker. See $assetAuditLog"
    }
    if (-not (Test-Path -LiteralPath $assetAuditReport)) {
        throw "Asset readiness audit did not produce its report: $assetAuditReport"
    }

    $assetAuditEvidence = Get-Content -Raw -LiteralPath $assetAuditReport | ConvertFrom-Json
    if (-not $assetAuditEvidence.readOnly) {
        throw "Asset readiness report did not confirm read-only execution. See $assetAuditReport"
    }
    if ([int]$assetAuditEvidence.summary.errors -ne 0) {
        throw "Asset readiness report contains $($assetAuditEvidence.summary.errors) audit errors. See $assetAuditReport"
    }

    Write-Host "[Asset readiness audit] Evidence passed: $assetAuditReport"
    Write-Host ("[Asset readiness audit] Review candidates: placeholders={0}, shipping-referenced={1}, unknown-static-LODs={2}" -f `
        $assetAuditEvidence.summary.placeholderCandidates,
        $assetAuditEvidence.summary.shippingReferencedPlaceholders,
        $assetAuditEvidence.summary.unknownStaticMeshLods)
}

if ($AudioFX) {
    $audioAuditScript = Join-Path $projectRoot "Content\Python\validate_audio_fx_readiness.py"
    $audioAuditLog = Join-Path $logDirectory "$LogPrefix-AudioFX.log"
    $audioAuditReport = Join-Path $projectRoot "Saved\Reports\BHAudioFXReadiness.json"
    Remove-Item -LiteralPath $audioAuditLog -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $audioAuditReport -Force -ErrorAction SilentlyContinue
    Invoke-CheckedProcess -FilePath $editorCmd -Label "Audio/FX readiness audit" -Arguments @(
        $uproject,
        "-run=pythonscript",
        "-script=$audioAuditScript",
        "-unattended",
        "-nop4",
        "-nosplash",
        "-nullrhi",
        "-DDC-ForceMemoryCache",
        "-abslog=$audioAuditLog"
    )
    Assert-CleanLog -LogPath $audioAuditLog -Label "Audio/FX readiness audit"

    $audioAuditLogContent = Get-Content -Raw -LiteralPath $audioAuditLog
    if ($audioAuditLogContent -notmatch "\[BH Audio FX Readiness\] ALL CHECKS PASSED") {
        throw "Audio/FX audit did not emit its completion marker. See $audioAuditLog"
    }
    if (-not (Test-Path -LiteralPath $audioAuditReport)) {
        throw "Audio/FX audit did not produce its report: $audioAuditReport"
    }
    $audioAuditEvidence = Get-Content -Raw -LiteralPath $audioAuditReport | ConvertFrom-Json
    if (-not $audioAuditEvidence.readOnly -or [int]$audioAuditEvidence.summary.errors -ne 0) {
        throw "Audio/FX readiness evidence is invalid. See $audioAuditReport"
    }
    Write-Host ("[Audio/FX readiness audit] Assignments: required={0}/{1}, optional={2}/{3}" -f `
        $audioAuditEvidence.summary.requiredAssigned,
        $audioAuditEvidence.summary.requiredAssignments,
        $audioAuditEvidence.summary.optionalAssigned,
        $audioAuditEvidence.summary.optionalAssignments)
}

if ($Localization) {
    $localizationConfig = Join-Path $projectRoot "Config\Localization\Game.ini"
    $localizationLog = Join-Path $logDirectory "$LogPrefix-Localization.log"
    $localizationRoot = Join-Path $projectRoot "Content\Localization\Game"
    $manifestOutput = Join-Path $localizationRoot "Game.manifest"
    $archiveOutput = Join-Path $localizationRoot "en\Game.archive"
    $poOutput = Join-Path $localizationRoot "en\Game.po"
    $resourceOutput = Join-Path $localizationRoot "en\Game.locres"
    $reportDirectory = Join-Path $projectRoot "Saved\LocalizationReports"
    $wordCountReport = Join-Path $reportDirectory "Game.csv"
    $conflictReport = Join-Path $reportDirectory "GameConflicts.txt"

    Remove-Item -LiteralPath $localizationLog -Force -ErrorAction SilentlyContinue
    Invoke-CheckedProcess -FilePath $editorCmd -Label "Localization gather" -Arguments @(
        $uproject,
        "-run=GatherText",
        "-config=$localizationConfig",
        "-unattended",
        "-nop4",
        "-NoSplash",
        "-abslog=$localizationLog"
    )
    Assert-CleanLog `
        -LogPath $localizationLog `
        -Label "Localization gather" `
        -AdditionalFailurePatterns @(
            "Text conflict from NSLOCTEXT macro",
            "GatherText completed with exit code 1"
        )

    foreach ($requiredOutput in @(
        $manifestOutput,
        $archiveOutput,
        $poOutput,
        $resourceOutput,
        $wordCountReport
    )) {
        if (-not (Test-Path -LiteralPath $requiredOutput) -or
            (Get-Item -LiteralPath $requiredOutput).Length -eq 0) {
            throw "Localization gather did not produce a non-empty output: $requiredOutput"
        }
    }

    if (-not (Test-Path -LiteralPath $conflictReport)) {
        throw "Localization gather did not produce its conflict report: $conflictReport"
    }
    [string]$conflicts = Get-Content -Raw -LiteralPath $conflictReport
    if (-not [string]::IsNullOrWhiteSpace($conflicts)) {
        throw "Localization gather produced conflicting namespace/key entries. See $conflictReport"
    }

    $wordCountRows = @(Import-Csv -LiteralPath $wordCountReport)
    if ($wordCountRows.Count -eq 0 -or
        [int]$wordCountRows[-1].'Word Count' -lt 100) {
        throw "Localization gather produced an unexpectedly small player-facing catalog. See $wordCountReport"
    }
    Write-Host "[Localization gather] Catalog words: $($wordCountRows[-1].'Word Count'); conflicts: 0"
}

if ($Performance -or $RenderedPerformance -or $RenderedTraversalPerformance -or $RenderedWorldPerformance) {
    $performanceScript = Join-Path $projectRoot "Scripts\Test-BrokenHorizonPerformance.ps1"
    $performanceLabel = if ($RenderedWorldPerformance) {
        "RenderedWorldPerformance"
    } elseif ($RenderedTraversalPerformance) {
        "RenderedTraversalPerformance"
    } elseif ($RenderedPerformance) {
        "RenderedPerformance"
    } else {
        "Performance"
    }
    $performancePrefix = "$LogPrefix-$performanceLabel"
    $performanceReport = Join-Path $projectRoot "Saved\Reports\$performancePrefix-Summary.json"
    if ($RenderedWorldPerformance) {
        & $performanceScript `
            -EngineRoot $EngineRoot `
            -LogPrefix $performancePrefix `
            -WorldTraversal
    } elseif ($RenderedTraversalPerformance) {
        & $performanceScript `
            -EngineRoot $EngineRoot `
            -LogPrefix $performancePrefix `
            -Rendered `
            -Traversal
    } elseif ($RenderedPerformance) {
        & $performanceScript `
            -EngineRoot $EngineRoot `
            -LogPrefix $performancePrefix `
            -Rendered
    } else {
        & $performanceScript `
            -EngineRoot $EngineRoot `
            -LogPrefix $performancePrefix
    }
    if (-not (Test-Path -LiteralPath $performanceReport)) {
        throw "Performance validation did not produce its evidence report: $performanceReport"
    }
    $performanceEvidence = Get-Content -Raw -LiteralPath $performanceReport | ConvertFrom-Json
    if (-not $performanceEvidence.passed) {
        throw "Performance evidence reports a failed budget. See $performanceReport"
    }
    $expectsRendererProof =
        $RenderedPerformance -or $RenderedTraversalPerformance -or $RenderedWorldPerformance
    if ([bool]$performanceEvidence.rendererProof -ne
        [bool]$expectsRendererProof -or
        $performanceEvidence.networkProof) {
        throw "Performance evidence contains incorrect renderer/network proof flags. See $performanceReport"
    }
    if ($expectsRendererProof -and
        ($performanceEvidence.hardware.rhi -ne "D3D12" -or
         $performanceEvidence.hardware.resolution -ne "1920x1080" -or
         [string]::IsNullOrWhiteSpace($performanceEvidence.hardware.gpu))) {
        throw "Rendered performance evidence is missing required hardware metadata. See $performanceReport"
    }
    if ([bool]$performanceEvidence.traversalProof -ne
        [bool]($RenderedTraversalPerformance -or $RenderedWorldPerformance) -or
        (($RenderedTraversalPerformance -or $RenderedWorldPerformance) -and
         [int]$performanceEvidence.traversalSteps -ne 8)) {
        throw "Performance evidence contains incorrect traversal proof. See $performanceReport"
    }
    if ([bool]$performanceEvidence.worldTraversalProof -ne
        [bool]$RenderedWorldPerformance -or
        ($RenderedWorldPerformance -and
         $performanceEvidence.map -ne "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World")) {
        throw "Performance evidence contains incorrect full-world proof. See $performanceReport"
    }
    if ([bool]$performanceEvidence.navigationProof -ne
        [bool]$RenderedWorldPerformance -or
        ($RenderedWorldPerformance -and
         [int]$performanceEvidence.navigationSteps -ne 8)) {
        throw "Performance evidence contains incorrect navigation proof. See $performanceReport"
    }
    Write-Host ("[Performance] Evidence passed: frame-p95={0}ms, frame-p99={1}ms, memory-max={2}MB" -f `
        $performanceEvidence.metrics.frameP95Ms,
        $performanceEvidence.metrics.frameP99Ms,
        $performanceEvidence.metrics.physicalMemoryMaxMB)
}

if ($NetworkBudget -or $NetworkScale -or $NetworkImpairment) {
    $networkBudgetScript = Join-Path $projectRoot "Scripts\Test-BrokenHorizonMultiplayer.ps1"
    $networkStress = $NetworkScale -or $NetworkImpairment
    $networkBudgetClientCount = if ($networkStress) { 4 } else { 2 }
    $networkBudgetLabel = if ($NetworkImpairment) {
        "NetworkImpairment"
    } elseif ($NetworkScale) {
        "NetworkScale"
    } else {
        "NetworkBudget"
    }
    $networkBudgetPrefix = "$LogPrefix-$networkBudgetLabel"
    $networkBudgetStart = [DateTime]::UtcNow
    if ($networkStress) {
        $networkStressArguments = @{
            RequireNetworkBudget = $true
            NetworkBudgetClientCount = $networkBudgetClientCount
            RequireActiveOperation = $true
            RequireSquadPingReplication = $true
            RequireNetworkCombatDensity = $true
            ConnectionTimeoutSeconds = 120
            LogPrefix = $networkBudgetPrefix
        }
        if ($NetworkImpairment) {
            $networkStressArguments.PacketLagMilliseconds = 80
            $networkStressArguments.PacketLossPercent = 3
            $networkStressArguments.AllowedObservedPacketLossPerSample = 20
        }
        & $networkBudgetScript @networkStressArguments
    } else {
        & $networkBudgetScript `
            -RequireNetworkBudget `
            -NetworkBudgetClientCount $networkBudgetClientCount `
            -ConnectionTimeoutSeconds 120 `
            -LogPrefix $networkBudgetPrefix
    }
    $networkBudgetSummary = Get-ChildItem `
        -LiteralPath $logDirectory `
        -Filter "$networkBudgetPrefix-*-Summary.json" `
        -File |
        Where-Object { $_.LastWriteTimeUtc -ge $networkBudgetStart.AddSeconds(-2) } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if (-not $networkBudgetSummary) {
        throw "Network budget validation did not produce a current summary."
    }
    $networkBudgetEvidence = Get-Content `
        -Raw `
        -LiteralPath $networkBudgetSummary.FullName |
        ConvertFrom-Json
    if (-not $networkBudgetEvidence.networkBudgetRequired -or
        -not $networkBudgetEvidence.networkBudgetVerified) {
        throw "Network budget evidence did not verify the required gate. See $($networkBudgetSummary.FullName)"
    }
    if ([int]$networkBudgetEvidence.networkBudget.metrics.connections -ne
        $networkBudgetClientCount) {
        throw "Network budget evidence measured the wrong client count. See $($networkBudgetSummary.FullName)"
    }
    if ($networkStress -and
        (-not $networkBudgetEvidence.additionalScaleClientsSnapshotApplied -or
         -not $networkBudgetEvidence.additionalScaleClientsActiveOperationVerified -or
         -not $networkBudgetEvidence.squadPingReplicationVerified -or
         -not $networkBudgetEvidence.networkCombatDensityVerified)) {
        throw "Network scale clients did not all prove snapshot, active-operation, and squad-ping replication. See $($networkBudgetSummary.FullName)"
    }
    if ($NetworkImpairment -and
        ([int]$networkBudgetEvidence.packetLagMilliseconds -ne 80 -or
         [int]$networkBudgetEvidence.packetLossPercent -ne 3)) {
        throw "Network impairment evidence did not retain the required 80ms/3% simulation profile. See $($networkBudgetSummary.FullName)"
    }
    if ($networkBudgetEvidence.networkBudget.rendererProof -or
        $networkBudgetEvidence.networkBudget.representativeInternetProof) {
        throw "Network budget evidence incorrectly claims rendered or representative-Internet proof. See $($networkBudgetSummary.FullName)"
    }
    Write-Host ("[Network budget] Evidence passed: aggregate-out-p95={0}B/s, per-client-out-max={1}B/s, channels={2}" -f `
        $networkBudgetEvidence.networkBudget.metrics.aggregateOutP95BytesPerSecond,
        $networkBudgetEvidence.networkBudget.metrics.perConnectionOutMaxBytesPerSecond,
        $networkBudgetEvidence.networkBudget.metrics.perConnectionChannelMax)
    # Give the OS and UE shared services a bounded teardown barrier before a
    # subsequent editor launch in the same validation command.
    Start-Sleep -Seconds 2
}

if ($RenderedMultiplayer -or
    $RenderedMultiplayerScale -or
    $RenderedMultiplayerSoak)
{
    $renderedMultiplayerScript = Join-Path `
        $projectRoot `
        "Scripts\Test-BrokenHorizonRenderedMultiplayer.ps1"
    $renderedMultiplayerClientCount = if (
        $RenderedMultiplayerScale
    ) { 4 } else { 2 }
    $renderedClientCount = if ($RenderedMultiplayerScale) { 1 } else { 2 }
    $renderedMultiplayerLabel = if ($RenderedMultiplayerSoak) {
        "RenderedMultiplayerSoak"
    } elseif ($RenderedMultiplayerScale) {
        "RenderedMultiplayerScale"
    } else {
        "RenderedMultiplayer"
    }
    $renderedMultiplayerPrefix =
        "$LogPrefix-$renderedMultiplayerLabel"
    $renderedMultiplayerStart = [DateTime]::UtcNow
    $renderedMultiplayerArguments = @{
        TimeoutSeconds = if ($RenderedMultiplayerSoak) { 900 } else { 120 }
        ClientCount = $renderedMultiplayerClientCount
        RenderedClientCount = $renderedClientCount
        LogPrefix = $renderedMultiplayerPrefix
    }
    if ($RenderedMultiplayerSoak) {
        # CSV FrameTime excludes the t.MaxFPS sleep on this editor build.
        # Capture enough real rendered work to exceed ten measured minutes
        # instead of treating 36,000 capped frames as ten-minute evidence.
        $renderedMultiplayerArguments.CaptureFrames = 62500
        $renderedMultiplayerArguments.WarmupFrames = 600
        $renderedMultiplayerArguments.SustainedSoak = $true
    }
    & $renderedMultiplayerScript @renderedMultiplayerArguments
    $renderedMultiplayerSummary = Get-ChildItem `
        -LiteralPath (Join-Path $projectRoot "Saved\Reports") `
        -Filter "$renderedMultiplayerPrefix-*-Summary.json" `
        -File |
        Where-Object {
            $_.LastWriteTimeUtc -ge
                $renderedMultiplayerStart.AddSeconds(-2)
        } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if (-not $renderedMultiplayerSummary) {
        throw "Rendered multiplayer validation did not produce a current summary."
    }
    $renderedMultiplayerEvidence = Get-Content `
        -Raw `
        -LiteralPath $renderedMultiplayerSummary.FullName |
        ConvertFrom-Json
    if ($renderedMultiplayerEvidence.result -ne "Passed" -or
        -not $renderedMultiplayerEvidence.rendererProof -or
        -not $renderedMultiplayerEvidence.networkProof -or
        ($RenderedMultiplayerSoak -and
            (-not $renderedMultiplayerEvidence.sustainedSoakProof -or
             [int]$renderedMultiplayerEvidence.minimumMeasuredSoakFrames -lt
                36000 -or
             [double]$renderedMultiplayerEvidence.minimumMeasuredSoakSeconds -lt
                570.0)) -or
        -not $renderedMultiplayerEvidence.dedicatedAuthority -or
        [int]$renderedMultiplayerEvidence.connectedPlayers -ne
            $renderedMultiplayerClientCount -or
        [int]$renderedMultiplayerEvidence.renderedPlayers -ne
            $renderedClientCount -or
        [int]$renderedMultiplayerEvidence.combatDensity.totalObservedAI -ne 19 -or
        @(
            $renderedMultiplayerEvidence.clients |
                Where-Object { -not $_.passed }
        ).Count -gt 0) {
        throw "Rendered multiplayer evidence did not prove the required $renderedMultiplayerClientCount-client gate. See $($renderedMultiplayerSummary.FullName)"
    }
    $renderedMetricSummary = @(
        $renderedMultiplayerEvidence.clients | ForEach-Object {
            "{0} frame/GPU p95={1}/{2}ms" -f `
                $_.label,
                $_.metrics.frameP95Ms,
                $_.metrics.gpuP95Ms
        }
    ) -join "; "
    Write-Host (
        "[Rendered multiplayer] Evidence passed: " +
        "$renderedMultiplayerClientCount connected; " +
        $renderedMetricSummary
    )
    Start-Sleep -Seconds 2
}

if ($RenderedUI -or $RenderedPseudoLocalization) {
    $renderedUIScript = Join-Path `
        $projectRoot `
        "Scripts\Test-BrokenHorizonRenderedUI.ps1"
    $renderedUIPrefix = if ($RenderedPseudoLocalization) {
        "$LogPrefix-RenderedPseudoLocalization"
    } else {
        "$LogPrefix-RenderedUI"
    }
    $renderedUIStart = [DateTime]::UtcNow
    $renderedUIArguments = @{
        EngineRoot = $EngineRoot
        LogPrefix = $renderedUIPrefix
    }
    if ($RenderedPseudoLocalization) {
        $renderedUIArguments.PseudoLocalization = $true
    }
    & $renderedUIScript @renderedUIArguments
    $renderedUISummary = Get-ChildItem `
        -LiteralPath (Join-Path $projectRoot "Saved\Reports") `
        -Filter "$renderedUIPrefix-*-Summary.json" `
        -File |
        Where-Object {
            $_.LastWriteTimeUtc -ge $renderedUIStart.AddSeconds(-2)
        } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if (-not $renderedUISummary) {
        throw "Rendered UI validation did not produce a current summary."
    }
    $renderedUIEvidence = Get-Content `
        -Raw `
        -LiteralPath $renderedUISummary.FullName |
        ConvertFrom-Json
    if ($renderedUIEvidence.result -ne "Passed" -or
        -not $renderedUIEvidence.rendererProof -or
        -not $renderedUIEvidence.staticLayoutProof -or
        $renderedUIEvidence.interactiveInputProof -or
        $renderedUIEvidence.multiplayerProof -or
        [bool]$renderedUIEvidence.pseudoLocalizationProof -ne
            [bool]$RenderedPseudoLocalization -or
        @($renderedUIEvidence.captures).Count -ne
            $(if ($RenderedPseudoLocalization) { 14 } else { 33 }) -or
        @(
            $renderedUIEvidence.captures |
                Where-Object { -not $_.passed }
        ).Count -gt 0) {
        throw "Rendered UI evidence did not prove the required capture gate. See $($renderedUISummary.FullName)"
    }
    if ($RenderedPseudoLocalization) {
        Write-Host "[Rendered pseudo-localization] Evidence passed: seven high-risk UI states at 720p/1080p with LEET expansion"
    } else {
        Write-Host (
            "[Rendered UI] Evidence passed: " +
            "seven gameplay/strategic UI states at 720p/1080p/4K, HUD/safe-area " +
            "extremes, and four session states at 720p/4K"
        )
    }
}

if ($Smoke) {
    $smokeLogName = "$LogPrefix-Smoke.log"
    $smokeLog = Join-Path $logDirectory $smokeLogName
    Remove-Item -LiteralPath $smokeLog -Force -ErrorAction SilentlyContinue
    Invoke-CheckedProcess -FilePath $editor -Label "Startup smoke" -Arguments @(
        $uproject,
        "-game",
        "-nullrhi",
        "-unattended",
        "-nosound",
        "-NoSplash",
        "-DDC-ForceMemoryCache",
        "-ExecCmds=quit",
        "-abslog=$smokeLog"
    )
    Assert-CleanLog -LogPath $smokeLog -Label "Startup smoke"
}

if ($FirstLight) {
    $firstLightLogName = "$LogPrefix-FirstLight.log"
    $firstLightLog = Join-Path $logDirectory $firstLightLogName
    Remove-Item -LiteralPath $firstLightLog -Force -ErrorAction SilentlyContinue
    Invoke-CheckedProcess -FilePath $editor -Label "First Light smoke" -Arguments @(
        $uproject,
        $manifest.maps.firstLight,
        "-game",
        "-nullrhi",
        "-unattended",
        "-nosound",
        "-NoSplash",
        "-DDC-ForceMemoryCache",
        "-BHTestNavigationGrenade",
        "-BHTestFirstLightPlayableRoute",
        "-abslog=$firstLightLog"
    )
    Assert-CleanLog `
        -LogPath $firstLightLog `
        -Label "First Light smoke" `
        -AdditionalFailurePatterns @(
            "tile limit reached",
            "Recreating dtNavMesh instance .* due mismatch",
            "Navmesh .* will be loaded empty because it has been built with different navmesh settings",
            "BH_TEST_NAVIGATION_GRENADE result=failure",
            "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE result=failure",
            "Supply .*BHAmmoSupply_.* has no persistence ID"
        )

    $firstLightContent = Get-Content -Raw -LiteralPath $firstLightLog
    if ($firstLightContent -notmatch
            "BH_TEST_NAVIGATION_GRENADE result=success" -or
        $firstLightContent -notmatch
            "BH_TEST_NAVIGATION_GRENADE_COMPLETE" -or
        $firstLightContent -notmatch "BH_FRAG_EXPLODED" -or
        $firstLightContent -notmatch
            "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=keycard result=success" -or
        $firstLightContent -notmatch
            "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=door result=success" -or
        $firstLightContent -notmatch
            "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=guards result=success" -or
        $firstLightContent -notmatch
            "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE step=ammo_drop result=success" -or
        $firstLightContent -notmatch
            "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE_COMPLETE result=success objectives=4") {
        throw "First Light smoke did not complete its navigation and playable-route fixtures. See $firstLightLog"
    }

    $navigationFallbackCount = @(
        Select-String `
            -LiteralPath $firstLightLog `
            -Pattern "BH_AI_NAVIGATION_FALLBACK"
    ).Count
    $navigationFallbackLimit = 12
    if ($navigationFallbackCount -gt $navigationFallbackLimit) {
        throw "First Light smoke observed $navigationFallbackCount AI navigation fallbacks (limit $navigationFallbackLimit). See $firstLightLog"
    }
    Write-Host "[First Light smoke] Navigation fallbacks: $navigationFallbackCount/$navigationFallbackLimit"
}

if ($Packaged) {
    $packagedExecutable = Join-Path $projectRoot $manifest.packagedBuild.executable
    $packagedLog = Join-Path $logDirectory "$LogPrefix-Packaged.log"
    Remove-Item -LiteralPath $packagedLog -Force -ErrorAction SilentlyContinue
    Invoke-CheckedProcess -FilePath $packagedExecutable -Label "Packaged smoke" -Arguments @(
        $manifest.packagedBuild.map,
        "-nullrhi",
        "-unattended",
        "-nosound",
        "-NoSplash",
        "-ExecCmds=quit",
        "-abslog=$packagedLog"
    )
    Assert-CleanLog `
        -LogPath $packagedLog `
        -Label "Packaged smoke" `
        -AdditionalFailurePatterns @(
            "will be loaded empty because it has been built with different navmesh settings",
            "SkipPackage: /Game/BrokenHorizon/",
            "Failed to find object 'Object /Game/BrokenHorizon/"
        )
}

Write-Host "Broken Horizon validation completed successfully."
