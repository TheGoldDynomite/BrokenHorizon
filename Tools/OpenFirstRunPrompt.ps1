param(
    [string]$ProjectRoot,
    [switch]$Open
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BH.Common.ps1")

$root = Resolve-BHProjectRoot -ProjectRoot $ProjectRoot -StartPath $PSScriptRoot
$promptPath = Join-Path $root "FIRST_RUN_PROMPT.txt"
if (-not (Test-Path -LiteralPath $promptPath -PathType Leaf)) {
    throw "First-run prompt is missing: $promptPath"
}

$text = Get-Content -LiteralPath $promptPath -Raw
$copied = $false
if (Get-Command Set-Clipboard -ErrorAction SilentlyContinue) {
    Set-Clipboard -Value $text
    $copied = $true
}
elseif (Get-Command clip.exe -ErrorAction SilentlyContinue) {
    $text | & clip.exe
    $copied = $true
}

if ($copied) {
    Write-Host "First-run prompt copied to the clipboard." -ForegroundColor Green
}
else {
    Write-Warning "Clipboard command was unavailable. Open the file manually: $promptPath"
}

if ($Open) {
    Start-Process notepad.exe -ArgumentList ('"' + $promptPath + '"')
}

exit 0
