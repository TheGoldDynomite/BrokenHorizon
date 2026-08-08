param([string]$OutputDirectory = "Builds\Launcher\GUI")

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$publishRoot = Join-Path $projectRoot $OutputDirectory
$project = Join-Path $PSScriptRoot "BrokenHorizonLauncher.csproj"

dotnet publish $project -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o $publishRoot
if ($LASTEXITCODE -ne 0) { throw "GUI launcher build failed." }

$zipPath = Join-Path $publishRoot "BrokenHorizonLauncher-Win64.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $publishRoot "BrokenHorizonLauncher.exe"), (Join-Path $publishRoot "launcher-config.json") -DestinationPath $zipPath
Write-Host "Launcher EXE: $(Join-Path $publishRoot 'BrokenHorizonLauncher.exe')"
Write-Host "Shareable ZIP: $zipPath"
