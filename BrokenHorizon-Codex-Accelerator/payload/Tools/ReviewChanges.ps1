param(
    [string]$ProjectRoot,
    [switch]$StagedOnly,
    [switch]$WarnOnlyGenerated
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BH.Common.ps1")

$root = Resolve-BHProjectRoot -ProjectRoot $ProjectRoot -StartPath $PSScriptRoot
$logRoot = Get-BHLogRoot -ProjectRoot $root

if ($null -eq (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Warning "git.exe is not available on PATH; change review could not run."
    exit 2
}

& git -C $root rev-parse --is-inside-work-tree *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Warning "The project is not a Git repository; change review could not run."
    exit 2
}

Write-Host ""
Write-Host "Broken Horizon change review" -ForegroundColor Cyan
Write-Host "Project: $root"
Write-Host ""

$statusLines = @(& git -C $root status --short 2>$null)
if ($statusLines.Count -eq 0) {
    Write-Host "Working tree is clean." -ForegroundColor Green
}
else {
    Write-Host "Git status:" -ForegroundColor Cyan
    $statusLines | ForEach-Object { Write-Host $_ }
}

$checkOutputs = New-Object System.Collections.ArrayList
$checkFailed = $false

function Invoke-DiffCheck {
    param([string[]]$Arguments)

    $output = @(& git -C $root diff @Arguments --check 2>&1)
    $code = $LASTEXITCODE
    foreach ($line in $output) {
        [void]$checkOutputs.Add([string]$line)
    }
    if ($code -ne 0) {
        $script:checkFailed = $true
    }
}

if ($StagedOnly) {
    Invoke-DiffCheck -Arguments @("--cached")
    $changedPaths = @(& git -C $root diff --cached --name-only --diff-filter=ACMRDTUXB 2>$null)
}
else {
    Invoke-DiffCheck -Arguments @()
    Invoke-DiffCheck -Arguments @("--cached")

    $changedPaths = @()
    $changedPaths += @(& git -C $root diff --name-only --diff-filter=ACMRDTUXB 2>$null)
    $changedPaths += @(& git -C $root diff --cached --name-only --diff-filter=ACMRDTUXB 2>$null)
    $changedPaths += @(& git -C $root ls-files --others --exclude-standard 2>$null)
}

$changedPaths = @($changedPaths | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
$generatedPattern = '^(Binaries|DerivedDataCache|Intermediate|Saved|\.vs)(/|\\)'
$generatedPaths = @($changedPaths | Where-Object { $_ -match $generatedPattern })
$binaryAssets = @($changedPaths | Where-Object { $_ -match '(?i)\.(uasset|umap)$' })

Write-Host ""
Write-Host "Changed paths: $($changedPaths.Count)"
if ($binaryAssets.Count -gt 0) {
    Write-Host "Unreal binary assets changed: $($binaryAssets.Count)" -ForegroundColor Yellow
}

if ($checkOutputs.Count -gt 0) {
    Write-Host ""
    Write-Host "git diff --check findings:" -ForegroundColor Red
    $checkOutputs | ForEach-Object { Write-Host $_ }
}
elseif (-not $checkFailed) {
    Write-Host "Whitespace/conflict-marker check passed." -ForegroundColor Green
}

$generatedFailure = ($generatedPaths.Count -gt 0 -and -not $WarnOnlyGenerated)
if ($generatedPaths.Count -gt 0) {
    Write-Host ""
    $messageColor = if ($generatedFailure) { "Red" } else { "Yellow" }
    Write-Host "Generated-output paths are included in the change set:" -ForegroundColor $messageColor
    $generatedPaths | ForEach-Object { Write-Host "  $_" }
}

$result = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    success = (-not $checkFailed -and -not $generatedFailure)
    projectRoot = $root
    stagedOnly = [bool]$StagedOnly
    changedPathCount = $changedPaths.Count
    changedPaths = $changedPaths
    binaryAssetCount = $binaryAssets.Count
    binaryAssets = $binaryAssets
    generatedPathCount = $generatedPaths.Count
    generatedPaths = $generatedPaths
    diffCheckFindings = @($checkOutputs)
}
Write-BHJson -InputObject $result -Path (Join-Path $logRoot "review-latest.json")

if ($checkFailed) {
    Write-Host "Change review failed because git diff --check reported a problem." -ForegroundColor Red
    exit 1
}
if ($generatedFailure) {
    Write-Host "Change review failed because generated output is included. Use -WarnOnlyGenerated only for an intentional exception." -ForegroundColor Red
    exit 1
}

Write-Host "Change review passed." -ForegroundColor Green
exit 0
