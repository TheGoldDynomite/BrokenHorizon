param(
    [string]$ProjectRoot,
    [switch]$Initialize
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BH.Common.ps1")

$root = Resolve-BHProjectRoot -ProjectRoot $ProjectRoot -StartPath $PSScriptRoot
$beginMarker = "# >>> Broken Horizon Codex Kit >>>"
$endMarker = "# <<< Broken Horizon Codex Kit <<<"

function Merge-MarkedBlock {
    param(
        [Parameter(Mandatory = $true)][string]$TargetPath,
        [Parameter(Mandatory = $true)][string]$TemplatePath
    )

    $existing = ""
    if (Test-Path -LiteralPath $TargetPath -PathType Leaf) {
        $existing = Get-Content -LiteralPath $TargetPath -Raw
    }

    $escapedBegin = [regex]::Escape($beginMarker)
    $escapedEnd = [regex]::Escape($endMarker)
    $existing = [regex]::Replace($existing, "(?ms)^$escapedBegin\r?\n.*?^$escapedEnd\r?\n?", "")
    $existing = $existing.TrimEnd()

    $body = (Get-Content -LiteralPath $TemplatePath -Raw).Trim()
    $block = "$beginMarker`r`n$body`r`n$endMarker"
    $combined = if ([string]::IsNullOrWhiteSpace($existing)) { $block + "`r`n" } else { $existing + "`r`n`r`n" + $block + "`r`n" }
    Set-Content -LiteralPath $TargetPath -Value $combined -Encoding UTF8
}

Merge-MarkedBlock -TargetPath (Join-Path $root ".gitignore") -TemplatePath (Join-Path $PSScriptRoot "Templates\gitignore.block.txt")
Merge-MarkedBlock -TargetPath (Join-Path $root ".gitattributes") -TemplatePath (Join-Path $PSScriptRoot "Templates\gitattributes.block.txt")
Write-Host "Merged Unreal source-control rules without replacing existing custom rules."

if ($null -eq (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Warning "Git is not installed or not on PATH. Rules were written, but repository/LFS setup could not run."
    exit 2
}

& git -C $root rev-parse --is-inside-work-tree *> $null
$isRepository = ($LASTEXITCODE -eq 0)
if (-not $isRepository -and $Initialize) {
    & git -C $root init
    if ($LASTEXITCODE -ne 0) {
        throw "git init failed."
    }
    $isRepository = $true
    Write-Host "Initialized a local Git repository. No files were staged or committed."
}
elseif (-not $isRepository) {
    Write-Warning "This folder is not a Git repository. Re-run with -Initialize to create one."
}
else {
    Write-Host "Existing Git repository preserved."
}

& git lfs version *> $null
if ($LASTEXITCODE -eq 0) {
    if ($isRepository) {
        & git -C $root lfs install --local
    }
    else {
        & git lfs install
    }
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Git LFS is ready for .uasset and .umap files."
    }
    else {
        Write-Warning "Git LFS is installed but its setup command failed."
    }
}
else {
    Write-Warning "Git LFS is not installed. Install it before committing Unreal binary assets."
}

if ($isRepository) {
    $trackedGenerated = @(& git -C $root ls-files -- Binaries DerivedDataCache Intermediate Saved .vs 2>$null)
    if ($trackedGenerated.Count -gt 0) {
        Write-Warning "$($trackedGenerated.Count) generated paths are already tracked. The kit did not remove them from the index."
    }
    Write-Host ""
    & git -C $root status --short
    Write-Host ""
    Write-Host "Nothing was committed or pushed."
}

exit 0
