param(
    [string]$ProjectRoot,
    [string]$EngineRoot,
    [string]$ArchiveDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BH.Common.ps1")

$root = Resolve-BHProjectRoot -ProjectRoot $ProjectRoot -StartPath $PSScriptRoot
$uproject = Get-BHUProject -ProjectRoot $root
$engine = Resolve-BHEngineRoot -EngineRoot $EngineRoot -ProjectRoot $root -UProject $uproject
$runUAT = Join-Path $engine "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path -LiteralPath $runUAT -PathType Leaf)) {
    throw "RunUAT.bat was not found: $runUAT"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
    $ArchiveDirectory = Join-Path $root "Packaged\Development-$timestamp"
}
$ArchiveDirectory = [System.IO.Path]::GetFullPath($ArchiveDirectory)
New-Item -ItemType Directory -Path $ArchiveDirectory -Force | Out-Null

$logRoot = Get-BHLogRoot -ProjectRoot $root
$logPath = Join-Path $logRoot "PackageDevelopment-$timestamp.log"
$arguments = @(
    "BuildCookRun",
    ("-project=" + $uproject.FullName),
    "-noP4",
    "-unattended",
    "-platform=Win64",
    "-clientconfig=Development",
    "-build",
    "-cook",
    "-stage",
    "-pak",
    "-prereqs",
    "-archive",
    ("-archivedirectory=" + $ArchiveDirectory),
    "-utf8output"
)

Write-Host ""
Write-Host "Broken Horizon Development package" -ForegroundColor Cyan
Write-Host "Project: $($uproject.FullName)"
Write-Host "Output:  $ArchiveDirectory"
Write-Host "Log:     $logPath"
Write-Host ""

$started = Get-Date
& $runUAT @arguments 2>&1 | Tee-Object -FilePath $logPath
$exitCode = $LASTEXITCODE
$duration = (Get-Date) - $started

$result = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    success = ($exitCode -eq 0)
    exitCode = $exitCode
    durationSeconds = [Math]::Round($duration.TotalSeconds, 2)
    project = $uproject.FullName
    engineRoot = $engine
    archiveDirectory = $ArchiveDirectory
    log = $logPath
}
Write-BHJson -InputObject $result -Path (Join-Path $logRoot "package-latest.json")

if ($exitCode -eq 0) {
    Write-Host "Development package completed: $ArchiveDirectory" -ForegroundColor Green
    exit 0
}
Write-Host "Development packaging failed with exit code $exitCode. See $logPath" -ForegroundColor Red
$returnCode = if ($exitCode -eq 0) { 1 } else { $exitCode }
exit $returnCode
