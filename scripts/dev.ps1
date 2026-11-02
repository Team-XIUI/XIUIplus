param(
    [int]$PollMilliseconds = 350,
    [string]$SdkPath,
    [Parameter(Mandatory)][string]$InstallPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$Root = Split-Path -Parent $PSScriptRoot
$BuildScript = Join-Path $PSScriptRoot 'build_all.ps1'
$ThemeSource = Join-Path $Root 'config\XIUIplus-theme.ini'
$Sdk = Resolve-XiuiSdk -PreferredPath $SdkPath
$AshitaRoot = Resolve-XiuiInstallPath -PreferredPath $InstallPath
$InstalledTheme = Join-Path $AshitaRoot 'config\XIUIplus\theme.ini'

function Get-Fingerprint {
    param([string[]]$Paths)

    $Sha = [Security.Cryptography.SHA256]::Create()
    try {
        $Builder = [Text.StringBuilder]::new()
        foreach ($Path in $Paths | Sort-Object) {
            if (-not (Test-Path $Path)) {
                [void]$Builder.AppendLine("missing|$Path")
                continue
            }
            $Bytes = [IO.File]::ReadAllBytes($Path)
            $Hash = [BitConverter]::ToString($Sha.ComputeHash($Bytes)).Replace('-', '')
            [void]$Builder.AppendLine("$Path|$Hash")
        }
        $Bytes = [Text.Encoding]::UTF8.GetBytes($Builder.ToString())
        return [BitConverter]::ToString($Sha.ComputeHash($Bytes)).Replace('-', '')
    }
    finally {
        $Sha.Dispose()
    }
}

function Publish-Theme {
    Publish-XiuiAtomic -Source $ThemeSource -Destination $InstalledTheme
    Write-Host "[$(Get-Date -Format T)] Theme published; XIUI+ will reload it automatically."
}

function Get-RuntimeFiles {
    return @(Get-ChildItem (Join-Path $Root 'src\runtime') -File -Recurse | ForEach-Object FullName)
}

function Get-StableFiles {
    return @(
        Get-ChildItem (Join-Path $Root 'src\host') -File -Recurse | ForEach-Object FullName
        Get-ChildItem (Join-Path $Root 'src\shared') -File -Recurse | ForEach-Object FullName
    ) + @((Join-Path $Root 'CMakeLists.txt'))
}

$RuntimeFiles = Get-RuntimeFiles
$StableFiles = Get-StableFiles

Write-Host 'XIUI+ development watcher'
Write-Host '  Runtime changes: rebuild and atomically publish XIUIplus-runtime.dll'
Write-Host '  Theme changes: publish immediately; no compilation'
Write-Host 'Press Ctrl+C to stop.'

& $BuildScript -Development -RuntimeOnly -SdkPath $Sdk -InstallPath $AshitaRoot
Publish-Theme

$RuntimeFingerprint = Get-Fingerprint $RuntimeFiles
$StableFingerprint = Get-Fingerprint $StableFiles
$ThemeFingerprint = Get-Fingerprint @($ThemeSource)
$RuntimeDirty = $false
$ThemeDirty = $false
$NextRuntimeAttempt = [DateTime]::MinValue
$NextThemeAttempt = [DateTime]::MinValue

while ($true) {
    Start-Sleep -Milliseconds ([Math]::Max(100, $PollMilliseconds))
    $RuntimeFiles = Get-RuntimeFiles
    $StableFiles = Get-StableFiles
    $CurrentRuntime = Get-Fingerprint $RuntimeFiles
    $CurrentStable = Get-Fingerprint $StableFiles
    $CurrentTheme = Get-Fingerprint @($ThemeSource)

    if ($CurrentStable -ne $StableFingerprint) {
        $StableFingerprint = $CurrentStable
        Write-Warning 'Development host source changed. Unload XIUI+ in the selected development install, then rebuild with -Development and its -InstallPath.'
    }
    if ($CurrentRuntime -ne $RuntimeFingerprint) {
        $RuntimeDirty = $true
    }
    if ($CurrentTheme -ne $ThemeFingerprint) {
        $ThemeDirty = $true
    }

    if ($ThemeDirty -and [DateTime]::UtcNow -ge $NextThemeAttempt) {
        try {
            Publish-Theme
            $ThemeFingerprint = $CurrentTheme
            $ThemeDirty = $false
        }
        catch {
            Write-Warning "Theme publish failed; retrying. $($_.Exception.Message)"
            $NextThemeAttempt = [DateTime]::UtcNow.AddSeconds(2)
        }
    }

    if ($RuntimeDirty -and [DateTime]::UtcNow -ge $NextRuntimeAttempt) {
        try {
            Write-Host "[$(Get-Date -Format T)] Runtime source changed; rebuilding..."
            & $BuildScript -Development -RuntimeOnly -SdkPath $Sdk -InstallPath $AshitaRoot
            $RuntimeFingerprint = $CurrentRuntime
            $RuntimeDirty = $false
        }
        catch {
            Write-Warning "Runtime build failed; the previous generation remains active and this change will be retried. $($_.Exception.Message)"
            $NextRuntimeAttempt = [DateTime]::UtcNow.AddSeconds(2)
        }
    }
}
