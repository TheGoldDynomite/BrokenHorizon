param()

$ErrorActionPreference = 'Stop'

$Repo = "TheGoldDynomite/BrokenHorizon"
$AssetFilter = "*FirstLight-Development*.zip"
$ExecutableAssetFilter = "BrokenHorizon-*.exe"
$AllowExecutableFallback = $false
$VersionPrefix = "v"
$InstallRoot = Join-Path $env:LocalAppData "BrokenHorizon"
$GameExeRelative = "Windows\BrokenHorizon.exe"
$UserAgent = "BrokenHorizonLauncher/1.0"
$PreserveSaved = $true
$ScriptDirectory = Split-Path -Parent $PSCommandPath
$RequireManualLaunch = $true
$BootstrapAttempted = $false
$CanPrompt = $true

$GitHubToken = $env:GITHUB_TOKEN
$ForceUpdate = $false
$ForceUpdateFromTag = $false
$DirectReleaseUrl = ""
$DirectAssetDownloadUrl = ""
$DirectAssetName = ""
$UseBitsTransfer = $true
$SkipGitHub = $false
$ReleaseTagFallback = ""
$ConfigPath = Join-Path $ScriptDirectory "BHLauncherConfig.json"

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Log([string]$msg) {
    Write-Host "[$([DateTime]::Now.ToString('yyyy-MM-dd HH:mm:ss'))] $msg"
}

if (Test-Path $ConfigPath) {
    try {
        $config = Get-Content $ConfigPath -Raw | ConvertFrom-Json
        if ($config.Repo) { $Repo = [string]$config.Repo }
        if ($config.AssetFilter) { $AssetFilter = [string]$config.AssetFilter }
        if ($config.ExecutableAssetFilter) { $ExecutableAssetFilter = [string]$config.ExecutableAssetFilter }
        if ($config.PSObject.Properties.Name -contains "PreserveSaved") { $PreserveSaved = [bool]$config.PreserveSaved }
        if ($config.PSObject.Properties.Name -contains "RequireManualLaunch") { $RequireManualLaunch = [bool]$config.RequireManualLaunch }
        if ($config.PSObject.Properties.Name -contains "ForceUpdate") { $ForceUpdate = [bool]$config.ForceUpdate }
        if ($config.PSObject.Properties.Name -contains "ForceUpdateFromTag") { $ForceUpdateFromTag = [bool]$config.ForceUpdateFromTag }
        if ($config.PSObject.Properties.Name -contains "DirectReleaseUrl") { $DirectReleaseUrl = [string]$config.DirectReleaseUrl }
        if ($config.PSObject.Properties.Name -contains "DirectAssetDownloadUrl") { $DirectAssetDownloadUrl = [string]$config.DirectAssetDownloadUrl }
        if ($config.PSObject.Properties.Name -contains "DirectAssetName") { $DirectAssetName = [string]$config.DirectAssetName }
        if ($config.PSObject.Properties.Name -contains "UseBitsTransfer") { $UseBitsTransfer = [bool]$config.UseBitsTransfer }
        if ($config.PSObject.Properties.Name -contains "SkipGitHub") { $SkipGitHub = [bool]$config.SkipGitHub }
        if ($config.PSObject.Properties.Name -contains "ReleaseTagFallback") { $ReleaseTagFallback = [string]$config.ReleaseTagFallback }
        if ($config.PSObject.Properties.Name -contains "AllowExecutableFallback") { $AllowExecutableFallback = [bool]$config.AllowExecutableFallback }
        Log "Loaded launcher config: $ConfigPath"
    }
    catch {
        Log "Ignoring invalid config file at ${ConfigPath}: $($_.Exception.Message)"
    }
}

if ($env:BH_LAUNCHER_REPO) { $Repo = $env:BH_LAUNCHER_REPO }
if ($env:BH_LAUNCHER_ASSET_FILTER) { $AssetFilter = $env:BH_LAUNCHER_ASSET_FILTER }
if ($env:BH_LAUNCHER_EXE_FILTER) { $ExecutableAssetFilter = $env:BH_LAUNCHER_EXE_FILTER }
if ($env:BH_LAUNCHER_FORCE_UPDATE -eq "1") { $ForceUpdate = $true }
if ($env:BH_LAUNCHER_FORCE_TAG_UPDATE -eq "1") { $ForceUpdateFromTag = $true }
if ($env:BH_LAUNCHER_SKIP_GITHUB -eq "1") { $SkipGitHub = $true }
if ($env:BH_LAUNCHER_DIRECT_URL) { $DirectReleaseUrl = [string]$env:BH_LAUNCHER_DIRECT_URL }
if ($env:BH_LAUNCHER_DIRECT_ASSET_URL) { $DirectAssetDownloadUrl = [string]$env:BH_LAUNCHER_DIRECT_ASSET_URL }
if ($env:BH_LAUNCHER_DIRECT_ASSET_NAME) { $DirectAssetName = [string]$env:BH_LAUNCHER_DIRECT_ASSET_NAME }
if ($env:BH_LAUNCHER_RELEASE_TAG) { $ReleaseTagFallback = [string]$env:BH_LAUNCHER_RELEASE_TAG }
if ($env:BH_LAUNCHER_ALLOW_EXE -eq "1") { $AllowExecutableFallback = $true }
if ($env:BH_LAUNCHER_USE_BITS -eq "0") { $UseBitsTransfer = $false }

function Download-File {
    param(
        [Parameter(Mandatory)][string]$Uri,
        [Parameter(Mandatory)][string]$OutFile,
        [int]$TimeoutSec = 120
    )

    if ($UseBitsTransfer -and (Get-Command Start-BitsTransfer -ErrorAction SilentlyContinue)) {
        try {
            Log "Downloading with BITS (faster/resumable when available): $Uri"
            Start-BitsTransfer -Source $Uri -Destination $OutFile -ErrorAction Stop | Out-Null
            if (Test-Path $OutFile) { return }
            Log "BITS download did not create output; falling back to Invoke-WebRequest."
        }
        catch {
            Log "BITS download failed: $($_.Exception.Message)"
            Log "Falling back to Invoke-WebRequest..."
        }
    }
    Log "Downloading with Invoke-WebRequest: $Uri"
    Invoke-WebRequest -Uri $Uri -OutFile $OutFile -TimeoutSec $TimeoutSec
}

function Validate-RepoFormat([string]$RepoName) {
    if (-not $RepoName -or $RepoName -notmatch '^[^/]+/[^/]+$') {
        throw "Repo must be exactly 'owner/repo'. Current value: '$RepoName'"
    }
}

function Normalize-Version([string]$Version) {
    $v = ($Version -replace '^v', '').Trim()
    if ([string]::IsNullOrWhiteSpace($v)) { return "0.0.0" }
    if ($v -notmatch '^\d+(\.\d+){0,2}$') { return $null }
    return if ([string]::IsNullOrWhiteSpace($v)) { "0.0.0" } else { $v }
}

function Compare-Version([string]$Left, [string]$Right) {
    $ln = Normalize-Version $Left
    $rn = Normalize-Version $Right
    if ($null -ne $ln -and $null -ne $rn) {
        $l = New-Object Version $ln
        $r = New-Object Version $rn
        return [int]($l.CompareTo($r))
    }
    if ($null -eq $ln -and $null -eq $rn) {
        return [string]::Compare($Left, $Right, $true)
    }
    if ($null -eq $ln -and $null -ne $rn) { return -1 }
    if ($null -ne $ln -and $null -eq $rn) { return -1 }
    return 0
}

function Show-RequestHeaders([string]$RepoName) {
    if (-not $GitHubToken) { return }
    Write-Host "Using GitHub auth from GITHUB_TOKEN."
}

function Resolve-Asset([object[]]$Assets, [string]$RemoteVersion, [string]$AssetFilter, [string]$ExeFilter, [bool]$AllowExeFallback) {
    $asset = $Assets | Where-Object { $_.name -like $AssetFilter } | Select-Object -First 1
    if (-not $asset) {
        $asset = $Assets | Where-Object { $_.name -like "*.zip" } | Sort-Object { $_.updated_at } -Descending | Select-Object -First 1
        if ($asset) { Log "No filter match for '$AssetFilter'. Using most recent zip: $($asset.name)" }
    }
    if (-not $asset -and $AllowExeFallback) {
        $asset = $Assets | Where-Object { $_.name -like $ExeFilter -and $_.name -notlike "*-symbols*" } | Select-Object -First 1
        if ($asset) { Log "No zip found. Using executable asset: $($asset.name)" }
    }

    if (-not $asset -and ($RemoteVersion)) {
        $versioned = $Assets | Where-Object { $_.name -match [regex]::Escape($RemoteVersion) } | Select-Object -First 1
        if ($versioned) {
            $asset = $versioned
            Log "No exact filter match. Using version-matched asset: $($asset.name)"
        }
    }
    if (-not $asset) {
        if ($AllowExeFallback) {
            $asset = $Assets | Sort-Object { $_.updated_at } -Descending | Select-Object -First 1
            if ($asset) { Log "Using most recent asset by fallback: $($asset.name)" }
        }
        else {
            $zipFallback = $Assets | Where-Object { $_.name -like "*.zip" } | Sort-Object { $_.updated_at } -Descending | Select-Object -First 1
            if ($zipFallback) {
                $asset = $zipFallback
                Log "No filter match. Forcing zip-only mode: $($asset.name)"
            }
        }
    }

    return $asset
}

function Build-ErrorText([object]$Err) {
    $statusCode = $null
    $statusText = $null
    try {
        if ($Err -and $Err.Exception -and $Err.Exception.Response) {
            $status = [int]$Err.Exception.Response.StatusCode
            $statusCode = "HTTP $status"
            if ($Err.Exception.Response.StatusDescription) {
                $statusText = $Err.Exception.Response.StatusDescription
            }
        }
    }
    catch { }

    $baseMsg = [string]$Err
    if ($statusCode) {
        if ($statusText) { return "$($statusCode) $($statusText): $baseMsg" }
        return "$($statusCode): $baseMsg"
    }
    return $baseMsg
}

function Try-DirectDownload([string]$DirectUrl, [string]$WriteVersion) {
    if ([string]::IsNullOrWhiteSpace($DirectUrl)) {
        return $false
    }

    $uri = [System.Uri]$DirectUrl
    $file = [System.IO.Path]::GetFileName($uri.AbsolutePath)
    if ([string]::IsNullOrWhiteSpace($file)) {
        Log "DirectReleaseUrl is invalid: $DirectUrl"
        return $false
    }

    if (-not [System.Uri]::TryCreate($DirectUrl, [System.UriKind]::Absolute, [ref]$uri)) {
        Log "DirectReleaseUrl is not a valid URL: $DirectUrl"
        return $false
    }

    $tmp = Join-Path $env:TEMP ("BHLauncher_" + [Guid]::NewGuid().ToString())
    New-Item -ItemType Directory -Path $tmp | Out-Null
    try {
        $path = Join-Path $tmp $file
        Log "Trying direct download from configured URL..."
        Download-File -Uri $DirectUrl -OutFile $path -TimeoutSec 120

        $ext = [System.IO.Path]::GetExtension($file).ToLowerInvariant()
        if ($ext -eq ".zip") {
            Install-ZipUpdate -ZipPath $path -InstallRoot $InstallRoot -PreserveSaved $PreserveSaved
        }
        else {
            Install-ExeUpdate -ExeDownloadPath $path -InstallRoot $InstallRoot -TargetRelativePath $GameExeRelative
        }
        $version = if ([string]::IsNullOrWhiteSpace($WriteVersion)) { "0.0.0" } else { $WriteVersion }
        Set-Content -Path $versionFile -Value $version -Encoding UTF8
        Log "Downloaded and installed from direct URL."
        return $true
    }
    catch {
        Log "Direct download failed: $($_.Exception.Message)"
        return $false
    }
    finally {
        Remove-Item -Path $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Resolve-TagAssetFromHtml([string]$TagReleaseUrl, [string]$AssetFilter) {
    try {
        Log "Trying to resolve asset directly from tag page: $TagReleaseUrl"
        $resp = Invoke-WebRequest -Uri $TagReleaseUrl -Method Get -TimeoutSec 20 -ErrorAction Stop
        $html = $resp.Content
        $matches = [regex]::Matches($html, 'href="([^"]*?/releases/download/[^"]+)"', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        if (-not $matches -or $matches.Count -eq 0) {
            throw "No release download links found on tag page."
        }

        $assets = @()
        foreach ($m in $matches) {
            $raw = $m.Groups[1].Value
            if ($raw -match '(^https?:)//') {
                $assetUrl = $raw
            }
            else {
                $assetUrl = "https://github.com$raw"
            }
            $name = [System.IO.Path]::GetFileName($assetUrl)
            if (-not $name) { continue }
            $assets += [PSCustomObject]@{ Name = $name; Url = $assetUrl }
        }

        $filtered = $assets | Where-Object { $_.Name -like $AssetFilter } | Select-Object -First 1
        if (-not $filtered) {
            $filtered = $assets | Where-Object { $_.Name -like "*.zip" } | Select-Object -First 1
            if ($filtered) { Log "No filter match for '$AssetFilter'. Using most recent zip from tag page: $($filtered.Name)" }
        }
        if (-not $filtered) {
            throw "No matching zip asset found on tag page."
        }

        return $filtered.Url
    }
    catch {
        throw "Unable to resolve tag release HTML assets from '$TagReleaseUrl': $($_.Exception.Message)"
    }
}

function Resolve-TagAssetFromApi([string]$TagReleaseUrl, [string]$ExactAssetName, [string]$AssetFilter) {
    $tagName = Extract-ReleaseTagFrom-Url $TagReleaseUrl
    if ([string]::IsNullOrWhiteSpace($tagName)) {
        throw "Could not extract release tag from '$TagReleaseUrl'."
    }

    $tagRelease = Get-ReleaseByTag -RepoName $Repo -TagName $tagName
    if (-not $tagRelease) {
        throw "Release '$tagName' was not returned by GitHub API."
    }

    $tagAssets = @($tagRelease.assets)
    if (-not $tagAssets -or $tagAssets.Count -eq 0) {
        throw "Release '$tagName' has no assets."
    }

    if (-not [string]::IsNullOrWhiteSpace($ExactAssetName)) {
        $exact = $tagAssets | Where-Object { $_.name -ieq $ExactAssetName } | Select-Object -First 1
        if ($exact) {
            return $exact.browser_download_url
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($AssetFilter)) {
        $filtered = $tagAssets | Where-Object { $_.name -like $AssetFilter } | Select-Object -First 1
        if ($filtered) {
            return $filtered.browser_download_url
        }
    }

    $zipFallback = $tagAssets | Where-Object { $_.name -like "*.zip" } | Sort-Object { $_.updated_at } -Descending | Select-Object -First 1
    if ($zipFallback) {
        return $zipFallback.browser_download_url
    }

    throw "No matching asset found in release '$tagName'."
}

function Get-GitHubRelease([string]$RepoName) {
    $headers = @{ "User-Agent" = $UserAgent; "Accept" = "application/vnd.github+json" }
    if ($GitHubToken) { $headers["Authorization"] = "Bearer $GitHubToken" }
    $url = "https://api.github.com/repos/$RepoName/releases/latest"
    Log "GET $url"
    try {
        return Invoke-RestMethod -Uri $url -Headers $headers -Method Get -TimeoutSec 20 -ErrorAction Stop
    }
    catch {
        $msg = Build-ErrorText $_
        if ($msg -match "HTTP 404") {
            Log "GitHub returned 404 for release endpoint. Please confirm BH_LAUNCHER_REPO is exact owner/repo and the release exists."
            Log "If this repo is private, set GITHUB_TOKEN and add releases there."
            Log "Tip: open https://github.com/$RepoName/releases and check that a release is published (not draft)."
        }
        Log "GET latest failed: $msg"

        if ($GitHubToken) {
            throw
        }

        try {
            Log "Retrying with public access headers only..."
            $anonHeaders = @{ "User-Agent" = $UserAgent; "Accept" = "application/vnd.github+json" }
            return Invoke-RestMethod -Uri $url -Headers $anonHeaders -Method Get -TimeoutSec 20 -ErrorAction Stop
        }
        catch {
            $msg2 = Build-ErrorText $_
            throw "Unable to query latest release for '$RepoName'. $msg. Retry details: $msg2. Check repo exists, is public, and that this app can access api.github.com."
        }
    }
}

function Get-First-Release([string]$RepoName) {
    $base = "https://api.github.com/repos/$RepoName/releases"
    $headers = @{ "User-Agent" = $UserAgent; "Accept" = "application/vnd.github+json" }
    if ($GitHubToken) { $headers["Authorization"] = "Bearer $GitHubToken" }
    try {
        Log "GET $base"
        $releases = Invoke-RestMethod -Uri $base -Headers $headers -Method Get -TimeoutSec 20 -ErrorAction Stop
        if ($releases -and $releases.Count -gt 0) {
            return $releases | Sort-Object -Property published_at -Descending | Select-Object -First 1
        }
        return $null
    }
    catch {
        $msg = Build-ErrorText $_
        throw "Unable to query releases list for '$RepoName'. $msg."
    }
}

function Get-ReleaseByTag([string]$RepoName, [string]$TagName) {
    $base = "https://api.github.com/repos/$RepoName/releases/tags/$TagName"
    $headers = @{ "User-Agent" = $UserAgent; "Accept" = "application/vnd.github+json" }
    if ($GitHubToken) { $headers["Authorization"] = "Bearer $GitHubToken" }
    try {
        Log "GET $base"
        return Invoke-RestMethod -Uri $base -Headers $headers -Method Get -TimeoutSec 20 -ErrorAction Stop
    }
    catch {
        $msg = Build-ErrorText $_
        throw "Unable to query release tag '$TagName' for '$RepoName'. $msg."
    }
}

function Resolve-ReleaseByTag([string]$RepoName, [string]$TagName, [string]$AssetFilter, [string]$ExecutableFilter, [bool]$AllowExeFallback) {
    Validate-RepoFormat $RepoName
    Log "Trying release tag fallback: $TagName"
    $tagRelease = Get-ReleaseByTag -RepoName $RepoName -TagName $TagName
    if (-not $tagRelease -or -not $tagRelease.tag_name) { throw "No release found for tag '$TagName'." }

    $tagAssets = @($tagRelease.assets)
    if (-not $tagAssets -or $tagAssets.Count -eq 0) { throw "Release '$TagName' has no assets." }

    $asset = $tagAssets | Where-Object { $_.name -like $AssetFilter } | Select-Object -First 1
    if (-not $asset) {
        $asset = $tagAssets | Where-Object { $_.name -like "*.zip" } | Sort-Object { $_.updated_at } -Descending | Select-Object -First 1
        if ($asset) { Log "No filter match for '$AssetFilter'. Using most recent zip: $($asset.name)" }
    }
    if (-not $asset -and $AllowExeFallback) {
        $asset = $tagAssets | Where-Object { $_.name -like $ExecutableFilter -and $_.name -notlike "*-symbols*" } | Select-Object -First 1
        if ($asset) { Log "No zip found. Using executable asset: $($asset.name)" }
    }
    if (-not $asset) { throw "No matching asset in release tag '$TagName'." }

    $downloadExt = [System.IO.Path]::GetExtension([string]$asset.name).ToLowerInvariant()
    return [PSCustomObject]@{
        RemoteVersion = [string]$tagRelease.tag_name
        Asset = $asset
        IsZip = ($downloadExt -eq ".zip")
    }
}

function Extract-ReleaseTagFrom-Url([string]$Url) {
    if ([string]::IsNullOrWhiteSpace($Url)) { return "" }
    if ($Url -match "/releases/tag/([^/]+)$") { return $Matches[1] }
    if ($Url -match "/releases/tag/([^/]+)/?$") { return $Matches[1] }
    return ""
}

function Build-TagAssetUrl([string]$TagUrl, [string]$AssetName) {
    if ([string]::IsNullOrWhiteSpace($TagUrl) -or [string]::IsNullOrWhiteSpace($AssetName)) {
        throw "DirectAssetName and tag URL are both required."
    }
    if ($TagUrl -notmatch "/releases/tag/") {
        throw "DirectReleaseUrl must be a valid GitHub tag URL."
    }
    if ($TagUrl -match "/releases/tag/([^/?#]+)") {
        $owner = $Repo.Split("/")[0]
        $repo = $Repo.Split("/")[1]
        $tag = $Matches[1]
    }
    else {
        throw "DirectReleaseUrl must be a valid GitHub tag URL."
    }
    return "https://github.com/$owner/$repo/releases/download/$tag/$AssetName"
}

function Try-Resolve-DirectRelease([string]$DirectUrl, [string]$AssetName, [string]$DefaultExtensionAssetName) {
    $url = $DirectUrl.Trim()
    if ([string]::IsNullOrWhiteSpace($url)) { return "" }

    if ($url -match "/releases/tag/") {
        if ([string]::IsNullOrWhiteSpace($AssetName)) {
            throw "DirectAssetName is required for tag URL workflows."
        }
        if (-not $SkipGitHub) {
            try {
                return Resolve-TagAssetFromApi -TagReleaseUrl $url -ExactAssetName $AssetName -AssetFilter $AssetFilter
            }
            catch {
                Log "Direct tag API lookup failed: $($_.Exception.Message)"
            }
        }

        return Resolve-TagAssetFromHtml -TagReleaseUrl $url -AssetFilter $AssetFilter
    }

    if ($url -match '^https?://github\.com/[^/]+/[^/]+/releases/download/.+/.+$') {
        return $url
    }

    if ($url -match '\.(zip|exe)$') {
        return $url
    }

    if (-not [string]::IsNullOrWhiteSpace($DefaultExtensionAssetName)) {
        return (Join-Path $url $DefaultExtensionAssetName)
    }

    throw "DirectReleaseUrl '$url' is not a supported direct download URL. Expected a GitHub tag URL or a GitHub release download asset URL."
}

function Get-TagFrom-DownloadUrl([string]$ReleaseDownloadUrl) {
    if ([string]::IsNullOrWhiteSpace($ReleaseDownloadUrl)) { return "" }
    if ($ReleaseDownloadUrl -notmatch '^https?://github\.com/[^/]+/[^/]+/releases/download/([^/]+)/.+$') { return "" }
    return $Matches[1]
}

function Try-Resolve-TagFrom-Url([string]$DirectUrl) {
    if ([string]::IsNullOrWhiteSpace($DirectUrl)) { return "" }
    if ($DirectUrl -match '^https?://github\.com/([^/]+)/([^/]+)/releases/tag/([^/]+)') {
        return "https://github.com/$($Matches[1])/$($Matches[2])/releases/tag/$($Matches[3])"
    }
    if ($DirectUrl -match '^https?://github\.com/([^/]+)/([^/]+)/releases/download/[^/]+/.+$') {
        $owner = $Matches[1]
        $repo = $Matches[2]
        $tag = Get-TagFrom-DownloadUrl -ReleaseDownloadUrl $DirectUrl
        if (-not [string]::IsNullOrWhiteSpace($tag)) {
            return "https://github.com/$owner/$repo/releases/tag/$tag"
        }
    }
    return ""
}

function Show-FallbackPaths {
    Write-Host "If this is first launch, place your packaged game in:"
    Write-Host "  $InstallRoot\Windows\"
    $projectFallbacks = @(
        "..\..\..\Builds\FirstLight-Development\Windows\",
        "..\..\..\Builds\FirstLight-Development\WindowsNoEditor\BrokenHorizon\Binaries\Win64\",
        "..\..\..\BrokenHorizon\Builds\FirstLight-Development\Windows\"
    )
    foreach ($rel in $projectFallbacks) {
        $path = Resolve-Path (Join-Path $ScriptDirectory $rel) -ErrorAction SilentlyContinue
        if ($path) { Write-Host "  $($path.ProviderPath)" }
    }
    Write-Host ""
}

function Fail([string]$msg) {
    Log $msg
    Show-FallbackPaths
    Write-Host "Script will now stop."
    exit 1
}

function Show-LauncherPrompt([string]$ExecutablePath, [string]$LocalVersion, [string]$RemoteVersion) {
    if (-not $CanPrompt) {
        Log "Non-interactive environment detected. Launching automatically."
        return
    }

    Write-Host ""
    Write-Host "======================================"
    Write-Host "        BROKEN HORIZON LAUNCHER"
    Write-Host "======================================"
    Write-Host "Local build:  $LocalVersion"
    if (-not [string]::IsNullOrWhiteSpace($RemoteVersion)) {
        Write-Host "Remote build: $RemoteVersion"
    }
    Write-Host "Found game:  $ExecutablePath"
    Write-Host ""
    Write-Host "Press Enter to launch, or Q to quit."
    try {
        $choice = Read-Host ">"
        if ($choice -match '^(q|Q)$') {
            Log "Cancelled by user."
            exit 0
        }
    }
    catch {
        Log "Prompt input unavailable in this environment. Launching automatically."
    }
}

function Is-ValidPE([string]$Path) {
    if (-not (Test-Path $Path)) { return $false }
    try {
        $bytes = [System.IO.File]::ReadAllBytes($Path)
        return ($bytes.Length -ge 2 -and $bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A)
    }
    catch {
        return $false
    }
}

function Resolve-GameExecutable([string]$InstallRootToCheck, [string]$ScriptDir) {
    $invalidCandidates = @()
    $candidates = @(
        (Join-Path $InstallRootToCheck $GameExeRelative),
        (Join-Path $InstallRootToCheck "Windows\BrokenHorizon-Win64-Development.exe"),
        (Join-Path $InstallRootToCheck "Windows\BrokenHorizon-Win64-Shipping.exe"),
        (Join-Path $InstallRootToCheck "WindowsNoEditor\BrokenHorizon\Binaries\Win64\BrokenHorizon.exe"),
        (Join-Path $InstallRootToCheck "WindowsNoEditor\BrokenHorizon-Win64-Shipping.exe"),
        (Join-Path $InstallRootToCheck "BrokenHorizon.exe")
    )

    $relativeCandidates = @(
        "..\..\..\Builds\FirstLight-Development\Windows\BrokenHorizon.exe",
        "..\..\..\Builds\FirstLight-Development\Windows\BrokenHorizon-Win64-Development.exe",
        "..\..\..\Builds\FirstLight-Development\WindowsNoEditor\BrokenHorizon\Binaries\Win64\BrokenHorizon.exe",
        "..\..\..\Builds\FirstLight-Development\WindowsNoEditor\BrokenHorizon\Binaries\Win64\BrokenHorizon-Win64-Shipping.exe",
        "..\..\..\Builds\FirstLight-Development\WindowsNoEditor\BrokenHorizon\Binaries\Win64\BrokenHorizon-Win64-Development.exe",
        "..\..\..\BrokenHorizon\Builds\FirstLight-Development\Windows\BrokenHorizon.exe"
    )

    foreach ($rel in $relativeCandidates) {
        $candidate = Resolve-Path (Join-Path $ScriptDir $rel) -ErrorAction SilentlyContinue
        if ($candidate) { $candidates += $candidate.ProviderPath }
    }

    foreach ($path in $candidates | Select-Object -Unique) {
        if (-not (Test-Path $path)) { continue }
        if (Is-ValidPE -Path $path) {
            return $path
        }
        $invalidCandidates += $path
    }

    # Last resort: quick filename search in likely folders only
    $searchRoots = @(
        $InstallRootToCheck,
        $ScriptDir,
        (Split-Path (Resolve-Path (Join-Path $ScriptDir "..") -ErrorAction SilentlyContinue) -ErrorAction SilentlyContinue),
        (Split-Path (Resolve-Path (Join-Path $ScriptDir "..\..\..") -ErrorAction SilentlyContinue) -ErrorAction SilentlyContinue),
        (Split-Path (Resolve-Path (Join-Path $ScriptDir "..\..\..\..") -ErrorAction SilentlyContinue) -ErrorAction SilentlyContinue)
    )
    foreach ($root in $searchRoots) {
        if (-not $root) { continue }
        $hits = Get-ChildItem -Path $root -Recurse -File -Filter "*BrokenHorizon*.exe" -ErrorAction SilentlyContinue
        foreach ($hit in $hits) {
            if (Is-ValidPE -Path $hit.FullName) {
                return $hit.FullName
            }
            $invalidCandidates += $hit.FullName
        }
    }

    if ($invalidCandidates.Count -gt 0) {
        Log "Skipping invalid exe candidates:"
        foreach ($bad in $invalidCandidates | Select-Object -Unique) {
            Log " - $bad"
        }
    }

    return $null
}

function Bootstrap-LocalBuild([string]$InstallRootToCheck, [string]$ScriptDir) {
    if ($BootstrapAttempted) {
        return $null
    }

    $BootstrapAttempted = $true

    $bootstrapSources = @(
        (Join-Path $ScriptDir "..\..\..\Builds\FirstLight-Development\Windows\BrokenHorizon.exe"),
        (Join-Path $ScriptDir "..\..\..\Builds\FirstLight-Development\Windows\BrokenHorizon-Win64-Development.exe"),
        (Join-Path $ScriptDir "..\..\..\Builds\FirstLight-Development\WindowsNoEditor\BrokenHorizon\Binaries\Win64\BrokenHorizon.exe")
    )

    foreach ($sourceRel in $bootstrapSources) {
        $resolvedSource = Resolve-Path $sourceRel -ErrorAction SilentlyContinue
        if (-not $resolvedSource) { continue }

        $sourcePath = $resolvedSource.ProviderPath
        if (-not (Test-Path $sourcePath)) { continue }
        if (-not (Is-ValidPE -Path $sourcePath)) {
            Log "Skipping non-PE bootstrap source: $sourcePath"
            continue
        }

        Log "Bootstrapping installer from local build: $sourcePath"
        $targetPath = Join-Path $InstallRootToCheck $GameExeRelative
        $targetDir = Split-Path -Parent $targetPath
        if (-not (Test-Path $targetDir)) {
            New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        }
        Copy-Item -Path $sourcePath -Destination $targetPath -Force
        return $targetPath
    }

    return $null
}

function Launch-Game([string]$ExecutablePath) {
    if (-not (Test-Path $ExecutablePath)) {
        Fail "Game executable not found: $ExecutablePath"
    }
    if (-not (Is-ValidPE -Path $ExecutablePath)) {
        Fail "Target is not a valid executable: $ExecutablePath"
    }

    Log "Launching $ExecutablePath"
    Start-Process -FilePath $ExecutablePath -WorkingDirectory (Split-Path $ExecutablePath)
    Log "Game started."
    exit 0
}

function Install-ZipUpdate([string]$ZipPath, [string]$InstallRoot, [bool]$PreserveSaved) {
    $pkgDir = Join-Path (Split-Path $ZipPath -Parent) "unpack"
    New-Item -ItemType Directory -Path $pkgDir -Force | Out-Null
    Expand-Archive -Path $ZipPath -DestinationPath $pkgDir -Force

    if ($PreserveSaved -and (Test-Path (Join-Path $InstallRoot "Saved"))) {
        $savedBackup = Join-Path (Split-Path $ZipPath -Parent) "Saved"
        Copy-Item -Path (Join-Path $InstallRoot "Saved") -Destination $savedBackup -Recurse -Force
    }

    Robocopy $pkgDir $InstallRoot /MIR /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) {
        throw "Robocopy failed with exit code $LASTEXITCODE."
    }

    if ($PreserveSaved -and (Test-Path (Join-Path (Split-Path $ZipPath -Parent) "Saved"))) {
        Copy-Item -Path (Join-Path (Split-Path $ZipPath -Parent) "Saved") -Destination (Join-Path $InstallRoot "Saved") -Recurse -Force
    }
}

function Install-ExeUpdate([string]$ExeDownloadPath, [string]$InstallRoot, [string]$TargetRelativePath) {
    $targetDir = Split-Path -Parent (Join-Path $InstallRoot $TargetRelativePath)
    if (-not (Test-Path $targetDir)) {
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    }
    Copy-Item -Path $ExeDownloadPath -Destination (Join-Path $InstallRoot $TargetRelativePath) -Force
    Log "Installed executable update to $InstallRoot\$TargetRelativePath"
}

try {
    $CanPrompt = [System.Environment]::UserInteractive -and (-not [Console]::IsInputRedirected)

    if (-not (Test-Path $InstallRoot)) {
        New-Item -ItemType Directory -Path $InstallRoot | Out-Null
    }

    $gamePath = Resolve-GameExecutable -InstallRootToCheck $InstallRoot -ScriptDir $ScriptDirectory
    $versionFile = Join-Path $InstallRoot "version.txt"

    if (-not $gamePath) {
        $bootstrapped = Bootstrap-LocalBuild -InstallRootToCheck $InstallRoot -ScriptDir $ScriptDirectory
        if ($bootstrapped) {
            $gamePath = $bootstrapped
            Log "Bootstrap complete: $(Split-Path -Leaf $InstallRoot)."
        } else {
            Fail "No executable found yet."
        }
    }

    $localVersion = "0.0.0"
    if (Test-Path $versionFile) {
        $v = (Get-Content $versionFile -ErrorAction SilentlyContinue).Trim()
        if (-not [string]::IsNullOrWhiteSpace($v)) { $localVersion = $v }
    }
    Log "Local build: $localVersion"
    Log "Launcher repo: $Repo"
    Log "Asset filter: $AssetFilter"
    Log "Executable filter: $ExecutableAssetFilter"
    Log "Allow executable fallback: $AllowExecutableFallback"

    $remoteVersion = $localVersion
    $downloadUrl = ""
    $downloadName = ""
    $usedDirectDownload = $false
    $usedTagDownload = $false
    $directTagVersion = if ([string]::IsNullOrWhiteSpace($ReleaseTagFallback)) { $localVersion } else { $ReleaseTagFallback }

    if (-not $ReleaseTagFallback -and $DirectReleaseUrl) {
        $ReleaseTagFallback = Extract-ReleaseTagFrom-Url $DirectReleaseUrl
        if ($ReleaseTagFallback) {
            Log "Detected release tag from URL: $ReleaseTagFallback"
        }
    }

    if ($DirectReleaseUrl) {
        Log "Using DirectReleaseUrl override."
        $tmp = ""
        try {
            if (-not [string]::IsNullOrWhiteSpace($DirectAssetDownloadUrl)) {
                Log "Using explicit DirectAssetDownloadUrl: $DirectAssetDownloadUrl"
                if (-not (Try-DirectDownload -DirectUrl $DirectAssetDownloadUrl -WriteVersion $directTagVersion)) {
                    throw "Direct asset URL download failed."
                }
                $downloadFromTagUrl = $DirectAssetDownloadUrl
                $downloadName = [System.IO.Path]::GetFileName(($DirectAssetDownloadUrl -split '\?')[0])
                $usedDirectDownload = $true
                $downloadUrl = $downloadFromTagUrl
                $directTagVersion = if ([string]::IsNullOrWhiteSpace($directTagVersion)) { $localVersion } else { $directTagVersion }
                Set-Content -Path $versionFile -Value $directTagVersion -Encoding UTF8
            } else {
                if (-not [string]::IsNullOrWhiteSpace($DirectAssetName)) {
                    try {
                        $downloadFromTagUrl = Try-Resolve-DirectRelease -DirectUrl $DirectReleaseUrl -AssetName $DirectAssetName -DefaultExtensionAssetName $DirectAssetName
                        Log "Trying direct release URL: $downloadFromTagUrl"
                    }
                    catch {
                        if ($DirectReleaseUrl.ToLowerInvariant().Contains("/releases/tag/") -and -not $SkipGitHub) {
                            throw
                        }
                        Log "Could not convert DirectReleaseUrl with DirectAssetName: $($_.Exception.Message)"
                        Log "Falling back to tag page parser (if reachable)."
                        $downloadFromTagUrl = Resolve-TagAssetFromHtml -TagReleaseUrl $DirectReleaseUrl -AssetFilter $AssetFilter
                    }
                }
                else {
                    $downloadFromTagUrl = Resolve-TagAssetFromHtml -TagReleaseUrl $DirectReleaseUrl -AssetFilter $AssetFilter
                }
            }

            if ([string]::IsNullOrWhiteSpace($downloadFromTagUrl)) {
                throw "Could not resolve a download URL from DirectReleaseUrl."
            }

            $downloadName = [System.IO.Path]::GetFileName(($downloadFromTagUrl -split '\?')[0])
            if ([string]::IsNullOrWhiteSpace($downloadName) -and -not [string]::IsNullOrWhiteSpace($DirectAssetName)) {
                $downloadName = $DirectAssetName
            }
            if ([string]::IsNullOrWhiteSpace($downloadName)) {
                throw "Could not determine download filename from DirectReleaseUrl."
            }

            $tmp = Join-Path $env:TEMP ("BHLauncher_" + [Guid]::NewGuid().ToString())
            New-Item -ItemType Directory -Path $tmp | Out-Null
            $path = Join-Path $tmp $downloadName
            Log "Downloading direct release asset from resolved URL: $downloadFromTagUrl"
            Download-File -Uri $downloadFromTagUrl -OutFile $path -TimeoutSec 120
            $downloadExt = [System.IO.Path]::GetExtension(($downloadName -split '\?')[0]).ToLowerInvariant()
            if ($downloadExt -eq ".zip") {
                Install-ZipUpdate -ZipPath $path -InstallRoot $InstallRoot -PreserveSaved $PreserveSaved
            }
            else {
                Install-ExeUpdate -ExeDownloadPath $path -InstallRoot $InstallRoot -TargetRelativePath $GameExeRelative
            }
            Set-Content -Path $versionFile -Value $directTagVersion -Encoding UTF8
            Log "Direct release asset downloaded from: $downloadFromTagUrl"
            $remoteVersion = $directTagVersion
            $usedDirectDownload = $true
            $downloadUrl = $downloadFromTagUrl
        }
        catch {
            Log "Direct release URL install failed: $($_.Exception.Message)"
            if ($tmp -and (Test-Path $tmp)) {
                Remove-Item -Path $tmp -Recurse -Force -ErrorAction SilentlyContinue
                    }

            $tagPageAssetUrl = ""
            if (-not [string]::IsNullOrWhiteSpace($DirectAssetName) -or -not [string]::IsNullOrWhiteSpace($AssetFilter)) {
                try {
                    $tagUrlForRecovery = if (-not [string]::IsNullOrWhiteSpace($ReleaseTagFallback)) {
                        "https://github.com/$Repo/releases/tag/$ReleaseTagFallback"
                    } else {
                        Try-Resolve-TagFrom-Url -DirectUrl $DirectReleaseUrl
                    }

                    if (-not [string]::IsNullOrWhiteSpace($tagUrlForRecovery) -and $tagUrlForRecovery -match "releases/tag/([^/]+)") {
                        if (-not [string]::IsNullOrWhiteSpace($DirectAssetName)) {
                            try {
                                if (-not $SkipGitHub) {
                                    $tagPageAssetUrl = Resolve-TagAssetFromApi -TagReleaseUrl $tagUrlForRecovery -ExactAssetName $DirectAssetName -AssetFilter $AssetFilter
                                }
                            }
                            catch {
                                Log "Could not build release tag asset URL from DirectAssetName: $($_.Exception.Message)"
                            }
                        }
                        if (-not $tagPageAssetUrl) {
                            $tagPageAssetUrl = Resolve-TagAssetFromHtml -TagReleaseUrl $tagUrlForRecovery -AssetFilter $AssetFilter
                        }
                    }
                }
                catch {
                    Log "Recovery tag-page resolution failed: $($_.Exception.Message)"
                }
            }

            if (-not [string]::IsNullOrWhiteSpace($tagPageAssetUrl)) {
                $tmp = Join-Path $env:TEMP ("BHLauncher_" + [Guid]::NewGuid().ToString())
                try {
                    New-Item -ItemType Directory -Path $tmp | Out-Null
                    $path = Join-Path $tmp ([System.IO.Path]::GetFileName($tagPageAssetUrl))
                    Log "Retrying fallback via direct tag asset URL: $tagPageAssetUrl"
                    Download-File -Uri $tagPageAssetUrl -OutFile $path -TimeoutSec 120
                    $downloadExt = [System.IO.Path]::GetExtension($path).ToLowerInvariant()
                    if ($downloadExt -eq ".zip") {
                        Install-ZipUpdate -ZipPath $path -InstallRoot $InstallRoot -PreserveSaved $PreserveSaved
                    }
                    else {
                        Install-ExeUpdate -ExeDownloadPath $path -InstallRoot $InstallRoot -TargetRelativePath $GameExeRelative
                    }
                    Set-Content -Path $versionFile -Value $directTagVersion -Encoding UTF8
                    $remoteVersion = $directTagVersion
                    Log "Direct tag-asset download installed from: $tagPageAssetUrl"
                    $usedDirectDownload = $true
                    $downloadUrl = $tagPageAssetUrl
                }
                catch {
                    Log "Direct tag-asset retry failed: $($_.Exception.Message)"
                }
                finally {
                    if ($tmp -and (Test-Path $tmp)) { Remove-Item -Path $tmp -Recurse -Force -ErrorAction SilentlyContinue }
                }
            }
        }
    }

    if (-not $usedDirectDownload -and -not [string]::IsNullOrWhiteSpace($ReleaseTagFallback) -and -not $SkipGitHub) {
        try {
            $resolvedTag = Resolve-ReleaseByTag -RepoName $Repo -TagName $ReleaseTagFallback -AssetFilter $AssetFilter -ExecutableFilter $ExecutableAssetFilter -AllowExeFallback $AllowExecutableFallback
            if ($resolvedTag) {
                $remoteVersion = [string]$resolvedTag.RemoteVersion
                $downloadUrl = [string]$resolvedTag.Asset.browser_download_url
                $downloadName = [string]$resolvedTag.Asset.name
                $isZip = [bool]$resolvedTag.IsZip
                Log "Using tagged release asset: $downloadName"
                $usedTagDownload = $true
            }
        }
        catch {
            Log "Release tag lookup failed: $($_.Exception.Message)"
        }
    }

    if (-not $usedDirectDownload -and -not $SkipGitHub -and [string]::IsNullOrWhiteSpace($downloadUrl)) {
        try {
            Log "Checking GitHub releases for $Repo ..."
            Show-RequestHeaders -RepoName $Repo
            $release = $null
            try {
                $release = Get-GitHubRelease -RepoName $Repo
            }
            catch {
                Log "Falling back to listing releases endpoint..."
                $release = Get-First-Release -RepoName $Repo
                if (-not $release) {
                    throw $_
                }
            }

            if ($release -and $release.tag_name) {
                $remoteVersion = [string]$release.tag_name
                Log "Latest online: $remoteVersion"

                $assets = @($release.assets)
                Log "Release contains $($assets.Count) assets."
                $assetNames = $assets | ForEach-Object { $_.name }
                if ($assetNames -and $assetNames.Count -gt 0) {
                    Log "Available assets:"
                    foreach ($name in $assetNames) { Log " - $name" }
                }
                $asset = Resolve-Asset -Assets $assets -RemoteVersion $remoteVersion -AssetFilter $AssetFilter -ExeFilter $ExecutableAssetFilter -AllowExeFallback $AllowExecutableFallback
                if (-not $asset) {
                    throw "No asset found in latest release."
                }

                $downloadUrl = [string]$asset.browser_download_url
                $downloadName = [string]$asset.name
                $downloadExt = [System.IO.Path]::GetExtension($downloadName).ToLowerInvariant()
                $isZip = ($downloadExt -eq ".zip")
                Log "Found asset: $downloadName"
            }
        }
        catch {
            Log "GitHub check failed: $($_.Exception.Message)"
            Log "Online update skipped. Continuing with local installed game."
        }
    }
    elseif ($usedDirectDownload) {
        Log "Direct update completed."
    }

    if ([string]::IsNullOrWhiteSpace($downloadUrl)) {
        if ($RequireManualLaunch) {
            Show-LauncherPrompt -ExecutablePath $gamePath -LocalVersion $localVersion -RemoteVersion $remoteVersion
        }
        Launch-Game $gamePath
    }

    $needsUpdate = $ForceUpdate -or (Compare-Version $localVersion $remoteVersion) -lt 0
    if (-not $needsUpdate -and $usedTagDownload -and $ForceUpdateFromTag) {
        $needsUpdate = $true
        Log "Forcing update because this launcher is pinned to a release tag."
    }

    if ($needsUpdate) {
        Log "Update available, downloading..."
        $tmp = Join-Path $env:TEMP ("BHLauncher_" + [Guid]::NewGuid().ToString())
        New-Item -ItemType Directory -Path $tmp | Out-Null
        $zipPath = Join-Path $tmp $downloadName
        $isZip = if ($null -ne $isZip) { $isZip } else { $true }

        try {
            Download-File -Uri $downloadUrl -OutFile $zipPath -TimeoutSec 120

            if ($isZip) {
                Install-ZipUpdate -ZipPath $zipPath -InstallRoot $InstallRoot -PreserveSaved $PreserveSaved
            }
            else {
                Install-ExeUpdate -ExeDownloadPath $zipPath -InstallRoot $InstallRoot -TargetRelativePath $GameExeRelative
            }

            Set-Content -Path $versionFile -Value $remoteVersion -Encoding UTF8
            Log "Updated to $remoteVersion"
        }
        catch {
            Log "Update failed: $($_.Exception.Message)"
            Log "Using local game instead."
        }
        finally {
            Remove-Item -Path $tmp -Recurse -Force -ErrorAction SilentlyContinue
        }
    } else {
        Log "Already on latest version."
    }

    # Refresh executable path in case update changed structure
    $gamePath = Resolve-GameExecutable -InstallRootToCheck $InstallRoot -ScriptDir $ScriptDirectory
    if (-not $gamePath) {
        Fail "Game executable missing after update. Re-run package build or reinstall."
    }

    if ($RequireManualLaunch) {
        Show-LauncherPrompt -ExecutablePath $gamePath -LocalVersion $localVersion -RemoteVersion $remoteVersion
    }

    Launch-Game $gamePath
}
catch {
    Fail "Launcher failed: $($_.Exception.Message)"
}




