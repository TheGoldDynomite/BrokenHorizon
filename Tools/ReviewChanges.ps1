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

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $rawOutput = @(& git -C $root diff @Arguments --check 2>&1)
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $code = $LASTEXITCODE
    $output = @($rawOutput | Where-Object {
        ([string]$_) -notmatch '(?i)^warning: in the working copy of .* LF will be replaced by CRLF'
    })
    foreach ($line in $output) {
        [void]$checkOutputs.Add([string]$line)
    }
    if ($code -ne 0) {
        $script:checkFailed = $true
    }
}

$previousErrorActionPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
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
}
finally {
    $ErrorActionPreference = $previousErrorActionPreference
}

$generatedPattern = '^(Binaries|DerivedDataCache|Intermediate|Saved|\.vs)(/|\\)'
$changedPaths = @($changedPaths | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)

function Invoke-UntrackedTextCheck {
    param([Parameter(Mandatory = $true)][string]$Path)

    $absolutePath = Join-Path $root $Path
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        return
    }

    try {
        $lines = @(Get-Content -LiteralPath $absolutePath -ErrorAction Stop)
    }
    catch {
        [void]$checkOutputs.Add(("{0}: unable to inspect untracked text file: {1}" -f $Path, $_.Exception.Message))
        $script:checkFailed = $true
        return
    }

    $lineNumber = 0
    foreach ($line in $lines) {
        $lineNumber++
        if ($line -match '[ \t]+$') {
            [void]$checkOutputs.Add("$Path`:$lineNumber`: trailing whitespace.")
            $script:checkFailed = $true
        }
        elseif ($line -match '^(<<<<<<<|=======|>>>>>>>)( |$)') {
            [void]$checkOutputs.Add("$Path`:$lineNumber`: conflict marker.")
            $script:checkFailed = $true
        }
    }
}

if (-not $StagedOnly) {
    foreach ($untrackedPath in @($changedPaths | Where-Object {
        $_ -notmatch $generatedPattern -and
        $_ -notmatch '(?i)\.(uasset|umap|png|jpe?g|bmp|tga|wav|mp3|ogg|flac|fbx|glb|gltf|bin|dll|exe|zip|7z|pak)$'
    })) {
        Invoke-UntrackedTextCheck -Path $untrackedPath
    }
}

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
