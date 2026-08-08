[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$LogPrefix = "BHPerformance",
    [int]$CaptureFrames = 600,
    [int]$WarmupFrames = 180,
    [double]$FrameP95BudgetMs = 8.0,
    [double]$FrameP99BudgetMs = 16.67,
    [int]$FrameHitchBudget = 1,
    [double]$PhysicalMemoryBudgetMB = 4096,
    [int]$ActorBudget = 500,
    [int]$TickFunctionBudget = 500,
    [switch]$Rendered,
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
$runtimeLog = Join-Path $logDirectory "$LogPrefix.log"
$profileReport = Join-Path $reportDirectory "$LogPrefix-Profile.csv"
$summaryReport = Join-Path $reportDirectory "$LogPrefix-Summary.json"
$captureUserDir = Join-Path $projectRoot "Saved\PerformanceUser\$LogPrefix"
$csvRoot = if ($Rendered) {
    Join-Path $env:LOCALAPPDATA "UnrealEngine\5.8\Saved\Profiling\CSV"
} else {
    Join-Path $captureUserDir "Saved\Profiling\CSV"
}

New-Item -ItemType Directory -Force -Path $logDirectory, $reportDirectory | Out-Null
if (-not $Rendered) {
    New-Item -ItemType Directory -Force -Path $csvRoot | Out-Null
}
Remove-Item -LiteralPath $runtimeLog, $profileReport, $summaryReport -Force -ErrorAction SilentlyContinue

if (-not (Test-Path -LiteralPath $editor)) {
    throw "Unreal Editor was not found: $editor"
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
$captureSeconds = [Math]::Ceiling($CaptureFrames / 60.0) + 5
$arguments = @(
    $uproject,
    $map,
    "-game",
    "-unattended",
    "-nosound",
    "-NoSplash",
    "-DDC-ForceMemoryCache",
    "-benchmark",
    "-fps=60",
    "-seconds=$captureSeconds",
    "-csvCaptureFrames=$CaptureFrames",
    "-abslog=$runtimeLog"
)
if ($Rendered) {
    $arguments += @(
        "-csvGpuStats",
        "-RenderOffscreen",
        "-ResX=1920",
        "-ResY=1080",
        "-ForceRes"
    )
} else {
    $arguments += "-nullrhi"
    $arguments += "-userdir=$captureUserDir"
}
if ($WorldTraversal) {
    $arguments += "-BHTestRenderedWorldTraversal"
} elseif ($Traversal) {
    $arguments += "-BHTestRenderedTraversal"
}

Write-Host "[Performance] Capturing $CaptureFrames frames on $map ($WarmupFrames warmup; rendered=$Rendered; worldTraversal=$WorldTraversal)"
# Keep profiler, config, and save writes inside the project so the gate is
# deterministic in restricted CI/agent environments.
$process = Start-Process -FilePath $editor -ArgumentList $arguments -PassThru -Wait
if ($process.ExitCode -ne 0) {
    throw "Performance capture failed with exit code $($process.ExitCode). See $runtimeLog"
}
if (-not (Test-Path -LiteralPath $runtimeLog)) {
    throw "Performance capture did not produce its runtime log: $runtimeLog"
}

$logContent = Get-Content -Raw -LiteralPath $runtimeLog
$failurePatterns = @("Fatal error:", "Assertion failed:", "Unhandled Exception:", "Failed to load package", "CreateExport: Failed to load")
if ($WorldTraversal) {
    $failurePatterns += "Navmesh bounds are too large!"
}
$failures = @($failurePatterns | Where-Object { $logContent -match [regex]::Escape($_) })
if ($failures.Count -gt 0) {
    throw "Performance log contains failure markers: $($failures -join ', '). See $runtimeLog"
}
if ($logContent -notmatch "LogCsvProfiler: Display: Capture Ended") {
    throw "Performance capture did not reach the CSV completion marker. See $runtimeLog"
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

$sourceProfile = Get-ChildItem -LiteralPath $csvRoot -Filter "*.csv" -File -ErrorAction Stop |
    Where-Object { $_.LastWriteTimeUtc -ge $captureStart.AddSeconds(-2) } |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1
if (-not $sourceProfile) {
    throw "Unreal did not produce a CSV profile under $csvRoot"
}
Copy-Item -LiteralPath $sourceProfile.FullName -Destination $profileReport -Force

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

$samples = New-Object System.Collections.Generic.List[object]
$rowIndex = 0
while (-not $parser.EndOfData) {
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
    if ($rowIndex -ge $WarmupFrames -and
        (-not $WorldTraversal -or $worldTraversalMeasure -ge 0.5)) {
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

$minimumSamples = if ($WorldTraversal) {
    # The custom CSV stat is emitted on each 0.1 s fixture movement tick.
    # Nineteen sampled movement frames across each of eight sectors are the
    # complete marked acceptance set; loading/recovery dwell frames stay out.
    152
} else {
    $CaptureFrames - $WarmupFrames
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
$gpuNameMatch = [regex]::Match(
    $logContent,
    "LogRHI:\s+Name:\s+(?<name>[^\r\n]+)"
)
$gpuDriverMatch = [regex]::Match(
    $logContent,
    "LogRHI:\s+Driver Version:\s+(?<driver>[^\s]+)"
)
$summary = [ordered]@{
    schemaVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    mode = if ($WorldTraversal) {
        "EditorD3D12Offscreen1080pWorldTraversal"
    } elseif ($Traversal) {
        "EditorD3D12Offscreen1080pTraversal"
    } elseif ($Rendered) {
        "EditorD3D12Offscreen1080p"
    } else {
        "EditorNullRHI"
    }
    captureFrames = $CaptureFrames
    warmupFrames = $WarmupFrames
    rendererProof = [bool]$Rendered
    traversalProof = [bool]$Traversal
    worldMapProof = [bool]$WorldMap
    worldTraversalProof = [bool]$WorldTraversal
    navigationProof = [bool]($WorldTraversal -and $navigationStepCount -eq 8)
    map = $map
    traversalSteps = if ($Traversal) { $traversalStepCount } else { 0 }
    navigationSteps = $navigationStepCount
    networkProof = $false
    budgets = $budgets
    metrics = $metrics
    checks = $checks
    passed = $passed
    hardware = if ($Rendered) {
        [ordered]@{
            resolution = "1920x1080"
            rhi = "D3D12"
            gpu = if ($gpuNameMatch.Success) {
                $gpuNameMatch.Groups["name"].Value.Trim()
            } else {
                "Unknown"
            }
            driver = if ($gpuDriverMatch.Success) {
                $gpuDriverMatch.Groups["driver"].Value.Trim()
            } else {
                "Unknown"
            }
        }
    } else {
        $null
    }
    limitations = if ($Rendered) {
        @(
            "Offscreen editor D3D12 capture validates this machine and content path, not every target hardware tier or a packaged build.",
            "Standalone capture does not validate multiplayer bandwidth, replication saturation, travel, reconnect, or soak stability.",
            "PSO counters cover the measured warm-cache editor window; cold-cache and packaged pipeline-cache stutter remain separate release gates.",
            "Automated counters do not replace manual review for visible mip transitions, traversal hitches, or image quality."
        )
    } else {
        @(
            "NullRHI does not validate GPU frame time, draw calls, lighting, material cost, texture quality, or streaming presentation.",
            "Standalone capture does not validate multiplayer bandwidth, replication saturation, travel, reconnect, or soak stability."
        )
    }
    runtimeLog = $runtimeLog
    profileCsv = $profileReport
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryReport -Encoding UTF8

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
    throw "Performance budget failed: $($failedChecks -join ', '). See $summaryReport"
}

Write-Host "BH_PERFORMANCE_ALL_CHECKS_PASSED"
