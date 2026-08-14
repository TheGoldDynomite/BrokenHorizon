param(
    [string]$ProjectRoot,
    [switch]$SkipGitSetup,
    [switch]$RunValidation,
    [switch]$ReplaceStateDocuments,
    [switch]$NoClipboard
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$KitVersion = "2.0.1"

function Select-ProjectFolder {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = "Select the folder that directly contains BrokenHorizon.uproject"
    $dialog.ShowNewFolderButton = $false

    if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        throw "Installation cancelled: no project folder was selected."
    }

    return $dialog.SelectedPath
}

function Copy-TreeContents {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Required package directory is missing: $Source"
    }

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Select-ProjectFolder
}

$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
if (-not (Test-Path -LiteralPath $ProjectRoot -PathType Container)) {
    throw "The selected folder does not exist: $ProjectRoot"
}

$uprojects = @(Get-ChildItem -LiteralPath $ProjectRoot -Filter *.uproject -File)
if ($uprojects.Count -eq 0) {
    throw "No .uproject file was found directly inside: $ProjectRoot`nSelect the Unreal project root, not Source or a parent folder."
}
if ($uprojects.Count -gt 1) {
    throw "More than one .uproject file was found. Select a folder containing exactly one Unreal project."
}

$uproject = $uprojects[0]
if ($uproject.BaseName -ne "BrokenHorizon") {
    Write-Warning "The selected project is '$($uproject.Name)', not BrokenHorizon.uproject. The kit can still install, but verify that this is the intended project."
}

if ($ProjectRoot -match "(?i)OneDrive") {
    Write-Warning "This project is inside OneDrive. Broken Horizon previously had build failures there. Move it to a normal local folder before major development."
}

$PayloadRoot = Join-Path $PSScriptRoot "payload"
if (-not (Test-Path -LiteralPath $PayloadRoot -PathType Container)) {
    throw "The payload folder is missing. Extract the complete ZIP before running the installer."
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backupRoot = Join-Path $ProjectRoot "CodexKitBackup-$timestamp"
$backupCreated = $false

function Backup-RelativePath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $source = Join-Path $ProjectRoot $RelativePath
    if (-not (Test-Path -LiteralPath $source)) {
        return
    }

    if (-not $script:backupCreated) {
        New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
        $script:backupCreated = $true
    }

    $destination = Join-Path $backupRoot $RelativePath
    $destinationParent = Split-Path -Parent $destination
    if (-not [string]::IsNullOrWhiteSpace($destinationParent)) {
        New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
    }

    Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
}

$pathsToBackup = @(
    "AGENTS.md",
    "CHATGPT_PROJECT_INSTRUCTIONS.md",
    "FIRST_RUN_PROMPT.txt",
    ".codex",
    "Source\AGENTS.md",
    "docs\AGENTS.md",
    "docs\CODEX_WORKFLOW.md",
    "docs\CODEX_KIT.md",
    "docs\PROMPTS",
    "Tools",
    "Build-BrokenHorizon.cmd",
    "Validate-BrokenHorizon.cmd",
    "Review-BrokenHorizon-Changes.cmd",
    "Project-Doctor.cmd",
    "Package-BrokenHorizon-Development.cmd",
    "Open-First-Run-Prompt.cmd",
    ".gitignore",
    ".gitattributes",
    "docs\PROJECT_STATE.md",
    "docs\ARCHITECTURE.md",
    "docs\ROADMAP.md",
    "docs\EDITOR_HANDOFF.md",
    "docs\TEST_MATRIX.md",
    "docs\DECISIONS.md"
)

foreach ($relativePath in $pathsToBackup) {
    Backup-RelativePath -RelativePath $relativePath
}

# Always-upgraded project instructions and tooling.
Copy-Item -LiteralPath (Join-Path $PayloadRoot "AGENTS.md") -Destination (Join-Path $ProjectRoot "AGENTS.md") -Force
Copy-Item -LiteralPath (Join-Path $PayloadRoot "CHATGPT_PROJECT_INSTRUCTIONS.md") -Destination (Join-Path $ProjectRoot "CHATGPT_PROJECT_INSTRUCTIONS.md") -Force
Copy-Item -LiteralPath (Join-Path $PayloadRoot "FIRST_RUN_PROMPT.txt") -Destination (Join-Path $ProjectRoot "FIRST_RUN_PROMPT.txt") -Force

Copy-TreeContents -Source (Join-Path $PayloadRoot ".codex") -Destination (Join-Path $ProjectRoot ".codex")
Copy-TreeContents -Source (Join-Path $PayloadRoot "Tools") -Destination (Join-Path $ProjectRoot "Tools")

$projectSource = Join-Path $ProjectRoot "Source"
New-Item -ItemType Directory -Path $projectSource -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PayloadRoot "Source\AGENTS.md") -Destination (Join-Path $projectSource "AGENTS.md") -Force

$projectDocs = Join-Path $ProjectRoot "docs"
New-Item -ItemType Directory -Path $projectDocs -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PayloadRoot "docs\AGENTS.md") -Destination (Join-Path $projectDocs "AGENTS.md") -Force
Copy-Item -LiteralPath (Join-Path $PayloadRoot "docs\CODEX_WORKFLOW.md") -Destination (Join-Path $projectDocs "CODEX_WORKFLOW.md") -Force
Copy-Item -LiteralPath (Join-Path $PayloadRoot "docs\CODEX_KIT.md") -Destination (Join-Path $projectDocs "CODEX_KIT.md") -Force
Copy-TreeContents -Source (Join-Path $PayloadRoot "docs\PROMPTS") -Destination (Join-Path $projectDocs "PROMPTS")

$stateDocuments = @(
    "PROJECT_STATE.md",
    "ARCHITECTURE.md",
    "ROADMAP.md",
    "EDITOR_HANDOFF.md",
    "TEST_MATRIX.md",
    "DECISIONS.md"
)

foreach ($documentName in $stateDocuments) {
    $source = Join-Path $PayloadRoot ("docs\" + $documentName)
    $destination = Join-Path $projectDocs $documentName
    if ($ReplaceStateDocuments -or -not (Test-Path -LiteralPath $destination)) {
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }
    else {
        Write-Host "Preserved existing state document: docs\$documentName"
    }
}

$rootCommands = @(
    "Build-BrokenHorizon.cmd",
    "Validate-BrokenHorizon.cmd",
    "Review-BrokenHorizon-Changes.cmd",
    "Project-Doctor.cmd",
    "Package-BrokenHorizon-Development.cmd",
    "Open-First-Run-Prompt.cmd"
)
foreach ($commandName in $rootCommands) {
    Copy-Item -LiteralPath (Join-Path $PayloadRoot $commandName) -Destination (Join-Path $ProjectRoot $commandName) -Force
}

$installationRecord = [ordered]@{
    kitVersion = $KitVersion
    installedAt = (Get-Date).ToString("o")
    project = $uproject.FullName
    stateDocumentsReplaced = [bool]$ReplaceStateDocuments
}
$installationRecord | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $ProjectRoot ".codex\broken-horizon-kit.json") -Encoding UTF8

$shellPath = (Get-Process -Id $PID).Path

if (-not $SkipGitSetup) {
    Write-Host ""
    Write-Host "Configuring local source-control rules..." -ForegroundColor Cyan
    $gitScript = Join-Path $ProjectRoot "Tools\SetupSourceControl.ps1"
    & $shellPath -NoProfile -ExecutionPolicy Bypass -File $gitScript -ProjectRoot $ProjectRoot -Initialize
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Source-control setup returned exit code $LASTEXITCODE. The kit is installed; see the messages above."
    }
}

Write-Host ""
Write-Host "Running the non-destructive project doctor..." -ForegroundColor Cyan
$doctorScript = Join-Path $ProjectRoot "Tools\ProjectDoctor.ps1"
& $shellPath -NoProfile -ExecutionPolicy Bypass -File $doctorScript -ProjectRoot $ProjectRoot
$doctorExit = $LASTEXITCODE
if ($doctorExit -ne 0) {
    Write-Warning "Project Doctor found one or more blockers. The kit is installed; its report identifies what must be corrected."
}

$promptPath = Join-Path $ProjectRoot "FIRST_RUN_PROMPT.txt"
$copiedPrompt = $false
if (-not $NoClipboard) {
    $promptText = Get-Content -LiteralPath $promptPath -Raw
    if (Get-Command Set-Clipboard -ErrorAction SilentlyContinue) {
        Set-Clipboard -Value $promptText
        $copiedPrompt = $true
    }
    elseif (Get-Command clip.exe -ErrorAction SilentlyContinue) {
        $promptText | & clip.exe
        $copiedPrompt = $true
    }
}

if ($RunValidation) {
    Write-Host ""
    Write-Host "Running full validation..." -ForegroundColor Cyan
    $validateScript = Join-Path $ProjectRoot "Tools\Validate.ps1"
    & $shellPath -NoProfile -ExecutionPolicy Bypass -File $validateScript -ProjectRoot $ProjectRoot
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Validation did not pass. Review the generated logs under Saved\Logs\Codex."
    }
}

Write-Host ""
Write-Host "Broken Horizon Codex Accelerator $KitVersion installed." -ForegroundColor Green
Write-Host "Project: $($uproject.FullName)"
if ($backupCreated) {
    Write-Host "Backup: $backupRoot"
}
if ($copiedPrompt) {
    Write-Host "The project-aware first-run prompt is already on your clipboard." -ForegroundColor Green
}
else {
    Write-Host "First-run prompt: $promptPath"
}
Write-Host ""
Write-Host "Next: open this project folder in Codex and paste the first-run prompt."
