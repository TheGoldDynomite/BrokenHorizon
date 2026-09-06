[CmdletBinding()]
param(
    [string]$EngineRoot,
    [ValidatePattern("^[A-Za-z0-9_-]{1,80}$")][string]$LogPrefix = "BHPerformance",
    [ValidateSet("Editor", "Packaged")][string]$Runtime = "Editor",
    [string]$PackageRoot,
    [switch]$RenderOffscreen,
    [int]$CaptureFrames = 600,
    [int]$WarmupFrames = 180,
    [double]$FrameP95BudgetMs = 8.0,
    [double]$FrameP99BudgetMs = 16.67,
    [int]$FrameHitchBudget = 1,
    [double]$PhysicalMemoryBudgetMB = 4096,
    [int]$ActorBudget = 500,
    [int]$TickFunctionBudget = 500,
    [switch]$Rendered,
    [switch]$LowerTier,
    [switch]$Traversal,
    [switch]$WorldMap,
    [switch]$WorldTraversal,
    [double]$GpuP95BudgetMs = 16.67,
    [double]$GpuP99BudgetMs = 20.0,
    [double]$RenderThreadP95BudgetMs = 16.67,
    [int]$DrawCallP95Budget = 2000,
    [int]$PrimitiveP95Budget = 1000000,
    [double]$GpuMemoryUsageBudgetPercent = 80.0,
    [double]$GpuBudgetP05MinimumOfMaxPercent = 80.0,
    [double]$DesiredTextureDataMinimumPercent = 99.0,
    [double]$PendingStreamInMaximum = 0.0,
    [double]$TraversalDesiredTextureP05MinimumPercent = 95.0,
    [double]$TraversalDesiredTextureFinalMinimumPercent = 99.0,
    [double]$TraversalPendingStreamInP95Maximum = 0.0,
    [double]$TraversalPendingStreamInFrameMaximumPercent = 5.0,
    [double]$TraversalLongHitchThresholdMs = 50.0,
    [int]$TraversalLongHitchBudget = 0,
    [int]$PsoMissesOnHitchBudget = 0,
    [int]$ComputePsoMissesOnHitchBudget = 0
)

$ErrorActionPreference = "Stop"

if ($WorldTraversal) {
    $Traversal = $true
    $Rendered = $true
    $WorldMap = $true
}

if ($WorldMap -and -not $PSBoundParameters.ContainsKey("ActorBudget")) {
    # The persistent six-sector runtime deliberately carries strategic actors,
    # routes, landscape proxies, and active operation state that First Light
    # does not. Keep a bounded world-scale capacity ceiling while timing and
    # tick budgets independently guard per-frame cost.
    $ActorBudget = 700
}

if ($WorldTraversal -and $CaptureFrames -lt 4200) {
    $CaptureFrames = 4200
}
if ($Traversal -and $CaptureFrames -lt 1800) {
    $CaptureFrames = 1800
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $projectRoot "Config\ProjectManifest.json") | ConvertFrom-Json
if (-not $EngineRoot) {
    $EngineRoot = $manifest.engineRoot
}

$editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$uproject = Join-Path $projectRoot $manifest.uproject
$map = if ($WorldMap) {
    "/Game/BrokenHorizon/Maps/L_BrokenHorizon_World"
} else {
    "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
}
$logDirectory = Join-Path $projectRoot "Saved\Logs"
$reportDirectory = Join-Path $projectRoot "Saved\Reports"
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$runDirectory = Join-Path $projectRoot "Saved\Automation\Performance\$runId"
$captureUserDir = Join-Path $runDirectory 'User'
$runtimeLog = Join-Path $runDirectory "$LogPrefix.log"
$profileReport = Join-Path $runDirectory "$LogPrefix-Profile.csv"
$summaryReport = Join-Path $runDirectory "$LogPrefix-Summary.json"
$compatibilitySummary = Join-Path $reportDirectory "$LogPrefix-Summary.json"
$resolvedPackageRoot = $null
if ($Runtime -eq 'Packaged') {
    if ([string]::IsNullOrWhiteSpace($PackageRoot)) { throw '-Runtime Packaged requires an explicit -PackageRoot Windows archive directory.' }
    $resolvedPackageRoot = if ([IO.Path]::IsPathRooted($PackageRoot)) { [IO.Path]::GetFullPath($PackageRoot) } else {
        [IO.Path]::GetFullPath((Join-Path $projectRoot $PackageRoot))
    }
}
$executable = if ($Runtime -eq 'Editor') { $editor } else { Join-Path $resolvedPackageRoot 'BrokenHorizon\Binaries\Win64\BrokenHorizon.exe' }
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Runtime executable not found: $executable" }
if ($RenderOffscreen -and -not $Rendered) { throw '-RenderOffscreen requires -Rendered.' }
if ($LowerTier -and -not $Rendered) { throw '-LowerTier requires -Rendered.' }
if ($WarmupFrames -lt 0 -or $CaptureFrames -lt 8) { throw 'WarmupFrames must be nonnegative and CaptureFrames at least8.' }
New-Item -ItemType Directory -Path $captureUserDir -Force | Out-Null
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null

function Resolve-PerformanceCsv {
    param([string]$LogContent, [string]$ExecutableDirectory, [string]$OwnedDirectory, [DateTime]$CaptureStart)
    $markers = [regex]::Matches($LogContent, 'LogCsvProfiler:\s+Display: Capture Ended\. Writing CSV to file :[ \t]*(?<path>[^\r\n]+)')
    if ($markers.Count -ne 1) { throw "Expected exactly one finalized CSV path marker; observed $($markers.Count)." }
    $rawPath = $markers[0].Groups['path'].Value.Trim().Trim('"')
    $csvPath = if ([IO.Path]::IsPathRooted($rawPath)) { [IO.Path]::GetFullPath($rawPath) } else {
        [IO.Path]::GetFullPath((Join-Path $ExecutableDirectory $rawPath))
    }
    $ownedPrefix = [IO.Path]::GetFullPath($OwnedDirectory).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $csvPath.StartsWith($ownedPrefix, [StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetExtension($csvPath) -ne '.csv') {
        throw "CSV marker resolves outside this run's owned CSV evidence: $csvPath"
    }
    $file = Get-Item -LiteralPath $csvPath -ErrorAction Stop
    if ($file.PSIsContainer -or $file.Length -le 0 -or $file.LastWriteTimeUtc -lt $CaptureStart -or $file.CreationTimeUtc -lt $CaptureStart) {
        throw "CSV marker points to missing, empty or stale capture evidence: $csvPath"
    }
    return $file
}
function Get-NativePerformanceMetadata {
    param([string]$RawFooter)
    # UE5.8 FCsvStreamWriter::Finalize deliberately writes commandline last.
    # That raw, quoted tail can contain unescaped quotes/commas. Keep the original
    # CSV untouched and parse only the preceding structured metadata as strict CSV.
    if (-not $RawFooter.StartsWith('[HasHeaderRowAtEnd],1,')) { throw 'Invalid native CSV metadata footer.' }
    $tailMarker = ',[commandline],'
    $tailIndex = $RawFooter.IndexOf($tailMarker, [StringComparison]::Ordinal)
    if ($tailIndex -lt 0) { throw 'Native CSV footer lacks the final commandline field.' }
    $tail = $RawFooter.Substring($tailIndex + $tailMarker.Length)
    if ($tail.Length -lt 2 -or -not $tail.StartsWith('"') -or -not $tail.EndsWith('"')) { throw 'Native CSV commandline tail is incomplete.' }
    $reader = [IO.StringReader]::new($RawFooter.Substring(0, $tailIndex))
    $metadataParser = [Microsoft.VisualBasic.FileIO.TextFieldParser]::new($reader)
    try {
        $metadataParser.TextFieldType = [Microsoft.VisualBasic.FileIO.FieldType]::Delimited
        $metadataParser.SetDelimiters(','); $metadataParser.HasFieldsEnclosedInQuotes = $true
        $fields = $metadataParser.ReadFields()
        if ($fields.Count % 2 -ne 0) { throw 'Native CSV metadata must contain complete key/value pairs.' }
        $allowed = @('platform', 'config', 'buildversion', 'engineversion', 'cpu', 'os', 'systemresolution.resx', 'systemresolution.resy',
            'rhiname', 'verbatimrhiname', 'gpu', 'gpudriver', 'deviceprofile', 'vsyncenabled', 'targetframerate', 'bh_run_id', 'bh_runtime', 'captureduration')
        $seen = @{}; $metadata = [ordered]@{}
        for ($i = 0; $i -lt $fields.Count; $i += 2) {
            if ($fields[$i] -notmatch '^\[[A-Za-z0-9_.]+\]$') { throw 'Malformed native CSV metadata key.' }
            $key = $fields[$i].Trim('[', ']')
            if ($seen.ContainsKey($key)) { throw "Duplicate native CSV metadata key: $key" }
            $seen[$key] = $true
            if ($key -in $allowed) { $metadata[$key] = $fields[$i + 1] }
        }
        return $metadata
    } finally { $metadataParser.Close(); $reader.Dispose() }
}
function Get-ObservedPerformanceSettings {
    param([string]$LogContent, [string[]]$Names)
    $observed = [ordered]@{}
    foreach ($name in $Names) {
        $pattern = '(?m)^[^\r\n]*?(?<![A-Za-z0-9_.])' + [regex]::Escape($name) + '\s*=\s*"(?<value>[^"\r\n]+)"\s+LastSetBy:\s*(?<source>[^\s\r\n]+)'
        $records = [regex]::Matches($LogContent, $pattern)
        if ($records.Count -lt 2) { throw "Missing early/final effective setting observations for $name." }
        $early = $records[$records.Count - 2]; $final = $records[$records.Count - 1]
        $observed[$name] = [ordered]@{
            earlyValue = $early.Groups['value'].Value; earlyLastSetBy = $early.Groups['source'].Value
            value = $final.Groups['value'].Value; lastSetBy = $final.Groups['source'].Value
            stable = $early.Groups['value'].Value -ceq $final.Groups['value'].Value
        }
    }
    return $observed
}
function Stop-OwnedPerformanceProcess {
    if ($null -eq $process -or $completion.cleanedUp) { return }
    $process.Refresh()
    if (-not $process.HasExited) {
        $completion.termination = 'OwnedProcessStopped'
        Stop-Process -Id $process.Id -ErrorAction Stop
        if (-not $process.WaitForExit(10000)) { throw "Owned performance process $($process.Id) failed to exit." }
    } else { $completion.termination = 'NaturalExit' }
    $completion.exitCode = $process.ExitCode
    $completion.cleanedUp = $true
}

if ($CaptureFrames -le $WarmupFrames) {
    throw "CaptureFrames must be greater than WarmupFrames."
}
if ($Traversal -and -not $Rendered) {
    throw "Traversal performance validation requires -Rendered."
}
if ($Rendered -and -not $PSBoundParameters.ContainsKey("FrameP95BudgetMs")) {
    $FrameP95BudgetMs = 16.67
}
if ($Rendered -and
    -not $PSBoundParameters.ContainsKey("PhysicalMemoryBudgetMB")) {
    $PhysicalMemoryBudgetMB = 6144
}

$captureStart = [DateTime]::UtcNow
$process = $null
$summary = $null
$completion = [ordered]@{ csvFinalized = $false; termination = 'NotStarted'; cleanedUp = $false; exitCode = $null }
$settingsNames = @('sg.ResolutionQuality', 'sg.ViewDistanceQuality', 'sg.AntiAliasingQuality', 'sg.ShadowQuality',
    'sg.GlobalIlluminationQuality', 'sg.ReflectionQuality', 'sg.PostProcessQuality', 'sg.TextureQuality',
    'sg.EffectsQuality', 'sg.FoliageQuality', 'sg.ShadingQuality', 'r.ScreenPercentage', 'r.VSync', 't.MaxFPS')
$queryFrames = @([Math]::Max(1, $WarmupFrames - 4), ($CaptureFrames - 4))
$csvCommands = @($queryFrames | ForEach-Object { $frame = $_; $settingsNames | ForEach-Object { "${frame}:$_" } }) -join ','
$requestedSettings = [ordered]@{ 't.MaxFPS' = '0' }
$executableHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $executable).Hash
$captureSeconds = [Math]::Ceiling($CaptureFrames / 60.0) + 5
$arguments = @()
if ($Runtime -eq 'Editor') { $arguments += $uproject }
$arguments += @(
    $map, '-game', '-unattended', '-nosound', '-NoSplash', '-DisablePython', '-DDC-ForceMemoryCache',
    '-benchmark', '-fps=60', '-ExecCmds=t.MaxFPS 0', "-seconds=$captureSeconds",
    "-csvCaptureFrames=$CaptureFrames", '-csvCompression=0', "-csvExecCmds=$csvCommands",
    "-csvMetadata=bh_run_id=$runId,bh_runtime=$Runtime", "-UserDir=$captureUserDir",
    "-BHTestSaveSlotSuffix=Performance_$($runId -replace '-', '_')", "-abslog=$runtimeLog"
)
if ($Rendered) {
    $arguments += @(
        "-csvGpuStats",
        "-ResX=1920",
        "-ResY=1080",
        "-ForceRes",
        "-NoVSync"
    )
    $requestedSettings['r.VSync'] = '0'
    if ($RenderOffscreen) { $arguments += '-RenderOffscreen' }
    if ($LowerTier) {
        # These are requests. Effective values must be observed after project settings apply.
        $arguments += @(
            "-ScalabilityQuality=1",
            "-sg.ResolutionQuality=75",
            "-sg.ViewDistanceQuality=1",
            "-sg.AntiAliasingQuality=1",
            "-sg.ShadowQuality=1",
            "-sg.GlobalIlluminationQuality=1",
            "-sg.ReflectionQuality=1",
            "-sg.PostProcessQuality=1",
            "-sg.TextureQuality=1",
            "-sg.EffectsQuality=1",
            "-r.ScreenPercentage=75"
        )
    }
        foreach ($name in $settingsNames | Where-Object { $_ -like 'sg.*' -and $_ -notin @('sg.ResolutionQuality', 'sg.FoliageQuality', 'sg.ShadingQuality') }) {
            if ($LowerTier) { $requestedSettings[$name] = '1' }
        }
        if ($LowerTier) { $requestedSettings['sg.ResolutionQuality'] = '75'; $requestedSettings['r.ScreenPercentage'] = '75' }
} else { $arguments += '-nullrhi' }
if ($WorldTraversal) {
    $arguments += "-BHTestRenderedWorldTraversal"
} elseif ($Traversal) {
    $arguments += "-BHTestRenderedTraversal"
}

try {
Write-Host "[Performance] Capturing $CaptureFrames boot frames on $map (discard prefix=$WarmupFrames; runtime=$Runtime; rendered=$Rendered; offscreen=$RenderOffscreen)"
$quotedArguments = foreach ($argument in $arguments) {
    if ($argument -match '["\r\n]' -or $argument.EndsWith('\')) { throw "Unsupported launch argument: $argument" }
    '"' + $argument + '"'
}
$process = Start-Process -FilePath $executable -ArgumentList $quotedArguments -WindowStyle Hidden -PassThru
$completion.processId = $process.Id
$completion.termination = 'Running'
$startupAllowanceSeconds = if ($Rendered) { 900 } else { 180 }
$captureDeadline = [DateTime]::UtcNow.AddSeconds($captureSeconds + $startupAllowanceSeconds)
while ([DateTime]::UtcNow -lt $captureDeadline) {
    if (Test-Path -LiteralPath $runtimeLog) {
        $liveLog = Get-Content -Raw -LiteralPath $runtimeLog
        if ($liveLog -match 'LogCsvProfiler:\s+Display: Capture Ended\. Writing CSV to file :') { break }
        if ($liveLog -match 'Fatal error:|Assertion failed:|Unhandled Exception:') { throw "Capture failed: $runtimeLog" }
    }
    $process.Refresh()
    if ($process.HasExited) { break }
    Start-Sleep -Seconds 1
}
if (-not (Test-Path -LiteralPath $runtimeLog)) { throw "Capture did not produce its own log: $runtimeLog" }
$logContent = Get-Content -Raw -LiteralPath $runtimeLog
$sourceProfile = Resolve-PerformanceCsv -LogContent $logContent -ExecutableDirectory (Split-Path -Parent $executable) -OwnedDirectory $runDirectory -CaptureStart $captureStart
$completion.csvFinalized = $true
$completion.sourceCsv = $sourceProfile.FullName
Stop-OwnedPerformanceProcess
$logContent = Get-Content -Raw -LiteralPath $runtimeLog
$failurePatterns = @("Fatal error:", "Assertion failed:", "Unhandled Exception:", "Failed to load package", "CreateExport: Failed to load")
if ($WorldTraversal) {
    $failurePatterns += "Navmesh bounds are too large!"
}
$failures = @($failurePatterns | Where-Object { $logContent -match [regex]::Escape($_) })
if ($failures.Count -gt 0) {
    throw "Performance log contains failure markers: $($failures -join ', '). See $runtimeLog"
}
$traversalMarkerPrefix = if ($WorldTraversal) {
    "BH_RENDERED_WORLD_TRAVERSAL"
} else {
    "BH_RENDERED_TRAVERSAL"
}
$traversalStepCount = [regex]::Matches(
    $logContent,
    "$traversalMarkerPrefix`_STEP result=success"
).Count
if ($Traversal -and
    ($traversalStepCount -ne 8 -or
     $logContent -notmatch
        "$traversalMarkerPrefix`_COMPLETE result=success steps=8 loops=2" -or
     $logContent -match "$traversalMarkerPrefix`_STEP result=failure")) {
    throw "Rendered traversal did not complete its canonical eight-step route. See $runtimeLog"
}
if ($WorldTraversal) {
    $navigationStepCount = [regex]::Matches(
        $logContent,
        "BH_RENDERED_WORLD_NAVIGATION result=success"
    ).Count
    if ($navigationStepCount -ne 8) {
        throw "Full-world traversal did not prove navigation at all eight sector visits. See $runtimeLog"
    }
}
else {
    $navigationStepCount = 0
}

Copy-Item -LiteralPath $sourceProfile.FullName -Destination $profileReport
$observedSettings = Get-ObservedPerformanceSettings -LogContent $logContent -Names $settingsNames

Add-Type -AssemblyName Microsoft.VisualBasic
$parser = New-Object Microsoft.VisualBasic.FileIO.TextFieldParser($profileReport)
$parser.TextFieldType = [Microsoft.VisualBasic.FileIO.FieldType]::Delimited
$parser.SetDelimiters(",")
$parser.HasFieldsEnclosedInQuotes = $true
$headers = $parser.ReadFields()
$requiredColumns = @("FrameTime", "GameThreadTime", "PhysicalUsedMB", "ActorCount/TotalActorCount", "Ticks/Total")
if ($Rendered) {
    $requiredColumns += @(
        "GPUTime",
        "RenderThreadTime",
        "GPUMem/LocalUsedMB",
        "GPUMem/LocalBudgetMB",
        "RHI/DrawCalls",
        "RHI/PrimitivesDrawn",
        "TextureStreaming/DesiredDataLoadedPercent",
        "TextureStreaming/PendingStreamInData",
        "PSO/PSOMisses",
        "PSO/PSOMissesOnHitch",
        "PSO/PSOComputeMisses",
        "PSO/PSOComputeMissesOnHitch",
        "Shaders/NumShadersLoaded",
        "Shaders/NumShaderMaps"
    )
}
if ($WorldTraversal) {
    $requiredColumns += "BrokenHorizon/WorldTraversalMeasure"
}
$columnIndices = @{}
foreach ($column in $requiredColumns) {
    $index = [Array]::IndexOf($headers, $column)
    if ($index -lt 0) {
        $parser.Close()
        throw "Performance profile is missing required column '$column'. See $profileReport"
    }
    $columnIndices[$column] = $index
}

$csvMetadata = [ordered]@{}
$diagnosticRows = [System.Collections.Generic.List[int]]::new()
$excludedDiagnosticSamples = 0
$eventsIndex = [Array]::IndexOf($headers, "EVENTS")
if ($eventsIndex -lt 0) { throw "CSV lacks EVENTS required to exclude settings diagnostics." }
$samples = New-Object System.Collections.Generic.List[object]
$rowIndex = 0
while (-not $parser.EndOfData) {
    if ($parser.PeekChars(32).StartsWith('[HasHeaderRowAtEnd],')) {
        $csvMetadata = Get-NativePerformanceMetadata -RawFooter $parser.ReadLine()
        if (-not $parser.EndOfData) { throw 'Native metadata footer must be the final CSV record.' }
        break
    }
    $fields = $parser.ReadFields()
    if ($fields.Count -lt $headers.Count) {
        continue
    }
    $frameTime = 0.0
    $gameThreadTime = 0.0
    $physicalUsedMB = 0.0
    $actorCount = 0.0
    $tickFunctions = 0.0
    $gpuTime = 0.0
    $renderThreadTime = 0.0
    $gpuMemoryUsedMB = 0.0
    $gpuMemoryBudgetMB = 0.0
    $drawCalls = 0.0
    $primitivesDrawn = 0.0
    $desiredTextureDataPercent = 0.0
    $pendingStreamInData = 0.0
    $psoMisses = 0.0
    $psoMissesOnHitch = 0.0
    $computePsoMisses = 0.0
    $computePsoMissesOnHitch = 0.0
    $numShadersLoaded = 0.0
    $numShaderMaps = 0.0
    $worldTraversalMeasure = 0.0
    $numberStyle = [Globalization.NumberStyles]::Float
    $culture = [Globalization.CultureInfo]::InvariantCulture
    $numericRow = [double]::TryParse($fields[$columnIndices["FrameTime"]], $numberStyle, $culture, [ref]$frameTime) -and
        [double]::TryParse($fields[$columnIndices["GameThreadTime"]], $numberStyle, $culture, [ref]$gameThreadTime) -and
        [double]::TryParse($fields[$columnIndices["PhysicalUsedMB"]], $numberStyle, $culture, [ref]$physicalUsedMB) -and
        [double]::TryParse($fields[$columnIndices["ActorCount/TotalActorCount"]], $numberStyle, $culture, [ref]$actorCount) -and
        [double]::TryParse($fields[$columnIndices["Ticks/Total"]], $numberStyle, $culture, [ref]$tickFunctions)
    if ($numericRow -and $Rendered) {
        $numericRow =
            [double]::TryParse($fields[$columnIndices["GPUTime"]], $numberStyle, $culture, [ref]$gpuTime) -and
            [double]::TryParse($fields[$columnIndices["RenderThreadTime"]], $numberStyle, $culture, [ref]$renderThreadTime) -and
            [double]::TryParse($fields[$columnIndices["GPUMem/LocalUsedMB"]], $numberStyle, $culture, [ref]$gpuMemoryUsedMB) -and
            [double]::TryParse($fields[$columnIndices["GPUMem/LocalBudgetMB"]], $numberStyle, $culture, [ref]$gpuMemoryBudgetMB) -and
            [double]::TryParse($fields[$columnIndices["RHI/DrawCalls"]], $numberStyle, $culture, [ref]$drawCalls) -and
            [double]::TryParse($fields[$columnIndices["RHI/PrimitivesDrawn"]], $numberStyle, $culture, [ref]$primitivesDrawn) -and
            [double]::TryParse($fields[$columnIndices["TextureStreaming/DesiredDataLoadedPercent"]], $numberStyle, $culture, [ref]$desiredTextureDataPercent) -and
            [double]::TryParse($fields[$columnIndices["TextureStreaming/PendingStreamInData"]], $numberStyle, $culture, [ref]$pendingStreamInData) -and
            [double]::TryParse($fields[$columnIndices["PSO/PSOMisses"]], $numberStyle, $culture, [ref]$psoMisses) -and
            [double]::TryParse($fields[$columnIndices["PSO/PSOMissesOnHitch"]], $numberStyle, $culture, [ref]$psoMissesOnHitch) -and
            [double]::TryParse($fields[$columnIndices["PSO/PSOComputeMisses"]], $numberStyle, $culture, [ref]$computePsoMisses) -and
            [double]::TryParse($fields[$columnIndices["PSO/PSOComputeMissesOnHitch"]], $numberStyle, $culture, [ref]$computePsoMissesOnHitch) -and
            [double]::TryParse($fields[$columnIndices["Shaders/NumShadersLoaded"]], $numberStyle, $culture, [ref]$numShadersLoaded) -and
            [double]::TryParse($fields[$columnIndices["Shaders/NumShaderMaps"]], $numberStyle, $culture, [ref]$numShaderMaps)
    }
    if ($numericRow -and $WorldTraversal) {
        $numericRow = [double]::TryParse(
            $fields[$columnIndices["BrokenHorizon/WorldTraversalMeasure"]],
            $numberStyle,
            $culture,
            [ref]$worldTraversalMeasure
        )
    }
    if (-not $numericRow) {
        continue
    }
    $isDiagnostic = $fields[$eventsIndex] -match 'CsvExecCommand :'
    if ($isDiagnostic) { $diagnosticRows.Add($rowIndex) }
    $eligibleSample = $rowIndex -ge $WarmupFrames -and (-not $WorldTraversal -or $worldTraversalMeasure -ge 0.5)
    if ($eligibleSample -and $isDiagnostic) { ++$excludedDiagnosticSamples }
    if ($eligibleSample -and -not $isDiagnostic) {
        $samples.Add([pscustomobject]@{
            FrameTime = $frameTime
            GameThreadTime = $gameThreadTime
            PhysicalUsedMB = $physicalUsedMB
            ActorCount = $actorCount
            TickFunctions = $tickFunctions
            GpuTime = $gpuTime
            RenderThreadTime = $renderThreadTime
            GpuMemoryUsedMB = $gpuMemoryUsedMB
            GpuMemoryBudgetMB = $gpuMemoryBudgetMB
            DrawCalls = $drawCalls
            PrimitivesDrawn = $primitivesDrawn
            DesiredTextureDataPercent = $desiredTextureDataPercent
            PendingStreamInData = $pendingStreamInData
            PsoMisses = $psoMisses
            # UE emits -1 when the frame was not classified as a hitch.
            # Preserve applicability separately and count only real misses.
            PsoMissesOnHitch = [Math]::Max(0.0, $psoMissesOnHitch)
            PsoHitchCounterApplicable = $psoMissesOnHitch -ge 0.0
            ComputePsoMisses = $computePsoMisses
            ComputePsoMissesOnHitch = [Math]::Max(
                0.0,
                $computePsoMissesOnHitch
            )
            ComputePsoHitchCounterApplicable =
                $computePsoMissesOnHitch -ge 0.0
            NumShadersLoaded = $numShadersLoaded
            NumShaderMaps = $numShaderMaps
        })
    }
    $rowIndex++
}
$parser.Close()
if ($csvMetadata['bh_run_id'] -cne $runId -or $csvMetadata['bh_runtime'] -cne $Runtime) { throw 'CSV metadata does not identify this run/runtime.' }
if ($diagnosticRows.Count -ne 2) { throw "Expected two excluded settings-query rows; observed $($diagnosticRows.Count)." }

$minimumSamples = if ($WorldTraversal) {
    # The custom CSV stat is emitted on each 0.1 s fixture movement tick.
    # Nineteen sampled movement frames across each of eight sectors are the
    # complete marked acceptance set; loading/recovery dwell frames stay out.
    152 - $excludedDiagnosticSamples
} else {
    $CaptureFrames - $WarmupFrames - $excludedDiagnosticSamples
}
if ($samples.Count -lt $minimumSamples) {
    throw "Performance profile contained $($samples.Count) measured frames; expected at least $minimumSamples. See $profileReport"
}

function Get-Percentile {
    param([double[]]$Values, [double]$Percentile)
    $sorted = @($Values | Sort-Object)
    $index = [Math]::Min(
        $sorted.Count - 1,
        [Math]::Max(0, [Math]::Ceiling($sorted.Count * $Percentile) - 1)
    )
    return [double]$sorted[$index]
}

$gameThreadValues = [double[]]@($samples | ForEach-Object { $_.GameThreadTime })
$frameValues = [double[]]@($samples | ForEach-Object { $_.FrameTime })
$metrics = [ordered]@{
    measuredFrames = $samples.Count
    frameP50Ms = [Math]::Round((Get-Percentile $frameValues 0.50), 3)
    frameP95Ms = [Math]::Round((Get-Percentile $frameValues 0.95), 3)
    frameP99Ms = [Math]::Round((Get-Percentile $frameValues 0.99), 3)
    frameMaxMs = [Math]::Round((($frameValues | Measure-Object -Maximum).Maximum), 3)
    frameHitchesOver33Ms = @($frameValues | Where-Object { $_ -gt 33.33 }).Count
    gameThreadP50Ms = [Math]::Round((Get-Percentile $gameThreadValues 0.50), 3)
    gameThreadP95Ms = [Math]::Round((Get-Percentile $gameThreadValues 0.95), 3)
    gameThreadP99Ms = [Math]::Round((Get-Percentile $gameThreadValues 0.99), 3)
    gameThreadMaxMs = [Math]::Round((($gameThreadValues | Measure-Object -Maximum).Maximum), 3)
    gameThreadHitchesOver33Ms = @($gameThreadValues | Where-Object { $_ -gt 33.33 }).Count
    physicalMemoryMaxMB = [Math]::Round((($samples.PhysicalUsedMB | Measure-Object -Maximum).Maximum), 1)
    actorCountMax = [int](($samples.ActorCount | Measure-Object -Maximum).Maximum)
    tickFunctionsMax = [int](($samples.TickFunctions | Measure-Object -Maximum).Maximum)
}
if ($Rendered) {
    $gpuValues = [double[]]@($samples | ForEach-Object { $_.GpuTime })
    $renderThreadValues = [double[]]@(
        $samples | ForEach-Object { $_.RenderThreadTime }
    )
    $positiveGpuMemoryBudgets = @(
        $samples.GpuMemoryBudgetMB | Where-Object { $_ -gt 0 }
    )
    $gpuMemoryBudgetMinimum = if ($positiveGpuMemoryBudgets.Count -gt 0) {
        [double](
            ($positiveGpuMemoryBudgets | Measure-Object -Minimum).Minimum
        )
    } else {
        0.0
    }
    $gpuMemoryBudgetMaximum = if ($positiveGpuMemoryBudgets.Count -gt 0) {
        [double](
            ($positiveGpuMemoryBudgets | Measure-Object -Maximum).Maximum
        )
    } else {
        0.0
    }
    $gpuMemoryBudgetP05 = if ($positiveGpuMemoryBudgets.Count -gt 0) {
        [double](Get-Percentile `
            ([double[]]$positiveGpuMemoryBudgets) 0.05)
    } else {
        0.0
    }
    $desiredTextureValues = [double[]]@(
        $samples | ForEach-Object { $_.DesiredTextureDataPercent }
    )
    $pendingStreamInValues = [double[]]@(
        $samples | ForEach-Object { $_.PendingStreamInData }
    )
    $gpuMemoryUsedMaximum = [double](
        ($samples.GpuMemoryUsedMB | Measure-Object -Maximum).Maximum
    )
    $metrics.gpuP50Ms = [Math]::Round(
        (Get-Percentile $gpuValues 0.50),
        3
    )
    $metrics.gpuP95Ms = [Math]::Round(
        (Get-Percentile $gpuValues 0.95),
        3
    )
    $metrics.gpuP99Ms = [Math]::Round(
        (Get-Percentile $gpuValues 0.99),
        3
    )
    $metrics.gpuMaxMs = [Math]::Round(
        (($gpuValues | Measure-Object -Maximum).Maximum),
        3
    )
    $metrics.renderThreadP95Ms = [Math]::Round(
        (Get-Percentile $renderThreadValues 0.95),
        3
    )
    $metrics.drawCallsP95 = [int](Get-Percentile `
        ([double[]]@($samples.DrawCalls)) 0.95)
    $metrics.drawCallsMax = [int](
        ($samples.DrawCalls | Measure-Object -Maximum).Maximum
    )
    $metrics.primitivesDrawnP95 = [int](Get-Percentile `
        ([double[]]@($samples.PrimitivesDrawn)) 0.95)
    $metrics.gpuMemoryUsedMaxMB = [Math]::Round(
        $gpuMemoryUsedMaximum,
        1
    )
    $metrics.gpuMemoryBudgetMinMB = [Math]::Round(
        $gpuMemoryBudgetMinimum,
        1
    )
    $metrics.gpuMemoryBudgetP05MB = [Math]::Round(
        $gpuMemoryBudgetP05,
        1
    )
    $metrics.gpuMemoryBudgetMaxMB = [Math]::Round(
        $gpuMemoryBudgetMaximum,
        1
    )
    $metrics.gpuBudgetP05OfMaxPercent = if (
        $gpuMemoryBudgetMaximum -gt 0
    ) {
        [Math]::Round(
            100.0 * $gpuMemoryBudgetP05 /
                $gpuMemoryBudgetMaximum,
            1
        )
    } else {
        0.0
    }
    $metrics.gpuMemoryUsageMaxPercent = if ($gpuMemoryBudgetMinimum -gt 0) {
        [Math]::Round(
            100.0 * $gpuMemoryUsedMaximum / $gpuMemoryBudgetMinimum,
            1
        )
    } else {
        100.0
    }
    $metrics.desiredTextureDataMinPercent = [Math]::Round(
        (($samples.DesiredTextureDataPercent |
            Measure-Object -Minimum).Minimum),
        2
    )
    $metrics.pendingStreamInDataMax = [Math]::Round(
        (($samples.PendingStreamInData |
            Measure-Object -Maximum).Maximum),
        3
    )
    $metrics.psoMisses = [int](
        ($samples.PsoMisses | Measure-Object -Sum).Sum
    )
    $metrics.psoMissesOnHitch = [int](
        ($samples.PsoMissesOnHitch | Measure-Object -Sum).Sum
    )
    $metrics.computePsoMisses = [int](
        ($samples.ComputePsoMisses | Measure-Object -Sum).Sum
    )
    $metrics.computePsoMissesOnHitch = [int](
        ($samples.ComputePsoMissesOnHitch | Measure-Object -Sum).Sum
    )
    $metrics.psoHitchCounterApplicableFrames = @(
        $samples | Where-Object { $_.PsoHitchCounterApplicable }
    ).Count
    $metrics.computePsoHitchCounterApplicableFrames = @(
        $samples |
            Where-Object { $_.ComputePsoHitchCounterApplicable }
    ).Count
    $metrics.shadersLoadedMax = [int](
        ($samples.NumShadersLoaded | Measure-Object -Maximum).Maximum
    )
    $metrics.shaderMapsMax = [int](
        ($samples.NumShaderMaps | Measure-Object -Maximum).Maximum
    )
    $metrics.desiredTextureDataP05Percent = [Math]::Round(
        (Get-Percentile $desiredTextureValues 0.05),
        2
    )
    $metrics.desiredTextureDataFinal30MinPercent = [Math]::Round(
        (($samples | Select-Object -Last 30 |
            ForEach-Object { $_.DesiredTextureDataPercent } |
            Measure-Object -Minimum).Minimum),
        2
    )
    $metrics.pendingStreamInDataP95 = [Math]::Round(
        (Get-Percentile $pendingStreamInValues 0.95),
        3
    )
    $metrics.pendingStreamInFramePercent = [Math]::Round(
        100.0 * @(
            $pendingStreamInValues | Where-Object { $_ -gt 0 }
        ).Count / $pendingStreamInValues.Count,
        2
    )
    if ($Traversal) {
        $metrics.traversalLongHitches = @(
            $frameValues |
                Where-Object { $_ -gt $TraversalLongHitchThresholdMs }
        ).Count
    }
}
$budgets = [ordered]@{
    frameP95Ms = $FrameP95BudgetMs
    frameP99Ms = $FrameP99BudgetMs
    frameHitchesOver33Ms = $FrameHitchBudget
    physicalMemoryMaxMB = $PhysicalMemoryBudgetMB
    actorCountMax = $ActorBudget
    tickFunctionsMax = $TickFunctionBudget
}
if ($Rendered) {
    $budgets.gpuP95Ms = $GpuP95BudgetMs
    $budgets.gpuP99Ms = $GpuP99BudgetMs
    $budgets.renderThreadP95Ms = $RenderThreadP95BudgetMs
    $budgets.drawCallsP95 = $DrawCallP95Budget
    $budgets.primitivesDrawnP95 = $PrimitiveP95Budget
    $budgets.gpuMemoryUsageMaxPercent =
        $GpuMemoryUsageBudgetPercent
    $budgets.gpuBudgetP05OfMaxMinPercent =
        $GpuBudgetP05MinimumOfMaxPercent
    $budgets.psoMissesOnHitch = $PsoMissesOnHitchBudget
    $budgets.computePsoMissesOnHitch =
        $ComputePsoMissesOnHitchBudget
    if ($Traversal) {
        $budgets.desiredTextureDataP05Percent =
            $TraversalDesiredTextureP05MinimumPercent
        $budgets.desiredTextureDataFinal30MinPercent =
            $TraversalDesiredTextureFinalMinimumPercent
        $budgets.pendingStreamInDataP95 =
            $TraversalPendingStreamInP95Maximum
        $budgets.pendingStreamInFramePercent =
            $TraversalPendingStreamInFrameMaximumPercent
        $budgets.traversalLongHitchThresholdMs =
            $TraversalLongHitchThresholdMs
        $budgets.traversalLongHitches = $TraversalLongHitchBudget
    } else {
        $budgets.desiredTextureDataMinPercent =
            $DesiredTextureDataMinimumPercent
        $budgets.pendingStreamInDataMax = $PendingStreamInMaximum
    }
}
$checks = [ordered]@{
    frameP95 = $metrics.frameP95Ms -le $budgets.frameP95Ms
    frameP99 = $metrics.frameP99Ms -le $budgets.frameP99Ms
    frameHitches = $metrics.frameHitchesOver33Ms -le $budgets.frameHitchesOver33Ms
    physicalMemory = $metrics.physicalMemoryMaxMB -le $budgets.physicalMemoryMaxMB
    actorCount = $metrics.actorCountMax -le $budgets.actorCountMax
    tickFunctions = $metrics.tickFunctionsMax -le $budgets.tickFunctionsMax
}
if ($Rendered) {
    $checks.gpuTimingPopulated = $metrics.gpuP50Ms -gt 0
    $checks.gpuP95 = $metrics.gpuP95Ms -le $budgets.gpuP95Ms
    $checks.gpuP99 = $metrics.gpuP99Ms -le $budgets.gpuP99Ms
    $checks.renderThreadP95 =
        $metrics.renderThreadP95Ms -le $budgets.renderThreadP95Ms
    $checks.drawCallsP95 =
        $metrics.drawCallsP95 -le $budgets.drawCallsP95
    $checks.primitivesDrawnP95 =
        $metrics.primitivesDrawnP95 -le $budgets.primitivesDrawnP95
    $checks.gpuMemory =
        $metrics.gpuMemoryUsageMaxPercent -le
        $budgets.gpuMemoryUsageMaxPercent
    $checks.gpuBudgetStable =
        $metrics.gpuBudgetP05OfMaxPercent -ge
        $budgets.gpuBudgetP05OfMaxMinPercent
    $checks.psoMissesOnHitch =
        $metrics.psoMissesOnHitch -le $budgets.psoMissesOnHitch
    $checks.computePsoMissesOnHitch =
        $metrics.computePsoMissesOnHitch -le
        $budgets.computePsoMissesOnHitch
    if ($Traversal) {
        $checks.desiredTextureDataP05 =
            $metrics.desiredTextureDataP05Percent -ge
            $budgets.desiredTextureDataP05Percent
        $checks.desiredTextureDataRecovered =
            $metrics.desiredTextureDataFinal30MinPercent -ge
            $budgets.desiredTextureDataFinal30MinPercent
        $checks.pendingStreamInP95 =
            $metrics.pendingStreamInDataP95 -le
            $budgets.pendingStreamInDataP95
        $checks.pendingStreamInFramePercent =
            $metrics.pendingStreamInFramePercent -le
            $budgets.pendingStreamInFramePercent
        $checks.traversalLongHitches =
            $metrics.traversalLongHitches -le
            $budgets.traversalLongHitches
    } else {
        $checks.desiredTextureData =
            $metrics.desiredTextureDataMinPercent -ge
            $budgets.desiredTextureDataMinPercent
        $checks.pendingStreamIn =
            $metrics.pendingStreamInDataMax -le
            $budgets.pendingStreamInDataMax
    }
}
$passed = @($checks.Values | Where-Object { -not $_ }).Count -eq 0
$budgetPassed = $passed
$settingsStable = @($observedSettings.Values | Where-Object { -not $_.stable }).Count -eq 0
$requestedSettingsMatched = $true
foreach ($name in $requestedSettings.Keys) {
    [double]$actual = 0; [double]$requested = 0
    $validNumbers = [double]::TryParse($observedSettings[$name].value, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$actual) -and
        [double]::TryParse($requestedSettings[$name], [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$requested)
    if (-not $validNumbers -or $actual -ne $requested) { $requestedSettingsMatched = $false }
}
$lowerTierVerified = $LowerTier -and $requestedSettingsMatched -and $settingsStable
$rhi = if ($csvMetadata.Contains('rhiname')) { $csvMetadata['rhiname'] } else { 'Unknown' }
$resolution = if ($csvMetadata.Contains('systemresolution.resx') -and $csvMetadata.Contains('systemresolution.resy')) {
    "$($csvMetadata['systemresolution.resx'])x$($csvMetadata['systemresolution.resy'])"
} else { 'Unknown' }
$presentationMode = if (-not $Rendered) { 'NullRHI' } elseif ($RenderOffscreen) { 'Offscreen' } else { 'OnscreenRequested' }
$passed = $budgetPassed -and $settingsStable -and $requestedSettingsMatched
$summary = [ordered]@{
    schemaVersion = 2
    generatedAtUtc = [DateTime]::UtcNow.ToString('o')
    runId = $runId; runtime = $Runtime; mode = "$Runtime-$presentationMode-$rhi"
    captureFrames = $CaptureFrames; warmupFrames = $WarmupFrames
    rendererProof = [bool]($Rendered -and $metrics.gpuP50Ms -gt 0 -and $rhi -ne 'Unknown')
    lowerTierProof = [bool]$lowerTierVerified; traversalProof = [bool]$Traversal
    worldMapProof = [bool]$WorldMap; worldTraversalProof = [bool]$WorldTraversal
    navigationProof = [bool]($WorldTraversal -and $navigationStepCount -eq 8)
    map = $map; traversalSteps = if ($Traversal) { $traversalStepCount } else { 0 }; navigationSteps = $navigationStepCount
    networkProof = $false; budgets = $budgets; metrics = $metrics; checks = $checks; passed = $passed
    captureValid = $settingsStable; budgetStatus = 'provisionalComparison'; budgetPassed = $budgetPassed
    settings = [ordered]@{
        requested = $requestedSettings; observed = $observedSettings; requestedMatched = $requestedSettingsMatched
        stableAcrossQueries = $settingsStable; lowerTierRequested = [bool]$LowerTier; queryFrames = $queryFrames
        source = 'Two csvExecCmds console queries with effective value and LastSetBy; exact CSV event rows excluded'
    }
    hardware = [ordered]@{
        resolution = $resolution; rhi = $rhi
        gpu = if ($csvMetadata.Contains('gpu')) { $csvMetadata['gpu'] } else { 'Unknown' }
        driver = if ($csvMetadata.Contains('gpudriver')) { $csvMetadata['gpudriver'] } else { 'Unknown' }
        cpu = if ($csvMetadata.Contains('cpu')) { $csvMetadata['cpu'] } else { 'Unknown' }
        os = if ($csvMetadata.Contains('os')) { $csvMetadata['os'] } else { 'Unknown' }
        source = 'Finalized CSV runtime metadata; resolution is observed engine system resolution'
        physicalDisplayPresentationVerified = $false
    }
    identity = [ordered]@{
        executable = $executable; executableSha256 = $executableHash
        executableLastWriteUtc = (Get-Item -LiteralPath $executable).LastWriteTimeUtc.ToString('o')
        packageRoot = if ($Runtime -eq 'Packaged') { $resolvedPackageRoot } else { $null }
        editorProject = if ($Runtime -eq 'Editor') { $uproject } else { $null }
        csvMetadata = $csvMetadata
    }
    frameAccounting = [ordered]@{
        requested = $CaptureFrames; parsedNumericRows = $rowIndex; startupPrefixDiscarded = [Math]::Min($rowIndex, $WarmupFrames)
        diagnosticRowIndices = @($diagnosticRows); diagnosticEligibleRowsExcluded = $excludedDiagnosticSamples
        measured = $samples.Count; otherUnmeasuredRows = $rowIndex - [Math]::Min($rowIndex, $WarmupFrames) - $excludedDiagnosticSamples - $samples.Count
    }
    measurementProtocol = [ordered]@{
        captureStart = 'Engine boot via csvCaptureFrames'; simulation = 'benchmark fixed1/60second timestep'
        timing = 'CSV elapsed execution timing; t.MaxFPS0 requested; actual effective cap recorded in settings'
        prefix = 'Startup prefix discard, not proof of warmed caches'; cacheState = 'Uncontrolled; no cache deletion or prewarming'
        traversal = if ($Traversal) { 'Scripted movement through canonical8steps/2loops; not interactive play' } else { 'Static startup location' }
        renderedRequested = [bool]$Rendered; renderOffscreenRequested = [bool]$RenderOffscreen
    }
    completion = $completion
    arguments = $arguments; runDirectory = $runDirectory; userDirectory = $captureUserDir
    sourceProfileCsv = $sourceProfile.FullName; profileSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $profileReport).Hash
    runtimeLog = $runtimeLog; profileCsv = $profileReport
    limitations = @(
        'Budgets are provisional comparisons on this machine, not a calibrated minimum-spec or release performance certification.',
        'Boot capture and discarded startup frames do not prove warm caches; first/repeat processes may reuse system and driver caches.',
        'Fixed simulation timestep and scripted traversal do not establish interactive feel, image quality or presentation pacing.',
        'Standalone profiling does not prove multiplayer bandwidth, travel, reconnect or soak stability.',
        $(if ($Rendered) { 'GPU metrics and runtime resolution do not replace rendered image inspection.' } else { 'NullRHI provides no GPU or rendered presentation proof.' })
    )
}
$summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $summaryReport -Encoding UTF8
Copy-Item -LiteralPath $summaryReport -Destination $compatibilitySummary -Force

Write-Host ("[Performance] Frame p95={0}ms p99={1}ms hitches={2}; memory={3}MB; actors={4}; ticks={5}" -f `
    $metrics.frameP95Ms, $metrics.frameP99Ms, $metrics.frameHitchesOver33Ms, `
    $metrics.physicalMemoryMaxMB, $metrics.actorCountMax, $metrics.tickFunctionsMax)
if ($Rendered) {
    Write-Host ("[Performance] GPU p95={0}ms p99={1}ms; RT p95={2}ms; draws={3}; VRAM={4}%; budget-p05/max={5}%" -f `
        $metrics.gpuP95Ms, $metrics.gpuP99Ms,
        $metrics.renderThreadP95Ms, $metrics.drawCallsP95,
        $metrics.gpuMemoryUsageMaxPercent,
        $metrics.gpuBudgetP05OfMaxPercent)
    Write-Host ("[Performance] PSO misses={0} compute={1}; hitch-associated={2}/{3}; shaders={4} maps={5}" -f `
        $metrics.psoMisses, $metrics.computePsoMisses, `
        $metrics.psoMissesOnHitch, $metrics.computePsoMissesOnHitch, `
        $metrics.shadersLoadedMax, $metrics.shaderMapsMax)
}
Write-Host "[Performance] Evidence: $summaryReport"
if (-not $passed) {
    $failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object { $_.Key })
    throw "Performance comparison failed (stable settings=$settingsStable; requested settings matched=$requestedSettingsMatched): $($failedChecks -join ', '). Measurements retained: $summaryReport"
}

Write-Host "BH_PERFORMANCE_ALL_CHECKS_PASSED"

}
catch {
    $captureError = $_
    if ($null -eq $summary) {
        $summary = [ordered]@{
            schemaVersion = 2; runId = $runId; runtime = $Runtime; map = $map; passed = $false; captureValid = $false
            budgetStatus = 'notEvaluated'; failure = $captureError.Exception.Message; completion = $completion
            rendererProof = $false; lowerTierProof = $false; networkProof = $false; traversalProof = $false
            metrics = $null; budgets = $null; checks = $null
            runtimeLog = $runtimeLog; profileCsv = $profileReport; runDirectory = $runDirectory; userDirectory = $captureUserDir
            executable = $executable; executableSha256 = $executableHash; arguments = $arguments
        }
    }
    throw
}
finally {
    try { Stop-OwnedPerformanceProcess }
    catch {
        if ($null -ne $summary) { $summary.passed = $false; $summary.cleanupFailure = $_.Exception.Message }
        throw
    }
    finally {
        if ($null -ne $summary) {
            $summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $summaryReport -Encoding UTF8
            Copy-Item -LiteralPath $summaryReport -Destination $compatibilitySummary -Force
        }
    }
}
