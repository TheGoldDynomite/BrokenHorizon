[CmdletBinding()]
param(
    [ValidateRange(1024, 65535)]
    [int]$Port = 0,
    [ValidateRange(30, 9000)]
    [int]$TimeoutSeconds = 120,
    [ValidateRange(600, 1000000)]
    [int]$CaptureFrames = 1800,
    [ValidateRange(60, 1200)]
    [int]$WarmupFrames = 600,
    [ValidateRange(2, 4)]
    [int]$ClientCount = 2,
    [ValidateRange(1, 4)]
    [int]$RenderedClientCount = 2,
    [switch]$SustainedSoak,
    [ValidateRange(570.0, 7200.0)]
    [double]$RequiredSoakSeconds = 570.0,
    [switch]$ProductionRouteHUD,
    [string]$LogPrefix = "BHRenderedMultiplayer"
)

$ErrorActionPreference = "Stop"
if ($WarmupFrames -ge $CaptureFrames) {
    throw "Warmup frames must be lower than capture frames."
}
$captureSeconds = [Math]::Ceiling($CaptureFrames / 60.0) + 5
if ($RenderedClientCount -gt $ClientCount) {
    throw "Rendered client count cannot exceed total client count."
}
if ($RequiredSoakSeconds -gt 570.0 -and -not $SustainedSoak) {
    throw "A required soak duration needs -SustainedSoak."
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath `
    (Join-Path $projectRoot "Config\ProjectManifest.json") |
    ConvertFrom-Json
$uproject = Join-Path $projectRoot $manifest.uproject
$editor = Join-Path `
    $manifest.engineRoot `
    "Engine\Binaries\Win64\UnrealEditor.exe"
$gpuMemoryCapacityMB = 0.0
try {
    $capacityText = & nvidia-smi `
        --query-gpu=memory.total `
        --format=csv,noheader,nounits 2>$null |
        Select-Object -First 1
    $parsedCapacity = 0.0
    if ([double]::TryParse(
            ([string]$capacityText).Trim(),
            [Globalization.NumberStyles]::Float,
            [Globalization.CultureInfo]::InvariantCulture,
            [ref]$parsedCapacity)) {
        $gpuMemoryCapacityMB = $parsedCapacity
    }
} catch {
    $gpuMemoryCapacityMB = 0.0
}
$logDirectory = Join-Path $projectRoot "Saved\Logs"
$reportDirectory = Join-Path $projectRoot "Saved\Reports"
$runId = Get-Date -Format "yyyyMMdd-HHmmss"
if ($Port -eq 0) {
    $Port = 9100 + (Get-Random -Minimum 0 -Maximum 500)
}

New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null

$hostLog = Join-Path $logDirectory "$LogPrefix-$runId-Host.log"
$summaryPath = Join-Path `
    $reportDirectory `
    "$LogPrefix-$runId-Summary.json"
$runRoot = Join-Path `
    $projectRoot `
    "Saved\RenderedMultiplayer\$runId"
$clientLabels = @(
    1..$ClientCount | ForEach-Object {
        "Client$([char](64 + $_))"
    }
)
$clientLogs = @(
    $clientLabels | ForEach-Object {
        Join-Path $logDirectory "$LogPrefix-$runId-$_.log"
    }
)
$failureLogPaths = @($hostLog) + $clientLogs
$clientUserDirectories = @(
    $clientLabels | ForEach-Object {
        Join-Path $runRoot $_
    }
)
$renderedPingScreenshots = @(
    1..$RenderedClientCount | ForEach-Object {
        Join-Path `
            $reportDirectory `
            "$LogPrefix-$runId-Client$([char](64 + $_))-SquadPing.png"
    }
)
$renderedLootHUDScreenshots = @(
    1..$RenderedClientCount | ForEach-Object {
        Join-Path `
            $reportDirectory `
            "$LogPrefix-$runId-Client$([char](64 + $_))-LootHUD.png"
    }
)
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
        if (-not $Process.WaitForExit(5000)) {
            Stop-Process `
                -Id $Process.Id `
                -Force `
                -ErrorAction SilentlyContinue
            $Process.WaitForExit(5000) | Out-Null
        }
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
        "Out of video memory trying to allocate"
    $markerTimeoutSeconds = if ($Label -match "CSV capture") {
        [Math]::Max($TimeoutSeconds, 900)
    } else {
        $TimeoutSeconds
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($markerTimeoutSeconds)
    do {
        foreach ($failureLogPath in $failureLogPaths) {
            if ($failureLogPath -eq $LogPath -or
                -not (Test-Path -LiteralPath $failureLogPath)) {
                continue
            }
            $failureContent = Get-Content -Raw -LiteralPath $failureLogPath
            if ($failureContent -match $failurePattern) {
                throw "$Label encountered a failure marker in $failureLogPath. See $failureLogPath"
            }
        }
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
        "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE result=failure|" +
        "Supply .*BHAmmoSupply_.* has no persistence ID"
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        for ($index = 0; $index -lt $LogPaths.Count; ++$index) {
            $logPath = $LogPaths[$index]
            if (Test-Path -LiteralPath $logPath) {
                $content = Get-Content -Raw -LiteralPath $logPath
                if ($content -match $failurePattern) {
                    throw "$Label encountered a failure marker. See $logPath"
                }
                if ($content -match $Pattern) {
                    return [pscustomobject]@{
                        Index = $index
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

function Wait-ForPng {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if (Test-Path -LiteralPath $Path) {
            $bytes = [IO.File]::ReadAllBytes($Path)
            if ($bytes.Length -ge 24 -and
                $bytes[0] -eq 137 -and
                $bytes[1] -eq 80 -and
                $bytes[2] -eq 78 -and
                $bytes[3] -eq 71) {
                $width = [Net.IPAddress]::NetworkToHostOrder(
                    [BitConverter]::ToInt32($bytes, 16)
                )
                $height = [Net.IPAddress]::NetworkToHostOrder(
                    [BitConverter]::ToInt32($bytes, 20)
                )
                if ($width -ne 1280 -or $height -ne 720) {
                    throw "$Label was ${width}x${height}, expected 1280x720."
                }
                return [pscustomobject]@{
                    path = $Path
                    width = $width
                    height = $height
                }
            }
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "$Label did not produce a valid PNG. Expected $Path"
}

function Get-Percentile {
    param([double[]]$Values, [double]$Percentile)

    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0) {
        throw "Cannot calculate a percentile from an empty sample set."
    }
    $index = [Math]::Min(
        $sorted.Count - 1,
        [Math]::Max(
            0,
            [Math]::Ceiling($sorted.Count * $Percentile) - 1
        )
    )
    return [double]$sorted[$index]
}

function Get-RenderedProfile {
    param(
        [Parameter(Mandatory = $true)][string]$UserDirectory,
        [Parameter(Mandatory = $true)][object]$LogContent,
        [Parameter(Mandatory = $true)][string]$ClientLabel
    )

    $normalizedLogContent = [string]::Join(
        [Environment]::NewLine,
        @($LogContent)
    )

    $csvRoot = Join-Path $UserDirectory "Saved\Profiling\CSV"
    $sourceProfile = Get-ChildItem `
        -LiteralPath $csvRoot `
        -Filter "*.csv" `
        -File `
        -Recurse `
        -ErrorAction Stop |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if (-not $sourceProfile) {
        throw "$ClientLabel did not produce a CSV profile under $csvRoot."
    }

    $profilePath = Join-Path `
        $reportDirectory `
        "$LogPrefix-$runId-$ClientLabel-Profile.csv"
    Copy-Item `
        -LiteralPath $sourceProfile.FullName `
        -Destination $profilePath `
        -Force

    Add-Type -AssemblyName Microsoft.VisualBasic
    $parser = New-Object `
        Microsoft.VisualBasic.FileIO.TextFieldParser($profilePath)
    $parser.TextFieldType =
        [Microsoft.VisualBasic.FileIO.FieldType]::Delimited
    $parser.SetDelimiters(",")
    $parser.HasFieldsEnclosedInQuotes = $true
    $headers = $parser.ReadFields()
    $requiredColumns = @(
        "FrameTime",
        "GPUTime",
        "RenderThreadTime",
        "GPUMem/LocalUsedMB",
        "GPUMem/LocalBudgetMB",
        "RHI/DrawCalls",
        "TextureStreaming/DesiredDataLoadedPercent",
        "TextureStreaming/PendingStreamInData"
    )
    $indices = @{}
    foreach ($column in $requiredColumns) {
        $index = [Array]::IndexOf($headers, $column)
        if ($index -lt 0) {
            $parser.Close()
            throw "$ClientLabel profile is missing '$column'. See $profilePath"
        }
        $indices[$column] = $index
    }

    $frameSamples = [System.Collections.Generic.List[double]]::new()
    $gpuSamples = [System.Collections.Generic.List[double]]::new()
    $renderSamples = [System.Collections.Generic.List[double]]::new()
    $drawCallSamples = [System.Collections.Generic.List[double]]::new()
    $desiredSamples = [System.Collections.Generic.List[double]]::new()
    $pendingSamples = [System.Collections.Generic.List[double]]::new()
    $lastDesiredSamples = [System.Collections.Generic.Queue[double]]::new()
    $numericRowIndex = 0
    $frameSum = 0.0
    $frameMaximum = [double]::NegativeInfinity
    $framesOver50Ms = 0
    $gpuUsedMaximum = [double]::NegativeInfinity
    $gpuBudgetMinimumValue = [double]::PositiveInfinity
    $style = [Globalization.NumberStyles]::Float
    $culture = [Globalization.CultureInfo]::InvariantCulture
    while (-not $parser.EndOfData) {
        $fields = $parser.ReadFields()
        if ($fields.Count -lt $headers.Count) {
            continue
        }

        $frame = 0.0
        $gpu = 0.0
        $renderThread = 0.0
        $gpuUsed = 0.0
        $gpuBudget = 0.0
        $drawCalls = 0.0
        $desired = 0.0
        $pending = 0.0
        if (
            -not [double]::TryParse(
                $fields[$indices["FrameTime"]],
                $style,
                $culture,
                [ref]$frame
            ) -or
            -not [double]::TryParse(
                $fields[$indices["GPUTime"]],
                $style,
                $culture,
                [ref]$gpu
            ) -or
            -not [double]::TryParse(
                $fields[$indices["RenderThreadTime"]],
                $style,
                $culture,
                [ref]$renderThread
            ) -or
            -not [double]::TryParse(
                $fields[$indices["GPUMem/LocalUsedMB"]],
                $style,
                $culture,
                [ref]$gpuUsed
            ) -or
            -not [double]::TryParse(
                $fields[$indices["GPUMem/LocalBudgetMB"]],
                $style,
                $culture,
                [ref]$gpuBudget
            ) -or
            -not [double]::TryParse(
                $fields[$indices["RHI/DrawCalls"]],
                $style,
                $culture,
                [ref]$drawCalls
            ) -or
            -not [double]::TryParse(
                $fields[$indices["TextureStreaming/DesiredDataLoadedPercent"]],
                $style,
                $culture,
                [ref]$desired
            ) -or
            -not [double]::TryParse(
                $fields[$indices["TextureStreaming/PendingStreamInData"]],
                $style,
                $culture,
                [ref]$pending
            )
        ) {
            continue
        }

        if ($numericRowIndex -ge $WarmupFrames) {
            [void]$frameSamples.Add($frame)
            [void]$gpuSamples.Add($gpu)
            [void]$renderSamples.Add($renderThread)
            [void]$drawCallSamples.Add($drawCalls)
            [void]$desiredSamples.Add($desired)
            [void]$pendingSamples.Add($pending)
            if ($lastDesiredSamples.Count -ge 30) {
                [void]$lastDesiredSamples.Dequeue()
            }
            [void]$lastDesiredSamples.Enqueue($desired)

            $frameSum += $frame
            if ($frame -gt $frameMaximum) {
                $frameMaximum = $frame
            }
            if ($frame -gt 50.0) {
                ++$framesOver50Ms
            }
            if ($gpuUsed -gt $gpuUsedMaximum) {
                $gpuUsedMaximum = $gpuUsed
            }
            if ($gpuBudget -gt 0.0 -and $gpuBudget -lt $gpuBudgetMinimumValue) {
                $gpuBudgetMinimumValue = $gpuBudget
            }
        }

        ++$numericRowIndex
        if ($numericRowIndex -ge $CaptureFrames) {
            break
        }
    }
    $parser.Close()

    $minimumSamples = $CaptureFrames - $WarmupFrames
    if ($frameSamples.Count -lt $minimumSamples) {
        throw "$ClientLabel profile contained $($frameSamples.Count) measured frames; expected $minimumSamples."
    }

    $frameValues = $frameSamples.ToArray()
    $gpuValues = $gpuSamples.ToArray()
    $renderValues = $renderSamples.ToArray()
    $drawCallValues = $drawCallSamples.ToArray()
    $desiredValues = $desiredSamples.ToArray()
    $pendingValues = $pendingSamples.ToArray()
    $gpuBudgetMinimum = if ($gpuBudgetMinimumValue -lt [double]::PositiveInfinity) {
        $gpuBudgetMinimumValue
    } else {
        0.0
    }
    $gpuMemoryDenominatorMB = if ($gpuMemoryCapacityMB -gt 0.0) {
        $gpuMemoryCapacityMB
    } else {
        $gpuBudgetMinimum
    }
    $desiredFinal30MinPercent = [double](
        ($lastDesiredSamples | Measure-Object -Minimum).Minimum
    )
    $gpuNameMatch = [regex]::Match(
        $normalizedLogContent,
        "LogRHI:\s+Name:\s+(?<name>[^\r\n]+)"
    )
    $gpuDriverMatch = [regex]::Match(
        $normalizedLogContent,
        "LogRHI:\s+Driver Version:\s+(?<driver>[^\s]+)"
    )

    $metrics = [ordered]@{
        measuredFrames = $frameSamples.Count
        measuredDurationSeconds = [Math]::Round(
            ($frameSum / 1000.0),
            1
        )
        frameP95Ms = [Math]::Round(
            (Get-Percentile $frameValues 0.95),
            3
        )
        frameP99Ms = [Math]::Round(
            (Get-Percentile $frameValues 0.99),
            3
        )
        frameMaxMs = [Math]::Round(
            $frameMaximum,
            3
        )
        framesOver50Ms = $framesOver50Ms
        gpuP95Ms = [Math]::Round(
            (Get-Percentile $gpuValues 0.95),
            3
        )
        gpuP99Ms = [Math]::Round(
            (Get-Percentile $gpuValues 0.99),
            3
        )
        renderThreadP95Ms = [Math]::Round(
            (Get-Percentile $renderValues 0.95),
            3
        )
        drawCallsP95 = [int](Get-Percentile `
            ([double[]]@($drawCallValues)) `
            0.95)
        gpuMemoryUsedMaxMB = [Math]::Round($gpuUsedMaximum, 1)
        gpuMemoryBudgetMinMB = [Math]::Round($gpuBudgetMinimum, 1)
        gpuMemoryCapacityMB = [Math]::Round(
            $gpuMemoryDenominatorMB,
            1
        )
        gpuMemoryUsageMaxPercent = if ($gpuMemoryDenominatorMB -gt 0) {
            [Math]::Round(
                100.0 * $gpuUsedMaximum / $gpuMemoryDenominatorMB,
                1
            )
        } else {
            100.0
        }
        gpuMemoryWddmBudgetUsageMaxPercent =
            if ($gpuBudgetMinimum -gt 0) {
                [Math]::Round(
                    100.0 * $gpuUsedMaximum / $gpuBudgetMinimum,
                    1
                )
            } else {
                100.0
            }
        desiredTextureDataP05Percent = [Math]::Round(
            (Get-Percentile $desiredValues 0.05),
            2
        )
        desiredTextureDataFinal30MinPercent = [Math]::Round(
            $desiredFinal30MinPercent,
            2
        )
        pendingStreamInDataP95 = [Math]::Round(
            (Get-Percentile $pendingValues 0.95),
            3
        )
    }
    $checks = [ordered]@{
        rendererMetadata =
            $gpuNameMatch.Success -and $gpuDriverMatch.Success
        frameP95 = $metrics.frameP95Ms -le 33.33
        frameP99 = $metrics.frameP99Ms -le 40.0
        longHitches = $metrics.framesOver50Ms -eq 0
        gpuP95 = $metrics.gpuP95Ms -le 16.67
        gpuP99 = $metrics.gpuP99Ms -le 20.0
        renderThreadP95 = $metrics.renderThreadP95Ms -le 33.33
        drawCallsP95 = $metrics.drawCallsP95 -le 2000
        gpuMemory = $metrics.gpuMemoryUsageMaxPercent -le 80.0
        desiredTextureP05 =
            $metrics.desiredTextureDataP05Percent -ge 95.0
        desiredTextureRecovered =
            $metrics.desiredTextureDataFinal30MinPercent -ge 99.0
        pendingStreamInP95 =
            $metrics.pendingStreamInDataP95 -le 0.0
    }

    return [ordered]@{
        label = $ClientLabel
        profile = $profilePath
        hardware = [ordered]@{
            resolution = "1280x720"
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
        metrics = $metrics
        checks = $checks
        passed =
            @($checks.Values | Where-Object { -not $_ }).Count -eq 0
    }
}

try {
    $hostArguments = @(
        $uproject,
        $manifest.maps.firstLight,
        "-server",
        "-port=$Port",
        "-nullrhi",
        "-unattended",
        "-nosound",
        "-NoSplash",
        "-DDC-ForceMemoryCache",
        "-abslog=$hostLog",
        "-BHTestSaveSlotSuffix=$runId",
        "-BHTestNetworkBudgetTelemetry",
        "-BHTestNetworkBudgetConnections=$ClientCount",
        "-BHTestNetworkCombatDensity",
        "-BHTestNetworkHostiles=12",
        "-BHTestNetworkFriendlies=4",
        "-BHTestCommitPriorityOperation",
        "-BHTestSquadPingReplication",
        "-BHTestSquadPingRenderedScreenshot"
    )
    if ($ProductionRouteHUD) {
        $hostArguments += @(
            "-BHTestFirstLightPlayableRoute",
            "-BHTestFirstLightPlayableRouteAfterSeconds=30"
        )
    }
    $hostProcess = Start-BHProcess -Arguments $hostArguments
    $hostContent = Wait-ForMarker `
        -LogPath $hostLog `
        -Pattern "BH_WAR_GAME_STATE_READY" `
        -Label "Rendered multiplayer host readiness"

    function Get-ClientArguments {
        param(
            [Parameter(Mandatory = $true)][string]$LogPath,
            [Parameter(Mandatory = $true)][string]$UserDirectory,
            [Parameter(Mandatory = $true)][bool]$Rendered,
            [string]$PingScreenshotPath,
            [string]$LootHUDScreenshotPath
        )

        $arguments = @(
            $uproject,
            "127.0.0.1:$Port",
            "-game",
            "-unattended",
            "-nosound",
            "-NoSplash",
            "-DDC-ForceMemoryCache",
            "-BHTestSaveSlotSuffix=$runId",
            "-DisablePython",
            "-BHTestNetworkCombatDensity",
            "-BHTestNetworkHostiles=12",
            "-BHTestNetworkFriendlies=4",
            "-abslog=$LogPath"
        )
        if ($Rendered) {
            $arguments += @(
                "-RenderOffscreen",
                "-ResX=1280",
                "-ResY=720",
                "-ForceRes",
                "-NoVSync",
                "-csvGpuStats",
                "-benchmark",
                "-fps=60",
                "-seconds=$captureSeconds",
                "-csvCaptureFrames=$CaptureFrames",
                "-userdir=$UserDirectory"
            )
            if ($SustainedSoak) {
                $arguments += '-ExecCmds="t.MaxFPS 60"'
            }
            if (-not [string]::IsNullOrWhiteSpace($PingScreenshotPath)) {
                $arguments +=
                    "-BHTestSquadPingScreenshotPath=$PingScreenshotPath"
            }
            if ($ProductionRouteHUD -and
                -not [string]::IsNullOrWhiteSpace($LootHUDScreenshotPath)) {
                $arguments += @(
                    "-BHTestBattlefieldLootAmmoReplication",
                    "-BHTestBattlefieldLootHUD",
                    "-BHTestBattlefieldLootHUDScreenshotPath=$LootHUDScreenshotPath"
                )
            }
        } else {
            $arguments += "-nullrhi"
        }
        return $arguments
    }

    $clientContents = @()
    for ($clientIndex = 0;
         $clientIndex -lt $ClientCount;
         ++$clientIndex) {
        $isRendered = $clientIndex -lt $RenderedClientCount
        $null = Start-BHProcess -Arguments (Get-ClientArguments `
            -LogPath $clientLogs[$clientIndex] `
            -UserDirectory $clientUserDirectories[$clientIndex] `
            -Rendered $isRendered `
            -PingScreenshotPath $(if ($isRendered -and -not $SustainedSoak) {
                $renderedPingScreenshots[$clientIndex]
            } else { "" }) `
            -LootHUDScreenshotPath $(if ($isRendered) {
                $renderedLootHUDScreenshots[$clientIndex]
            } else { "" }))
        if ($clientIndex -lt $ClientCount - 1) {
            Start-Sleep -Seconds 2
        }
    }

    $snapshotPattern = "BH_WAR_SNAPSHOT_APPLIED"
    $densityPattern =
        "BH_NET_COMBAT_DENSITY_REPLICATED result=success .*total=19"
    $pingPattern =
        "BH_SQUAD_PING_APPLIED revision=1 context=HOSTILE issuer=HOST_FIXTURE"
    for ($clientIndex = 0;
         $clientIndex -lt $ClientCount;
         ++$clientIndex) {
        $label = $clientLabels[$clientIndex]
        $content = Wait-ForMarker `
            -LogPath $clientLogs[$clientIndex] `
            -Pattern $snapshotPattern `
            -Label "$label war snapshot"
        $content = Wait-ForMarker `
            -LogPath $clientLogs[$clientIndex] `
            -Pattern $densityPattern `
            -Label "$label combat density"
        $content = Wait-ForMarker `
            -LogPath $clientLogs[$clientIndex] `
            -Pattern $pingPattern `
            -Label "$label squad ping"
        if ($clientIndex -lt $RenderedClientCount -and -not $SustainedSoak) {
            $content = Wait-ForMarker `
                -LogPath $clientLogs[$clientIndex] `
                -Pattern (
                    "BH_SQUAD_PING_SCREENSHOT result=requested " +
                    "revision=1 context=HOSTILE issuer=HOST_FIXTURE"
                ) `
                -Label "$label squad ping screenshot request"
        } elseif ($clientIndex -lt $RenderedClientCount) {
            $content = Wait-ForMarker `
                -LogPath $clientLogs[$clientIndex] `
                -Pattern (
                    "BH_SQUAD_PING_PRESENTATION revision=1 " +
                    "tracked=1"
                ) `
                -Label "$label live squad ping presentation"
        }
        $clientContents += $content
    }
    $lootHUDEvidence = $null
    if ($ProductionRouteHUD) {
        $hostContent = Wait-ForMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_FIRST_LIGHT_PLAYABLE_ROUTE_COMPLETE result=success objectives=4 players=$ClientCount completed=$ClientCount" `
            -Label "Rendered multiplayer production First Light route"
        $lootHUDOwner = Wait-ForAnyMarker `
            -LogPaths $clientLogs[0..($RenderedClientCount - 1)] `
            -Pattern 'BH_BATTLEFIELD_LOOT_HUD_UPDATED result=success magazine=30 reserve=180' `
            -Label "Rendered owning-client battlefield-loot HUD"
        $lootHUDEvidence = Wait-ForPng `
            -Path $renderedLootHUDScreenshots[$lootHUDOwner.Index] `
            -Label "$($clientLabels[$lootHUDOwner.Index]) battlefield-loot HUD screenshot"
        $lootHUDEvidence | Add-Member `
            -NotePropertyName ownerClient `
            -NotePropertyValue $clientLabels[$lootHUDOwner.Index]
    }
    $hostContent = Wait-ForMarker `
        -LogPath $hostLog `
        -Pattern "BH_NET_BUDGET_WINDOW_READY samples=10 connections=$ClientCount" `
        -Label "Rendered multiplayer network budget window"
    $renderedClientEvidence = @()
    for ($clientIndex = 0;
         $clientIndex -lt $RenderedClientCount;
         ++$clientIndex) {
        $label = $clientLabels[$clientIndex]
        $clientContents[$clientIndex] = Wait-ForMarker `
            -LogPath $clientLogs[$clientIndex] `
            -Pattern "LogCsvProfiler: Display: Capture Ended" `
            -Label "$label CSV capture"
        $profileEvidence = [pscustomobject](
            Get-RenderedProfile `
                -UserDirectory $clientUserDirectories[$clientIndex] `
                -LogContent $clientContents[$clientIndex] `
                -ClientLabel $label
        )
        $screenshotEvidence = if ($SustainedSoak) {
            $null
        } else {
            Wait-ForPng `
                -Path $renderedPingScreenshots[$clientIndex] `
                -Label "$label squad ping screenshot"
        }
        $profileEvidence | Add-Member `
            -NotePropertyName squadPingScreenshot `
            -NotePropertyValue $screenshotEvidence
        $profileEvidence | Add-Member `
            -NotePropertyName squadPingScreenshotProof `
            -NotePropertyValue (-not $SustainedSoak)
        $renderedClientEvidence += $profileEvidence
    }

    $minimumMeasuredSoakFrames = if ($SustainedSoak -and $RequiredSoakSeconds -gt 570.0) {
        $CaptureFrames - $WarmupFrames
    } else {
        36000
    }
    $minimumMeasuredSoakSeconds = if ($SustainedSoak) {
        $RequiredSoakSeconds
    } else {
        0.0
    }
    $soakProof = -not $SustainedSoak -or
        @(
            $renderedClientEvidence |
                Where-Object {
                    [int]$_.metrics.measuredFrames -lt
                        $minimumMeasuredSoakFrames -or
                    [double]$_.metrics.measuredDurationSeconds -lt
                        $minimumMeasuredSoakSeconds
                }
        ).Count -eq 0

    $summary = [ordered]@{
        schemaVersion = 1
        generatedAtUtc = [DateTime]::UtcNow.ToString("o")
        result = if (@(
            $renderedClientEvidence |
                Where-Object { -not $_.passed }
        ).Count -eq 0) { "Passed" } else { "Failed" }
        mode = if ($SustainedSoak) {
            "DedicatedServer${ClientCount}RenderedClientsCombatDensitySoak"
        } elseif ($ClientCount -eq $RenderedClientCount) {
            "DedicatedServer${ClientCount}RenderedClientsCombatDensity"
        } else {
            "DedicatedServer${ClientCount}Clients${RenderedClientCount}RenderedCombatDensity"
        }
        map = $manifest.maps.firstLight
        port = $Port
        captureFramesPerClient = $CaptureFrames
        warmupFramesPerClient = $WarmupFrames
        rendererProof = $true
        networkProof = $true
        sustainedSoakRequested = [bool]$SustainedSoak
        requiredSoakSeconds = if ($SustainedSoak) {
            $RequiredSoakSeconds
        } else { 0.0 }
        sustainedSoakProof = $soakProof
        minimumMeasuredSoakFrames = if ($SustainedSoak) {
            $minimumMeasuredSoakFrames
        } else { 0 }
        minimumMeasuredSoakSeconds = if ($SustainedSoak) {
            $minimumMeasuredSoakSeconds
        } else { 0.0 }
        dedicatedAuthority = $true
        connectedPlayers = $ClientCount
        renderedPlayers = $RenderedClientCount
        gpuMemoryCapacityMB = $gpuMemoryCapacityMB
        combatDensity = [ordered]@{
            hostileAI = 12
            friendlyAI = 4
            totalObservedAI = 19
        }
        replication = [ordered]@{
            warSnapshot = $true
            combatDensity = $true
            squadPing = $true
            squadPingRendered = $true
            productionRoute = [bool]$ProductionRouteHUD
            battlefieldLootHUD = [bool]$ProductionRouteHUD
        }
        battlefieldLootHUD = $lootHUDEvidence
        networkBudgetWindowSamples = 10
        clients = $renderedClientEvidence
        hostLog = $hostLog
        clientLogs = $clientLogs
        limitations = @(
            "Localhost editor clients do not prove geographic Internet behavior.",
            "GPU-memory acceptance uses detected physical capacity; each client's fluctuating WDDM local budget is retained as a diagnostic metric.",
            "Offscreen 720p evidence does not replace visible multi-client UI and image-quality review.",
            $(if ($SustainedSoak) {
                if ($RequiredSoakSeconds -gt 570.0) {
                    "This rendered soak is not packaged-build or geographic-network proof."
                } else {
                    "This ten-minute rendered soak is not a two-hour rendered or packaged-build proof."
                }
            } else {
                "This bounded First Light combat-density window is not a long-session rendered soak or packaged-build proof."
            })
        )
    }
    $summary | ConvertTo-Json -Depth 10 |
        Set-Content -LiteralPath $summaryPath

    if ($summary.result -ne "Passed") {
        $failed = @(
            $summary.clients |
                ForEach-Object {
                    $label = $_.label
                    $_.checks.GetEnumerator() |
                        Where-Object { -not $_.Value } |
                        ForEach-Object { "$label/$($_.Key)" }
                }
        )
        throw "Rendered multiplayer budgets failed: $($failed -join ', '). See $summaryPath"
    }
    if (-not $soakProof) {
        throw "Rendered multiplayer soak did not meet the sustained frame/time contract. See $summaryPath"
    }

    $metricSummary = @(
        $renderedClientEvidence | ForEach-Object {
            "{0} frame/GPU p95={1}/{2}ms" -f `
                $_.label,
                $_.metrics.frameP95Ms,
                $_.metrics.gpuP95Ms
        }
    ) -join "; "
    Write-Host (
        "Rendered multiplayer passed: $ClientCount connected players; " +
        $metricSummary
    )
    Write-Host "Evidence: $summaryPath"
}
finally {
    foreach ($process in $launchedProcesses) {
        Stop-OwnedProcess -Process $process
    }
}
