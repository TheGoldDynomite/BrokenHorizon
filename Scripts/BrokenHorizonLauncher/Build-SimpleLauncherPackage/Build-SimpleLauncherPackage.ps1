param(
    [string]$OutputRoot = "Builds\Launcher\SimpleLauncher",
    [string]$PackageName = "BHLauncherPackage",
    [string]$ZipName = "BrokenHorizonLauncher-{0}.zip" -f (Get-Date -Format "yyyyMMdd"),
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

$scriptRoot = $PSScriptRoot
$sourcePath = Join-Path $scriptRoot $PackageName
$projectRoot = (Resolve-Path (Join-Path $scriptRoot "..\\..\\..")).ProviderPath
$outputDir = Join-Path $projectRoot $OutputRoot
$zipPath = Join-Path $outputDir $ZipName

if (-not (Test-Path $sourcePath)) {
    throw "Missing launcher package folder: $sourcePath"
}

if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

$stamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
Write-Host "[Launcher] Building package at $zipPath"
Write-Host "[Launcher] Source: $sourcePath"

$payload = @{}
$payload["Version"] = if (Test-Path (Join-Path $sourcePath "version.txt")) {
    (Get-Content (Join-Path $sourcePath "version.txt") -ErrorAction SilentlyContinue).Trim()
} else { "0.0.0" }
$payload["BuiltAtUtc"] = (Get-Date).ToString("o")
$payload["Output"] = $zipPath
$payload["Stamp"] = $stamp
$payload["Source"] = $sourcePath

$metaPath = Join-Path $sourcePath "BuildManifest.json"
Set-Content -Path $metaPath -Value ($payload | ConvertTo-Json -Depth 10) -Encoding UTF8

if (-not (Test-Path $sourcePath)) {
    New-Item -ItemType Directory -Path $sourcePath -Force | Out-Null
}

$toCopy = @(
    "Launch-BrokenHorizon-OneClick.ps1",
    "Launch-BrokenHorizon-OneClick.bat",
    "BHLauncherConfig.example.json",
    "BHLauncherConfig.json",
    "README.txt",
    "README-OneClick.txt"
)

foreach ($file in $toCopy) {
    $sourceFile = Join-Path $scriptRoot $file
    if (Test-Path $sourceFile) {
        Copy-Item -Path $sourceFile -Destination (Join-Path $sourcePath $file) -Force
    }
}

if ($DryRun) {
    Write-Host "[Launcher] Dry-run enabled. No zip generated."
    Write-Host (Get-Content $metaPath -Raw)
    exit 0
}

if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}

Compress-Archive -Path (Join-Path $sourcePath "*") -DestinationPath $zipPath -Force
Write-Host "[Launcher] Done: $zipPath"
Write-Host "[Launcher] Size MB: $([math]::Round((Get-Item $zipPath).Length / 1MB, 2))"
