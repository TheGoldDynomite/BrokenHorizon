[CmdletBinding()]
param(
    [ValidateRange(1024, 65535)]
    [int]$Port = 0,
    [ValidateRange(5, 120)]
    [int]$ConnectionTimeoutSeconds = 45,
    [ValidateRange(0, 1000)]
    [int]$PacketLagMilliseconds = 0,
    [ValidateRange(0, 25)]
    [int]$PacketLossPercent = 0,
    [ValidateRange(0, 7200)]
    [int]$SoakSeconds = 0,
    [ValidateRange(1024, 10485760)]
    [int]$AggregateOutBudgetBytesPerSecond = 65536,
    [ValidateRange(1024, 10485760)]
    [int]$PerConnectionOutBudgetBytesPerSecond = 32768,
    [ValidateRange(1, 1024)]
    [int]$PerConnectionChannelBudget = 64,
    [ValidateRange(0, 1000)]
    [int]$AllowedObservedPacketLossPerSample = 0,
    [ValidateRange(2, 16)]
    [int]$NetworkBudgetClientCount = 2,
    [ValidateSet("Priority", "Attack", "Defend", "Raid", "Resupply")]
    [string]$OperationType = "Priority",
    [ValidateRange(0, 10000)]
    [int]$LegacySchemaVersion = 0,
    [string]$LogPrefix = "G3-Multiplayer",
    [switch]$RequireActiveOperation,
    [switch]$RequireWatercraftDelivery,
    [switch]$RequireCivilianAidDelivery,
    [switch]$RequireMissionCacheTransfer,
    [switch]$RequireArmoredThreatTargeting,
    [switch]$RequireServerTravel,
    [switch]$RequireTransportPersistence,
    [switch]$RequireTransportCasualtyPersistence,
    [switch]$RequireOperationCompletion,
    [switch]$RequireOperationFailure,
    [switch]$RequireContextOwnership,
    [switch]$RequireMedicalRecoveryReplication,
    [switch]$RequireCustomDifficultyReplication,
    [switch]$RequireSquadPingReplication,
    [switch]$RequireNetworkBudget,
    [switch]$RequireNetworkCombatDensity,
    [switch]$RequireHostRecovery,
    [switch]$RequireCorruptPrimaryRecovery
)

$ErrorActionPreference = "Stop"

if ($RequireTransportPersistence -and -not $RequireServerTravel) {
    throw "Transport persistence validation requires -RequireServerTravel."
}
if ($RequireOperationCompletion -and -not $RequireTransportPersistence) {
    throw "Operation completion validation requires transport persistence travel."
}
if ($RequireOperationFailure -and -not $RequireActiveOperation) {
    throw "Operation failure validation requires -RequireActiveOperation."
}
if ($RequireOperationFailure -and
    $OperationType -notin @("Attack", "Defend", "Raid")) {
    throw "Operation failure validation requires a tactical operation type."
}
if ($RequireOperationFailure -and $RequireServerTravel) {
    throw "Operation failure validation is a reconnect fixture, not a travel fixture."
}
if ($RequireHostRecovery -and -not $RequireTransportPersistence) {
    throw "Host recovery validation requires transport persistence travel."
}
if ($RequireTransportCasualtyPersistence -and
    -not $RequireTransportPersistence) {
    throw "Transport casualty validation requires transport persistence travel."
}
if ($RequireCorruptPrimaryRecovery -and -not $RequireHostRecovery) {
    throw "Corrupt-primary recovery validation requires -RequireHostRecovery."
}
if ($LegacySchemaVersion -gt 0 -and -not $RequireHostRecovery) {
    throw "Legacy migration validation requires -RequireHostRecovery."
}
if ($LegacySchemaVersion -gt 0 -and $RequireCorruptPrimaryRecovery) {
    throw "Legacy migration and protected corruption are separate fixtures."
}
if ($OperationType -ne "Priority" -and -not $RequireActiveOperation) {
    throw "A specific operation type requires -RequireActiveOperation."
}
if ($RequireNetworkCombatDensity -and -not $RequireNetworkBudget) {
    throw "Network combat density requires -RequireNetworkBudget."
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $projectRoot "Config\ProjectManifest.json"
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$uproject = Join-Path $projectRoot $manifest.uproject
$editor = Join-Path $manifest.engineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$logDirectory = Join-Path $projectRoot "Saved\Logs"
$firstLightMap = $manifest.maps.firstLight
$testMap = if (($RequireTransportPersistence -or $SoakSeconds -gt 0) -and
    -not ($RequireActiveOperation -and $OperationType -eq "Attack")) {
    $manifest.maps.openWorld
} else {
    $firstLightMap
}
$testMapShortName = ($testMap -split '/')[-1]

if ($Port -eq 0) {
    $Port = 7800 + (Get-Random -Minimum 0 -Maximum 800)
}

New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

$runId = Get-Date -Format "yyyyMMdd-HHmmss"
$safeRunId = $runId -replace '-', '_'
$saveDirectory = Join-Path $projectRoot "Saved\SaveGames"
$primarySavePath = Join-Path $saveDirectory `
    "BrokenHorizon_Checkpoint_$safeRunId.sav"
$backupSavePath = Join-Path $saveDirectory `
    "BrokenHorizon_Checkpoint_Backup_$safeRunId.sav"
$hostLog = Join-Path $logDirectory "$LogPrefix-$runId-Host.log"
$initialClientLog = Join-Path $logDirectory "$LogPrefix-$runId-ClientA.log"
$secondClientLog = Join-Path $logDirectory "$LogPrefix-$runId-ClientB.log"
$rejoinClientLog = Join-Path $logDirectory "$LogPrefix-$runId-RejoinA.log"
$additionalClientLogs = [System.Collections.Generic.List[string]]::new()
$summaryPath = Join-Path $logDirectory "$LogPrefix-$runId-Summary.json"
$crashedHostLog = Join-Path $logDirectory "$LogPrefix-$runId-CrashedHost.log"
$crashedClientALog = Join-Path $logDirectory "$LogPrefix-$runId-CrashedClientA.log"
$crashedClientBLog = Join-Path $logDirectory "$LogPrefix-$runId-CrashedClientB.log"
$recoveredHostLog = $hostLog
$recoveredClientALog = $initialClientLog
$recoveredClientBLog = $secondClientLog
if ($RequireHostRecovery) {
    $hostLog = $crashedHostLog
    $initialClientLog = $crashedClientALog
    $secondClientLog = $crashedClientBLog
}
$launchedProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()

function Start-BHProcess {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $process = Start-Process `
        -FilePath $editor `
        -ArgumentList $Arguments `
        -WindowStyle Hidden `
        -PassThru
    $launchedProcesses.Add($process)
    return $process
}

function Wait-ForLogMarker {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $deadline = (Get-Date).AddSeconds($ConnectionTimeoutSeconds)
    do {
        if (Test-Path -LiteralPath $LogPath) {
            $content = Get-Content -Raw -LiteralPath $LogPath
            if ($content -match "Fatal error:|Assertion failed:|Unhandled Exception:|NetworkFailure|TravelFailure|PIE: Error:|Checkpoint save failed|BH_(WAR|FIELD)_AUTOSAVE_FAILED") {
                throw "$Label encountered a failure marker. See $LogPath"
            }
            if ($content -match $Pattern) {
                return $content
            }
        }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    throw "$Label timed out waiting for '$Pattern'. See $LogPath"
}

function Wait-ForLogMarkerCount {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][int]$Count,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $deadline = (Get-Date).AddSeconds($ConnectionTimeoutSeconds)
    do {
        if (Test-Path -LiteralPath $LogPath) {
            $content = Get-Content -Raw -LiteralPath $LogPath
            $matches = [regex]::Matches($content, $Pattern)
            if ($content -match "Fatal error:|Assertion failed:|Unhandled Exception:|NetworkFailure|TravelFailure|PIE: Error:|Checkpoint save failed|BH_(WAR|FIELD)_AUTOSAVE_FAILED") {
                throw "$Label encountered a failure marker. See $LogPath"
            }
            if ($matches.Count -ge $Count) {
                return $content
            }
        }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    throw "$Label timed out waiting for $Count occurrence(s) of '$Pattern'. See $LogPath"
}

function Stop-OwnedProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -ne $Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id
        if (-not $Process.WaitForExit(5000)) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            $Process.WaitForExit(5000) | Out-Null
        }
    }
}

function Get-SnapshotSignature {
    param([Parameter(Mandatory = $true)][string]$Content)

    $pattern = "BH_WAR_SNAPSHOT_APPLIED revision=(?<revision>\d+) turn=(?<turn>\d+) sectors=(?<sectors>\d+) operation_id=(?<operation>\S+)"
    $snapshotMatches = [regex]::Matches($Content, $pattern)
    if ($snapshotMatches.Count -le 0) {
        throw "A client log did not contain a complete war snapshot signature."
    }

    $latest = $snapshotMatches[$snapshotMatches.Count - 1]
    return "$($latest.Groups['revision'].Value):$($latest.Groups['turn'].Value):$($latest.Groups['sectors'].Value):$($latest.Groups['operation'].Value)"
}

function Get-Percentile {
    param(
        [Parameter(Mandatory = $true)][double[]]$Values,
        [Parameter(Mandatory = $true)][double]$Percentile
    )

    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0) {
        throw "Cannot calculate a percentile from an empty sample set."
    }
    $index = [Math]::Min(
        $sorted.Count - 1,
        [Math]::Max(0, [Math]::Ceiling($sorted.Count * $Percentile) - 1)
    )
    return [double]$sorted[$index]
}

function Invoke-BHSoak {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process[]]$Processes,
        [Parameter(Mandatory = $true)]
        [string[]]$LogPaths,
        [Parameter(Mandatory = $true)]
        [int]$DurationSeconds
    )

    if ($DurationSeconds -le 0) {
        return
    }

    $failurePattern =
        "Fatal error:|Assertion failed:|Unhandled Exception:|NetworkFailure|TravelFailure|PIE: Error:|Checkpoint save failed|BH_(WAR|FIELD)_AUTOSAVE_FAILED"
    $deadline = [DateTime]::UtcNow.AddSeconds($DurationSeconds)
    $nextProgress = [DateTime]::UtcNow.AddSeconds(300)
    while ([DateTime]::UtcNow -lt $deadline) {
        foreach ($process in $Processes) {
            if ($null -eq $process -or $process.HasExited) {
                throw "A required multiplayer process exited during the soak."
            }
        }
        foreach ($logPath in $LogPaths) {
            if (Test-Path -LiteralPath $logPath) {
                $content = Get-Content -Raw -LiteralPath $logPath
                if ($content -match $failurePattern) {
                    throw "A multiplayer soak log contains a failure marker: $logPath"
                }
            }
        }
        if ([DateTime]::UtcNow -ge $nextProgress) {
            $remaining = [Math]::Max(
                0,
                [int]($deadline - [DateTime]::UtcNow).TotalSeconds
            )
            Write-Host "Multiplayer soak healthy; $remaining seconds remain."
            $nextProgress = $nextProgress.AddSeconds(300)
        }
        Start-Sleep -Seconds 1
    }
}

function Wait-ForMatchingSnapshotSignatures {
    param(
        [Parameter(Mandatory = $true)][string]$FirstLogPath,
        [Parameter(Mandatory = $true)][string]$SecondLogPath,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $deadline = (Get-Date).AddSeconds($ConnectionTimeoutSeconds)
    do {
        if ((Test-Path -LiteralPath $FirstLogPath) -and
            (Test-Path -LiteralPath $SecondLogPath)) {
            $firstContent = Get-Content -Raw -LiteralPath $FirstLogPath
            $secondContent = Get-Content -Raw -LiteralPath $SecondLogPath
            $firstSignature = Get-SnapshotSignature -Content $firstContent
            $secondSignature = Get-SnapshotSignature -Content $secondContent
            if ($firstSignature -eq $secondSignature) {
                return [ordered]@{
                    firstContent = $firstContent
                    secondContent = $secondContent
                    signature = $firstSignature
                }
            }
        }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    throw "$Label did not converge on one replicated war snapshot."
}

try {
    $commonArguments = @(
        "-nullrhi",
        "-unattended",
        "-nosound",
        "-NoSplash",
        "-DDC-ForceMemoryCache"
    )
    if ($RequireTransportPersistence) {
        $commonArguments += "-BHTestTransportTravelPersistence"
    }
    if ($RequireTransportCasualtyPersistence) {
        $commonArguments += "-BHTestTransportCasualtyPersistence"
    }
    if ($RequireOperationFailure) {
        $commonArguments += "-BHTestOperationFailure"
    }
    if ($RequireMedicalRecoveryReplication) {
        $commonArguments += "-BHTestMedicalRecoveryReplication"
    }
    if ($RequireCustomDifficultyReplication) {
        $commonArguments += "-BHTestCustomDifficultyReplication"
    }
    if ($RequireSquadPingReplication) {
        $commonArguments += "-BHTestSquadPingReplication"
    }
    if ($RequireNetworkBudget) {
        $commonArguments += "-BHTestNetworkBudgetTelemetry"
        $commonArguments +=
            "-BHTestNetworkBudgetConnections=$NetworkBudgetClientCount"
    }
    if ($RequireNetworkCombatDensity) {
        $commonArguments += "-BHTestNetworkCombatDensity"
        $commonArguments += "-BHTestNetworkHostiles=12"
        $commonArguments += "-BHTestNetworkFriendlies=4"
    }
    if ($PacketLagMilliseconds -gt 0) {
        $commonArguments += "-PktLag=$PacketLagMilliseconds"
    }
    if ($PacketLossPercent -gt 0) {
        $commonArguments += "-PktLoss=$PacketLossPercent"
    }

    $hostArguments = @(
        $uproject,
        $testMap,
        "-server",
        "-port=$Port",
        "-abslog=$hostLog"
    ) + $commonArguments
    if ($RequireTransportPersistence -or
        $RequireMissionCacheTransfer -or
        $RequireActiveOperation -or
        $SoakSeconds -gt 0)
    {
        $hostArguments += "-BHTestSaveSlotSuffix=$runId"
    }
    if ($RequireOperationCompletion) {
        $hostArguments += "-BHTestOperationCompletionAfterTravel"
    }
    if ($RequireHostRecovery) {
        $hostArguments += "-BHTestPrepareCrashRecovery"
    }
    if ($LegacySchemaVersion -gt 0) {
        $hostArguments += "-BHTestLegacySaveSchema=$LegacySchemaVersion"
    }
    if ($RequireActiveOperation) {
        $hostArguments += "-BHTestCommitPriorityOperation"
        if ($OperationType -ne "Priority") {
            $hostArguments += "-BHTestOperationType=$OperationType"
        }
        if ($OperationType -eq "Attack") {
            $hostArguments += "-BHTestForceAttackTarget"
        }
        if ($RequireOperationFailure -or
            $RequireTransportPersistence -or
            $OperationType -eq "Attack") {
            $hostArguments += "-BHTestBeginCommittedOperation"
        }
    }
    if ($RequireWatercraftDelivery) {
        $hostArguments += "-BHTestWatercraftDeliveryWhenOccupied"
    }
    if ($RequireCivilianAidDelivery) {
        $hostArguments += "-BHTestCivilianAidDeliveryWhenOccupied"
    }
    if ($RequireMissionCacheTransfer) {
        $hostArguments += "-BHTestMissionCacheTransferRuntime"
    }
    if ($RequireArmoredThreatTargeting) {
        $hostArguments += "-BHTestArmoredThreatTargetingRuntime"
    }
    if ($RequireContextOwnership) {
        $hostArguments += "-BHTestFieldSquadContextOwnership"
    }
    if ($RequireServerTravel) {
        $travelDelaySeconds = 20
        $hostArguments += "-BHTestServerTravelAfterSeconds=$travelDelaySeconds"
    }
    $hostProcess = Start-BHProcess -Arguments $hostArguments

    $hostContent = Wait-ForLogMarker `
        -LogPath $hostLog `
        -Pattern "BH_WAR_GAME_STATE_READY" `
        -Label "Dedicated host readiness"
    $expectedOperationTypeValue = switch ($OperationType) {
        "Attack" { 1 }
        "Defend" { 2 }
        "Raid" { 3 }
        "Resupply" { 4 }
        default { $null }
    }

    $initialClientArguments = @(
        $uproject,
        "127.0.0.1:$Port",
        "-game",
        "-abslog=$initialClientLog"
    ) + $commonArguments
    $initialClient = Start-BHProcess -Arguments $initialClientArguments
    Start-Sleep -Seconds 2

    $secondClientArguments = @(
        $uproject,
        "127.0.0.1:$Port",
        "-game",
        "-abslog=$secondClientLog"
    ) + $commonArguments
    $secondClient = Start-BHProcess -Arguments $secondClientArguments

    $initialContent = Wait-ForLogMarker `
        -LogPath $initialClientLog `
        -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
        -Label "Initial client snapshot"
    $secondContent = Wait-ForLogMarker `
        -LogPath $secondClientLog `
        -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
        -Label "Second client snapshot"

    $additionalClientContents = @()
    if ($RequireNetworkBudget -and $NetworkBudgetClientCount -gt 2) {
        for ($clientIndex = 3;
             $clientIndex -le $NetworkBudgetClientCount;
             ++$clientIndex)
        {
            $additionalLog = Join-Path `
                $logDirectory `
                "$LogPrefix-$runId-Client$clientIndex.log"
            $additionalClientLogs.Add($additionalLog)
            $additionalClient = Start-BHProcess -Arguments (@(
                $uproject,
                "127.0.0.1:$Port",
                "-game",
                "-abslog=$additionalLog"
            ) + $commonArguments)
            $additionalClientContents += Wait-ForLogMarker `
                -LogPath $additionalLog `
                -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
                -Label "Scale Client $clientIndex snapshot"
        }
    }

    $networkBudgetMetrics = $null
    $networkBudgetChecks = $null
    if ($RequireNetworkBudget) {
        if ($RequireNetworkCombatDensity) {
            $hostContent = Wait-ForLogMarker `
                -LogPath $hostLog `
                -Pattern "BH_NET_COMBAT_DENSITY_CONFIGURED result=success hostiles=12 friendlies=4 total=16" `
                -Label "Bounded network combat-density fixture"
            $combatDensityReplicationPattern =
                "BH_NET_COMBAT_DENSITY_REPLICATED result=success hostiles=\d+ friendlies=\d+ total=\d+"
            $initialContent = Wait-ForLogMarker `
                -LogPath $initialClientLog `
                -Pattern $combatDensityReplicationPattern `
                -Label "Initial client combat-density replication"
            $secondContent = Wait-ForLogMarker `
                -LogPath $secondClientLog `
                -Pattern $combatDensityReplicationPattern `
                -Label "Second client combat-density replication"
            for ($additionalIndex = 0;
                 $additionalIndex -lt $additionalClientLogs.Count;
                 ++$additionalIndex)
            {
                $additionalClientContents[$additionalIndex] =
                    Wait-ForLogMarker `
                        -LogPath $additionalClientLogs[$additionalIndex] `
                        -Pattern $combatDensityReplicationPattern `
                        -Label "Scale Client $($additionalIndex + 3) combat-density replication"
            }
        }
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_NET_BUDGET_WINDOW_READY samples=10 connections=$NetworkBudgetClientCount" `
            -Label "$NetworkBudgetClientCount-client network budget window"

        $samplePattern =
            "BH_NET_BUDGET_SAMPLE sample=(?<sample>\d+) connections=(?<connections>\d+) " +
            "driver_in_bps=(?<driverIn>\d+) driver_out_bps=(?<driverOut>\d+) " +
            "aggregate_in_bps=(?<aggregateIn>\d+) aggregate_out_bps=(?<aggregateOut>\d+) " +
            "peak_connection_in_bps=(?<peakIn>\d+) peak_connection_out_bps=(?<peakOut>\d+) " +
            "aggregate_in_pps=(?<inPps>\d+) aggregate_out_pps=(?<outPps>\d+) " +
            "peak_channels=(?<channels>\d+) packet_loss=(?<loss>\d+) total_rpcs=(?<rpcs>\d+)"
        $sampleMatches = @([regex]::Matches($hostContent, $samplePattern))
        if ($sampleMatches.Count -lt 10) {
            throw "Network budget window contained only $($sampleMatches.Count) samples. See $hostLog"
        }

        $driverOutSamples = [double[]]@(
            $sampleMatches | ForEach-Object {
                [double]$_.Groups["driverOut"].Value
            }
        )
        $networkBudgetMetrics = [ordered]@{
            samples = $sampleMatches.Count
            connections = $NetworkBudgetClientCount
            aggregateOutP95BytesPerSecond = [Math]::Round(
                (Get-Percentile $driverOutSamples 0.95),
                1
            )
            aggregateOutMaxBytesPerSecond = [int](
                ($driverOutSamples | Measure-Object -Maximum).Maximum
            )
            perConnectionOutMaxBytesPerSecond = [int](
                ($sampleMatches | ForEach-Object {
                    [int]$_.Groups["peakOut"].Value
                } | Measure-Object -Maximum).Maximum
            )
            perConnectionChannelMax = [int](
                ($sampleMatches | ForEach-Object {
                    [int]$_.Groups["channels"].Value
                } | Measure-Object -Maximum).Maximum
            )
            observedPacketLoss = [int](
                ($sampleMatches | ForEach-Object {
                    [int]$_.Groups["loss"].Value
                } | Measure-Object -Maximum).Maximum
            )
            aggregateOutPacketsPerSecondMax = [int](
                ($sampleMatches | ForEach-Object {
                    [int]$_.Groups["outPps"].Value
                } | Measure-Object -Maximum).Maximum
            )
            totalRpcsMax = [int](
                ($sampleMatches | ForEach-Object {
                    [int]$_.Groups["rpcs"].Value
                } | Measure-Object -Maximum).Maximum
            )
        }
        $networkBudgetChecks = [ordered]@{
            aggregateOutP95 =
                $networkBudgetMetrics.aggregateOutP95BytesPerSecond -le
                $AggregateOutBudgetBytesPerSecond
            perConnectionOutMax =
                $networkBudgetMetrics.perConnectionOutMaxBytesPerSecond -le
                $PerConnectionOutBudgetBytesPerSecond
            perConnectionChannels =
                $networkBudgetMetrics.perConnectionChannelMax -le
                $PerConnectionChannelBudget
            packetLossWithinBudget =
                $networkBudgetMetrics.observedPacketLoss -le
                $AllowedObservedPacketLossPerSample
        }
        $failedNetworkChecks = @(
            $networkBudgetChecks.GetEnumerator() |
                Where-Object { -not $_.Value } |
                ForEach-Object { $_.Key }
        )
        if ($failedNetworkChecks.Count -gt 0) {
            throw "Network budget failed: $($failedNetworkChecks -join ', '). See $hostLog"
        }
    }

    $squadPingPattern =
        "BH_SQUAD_PING_APPLIED revision=1 context=HOSTILE issuer=HOST_FIXTURE location=V\([^\r\n]+\) tracked=(?!none\b)[^\s]+"
    $squadPingPresentationPattern =
        "BH_SQUAD_PING_PRESENTATION revision=1 tracked=1 visible=[01] mode=(TRACKED|LAST_KNOWN)"
    if ($RequireSquadPingReplication) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_SQUAD_PING_CONFIGURED revision=1 context=HOSTILE issuer=HOST_FIXTURE" `
            -Label "Host shared squad ping fixture"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern $squadPingPattern `
            -Label "Initial client shared squad ping"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern $squadPingPresentationPattern `
            -Label "Initial client tracked squad ping presentation"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern $squadPingPattern `
            -Label "Second client shared squad ping"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern $squadPingPresentationPattern `
            -Label "Second client tracked squad ping presentation"
        for ($additionalIndex = 0;
             $additionalIndex -lt $additionalClientLogs.Count;
             ++$additionalIndex)
        {
            $additionalClientContents[$additionalIndex] =
                Wait-ForLogMarker `
                    -LogPath $additionalClientLogs[$additionalIndex] `
                    -Pattern $squadPingPattern `
                    -Label "Scale Client $($additionalIndex + 3) shared squad ping"
            $additionalClientContents[$additionalIndex] =
                Wait-ForLogMarker `
                    -LogPath $additionalClientLogs[$additionalIndex] `
                    -Pattern $squadPingPresentationPattern `
                    -Label "Scale Client $($additionalIndex + 3) tracked squad ping presentation"
        }
    }

    $contextOwnershipTargets = @()
    if ($RequireContextOwnership) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_FIELD_SQUAD_CONTEXT_OWNERSHIP result=success players=2" `
            -Label "Host Context ownership fixture"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern "BH_FIELD_SQUAD_CONTEXT_REPLICATED .*local=1 .*target=CONTEXT_OWNER_[AB]" `
            -Label "Initial client owner-only Context state"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern "BH_FIELD_SQUAD_CONTEXT_REPLICATED .*local=1 .*target=CONTEXT_OWNER_[AB]" `
            -Label "Second client owner-only Context state"

        $targetPattern =
            "BH_FIELD_SQUAD_CONTEXT_REPLICATED .*local=1 .*target=(?<target>CONTEXT_OWNER_[AB])"
        $initialTargets = @(
            [regex]::Matches($initialContent, $targetPattern) |
                ForEach-Object { $_.Groups["target"].Value } |
                Sort-Object -Unique
        )
        $secondTargets = @(
            [regex]::Matches($secondContent, $targetPattern) |
                ForEach-Object { $_.Groups["target"].Value } |
                Sort-Object -Unique
        )
        if ($initialTargets.Count -ne 1 -or
            $secondTargets.Count -ne 1 -or
            $initialTargets[0] -eq $secondTargets[0]) {
            throw "Context state leaked across owning clients. ClientA=$($initialTargets -join ',') ClientB=$($secondTargets -join ',')"
        }
        $contextOwnershipTargets = @(
            $initialTargets[0],
            $secondTargets[0]
        )
    }

    $medicalRecoveryOwnerStates = @()
    if ($RequireMedicalRecoveryReplication) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_MEDICAL_RECOVERY_REPLICATION result=success players=2 .*vehicle_recovered=1 fuel=1.00 hull=1.00" `
            -Label "Host medical and vehicle recovery fixture"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern "BH_MEDICAL_STATE_REPLICATED .*local=1 .*medkits=[12] dressings=1 helmet=1.00 body=1.00" `
            -Label "Initial client owner medical recovery"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern "BH_MEDICAL_STATE_REPLICATED .*local=1 .*medkits=[12] dressings=1 helmet=1.00 body=1.00" `
            -Label "Second client owner medical recovery"

        $medicalStatePattern =
            "BH_MEDICAL_STATE_REPLICATED .*local=1 .*medkits=(?<medkits>[12]) dressings=1 helmet=1.00 body=1.00"
        $initialMedicalStates = @(
            [regex]::Matches($initialContent, $medicalStatePattern) |
                ForEach-Object { $_.Groups["medkits"].Value } |
                Sort-Object -Unique
        )
        $secondMedicalStates = @(
            [regex]::Matches($secondContent, $medicalStatePattern) |
                ForEach-Object { $_.Groups["medkits"].Value } |
                Sort-Object -Unique
        )
        if ($initialMedicalStates.Count -ne 1 -or
            $secondMedicalStates.Count -ne 1 -or
            $initialMedicalStates[0] -eq $secondMedicalStates[0]) {
            throw "Medical state did not remain owner-isolated. ClientA=$($initialMedicalStates -join ',') ClientB=$($secondMedicalStates -join ',')"
        }
        $medicalRecoveryOwnerStates = @(
            [int]$initialMedicalStates[0],
            [int]$secondMedicalStates[0]
        )
    }

    $customDifficultyPattern =
        "BH_CUSTOM_DIFFICULTY_REPLICATED preset=3 damage=0.85 perception=0.95 coordination=1.10 medical=1.20 strategic=1.30 checkpoint=0.75"
    if ($RequireCustomDifficultyReplication) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_CUSTOM_DIFFICULTY_CONFIGURED result=success" `
            -Label "Host custom difficulty configuration"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern $customDifficultyPattern `
            -Label "Initial client custom difficulty snapshot"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern $customDifficultyPattern `
            -Label "Second client custom difficulty snapshot"
    }

    if ($RequireActiveOperation -and
        $null -ne $expectedOperationTypeValue) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_OPERATION_COMMIT result=success .*type=$expectedOperationTypeValue" `
            -Label "$OperationType operation routing"
    }

    $operationFailureVerified = $false
    $operationFailureReconnected = $false
    if ($RequireOperationFailure) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_OPERATION_FAILURE result=success .*participants=2 failed=2 director_in_progress=0" `
            -Label "Authoritative operation failure fixture"
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_SHARED_OPERATION_FAILURE_DEBRIEF .*participants=2" `
            -Label "Shared operation failure debrief handoff"

        $failureStatePattern =
            "BH_MISSION_STATE_REPLICATED .*complete=0 failed=1"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern $failureStatePattern `
            -Label "Initial client terminal failure state"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern $failureStatePattern `
            -Label "Second client terminal failure state"

        $debriefClientPattern =
            "BH_TEST_OPERATION_DEBRIEF_CLIENT result=success"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern $debriefClientPattern `
            -Label "Initial client operation debrief"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern $debriefClientPattern `
            -Label "Second client operation debrief"
        $operationFailureVerified = $true
    }


    if ($RequireWatercraftDelivery) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_WATERCRAFT_DELIVERY_OCCUPIED .*remaining=0\.0 occupant=1" `
            -Label "Occupied watercraft cargo delivery"
    }

    if ($RequireCivilianAidDelivery) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_CIVILIAN_AID_DELIVERY_OCCUPIED result=success .*remaining=0\.0 occupant=1" `
            -Label "Occupied civilian-aid transport delivery"
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_FIELD_CIVILIAN_AID_CHECKPOINT stage=load .*result=success" `
            -Label "Civilian-aid load checkpoint"
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_FIELD_CIVILIAN_AID_CHECKPOINT stage=delivery .*result=success" `
            -Label "Civilian-aid delivery checkpoint"
    }

    $missionCacheOwnerCounts = @()
    $missionCacheTransferVerified = $false
    $missionCacheRejoinVerified = $false
    if ($RequireMissionCacheTransfer) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_MISSION_CACHE_TRANSFER result=success .*owner_a_has=0 owner_b_has=1 stored=None" `
            -Label "Two-player mission-cache owner transfer"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern "BH_MISSION_CACHE_REPLICATED container=FirstLightMissionCache01 stored=(RedKeycard|None)" `
            -Label "Initial client mission-cache replication"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern "BH_MISSION_CACHE_REPLICATED container=FirstLightMissionCache01 stored=(RedKeycard|None)" `
            -Label "Second client mission-cache replication"

        $ownerInventoryPattern =
            "BH_KEYCARD_INVENTORY_REPLICATED mission_items=(?<count>[01])"
        $missionCacheOwnerCounts = @(
            @($initialContent, $secondContent) |
                ForEach-Object {
                    [regex]::Matches($_, $ownerInventoryPattern) |
                        ForEach-Object {
                            [int]$_.Groups["count"].Value
                        }
                } |
                Sort-Object -Unique
        )
        if ($missionCacheOwnerCounts.Count -ne 2 -or
            $missionCacheOwnerCounts[0] -ne 0 -or
            $missionCacheOwnerCounts[1] -ne 1) {
            throw "Mission-cache ownership did not converge to one owner. Counts=$($missionCacheOwnerCounts -join ',')"
        }
        $missionCacheTransferVerified = $true
    }

    $armoredThreatTargetingVerified = $false
    $armoredThreatPresentationVerified = $false
    if ($RequireArmoredThreatTargeting) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_ARMORED_THREAT_TARGETING result=success .*initial_target=\S+ switched_target=\S+" `
            -Label "Two-player armored-threat target switching"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern "BH_ARMORED_THREAT_STATE id=FirstLightArmoredThreat01 reason=replicated_target" `
            -Label "Initial client armored-threat target replication"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern "BH_ARMORED_THREAT_STATE id=FirstLightArmoredThreat01 reason=replicated_target" `
            -Label "Second client armored-threat target replication"
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern "BH_ARMORED_THREAT_PRESENTATION local=1 threat=\S+ active=1 distance_m=\d+" `
            -Label "Initial client armored-threat HUD presentation"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern "BH_ARMORED_THREAT_PRESENTATION local=1 threat=\S+ active=1 distance_m=\d+" `
            -Label "Second client armored-threat HUD presentation"
        $armoredThreatTargetingVerified = $true
        $armoredThreatPresentationVerified = $true
    }

    $crashPreparedContent = $null
    if ($RequireHostRecovery) {
        $crashPreparedContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_CRASH_RECOVERY_PREPARED result=success" `
            -Label "Crash-recovery checkpoint preparation"

        if ($LegacySchemaVersion -gt 0) {
            $crashPreparedContent = Wait-ForLogMarker `
                -LogPath $hostLog `
                -Pattern "BH_TEST_LEGACY_CHECKPOINT_WRITTEN .*schema=$LegacySchemaVersion result=success" `
                -Label "Legacy checkpoint fixture"
        }

        Stop-OwnedProcess -Process $initialClient
        Stop-OwnedProcess -Process $secondClient
        Stop-OwnedProcess -Process $hostProcess
        Start-Sleep -Seconds 1

        if ($RequireCorruptPrimaryRecovery) {
            if (-not (Test-Path -LiteralPath $primarySavePath) -or
                -not (Test-Path -LiteralPath $backupSavePath)) {
                throw "Crash fixture did not produce both isolated checkpoint slots."
            }
            $primaryBytes = [System.IO.File]::ReadAllBytes(
                $primarySavePath
            )
            if ($primaryBytes.Length -le 16) {
                throw "Primary checkpoint is too small for corruption fixture."
            }
            $primaryBytes[$primaryBytes.Length - 1] =
                $primaryBytes[$primaryBytes.Length - 1] -bxor 0xFF
            [System.IO.File]::WriteAllBytes(
                $primarySavePath,
                $primaryBytes
            )
        }

        $hostLog = $recoveredHostLog
        $initialClientLog = $recoveredClientALog
        $secondClientLog = $recoveredClientBLog
        $recoveryHostArguments = @(
            $uproject,
            $testMap,
            "-server",
            "-port=$Port",
            "-abslog=$hostLog"
        ) + $commonArguments + @(
            "-BHTestSaveSlotSuffix=$runId",
            "-BHTestServerTravelAfterSeconds=$travelDelaySeconds",
            "-BHTestRestoreCrashRecovery"
        )
        if ($RequireOperationCompletion) {
            $recoveryHostArguments +=
                "-BHTestOperationCompletionAfterTravel"
        }
        $hostProcess = Start-BHProcess -Arguments $recoveryHostArguments
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_WAR_GAME_STATE_READY" `
            -Label "Recovered dedicated host readiness"

        $recoveryInitialClientArguments = @(
            $uproject,
            "127.0.0.1:$Port",
            "-game",
            "-abslog=$initialClientLog"
        ) + $commonArguments
        $initialClient = Start-BHProcess `
            -Arguments $recoveryInitialClientArguments
        Start-Sleep -Seconds 2
        $recoverySecondClientArguments = @(
            $uproject,
            "127.0.0.1:$Port",
            "-game",
            "-abslog=$secondClientLog"
        ) + $commonArguments
        $secondClient = Start-BHProcess `
            -Arguments $recoverySecondClientArguments
        $initialContent = Wait-ForLogMarker `
            -LogPath $initialClientLog `
            -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
            -Label "Recovered initial client snapshot"
        $secondContent = Wait-ForLogMarker `
            -LogPath $secondClientLog `
            -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
            -Label "Recovered second client snapshot"
        if ($RequireCorruptPrimaryRecovery) {
            $hostContent = Wait-ForLogMarker `
                -LogPath $hostLog `
                -Pattern "BH_CHECKPOINT_HEALED source=backup .*result=success" `
                -Label "Corrupt primary checkpoint healing"
        }
        if ($LegacySchemaVersion -gt 0) {
            $hostContent = Wait-ForLogMarker `
                -LogPath $hostLog `
                -Pattern "BH_CHECKPOINT_HEALED source=legacy_primary schema_from=$LegacySchemaVersion .*result=success" `
                -Label "Legacy checkpoint migration"
        }
    }

    if ($RequireServerTravel) {
        $hostContent = Wait-ForLogMarker `
            -LogPath $hostLog `
            -Pattern "BH_TEST_SERVER_TRAVEL_COMPLETE" `
            -Label "Dedicated host server travel"
        if ($RequireTransportPersistence) {
            $hostContent = Wait-ForLogMarker `
                -LogPath $hostLog `
                -Pattern "BH_TEST_TRANSPORT_TRAVEL_RESTORED result=success" `
                -Label "Transport and passenger travel restoration"
        }
        if ($RequireTransportCasualtyPersistence) {
            $hostContent = Wait-ForLogMarker `
                -LogPath $hostLog `
                -Pattern "BH_TEST_TRANSPORT_CASUALTY_RESTORED result=success" `
                -Label "Field-squad casualty travel restoration"
        }
        if ($RequireOperationCompletion) {
            $completionPattern =
                "BH_TEST_OPERATION_COMPLETED_AFTER_TRAVEL result=success"
            if ($null -ne $expectedOperationTypeValue) {
                $completionPattern +=
                    ".*type=$expectedOperationTypeValue"
            }
            $hostContent = Wait-ForLogMarker `
                -LogPath $hostLog `
                -Pattern $completionPattern `
                -Label "Operation completion after travel"
        }
        $initialContent = Wait-ForLogMarkerCount `
            -LogPath $initialClientLog `
            -Pattern "Bringing World .*$testMapShortName.* up for play" `
            -Count 2 `
            -Label "Initial client retained through server travel"
        $secondContent = Wait-ForLogMarkerCount `
            -LogPath $secondClientLog `
            -Pattern "Bringing World .*$testMapShortName.* up for play" `
            -Count 2 `
            -Label "Second client retained through server travel"

        $initialContent = Wait-ForLogMarkerCount `
            -LogPath $initialClientLog `
            -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
            -Count 3 `
            -Label "Initial client post-travel snapshot"
        $secondContent = Wait-ForLogMarkerCount `
            -LogPath $secondClientLog `
            -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
            -Count 3 `
            -Label "Second client post-travel snapshot"
    }
    Invoke-BHSoak `
        -Processes @($hostProcess, $initialClient, $secondClient) `
        -LogPaths @($hostLog, $initialClientLog, $secondClientLog) `
        -DurationSeconds $SoakSeconds
    if ($SoakSeconds -gt 0) {
        $hostContent = Get-Content -Raw -LiteralPath $hostLog
        $initialContent = Get-Content -Raw -LiteralPath $initialClientLog
        $secondContent = Get-Content -Raw -LiteralPath $secondClientLog
    }
    $retainedConvergence = Wait-ForMatchingSnapshotSignatures `
        -FirstLogPath $initialClientLog `
        -SecondLogPath $secondClientLog `
        -Label "Retained clients"
    $initialContent = $retainedConvergence.firstContent
    $secondContent = $retainedConvergence.secondContent
    Stop-OwnedProcess -Process $initialClient

    $rejoinClientArguments = @(
        $uproject,
        "127.0.0.1:$Port",
        "-game",
        "-abslog=$rejoinClientLog"
    ) + $commonArguments
    $rejoinClient = Start-BHProcess -Arguments $rejoinClientArguments

    $rejoinContent = Wait-ForLogMarker `
        -LogPath $rejoinClientLog `
        -Pattern "BH_WAR_SNAPSHOT_APPLIED" `
        -Label "Rejoining client snapshot"

    if ($RequireOperationFailure) {
        $rejoinContent = Wait-ForLogMarker `
            -LogPath $rejoinClientLog `
            -Pattern "BH_MISSION_STATE_REPLICATED .*complete=0 failed=1" `
            -Label "Rejoining client terminal failure state"
        $rejoinContent = Wait-ForLogMarker `
            -LogPath $rejoinClientLog `
            -Pattern "BH_TEST_OPERATION_DEBRIEF_RESTORED result=success failed=1" `
            -Label "Rejoining client operation debrief"
        $operationFailureReconnected = $true
    }

    if ($RequireMissionCacheTransfer) {
        $rejoinContent = Wait-ForLogMarker `
            -LogPath $rejoinClientLog `
            -Pattern "BH_KEYCARD_INVENTORY_REPLICATED mission_items=1" `
            -Label "Rejoining client mission-cache owner state"
        $missionCacheRejoinVerified = $true
    }

    if ($RequireNetworkCombatDensity) {
        $rejoinContent = Wait-ForLogMarker `
            -LogPath $rejoinClientLog `
            -Pattern $combatDensityReplicationPattern `
            -Label "Rejoining client combat-density replication"
    }

    if ($RequireCustomDifficultyReplication) {
        $rejoinContent = Wait-ForLogMarker `
            -LogPath $rejoinClientLog `
            -Pattern $customDifficultyPattern `
            -Label "Rejoining client custom difficulty snapshot"
    }

    if ($RequireSquadPingReplication) {
        $rejoinContent = Wait-ForLogMarker `
            -LogPath $rejoinClientLog `
            -Pattern $squadPingPattern `
            -Label "Rejoining client shared squad ping"
    }

    $rejoinConvergence = Wait-ForMatchingSnapshotSignatures `
        -FirstLogPath $secondClientLog `
        -SecondLogPath $rejoinClientLog `
        -Label "Retained and rejoining clients"
    $secondContent = $rejoinConvergence.firstContent
    $rejoinContent = $rejoinConvergence.secondContent
    $initialSignature = $retainedConvergence.signature
    $secondSignature = $rejoinConvergence.signature
    $rejoinSignature = $rejoinConvergence.signature

    $operationPattern = "operation_id=(?<id>Operation_[A-Fa-f0-9]+)"
    $additionalClientsActiveOperationVerified =
        -not $RequireActiveOperation -or
        @(
            $additionalClientContents |
                Where-Object { $_ -notmatch $operationPattern }
        ).Count -eq 0
    $committedRouteMatch = [regex]::Match(
        $hostContent,
        "BH_TEST_OPERATION_COMMIT result=success id=(?<id>Operation_[A-Fa-f0-9]+) sector=(?<sector>\S+) type=(?<type>\d+)"
    )
    $initialOperationId = if ($initialContent -match $operationPattern) {
        $Matches.id
    } else {
        $null
    }
    $rejoinOperationId = if ($rejoinContent -match $operationPattern) {
        $Matches.id
    } else {
        $null
    }

    if ($initialOperationId -and
        $rejoinOperationId -and
        $initialOperationId -ne $rejoinOperationId) {
        throw "Operation identity diverged across reconnect."
    }
    if ($RequireActiveOperation -and
        -not $RequireOperationCompletion -and
        -not $RequireOperationFailure -and
        (-not $initialOperationId -or -not $rejoinOperationId)) {
        throw "Active operation identity was required but not replicated."
    }
    if ($RequireOperationCompletion -and $rejoinOperationId) {
        throw "Completed operation remained active for the rejoining client."
    }
    if ($RequireOperationFailure -and $rejoinOperationId) {
        throw "Failed operation remained active for the rejoining client."
    }

    $summary = [ordered]@{
        result = "Passed"
        runId = $runId
        port = $Port
        map = $testMap
        activeOperationRequired = [bool]$RequireActiveOperation
        requestedOperationType = $OperationType
        requestedOperationTypeVerified =
            $null -eq $expectedOperationTypeValue -or
            $hostContent -match
                "BH_TEST_OPERATION_COMMIT result=success .*type=$expectedOperationTypeValue"
        committedOperationSector = if ($committedRouteMatch.Success) {
            $committedRouteMatch.Groups["sector"].Value
        } else {
            $null
        }
        committedOperationType = if ($committedRouteMatch.Success) {
            [int]$committedRouteMatch.Groups["type"].Value
        } else {
            $null
        }
        serverTravelRequired = [bool]$RequireServerTravel
        transportPersistenceRequired = [bool]$RequireTransportPersistence
        transportCasualtyPersistenceRequired =
            [bool]$RequireTransportCasualtyPersistence
        operationCompletionRequired = [bool]$RequireOperationCompletion
        operationFailureRequired = [bool]$RequireOperationFailure
        operationFailureVerified = $operationFailureVerified
        operationFailureReconnected = $operationFailureReconnected
        civilianAidDeliveryRequired = [bool]$RequireCivilianAidDelivery
        civilianAidDeliveryVerified =
            -not $RequireCivilianAidDelivery -or
            ($hostContent -match
                "BH_TEST_CIVILIAN_AID_DELIVERY_OCCUPIED result=success .*remaining=0\.0 occupant=1" -and
             $hostContent -match
                "BH_FIELD_CIVILIAN_AID_CHECKPOINT stage=load .*result=success" -and
             $hostContent -match
                "BH_FIELD_CIVILIAN_AID_CHECKPOINT stage=delivery .*result=success")
        missionCacheTransferRequired = [bool]$RequireMissionCacheTransfer
        missionCacheTransferVerified = $missionCacheTransferVerified
        missionCacheOwnerCounts = $missionCacheOwnerCounts
        missionCacheRejoinVerified = $missionCacheRejoinVerified
        armoredThreatTargetingRequired = [bool]$RequireArmoredThreatTargeting
        armoredThreatTargetingVerified = $armoredThreatTargetingVerified
        armoredThreatPresentationVerified = $armoredThreatPresentationVerified
        contextOwnershipRequired = [bool]$RequireContextOwnership
        contextOwnershipVerified =
            -not $RequireContextOwnership -or
            ($contextOwnershipTargets.Count -eq 2 -and
             $contextOwnershipTargets[0] -ne
                $contextOwnershipTargets[1])
        contextOwnershipTargets = $contextOwnershipTargets
        medicalRecoveryReplicationRequired =
            [bool]$RequireMedicalRecoveryReplication
        medicalRecoveryReplicationVerified =
            -not $RequireMedicalRecoveryReplication -or
            ($medicalRecoveryOwnerStates.Count -eq 2 -and
             $medicalRecoveryOwnerStates[0] -ne
                $medicalRecoveryOwnerStates[1] -and
             $hostContent -match
                "BH_TEST_MEDICAL_RECOVERY_REPLICATION result=success")
        medicalRecoveryOwnerMedkitStates =
            $medicalRecoveryOwnerStates
        customDifficultyReplicationRequired =
            [bool]$RequireCustomDifficultyReplication
        customDifficultyReplicationVerified =
            -not $RequireCustomDifficultyReplication -or
            ($hostContent -match
                "BH_TEST_CUSTOM_DIFFICULTY_CONFIGURED result=success" -and
             $initialContent -match $customDifficultyPattern -and
             $secondContent -match $customDifficultyPattern -and
             $rejoinContent -match $customDifficultyPattern)
        squadPingReplicationRequired =
            [bool]$RequireSquadPingReplication
        squadPingReplicationVerified =
            -not $RequireSquadPingReplication -or
            ($hostContent -match
                "BH_TEST_SQUAD_PING_CONFIGURED revision=1 context=HOSTILE issuer=HOST_FIXTURE" -and
             $initialContent -match $squadPingPattern -and
             $initialContent -match $squadPingPresentationPattern -and
             $secondContent -match $squadPingPattern -and
             $secondContent -match $squadPingPresentationPattern -and
             $rejoinContent -match $squadPingPattern -and
             @(
                 $additionalClientContents |
                     Where-Object {
                         $_ -notmatch $squadPingPattern -or
                         $_ -notmatch $squadPingPresentationPattern
                     }
             ).Count -eq 0)
        additionalScaleClientsSnapshotApplied =
            $additionalClientContents.Count -eq
                [Math]::Max(0, $NetworkBudgetClientCount - 2)
        additionalScaleClientsActiveOperationVerified =
            $additionalClientsActiveOperationVerified
        networkBudgetRequired = [bool]$RequireNetworkBudget
        networkCombatDensityRequired =
            [bool]$RequireNetworkCombatDensity
        networkCombatDensityVerified =
            -not $RequireNetworkCombatDensity -or
            ($hostContent -match
                "BH_NET_COMBAT_DENSITY_CONFIGURED result=success hostiles=12 friendlies=4 total=16" -and
             $initialContent -match $combatDensityReplicationPattern -and
             $secondContent -match $combatDensityReplicationPattern -and
             $rejoinContent -match $combatDensityReplicationPattern -and
             @(
                 $additionalClientContents |
                     Where-Object {
                         $_ -notmatch $combatDensityReplicationPattern
                     }
             ).Count -eq 0)
        networkBudgetVerified =
            -not $RequireNetworkBudget -or
            (@($networkBudgetChecks.Values | Where-Object { -not $_ }).Count -eq 0)
        networkBudget = if ($RequireNetworkBudget) {
            [ordered]@{
                mode = "DedicatedServer${NetworkBudgetClientCount}ClientsNullRHI"
                budgets = [ordered]@{
                    aggregateOutP95BytesPerSecond =
                        $AggregateOutBudgetBytesPerSecond
                    perConnectionOutMaxBytesPerSecond =
                        $PerConnectionOutBudgetBytesPerSecond
                    perConnectionChannelMax =
                        $PerConnectionChannelBudget
                    observedPacketLossMax =
                        $AllowedObservedPacketLossPerSample
                }
                metrics = $networkBudgetMetrics
                checks = $networkBudgetChecks
                rendererProof = $false
                representativeInternetProof = $false
                combatDensity = if ($RequireNetworkCombatDensity) {
                    [ordered]@{
                        hostileAI = 12
                        friendlyAI = 4
                        connectedPlayers = $NetworkBudgetClientCount
                        minimumFixtureAndPlayerParticipants =
                            16 + $NetworkBudgetClientCount
                    }
                } else {
                    $null
                }
            }
        } else {
            $null
        }
        hostRecoveryRequired = [bool]$RequireHostRecovery
        corruptPrimaryRecoveryRequired =
            [bool]$RequireCorruptPrimaryRecovery
        legacySchemaVersion = $LegacySchemaVersion
        packetLagMilliseconds = $PacketLagMilliseconds
        packetLossPercent = $PacketLossPercent
        soakSeconds = $SoakSeconds
        soakCompleted = $SoakSeconds -gt 0
        hostReady = $hostContent -match "BH_WAR_GAME_STATE_READY"
        serverTravelCompleted = -not $RequireServerTravel -or
            $hostContent -match "BH_TEST_SERVER_TRAVEL_COMPLETE"
        transportPersistenceRestored = [bool]$RequireTransportPersistence -and
            $hostContent -match "BH_TEST_TRANSPORT_TRAVEL_RESTORED result=success"
        transportCasualtyPersistenceRestored =
            -not $RequireTransportCasualtyPersistence -or
            $hostContent -match
                "BH_TEST_TRANSPORT_CASUALTY_RESTORED result=success"
        operationCompletedAfterTravel = [bool]$RequireOperationCompletion -and
            $hostContent -match $completionPattern
        hostRecoveryCheckpointPrepared = [bool]$RequireHostRecovery -and
            $crashPreparedContent -match "BH_TEST_CRASH_RECOVERY_PREPARED result=success"
        corruptPrimaryRecovered =
            -not $RequireCorruptPrimaryRecovery -or
            $hostContent -match
                "BH_CHECKPOINT_HEALED source=backup .*result=success"
        legacyCheckpointMigrated =
            $LegacySchemaVersion -eq 0 -or
            $hostContent -match
                "BH_CHECKPOINT_HEALED source=legacy_primary schema_from=$LegacySchemaVersion .*result=success"
        initialClientSnapshotApplied = $true
        secondClientSnapshotApplied = $true
        rejoinClientSnapshotApplied = $true
        snapshotSignature = $initialSignature
        operationId = $initialOperationId
        hostLog = $hostLog
        initialClientLog = $initialClientLog
        secondClientLog = $secondClientLog
        rejoinClientLog = $rejoinClientLog
        additionalClientLogs = @($additionalClientLogs)
        crashedHostLog = if ($RequireHostRecovery) { $crashedHostLog } else { $null }
        crashedClientALog = if ($RequireHostRecovery) { $crashedClientALog } else { $null }
        crashedClientBLog = if ($RequireHostRecovery) { $crashedClientBLog } else { $null }
    }
    $summary | ConvertTo-Json | Set-Content -LiteralPath $summaryPath
    Write-Host "Multiplayer reconnect validation passed: $summaryPath"
}
finally {
    foreach ($process in $launchedProcesses) {
        Stop-OwnedProcess -Process $process
    }
    if ($RequireTransportPersistence -or
        $RequireMissionCacheTransfer -or
        $RequireOperationFailure -or
        $SoakSeconds -gt 0) {
        $testSaveNames = @(
            "BrokenHorizon_Checkpoint_$safeRunId.sav",
            "BrokenHorizon_Checkpoint_Backup_$safeRunId.sav"
        )
        foreach ($testSaveName in $testSaveNames) {
            $testSavePath = Join-Path $saveDirectory $testSaveName
            if (Test-Path -LiteralPath $testSavePath) {
                Remove-Item -LiteralPath $testSavePath -Force
            }
        }
    }
}
