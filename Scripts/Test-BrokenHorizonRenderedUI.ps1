[CmdletBinding()]
param(
    [string]$EngineRoot,
    [ValidateRange(30, 180)]
    [int]$TimeoutSeconds = 90,
    [string]$LogPrefix = "BHRenderedUI",
    [switch]$PseudoLocalization,
    [string]$CaseFilter
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath `
    (Join-Path $projectRoot "Config\ProjectManifest.json") |
    ConvertFrom-Json
if (-not $EngineRoot) {
    $EngineRoot = $manifest.engineRoot
}

$editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$uproject = Join-Path $projectRoot $manifest.uproject
$firstLightMap = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
$mainMenuMap = "/Game/BrokenHorizon/Maps/L_MainMenu"
$logDirectory = Join-Path $projectRoot "Saved\Logs"
$reportDirectory = Join-Path $projectRoot "Saved\Reports"
$runId = Get-Date -Format "yyyyMMdd-HHmmss"
$summaryPath = Join-Path `
    $reportDirectory `
    "$LogPrefix-$runId-Summary.json"

New-Item -ItemType Directory -Force `
    -Path $logDirectory, $reportDirectory | Out-Null

if (-not (Test-Path -LiteralPath $editor)) {
    throw "Unreal Editor was not found: $editor"
}

function Get-PngDimensions {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 24 -or
        $bytes[0] -ne 137 -or
        $bytes[1] -ne 80 -or
        $bytes[2] -ne 78 -or
        $bytes[3] -ne 71) {
        throw "Rendered UI capture is not a valid PNG: $Path"
    }

    return [pscustomobject]@{
        width = [Net.IPAddress]::NetworkToHostOrder(
            [BitConverter]::ToInt32($bytes, 16)
        )
        height = [Net.IPAddress]::NetworkToHostOrder(
            [BitConverter]::ToInt32($bytes, 20)
        )
    }
}

function Stop-RenderedUIProcessTree {
    param([Parameter(Mandatory = $true)][int]$ProcessId)

    $children = @(Get-CimInstance Win32_Process `
        -Filter "ParentProcessId = $ProcessId" `
        -ErrorAction SilentlyContinue)
    foreach ($child in $children) {
        Stop-RenderedUIProcessTree -ProcessId ([int]$child.ProcessId)
    }

    Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
}

$captureCases = @(
    [pscustomobject]@{ mode = "HUD"; width = 1280; height = 720 },
    [pscustomobject]@{ mode = "HUD_INTERACTION_PROMPT"; width = 1920; height = 1080 },
    [pscustomobject]@{ mode = "BRIEFING"; width = 1280; height = 720 },
    [pscustomobject]@{ mode = "PAUSE"; width = 1280; height = 720 },
    [pscustomobject]@{ mode = "SETTINGS"; width = 1280; height = 720 },
    [pscustomobject]@{ mode = "REMAPPING"; width = 1280; height = 720 },
    [pscustomobject]@{ mode = "WAR_MAP"; width = 1280; height = 720 },
    [pscustomobject]@{ mode = "CUSTOM_DIFFICULTY"; width = 1280; height = 720 },
    [pscustomobject]@{ mode = "HUD"; width = 1920; height = 1080 },
    [pscustomobject]@{ mode = "BRIEFING"; width = 1920; height = 1080 },
    [pscustomobject]@{ mode = "PAUSE"; width = 1920; height = 1080 },
    [pscustomobject]@{ mode = "SETTINGS"; width = 1920; height = 1080 },
    [pscustomobject]@{ mode = "REMAPPING"; width = 1920; height = 1080 },
    [pscustomobject]@{ mode = "WAR_MAP"; width = 1920; height = 1080 },
    [pscustomobject]@{ mode = "CUSTOM_DIFFICULTY"; width = 1920; height = 1080 },
    [pscustomobject]@{ mode = "HUD"; width = 3840; height = 2160 },
    [pscustomobject]@{ mode = "BRIEFING"; width = 3840; height = 2160 },
    [pscustomobject]@{ mode = "PAUSE"; width = 3840; height = 2160 },
    [pscustomobject]@{ mode = "SETTINGS"; width = 3840; height = 2160 },
    [pscustomobject]@{ mode = "REMAPPING"; width = 3840; height = 2160 },
    [pscustomobject]@{ mode = "WAR_MAP"; width = 3840; height = 2160 },
    [pscustomobject]@{ mode = "CUSTOM_DIFFICULTY"; width = 3840; height = 2160 },
    [pscustomobject]@{ mode = "HUD"; width = 1280; height = 720; profile = "HUD75-SAFE80"; hudScale = 0.75; safeAreaScale = 0.8 },
    [pscustomobject]@{ mode = "HUD"; width = 1280; height = 720; profile = "HUD150-SAFE100"; hudScale = 1.5; safeAreaScale = 1.0 },
    [pscustomobject]@{ mode = "HUD"; width = 3840; height = 2160; profile = "HUD75-SAFE80"; hudScale = 0.75; safeAreaScale = 0.8 },
    [pscustomobject]@{ mode = "HUD"; width = 3840; height = 2160; profile = "HUD150-SAFE100"; hudScale = 1.5; safeAreaScale = 1.0 },
    [pscustomobject]@{ mode = "SESSION_READY"; width = 1280; height = 720; sessionReview = "READY" },
    [pscustomobject]@{ mode = "SESSION_SEARCHING"; width = 1280; height = 720; sessionReview = "SEARCHING" },
    [pscustomobject]@{ mode = "SESSION_CONNECTED"; width = 1280; height = 720; sessionReview = "CONNECTED" },
    [pscustomobject]@{ mode = "SESSION_ERROR"; width = 1280; height = 720; sessionReview = "ERROR" },
    [pscustomobject]@{ mode = "SESSION_READY"; width = 3840; height = 2160; sessionReview = "READY" },
    [pscustomobject]@{ mode = "SESSION_SEARCHING"; width = 3840; height = 2160; sessionReview = "SEARCHING" },
    [pscustomobject]@{ mode = "SESSION_CONNECTED"; width = 3840; height = 2160; sessionReview = "CONNECTED" },
    [pscustomobject]@{ mode = "SESSION_ERROR"; width = 3840; height = 2160; sessionReview = "ERROR" }
)
if ($PseudoLocalization) {
    $captureCases = @(
        [pscustomobject]@{ mode = "SETTINGS"; width = 1280; height = 720; profile = "LEET" },
        [pscustomobject]@{ mode = "REMAPPING"; width = 1280; height = 720; profile = "LEET" },
        [pscustomobject]@{ mode = "WAR_MAP"; width = 1280; height = 720; profile = "LEET" },
        [pscustomobject]@{ mode = "WAR_MAP_DEPLOYMENT"; width = 1280; height = 720; profile = "LEET" },
        [pscustomobject]@{ mode = "CUSTOM_DIFFICULTY"; width = 1280; height = 720; profile = "LEET" },
        [pscustomobject]@{ mode = "SESSION_READY"; width = 1280; height = 720; profile = "LEET"; sessionReview = "READY" },
        [pscustomobject]@{ mode = "SESSION_ERROR"; width = 1280; height = 720; profile = "LEET"; sessionReview = "ERROR" },
        [pscustomobject]@{ mode = "SETTINGS"; width = 1920; height = 1080; profile = "LEET" },
        [pscustomobject]@{ mode = "REMAPPING"; width = 1920; height = 1080; profile = "LEET" },
        [pscustomobject]@{ mode = "WAR_MAP"; width = 1920; height = 1080; profile = "LEET" },
        [pscustomobject]@{ mode = "WAR_MAP_DEPLOYMENT"; width = 1920; height = 1080; profile = "LEET" },
        [pscustomobject]@{ mode = "CUSTOM_DIFFICULTY"; width = 1920; height = 1080; profile = "LEET" },
        [pscustomobject]@{ mode = "SESSION_READY"; width = 1920; height = 1080; profile = "LEET"; sessionReview = "READY" },
        [pscustomobject]@{ mode = "SESSION_ERROR"; width = 1920; height = 1080; profile = "LEET"; sessionReview = "ERROR" }
    )
}
if (-not [string]::IsNullOrWhiteSpace($CaseFilter)) {
    $captureCases = @(
        $captureCases | Where-Object {
            $caseName = "$($_.mode)-$($_.width)x$($_.height)"
            $caseName -match $CaseFilter
        }
    )
    if ($captureCases.Count -eq 0) {
        throw "CaseFilter '$CaseFilter' matched no rendered UI cases."
    }
    Write-Host "[Rendered UI] Case filter '$CaseFilter' selected $($captureCases.Count) case(s)."
}
$results = [System.Collections.Generic.List[object]]::new()
$failures = [System.Collections.Generic.List[object]]::new()

foreach ($captureCase in $captureCases) {
    $resolution = "$($captureCase.width)x$($captureCase.height)"
    $profile = if (
        $captureCase.PSObject.Properties.Name -contains "profile"
    ) { $captureCase.profile } else { "DEFAULT" }
    $sourcePath = Join-Path `
        $reportDirectory `
        "$LogPrefix-$runId-Source-$($captureCase.mode)-$resolution-$profile.png"
    $capturePath = Join-Path `
        $reportDirectory `
        "$LogPrefix-$runId-$($captureCase.mode)-$resolution-$profile.png"
    $logPath = Join-Path `
        $logDirectory `
        "$LogPrefix-$runId-$($captureCase.mode)-$resolution-$profile.log"

    Remove-Item -LiteralPath $sourcePath, $capturePath, $logPath `
        -Force -ErrorAction SilentlyContinue

    Write-Host "[Rendered UI $($captureCase.mode) $resolution $profile] Starting"
    $stage = "launch"
    $process = $null
    try {
    $isSessionReview =
        $captureCase.PSObject.Properties.Name -contains "sessionReview"
    $caseMap = if ($isSessionReview) { $mainMenuMap } else { $firstLightMap }
    $arguments = @(
        $uproject,
        $caseMap,
        "-game",
        "-RenderOffscreen",
        "-windowed",
        "-ForceRes",
        "-ResX=$($captureCase.width)",
        "-ResY=$($captureCase.height)",
        "-unattended",
        "-nosound",
        "-NoSplash",
        "-DDC-ForceMemoryCache",
        "-abslog=$logPath"
    )
    if ($isSessionReview) {
        $arguments +=
            "-BHTestRenderedSessionReview=$($captureCase.sessionReview)"
        $arguments += "-BHTestRenderedSessionScreenshotPath=$sourcePath"
    } else {
        $arguments += "-BHTestRenderedUIReview=$($captureCase.mode)"
        $arguments += "-BHTestRenderedUIScreenshotPath=$sourcePath"
    }
    if ($PseudoLocalization) {
        $arguments += "-culture=leet"
    }
    if ($captureCase.PSObject.Properties.Name -contains "hudScale") {
        $arguments += "-BHTestHUDScale=$($captureCase.hudScale)"
        $arguments +=
            "-BHTestUISafeAreaScale=$($captureCase.safeAreaScale)"
    }
    $process = Start-Process `
        -FilePath $editor `
        -ArgumentList $arguments `
        -WindowStyle Hidden `
        -PassThru

    $stage = "process"
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-RenderedUIProcessTree -ProcessId $process.Id
        throw "Rendered UI $($captureCase.mode) $resolution timed out."
    }
    if ($process.ExitCode -ne 0) {
        throw "Rendered UI $($captureCase.mode) $resolution exited $($process.ExitCode). See $logPath"
    }
    $stage = "log"
    if (-not (Test-Path -LiteralPath $logPath)) {
        throw "Rendered UI $($captureCase.mode) $resolution did not produce $logPath"
    }

    $logContent = Get-Content -Raw -LiteralPath $logPath
    $failurePattern =
        "Fatal error:|Assertion failed:|Unhandled Exception:|" +
        "Failed to load package|CreateExport: Failed to load"
    if ($logContent -match $failurePattern) {
        throw "Rendered UI $($captureCase.mode) $resolution encountered a failure marker. See $logPath"
    }
    $requestMarker = if ($isSessionReview) {
        "BH_RENDERED_SESSION_REVIEW result=requested mode=$($captureCase.mode)"
    } else {
        "BH_RENDERED_UI_REVIEW result=requested mode=$($captureCase.mode)"
    }
    if ($logContent -notmatch [regex]::Escape($requestMarker)) {
        throw "Rendered UI $($captureCase.mode) $resolution did not execute its fixture. See $logPath"
    }
    if ($PseudoLocalization -and
        $logContent -notmatch "Overriding language with command-line option \(leet\)") {
        throw "Rendered UI $($captureCase.mode) did not activate LEET pseudo-culture. See $logPath"
    }
    $sizeMarker = "taken with size: $($captureCase.width) x $($captureCase.height)"
    if ($logContent -notmatch [regex]::Escape($sizeMarker)) {
        throw "Rendered UI $($captureCase.mode) did not render at $resolution. See $logPath"
    }

    $stage = "capture"
    $dimensions = $null
    $captureDeadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $captureDeadline) {
        if (Test-Path -LiteralPath $sourcePath) {
            try {
                $candidateDimensions = Get-PngDimensions -Path $sourcePath
                if ($candidateDimensions.width -eq $captureCase.width -and
                    $candidateDimensions.height -eq $captureCase.height) {
                    $dimensions = $candidateDimensions
                    break
                }
            } catch {
                # The renderer may still be completing the asynchronous PNG write.
            }
        }
        Start-Sleep -Milliseconds 250
    }
    if ($null -eq $dimensions) {
        if (Test-Path -LiteralPath $sourcePath) {
            $actualDimensions = Get-PngDimensions -Path $sourcePath
            throw "Rendered UI $($captureCase.mode) PNG was $($actualDimensions.width)x$($actualDimensions.height), expected $resolution."
        }
        throw "Rendered UI $($captureCase.mode) $resolution did not produce $sourcePath"
    }

    $stage = "copy"
    $copied = $false
    for ($copyAttempt = 0; $copyAttempt -lt 20; $copyAttempt++) {
        try {
            Copy-Item -LiteralPath $sourcePath -Destination $capturePath -Force
            $copied = $true
            break
        } catch {
            Start-Sleep -Milliseconds 250
        }
    }
    if (-not $copied) {
        throw "Rendered UI $($captureCase.mode) $resolution could not copy $sourcePath to $capturePath"
    }
    $results.Add([pscustomobject]@{
        mode = $captureCase.mode
        map = $caseMap
        resolution = $resolution
        profile = $profile
        hudScale = if ($captureCase.PSObject.Properties.Name -contains "hudScale") { $captureCase.hudScale } else { $null }
        safeAreaScale = if ($captureCase.PSObject.Properties.Name -contains "safeAreaScale") { $captureCase.safeAreaScale } else { $null }
        width = $dimensions.width
        height = $dimensions.height
        image = $capturePath
        log = $logPath
        passed = $true
    })
    Write-Host "[Rendered UI $($captureCase.mode) $resolution $profile] Passed"
    }
    catch {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-RenderedUIProcessTree -ProcessId $process.Id
        }
        $failure = [pscustomobject]@{
            mode = $captureCase.mode
            resolution = $resolution
            profile = $profile
            stage = $stage
            error = $_.Exception.Message
            log = $logPath
        }
        $failures.Add($failure)
        Write-Warning "[Rendered UI $($captureCase.mode) $resolution $profile] Failed at ${stage}: $($_.Exception.Message)"
    }
}

$summary = [ordered]@{
    result = "Passed"
    rendererProof = $true
    staticLayoutProof = $true
    pseudoLocalizationProof = [bool]$PseudoLocalization
    culture = if ($PseudoLocalization) { "leet" } else { "en" }
    interactiveInputProof = $false
    multiplayerProof = $false
    passed = ($failures.Count -eq 0)
    failureCount = $failures.Count
    failures = @($failures)
    runId = $runId
    maps = @($firstLightMap, $mainMenuMap)
    captures = @($results)
}
$summary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

if ($failures.Count -eq 0) {
    Write-Host "Rendered UI validation passed: $summaryPath"
}
if ($failures.Count -gt 0) {
    Write-Error "Rendered UI validation completed with $($failures.Count) failed case(s). See $summaryPath"
    exit 1
}
