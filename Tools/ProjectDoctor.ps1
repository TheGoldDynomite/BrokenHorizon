param(
    [string]$ProjectRoot,
    [string]$EngineRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BH.Common.ps1")

$root = Resolve-BHProjectRoot -ProjectRoot $ProjectRoot -StartPath $PSScriptRoot
$uproject = Get-BHUProject -ProjectRoot $root
$checks = New-Object System.Collections.ArrayList

function Add-Check {
    param([string]$Name, [ValidateSet("OK", "Warning", "Error", "Info")][string]$Status, [string]$Details)
    [void]$checks.Add([pscustomobject]@{ Name = $Name; Status = $Status; Details = $Details })
}

Add-Check -Name "Project descriptor" -Status "OK" -Details $uproject.FullName

if ($root -match "(?i)OneDrive") {
    Add-Check -Name "Project location" -Status "Warning" -Details "Project path is inside OneDrive. Move it to a normal local folder."
}
else {
    Add-Check -Name "Project location" -Status "OK" -Details $root
}

if ($root.Length -gt 150) {
    Add-Check -Name "Path length" -Status "Warning" -Details "Project root is $($root.Length) characters long; shorter paths reduce Windows/Unreal tool friction."
}
else {
    Add-Check -Name "Path length" -Status "OK" -Details "$($root.Length) characters"
}

$resolvedEngine = $null
try {
    $resolvedEngine = Resolve-BHEngineRoot -EngineRoot $EngineRoot -ProjectRoot $root -UProject $uproject
    $engineVersion = Get-BHEngineVersion -EngineRoot $resolvedEngine
    Add-Check -Name "Unreal Engine" -Status "OK" -Details "$resolvedEngine (version $engineVersion)"

    $editorCmd = Join-Path $resolvedEngine "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    if (Test-Path -LiteralPath $editorCmd -PathType Leaf) {
        Add-Check -Name "UnrealEditor-Cmd" -Status "OK" -Details $editorCmd
    }
    else {
        Add-Check -Name "UnrealEditor-Cmd" -Status "Error" -Details "Missing: $editorCmd"
    }
}
catch {
    Add-Check -Name "Unreal Engine" -Status "Error" -Details $_.Exception.Message
}

$editorTarget = Get-BHTargetName -ProjectRoot $root -UProject $uproject -Kind Editor
$gameTarget = Get-BHTargetName -ProjectRoot $root -UProject $uproject -Kind Game
Add-Check -Name "Editor target" -Status "Info" -Details $editorTarget
Add-Check -Name "Game target" -Status "Info" -Details $gameTarget

$sourceRoot = Join-Path $root "Source"
if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
    $cppCount = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -Include *.h, *.cpp, *.cs -File -ErrorAction SilentlyContinue).Count
    Add-Check -Name "Source tree" -Status "OK" -Details "$cppCount C++/C# source files"
}
else {
    Add-Check -Name "Source tree" -Status "Error" -Details "Missing Source directory"
}

foreach ($required in @(
    "AGENTS.md",
    "Source\AGENTS.md",
    "docs\PROJECT_STATE.md",
    "docs\ARCHITECTURE.md",
    "docs\DECISIONS.md",
    "docs\TEST_MATRIX.md",
    ".codex\config.toml",
    "Tools\BuildEditor.ps1",
    "Tools\ReviewChanges.ps1",
    "Tools\Validate.ps1"
)) {
    $path = Join-Path $root $required
    if (Test-Path -LiteralPath $path) {
        Add-Check -Name ("Kit file " + $required) -Status "OK" -Details "Present"
    }
    else {
        Add-Check -Name ("Kit file " + $required) -Status "Warning" -Details "Missing"
    }
}

$gitCommand = Get-Command git -ErrorAction SilentlyContinue
if ($null -eq $gitCommand) {
    Add-Check -Name "Git" -Status "Warning" -Details "git.exe is not available on PATH"
}
else {
    $inside = (& git -C $root rev-parse --is-inside-work-tree 2>$null)
    if ($LASTEXITCODE -eq 0 -and $inside -eq "true") {
        $branch = (& git -C $root branch --show-current 2>$null)
        $statusLines = @(& git -C $root status --porcelain 2>$null)
        Add-Check -Name "Git repository" -Status "OK" -Details "Branch '$branch'; $($statusLines.Count) changed/untracked entries"

        $trackedGenerated = @(& git -C $root ls-files -- Binaries DerivedDataCache Intermediate Saved .vs 2>$null)
        if ($trackedGenerated.Count -gt 0) {
            Add-Check -Name "Tracked generated output" -Status "Warning" -Details "$($trackedGenerated.Count) generated paths are tracked; inspect before removing from the index."
        }
        else {
            Add-Check -Name "Tracked generated output" -Status "OK" -Details "None detected"
        }
    }
    else {
        Add-Check -Name "Git repository" -Status "Warning" -Details "Project is not initialized as a Git repository"
    }

    & git lfs version *> $null
    if ($LASTEXITCODE -eq 0) {
        Add-Check -Name "Git LFS" -Status "OK" -Details "Available"
    }
    else {
        Add-Check -Name "Git LFS" -Status "Warning" -Details "Git LFS is not available; .uasset/.umap rules are present but LFS must be installed."
    }
}

$gitignorePath = Join-Path $root ".gitignore"
if ((Test-Path -LiteralPath $gitignorePath) -and ((Get-Content -LiteralPath $gitignorePath -Raw) -match "Intermediate/")) {
    Add-Check -Name ".gitignore" -Status "OK" -Details "Unreal generated-output rules detected"
}
else {
    Add-Check -Name ".gitignore" -Status "Warning" -Details "Unreal generated-output rules were not detected"
}

$attributesPath = Join-Path $root ".gitattributes"
if ((Test-Path -LiteralPath $attributesPath) -and ((Get-Content -LiteralPath $attributesPath -Raw) -match "\*\.uasset\s+filter=lfs")) {
    Add-Check -Name ".gitattributes" -Status "OK" -Details "Unreal binary LFS rules detected"
}
else {
    Add-Check -Name ".gitattributes" -Status "Warning" -Details "Unreal binary LFS rules were not detected"
}

$programFilesX86 = [System.Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
$vswhere = $null
if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
    $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
}
if (-not [string]::IsNullOrWhiteSpace($vswhere) -and (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    $vsInstall = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null)
    if (-not [string]::IsNullOrWhiteSpace($vsInstall)) {
        Add-Check -Name "Visual C++ toolchain" -Status "OK" -Details $vsInstall
    }
    else {
        Add-Check -Name "Visual C++ toolchain" -Status "Warning" -Details "Visual Studio was found, but the C++ tool component was not confirmed."
    }
}
else {
    Add-Check -Name "Visual C++ toolchain" -Status "Info" -Details "vswhere.exe not found; Unreal Build Tool will provide the authoritative result."
}

Write-Host ""
Write-Host "Broken Horizon Project Doctor" -ForegroundColor Cyan
Write-Host "Project: $($uproject.FullName)"
Write-Host ""
$checks | Format-Table -AutoSize Name, Status, Details | Out-Host

$logRoot = Get-BHLogRoot -ProjectRoot $root
$report = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    projectRoot = $root
    uproject = $uproject.FullName
    engineRoot = $resolvedEngine
    checks = @($checks)
}
Write-BHJson -InputObject $report -Path (Join-Path $logRoot "doctor-latest.json")

$errorCount = @($checks | Where-Object { $_.Status -eq "Error" }).Count
$warningCount = @($checks | Where-Object { $_.Status -eq "Warning" }).Count
Write-Host "Doctor summary: $errorCount error(s), $warningCount warning(s)."
if ($errorCount -gt 0) { exit 1 }
exit 0
