param(
    [string]$ProjectRoot,
    [string]$EngineRoot,
    [string]$TestFilter = "BrokenHorizon",
    [int]$TimeoutMinutes = 30,
    [switch]$WithRHI
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BH.Common.ps1")

$root = Resolve-BHProjectRoot -ProjectRoot $ProjectRoot -StartPath $PSScriptRoot
$uproject = Get-BHUProject -ProjectRoot $root
$engine = Resolve-BHEngineRoot -EngineRoot $EngineRoot -ProjectRoot $root -UProject $uproject
$editorCmd = Join-Path $engine "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe was not found: $editorCmd"
}

$logRoot = Get-BHLogRoot -ProjectRoot $root
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$stdoutPath = Join-Path $logRoot "Automation-$timestamp.stdout.log"
$stderrPath = Join-Path $logRoot "Automation-$timestamp.stderr.log"
$unrealLogPath = Join-Path $logRoot "Automation-$timestamp.unreal.log"
$combinedPath = Join-Path $logRoot "Automation-$timestamp.combined.log"
$reportPath = Join-Path $logRoot "AutomationReport-$timestamp"
New-Item -ItemType Directory -Path $reportPath -Force | Out-Null

$arguments = @(
    $uproject.FullName,
    "-unattended",
    "-nop4",
    "-nosplash",
    "-nosound",
    "-stdout",
    "-FullStdOutLogOutput",
    ("-ExecCmds=Automation RunTests " + $TestFilter),
    "-TestExit=Automation Test Queue Empty",
    ("-ReportExportPath=" + $reportPath),
    ("-abslog=" + $unrealLogPath)
)
if (-not $WithRHI) {
    $arguments += "-NullRHI"
}

Write-Host ""
Write-Host "Broken Horizon automation tests" -ForegroundColor Cyan
Write-Host "Filter:  $TestFilter"
Write-Host "Engine:  $engine"
Write-Host "Timeout: $TimeoutMinutes minute(s)"
Write-Host "Report:  $reportPath"
Write-Host ""

$started = Get-Date
$processResult = Start-BHProcessWithTimeout -FilePath $editorCmd -Arguments $arguments -StdOutPath $stdoutPath -StdErrPath $stderrPath -TimeoutMinutes $TimeoutMinutes
$duration = (Get-Date) - $started

$combinedParts = @()
foreach ($candidate in @($stdoutPath, $stderrPath, $unrealLogPath)) {
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $combinedParts += "===== $candidate ====="
        $combinedParts += Get-Content -LiteralPath $candidate -Raw
    }
}
$combinedText = ($combinedParts -join "`r`n")
Set-Content -LiteralPath $combinedPath -Value $combinedText -Encoding UTF8

$reportText = ""
$reportJsonFiles = @(Get-ChildItem -LiteralPath $reportPath -Recurse -Filter *.json -File -ErrorAction SilentlyContinue)
foreach ($jsonFile in $reportJsonFiles) {
    try { $reportText += "`r`n" + (Get-Content -LiteralPath $jsonFile.FullName -Raw) } catch { }
}
$analysisText = $combinedText + "`r`n" + $reportText

$noTestsPattern = '(?i)(no automation tests matched|no tests found|found\s+0\s+(automation\s+)?tests|0\s+tests?\s+(were\s+)?run|"tests"\s*:\s*\[\s*\])'
$failurePattern = '(?i)(Result\s*=\s*\{?Fail|Automation Test Failed|Test Completed[^\r\n]*\bFail(?:ed|ure)?\b|"state"\s*:\s*"Fail|"status"\s*:\s*"Fail|critical error|fatal error)'

$status = "Passed"
$exitCode = 0
if ($processResult.TimedOut) {
    $status = "TimedOut"
    $exitCode = 124
}
elseif ($processResult.ExitCode -ne 0) {
    $status = "Failed"
    $exitCode = $processResult.ExitCode
}
elseif ($analysisText -match $failurePattern) {
    $status = "Failed"
    $exitCode = 1
}
elseif ($analysisText -match $noTestsPattern) {
    $status = "NoTests"
    $exitCode = 2
}

$result = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    status = $status
    exitCode = $exitCode
    processExitCode = $processResult.ExitCode
    timedOut = $processResult.TimedOut
    durationSeconds = [Math]::Round($duration.TotalSeconds, 2)
    project = $uproject.FullName
    engineRoot = $engine
    testFilter = $TestFilter
    nullRHI = (-not $WithRHI)
    combinedLog = $combinedPath
    report = $reportPath
    commandLine = $processResult.ArgumentLine
}
Write-BHJson -InputObject $result -Path (Join-Path $logRoot "tests-latest.json")

switch ($status) {
    "Passed" {
        Write-Host "Automation command completed without a detected failure." -ForegroundColor Green
        Write-Host "Review report: $reportPath"
        exit 0
    }
    "NoTests" {
        Write-Warning "No tests matching '$TestFilter' were detected. This is a separate result from a test failure."
        exit 2
    }
    "TimedOut" {
        Write-Host "Automation timed out. Logs: $combinedPath" -ForegroundColor Red
        exit 124
    }
    default {
        Write-Host "Automation failed. Logs: $combinedPath" -ForegroundColor Red
        $returnCode = if ($exitCode -eq 0) { 1 } else { $exitCode }
        exit $returnCode
    }
}
