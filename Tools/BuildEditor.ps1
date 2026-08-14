param(
    [string]$ProjectRoot,
    [string]$EngineRoot,
    [string]$TargetName
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BH.Common.ps1")

$root = Resolve-BHProjectRoot -ProjectRoot $ProjectRoot -StartPath $PSScriptRoot
$uproject = Get-BHUProject -ProjectRoot $root
$engine = Resolve-BHEngineRoot -EngineRoot $EngineRoot -ProjectRoot $root -UProject $uproject
if ([string]::IsNullOrWhiteSpace($TargetName)) {
    $TargetName = Get-BHTargetName -ProjectRoot $root -UProject $uproject -Kind Editor
}

$buildBat = Join-Path $engine "Engine\Build\BatchFiles\Build.bat"
$logRoot = Get-BHLogRoot -ProjectRoot $root
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $logRoot "BuildEditor-$timestamp.log"
$arguments = @($TargetName, "Win64", "Development", $uproject.FullName, "-WaitMutex", "-NoHotReloadFromIDE")

Write-Host ""
Write-Host "Broken Horizon incremental editor build" -ForegroundColor Cyan
Write-Host "Engine:  $engine"
Write-Host "Target:  $TargetName Win64 Development"
Write-Host "Project: $($uproject.FullName)"
Write-Host "Log:     $logPath"
Write-Host ""

$started = Get-Date
& $buildBat @arguments 2>&1 | Tee-Object -FilePath $logPath
$exitCode = $LASTEXITCODE
$duration = (Get-Date) - $started

$result = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    success = ($exitCode -eq 0)
    exitCode = $exitCode
    durationSeconds = [Math]::Round($duration.TotalSeconds, 2)
    project = $uproject.FullName
    engineRoot = $engine
    target = $TargetName
    platform = "Win64"
    configuration = "Development"
    log = $logPath
}
Write-BHJson -InputObject $result -Path (Join-Path $logRoot "build-latest.json")

if ($exitCode -eq 0) {
    Write-Host ""
    Write-Host "Editor build passed in $([Math]::Round($duration.TotalMinutes, 2)) minute(s)." -ForegroundColor Green
    exit 0
}

Write-Host ""
Write-Host "Editor build failed with exit code $exitCode. Inspect the first meaningful error in $logPath" -ForegroundColor Red
$returnCode = if ($exitCode -eq 0) { 1 } else { $exitCode }
exit $returnCode
