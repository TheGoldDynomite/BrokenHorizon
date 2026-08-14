Set-StrictMode -Version Latest

function Resolve-BHProjectRoot {
    param(
        [string]$ProjectRoot,
        [string]$StartPath = $PSScriptRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        $resolved = [System.IO.Path]::GetFullPath($ProjectRoot)
        if (-not (Test-Path -LiteralPath $resolved -PathType Container)) {
            throw "Project root does not exist: $resolved"
        }
        return $resolved.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    }

    $current = [System.IO.Path]::GetFullPath($StartPath)
    while (-not [string]::IsNullOrWhiteSpace($current)) {
        $projects = @(Get-ChildItem -LiteralPath $current -Filter *.uproject -File -ErrorAction SilentlyContinue)
        if ($projects.Count -eq 1) {
            return $current.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
        }

        $parent = [System.IO.Directory]::GetParent($current)
        if ($null -eq $parent) {
            break
        }
        $current = $parent.FullName
    }

    throw "Could not locate a folder containing exactly one .uproject. Pass -ProjectRoot explicitly."
}

function Get-BHUProject {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $projects = @(Get-ChildItem -LiteralPath $ProjectRoot -Filter *.uproject -File)
    if ($projects.Count -eq 0) {
        throw "No .uproject file exists directly under: $ProjectRoot"
    }
    if ($projects.Count -gt 1) {
        throw "More than one .uproject exists under: $ProjectRoot"
    }
    return $projects[0]
}

function Test-BHEngineRoot {
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return $false
    }

    $full = [System.IO.Path]::GetFullPath($Candidate)
    return (Test-Path -LiteralPath (Join-Path $full "Engine\Build\BatchFiles\Build.bat") -PathType Leaf)
}

function Get-BHEngineAssociation {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo]$UProject)

    try {
        $descriptor = Get-Content -LiteralPath $UProject.FullName -Raw | ConvertFrom-Json
        $property = $descriptor.PSObject.Properties["EngineAssociation"]
        if ($null -ne $property -and -not [string]::IsNullOrWhiteSpace([string]$property.Value)) {
            return [string]$property.Value
        }
    }
    catch {
        Write-Warning "Could not parse EngineAssociation from $($UProject.FullName): $($_.Exception.Message)"
    }
    return ""
}

function Resolve-BHEngineRoot {
    param(
        [string]$EngineRoot,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][System.IO.FileInfo]$UProject
    )

    if (-not [string]::IsNullOrWhiteSpace($EngineRoot)) {
        $explicit = [System.IO.Path]::GetFullPath($EngineRoot)
        if (-not (Test-BHEngineRoot -Candidate $explicit)) {
            throw "The supplied EngineRoot is not a valid Unreal Engine root: $explicit"
        }
        return $explicit.TrimEnd('\', '/')
    }

    $association = Get-BHEngineAssociation -UProject $UProject
    $candidates = @()

    foreach ($environmentName in @("UE_ENGINE_ROOT", "UNREAL_ENGINE_ROOT", "UE5_ROOT")) {
        $value = [System.Environment]::GetEnvironmentVariable($environmentName)
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $candidates += $value
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($association)) {
        if (-not [string]::IsNullOrWhiteSpace($env:ProgramFiles)) {
            $candidates += (Join-Path $env:ProgramFiles ("Epic Games\UE_" + $association))
        }
        $programFilesX86 = [System.Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
        if (-not [string]::IsNullOrWhiteSpace($programFilesX86)) {
            $candidates += (Join-Path $programFilesX86 ("Epic Games\UE_" + $association))
        }
    }

    try {
        $buildsKey = "HKCU:\Software\Epic Games\Unreal Engine\Builds"
        if (Test-Path $buildsKey) {
            $properties = Get-ItemProperty -Path $buildsKey
            if (-not [string]::IsNullOrWhiteSpace($association)) {
                $match = $properties.PSObject.Properties | Where-Object { $_.Name -eq $association } | Select-Object -First 1
                if ($null -ne $match) {
                    $candidates += [string]$match.Value
                }
            }
            foreach ($property in $properties.PSObject.Properties) {
                if ($property.Name -notmatch '^PS' -and $property.Value -is [string]) {
                    $candidates += [string]$property.Value
                }
            }
        }
    }
    catch {
        Write-Verbose "Unreal custom-build registry lookup failed: $($_.Exception.Message)"
    }

    if (-not [string]::IsNullOrWhiteSpace($association)) {
        foreach ($registryPath in @(
            "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association",
            "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$association"
        )) {
            try {
                if (Test-Path $registryPath) {
                    $installed = (Get-ItemProperty -Path $registryPath -Name InstalledDirectory -ErrorAction Stop).InstalledDirectory
                    if (-not [string]::IsNullOrWhiteSpace($installed)) {
                        $candidates += [string]$installed
                    }
                }
            }
            catch {
                Write-Verbose "Registry lookup failed for $registryPath"
            }
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:ProgramData)) {
        $launcherData = Join-Path $env:ProgramData "Epic\UnrealEngineLauncher\LauncherInstalled.dat"
        if (Test-Path -LiteralPath $launcherData -PathType Leaf) {
            try {
                $launcher = Get-Content -LiteralPath $launcherData -Raw | ConvertFrom-Json
                $installations = @()
                $installationListProperty = $launcher.PSObject.Properties["InstallationList"]
                $installedListProperty = $launcher.PSObject.Properties["InstalledList"]
                if ($null -ne $installationListProperty) {
                    $installations = @($installationListProperty.Value)
                }
                elseif ($null -ne $installedListProperty) {
                    $installations = @($installedListProperty.Value)
                }

                if (-not [string]::IsNullOrWhiteSpace($association)) {
                    foreach ($item in $installations | Where-Object { $_.AppName -eq ("UE_" + $association) }) {
                        $candidates += [string]$item.InstallLocation
                    }
                }
                foreach ($item in $installations | Where-Object { $_.AppName -like "UE_*" }) {
                    $candidates += [string]$item.InstallLocation
                }
            }
            catch {
                Write-Verbose "Epic Launcher installation metadata could not be parsed."
            }
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:ProgramFiles)) {
        $epicRoot = Join-Path $env:ProgramFiles "Epic Games"
        if (Test-Path -LiteralPath $epicRoot -PathType Container) {
            $candidates += @(Get-ChildItem -LiteralPath $epicRoot -Directory -Filter "UE_*" -ErrorAction SilentlyContinue | Sort-Object Name -Descending | ForEach-Object { $_.FullName })
        }
    }

    # Support projects checked out inside a source-engine tree.
    $walk = $ProjectRoot
    for ($i = 0; $i -lt 6; $i++) {
        $candidates += $walk
        $parent = [System.IO.Directory]::GetParent($walk)
        if ($null -eq $parent) { break }
        $walk = $parent.FullName
    }

    foreach ($candidate in $candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique) {
        try {
            $fullCandidate = [System.IO.Path]::GetFullPath([string]$candidate)
            if (Test-BHEngineRoot -Candidate $fullCandidate) {
                return $fullCandidate.TrimEnd('\', '/')
            }
        }
        catch {
            Write-Verbose "Ignored invalid engine candidate: $candidate"
        }
    }

    throw "Could not locate the Unreal Engine installation for '$($UProject.Name)' (EngineAssociation '$association'). Pass -EngineRoot explicitly, for example -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'."
}

function Get-BHEngineVersion {
    param([Parameter(Mandatory = $true)][string]$EngineRoot)

    $buildVersion = Join-Path $EngineRoot "Engine\Build\Build.version"
    if (Test-Path -LiteralPath $buildVersion -PathType Leaf) {
        try {
            $data = Get-Content -LiteralPath $buildVersion -Raw | ConvertFrom-Json
            $major = $data.PSObject.Properties["MajorVersion"]
            $minor = $data.PSObject.Properties["MinorVersion"]
            $patch = $data.PSObject.Properties["PatchVersion"]
            if ($null -ne $major -and $null -ne $minor -and $null -ne $patch) {
                return "$($major.Value).$($minor.Value).$($patch.Value)"
            }
            return "Unknown"
        }
        catch {
            return "Unknown"
        }
    }
    return "Unknown"
}

function Get-BHTargetName {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][System.IO.FileInfo]$UProject,
        [ValidateSet("Editor", "Game")][string]$Kind = "Editor"
    )

    $sourceRoot = Join-Path $ProjectRoot "Source"
    $targetFiles = @()
    if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
        $targetFiles = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -Filter *.Target.cs -File -ErrorAction SilentlyContinue)
    }

    if ($Kind -eq "Editor") {
        $exact = $targetFiles | Where-Object { $_.Name -eq ($UProject.BaseName + "Editor.Target.cs") } | Select-Object -First 1
        if ($null -ne $exact) {
            return ($exact.Name -replace '\.Target\.cs$', '')
        }
        $editor = $targetFiles | Where-Object { $_.Name -like "*Editor.Target.cs" } | Sort-Object Name | Select-Object -First 1
        if ($null -ne $editor) {
            return ($editor.Name -replace '\.Target\.cs$', '')
        }
        return ($UProject.BaseName + "Editor")
    }

    $exactGame = $targetFiles | Where-Object { $_.Name -eq ($UProject.BaseName + ".Target.cs") } | Select-Object -First 1
    if ($null -ne $exactGame) {
        return ($exactGame.Name -replace '\.Target\.cs$', '')
    }
    $game = $targetFiles | Where-Object { $_.Name -notlike "*Editor.Target.cs" } | Sort-Object Name | Select-Object -First 1
    if ($null -ne $game) {
        return ($game.Name -replace '\.Target\.cs$', '')
    }
    return $UProject.BaseName
}

function Get-BHLogRoot {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $logRoot = Join-Path $ProjectRoot "Saved\Logs\Codex"
    New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
    return $logRoot
}

function ConvertTo-BHCommandLineArgument {
    param([AllowEmptyString()][string]$Value)

    if ($null -eq $Value -or $Value.Length -eq 0) {
        return '""'
    }
    if ($Value -notmatch '[\s"]') {
        return $Value
    }

    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}

function Start-BHProcessWithTimeout {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$StdOutPath,
        [Parameter(Mandatory = $true)][string]$StdErrPath,
        [int]$TimeoutMinutes = 30
    )

    $argumentLine = (($Arguments | ForEach-Object { ConvertTo-BHCommandLineArgument -Value ([string]$_) }) -join ' ')
    $process = Start-Process -FilePath $FilePath -ArgumentList $argumentLine -PassThru -NoNewWindow -RedirectStandardOutput $StdOutPath -RedirectStandardError $StdErrPath
    $finished = $process.WaitForExit([Math]::Max(1, $TimeoutMinutes) * 60 * 1000)

    if (-not $finished) {
        try {
            if ($env:OS -eq "Windows_NT" -and (Get-Command taskkill.exe -ErrorAction SilentlyContinue)) {
                & taskkill.exe /PID $process.Id /T /F *> $null
            }
            else {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            }
        }
        catch {
            try { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } catch { }
        }
        return [pscustomobject]@{
            ExitCode = 124
            TimedOut = $true
            ProcessId = $process.Id
            ArgumentLine = $argumentLine
        }
    }

    $process.WaitForExit()
    $process.Refresh()
    return [pscustomobject]@{
        ExitCode = $process.ExitCode
        TimedOut = $false
        ProcessId = $process.Id
        ArgumentLine = $argumentLine
    }
}

function Write-BHJson {
    param(
        [Parameter(Mandatory = $true)]$InputObject,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $InputObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding UTF8
}
