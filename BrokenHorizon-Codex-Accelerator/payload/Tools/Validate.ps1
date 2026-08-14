param(
    [string]$ProjectRoot,
    [string]$EngineRoot,
    [string]$TestFilter = "BrokenHorizon",
    [switch]$RequireTests,
    [switch]$SkipDoctor,
    [switch]$SkipTests,
    [switch]$SkipReview,
    [switch]$WithRHI
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BH.Common.ps1")

$root = Resolve-BHProjectRoot -ProjectRoot $ProjectRoot -StartPath $PSScriptRoot
$uproject = Get-BHUProject -ProjectRoot $root
$engine = Resolve-BHEngineRoot -EngineRoot $EngineRoot -ProjectRoot $root -UProject $uproject
$logRoot = Get-BHLogRoot -ProjectRoot $root
$shellPath = (Get-Process -Id $PID).Path
$steps = New-Object System.Collections.ArrayList

function Invoke-ValidationStep {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    Write-Host ""
    Write-Host "=== $Name ===" -ForegroundColor Cyan
    $invokeArguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $Arguments
    $started = Get-Date
    & $shellPath @invokeArguments
    $code = $LASTEXITCODE
    $duration = (Get-Date) - $started
    [void]$steps.Add([pscustomobject]@{
        Name = $Name
        ExitCode = $code
        DurationSeconds = [Math]::Round($duration.TotalSeconds, 2)
    })
    return $code
}

Write-Host ""
Write-Host "Broken Horizon validation gate" -ForegroundColor Cyan
Write-Host "Project: $($uproject.FullName)"
Write-Host "Engine:  $engine"

if (-not $SkipDoctor) {
    $doctorCode = Invoke-ValidationStep -Name "Project Doctor" -ScriptPath (Join-Path $PSScriptRoot "ProjectDoctor.ps1") -Arguments @("-ProjectRoot", $root, "-EngineRoot", $engine)
    if ($doctorCode -ne 0) {
        $summary = [ordered]@{ generatedAt = (Get-Date).ToString("o"); success = $false; stoppedAt = "Project Doctor"; steps = @($steps) }
        Write-BHJson -InputObject $summary -Path (Join-Path $logRoot "validation-latest.json")
        exit $doctorCode
    }
}

$buildCode = Invoke-ValidationStep -Name "Incremental Editor Build" -ScriptPath (Join-Path $PSScriptRoot "BuildEditor.ps1") -Arguments @("-ProjectRoot", $root, "-EngineRoot", $engine)
if ($buildCode -ne 0) {
    $summary = [ordered]@{ generatedAt = (Get-Date).ToString("o"); success = $false; stoppedAt = "Editor Build"; steps = @($steps) }
    Write-BHJson -InputObject $summary -Path (Join-Path $logRoot "validation-latest.json")
    exit $buildCode
}

$testsStatus = "Skipped"
if (-not $SkipTests) {
    $testArguments = @("-ProjectRoot", $root, "-EngineRoot", $engine, "-TestFilter", $TestFilter)
    if ($WithRHI) { $testArguments += "-WithRHI" }
    $testCode = Invoke-ValidationStep -Name "Automation Tests" -ScriptPath (Join-Path $PSScriptRoot "RunTests.ps1") -Arguments $testArguments

    if ($testCode -eq 2) {
        $testsStatus = "NoTests"
        if ($RequireTests) {
            $summary = [ordered]@{ generatedAt = (Get-Date).ToString("o"); success = $false; stoppedAt = "Automation Tests: no matching tests"; steps = @($steps) }
            Write-BHJson -InputObject $summary -Path (Join-Path $logRoot "validation-latest.json")
            Write-Host "Validation failed because -RequireTests was specified." -ForegroundColor Red
            exit 2
        }
        Write-Warning "Validation continues because no matching tests are currently allowed during bootstrap."
    }
    elseif ($testCode -ne 0) {
        $summary = [ordered]@{ generatedAt = (Get-Date).ToString("o"); success = $false; stoppedAt = "Automation Tests"; steps = @($steps) }
        Write-BHJson -InputObject $summary -Path (Join-Path $logRoot "validation-latest.json")
        exit $testCode
    }
    else {
        $testsStatus = "Passed"
    }
}

$reviewStatus = "Skipped"
if (-not $SkipReview) {
    $reviewCode = Invoke-ValidationStep -Name "Change Review" -ScriptPath (Join-Path $PSScriptRoot "ReviewChanges.ps1") -Arguments @("-ProjectRoot", $root)
    if ($reviewCode -eq 2) {
        $reviewStatus = "Unavailable"
        Write-Warning "Git change review was unavailable; validation is continuing because the build/test gate still ran."
    }
    elseif ($reviewCode -ne 0) {
        $reviewStatus = "Failed"
        $summary = [ordered]@{ generatedAt = (Get-Date).ToString("o"); success = $false; stoppedAt = "Change Review"; steps = @($steps) }
        Write-BHJson -InputObject $summary -Path (Join-Path $logRoot "validation-latest.json")
        exit $reviewCode
    }
    else {
        $reviewStatus = "Passed"
    }
}

$validationSummary = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    success = $true
    project = $uproject.FullName
    engineRoot = $engine
    testFilter = $TestFilter
    testsStatus = $testsStatus
    reviewStatus = $reviewStatus
    requireTests = [bool]$RequireTests
    steps = @($steps)
}
Write-BHJson -InputObject $validationSummary -Path (Join-Path $logRoot "validation-latest.json")

Write-Host ""
Write-Host "Validation passed." -ForegroundColor Green
if ($testsStatus -eq "NoTests") {
    Write-Host "Editor build passed; no matching tests exist yet. Use -RequireTests after the test baseline is established."
}
exit 0
