param(
    [string]$InstallPath,
    [switch]$FromSaved,
    [string]$SettingsPath,
    [string]$ThemePath,
    [string]$OutputPath,
    [string]$ThemeOutputPath,
    # Script that forwards commands to the running game and prints JSON replies; defaults to XIUI_GAME_CLI.
    [string]$GameCli = $env:XIUI_GAME_CLI,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$Root = Split-Path -Parent $PSScriptRoot
if (-not $FromSaved -and $SettingsPath) { throw 'Use -FromSaved when selecting an existing settings file.' }
if (-not $SettingsPath) {
    $PhoenixInstall = Resolve-XiuiInstallPath -PreferredPath $InstallPath
    $SettingsPath = Join-Path $PhoenixInstall 'config/XIUIplus/settings.ini'
    foreach ($LegacySettings in @((Join-Path $PhoenixInstall 'config/xiuinative/settings.ini'), (Join-Path $PhoenixInstall 'config/xiuinative.ini'))) {
        if (-not (Test-Path -LiteralPath $SettingsPath) -and (Test-Path -LiteralPath $LegacySettings)) { $SettingsPath = $LegacySettings }
    }
}
if (-not $ThemePath) {
    $ThemeName = if ((Split-Path -Leaf $SettingsPath) -eq 'xiuinative.ini') { 'xiuinative-theme.ini' } else { 'theme.ini' }
    $ThemePath = Join-Path (Split-Path -Parent $SettingsPath) $ThemeName
}
if (-not $OutputPath) { $OutputPath = Join-Path $Root 'config/XIUIplus-defaults.ini' }
if (-not $ThemeOutputPath) { $ThemeOutputPath = Join-Path $Root 'config/XIUIplus-theme.ini' }
if (-not $FromSaved) {
    if (-not $GameCli -or -not (Test-Path -LiteralPath $GameCli)) { throw 'A live capture needs a game command script: pass -GameCli or set XIUI_GAME_CLI. Otherwise use -FromSaved with a saved settings file.' }
    $Probe = @(& $GameCli '?state','?loaded XIUIplus' -Game $PhoenixInstall)
    $Loaded = @($Probe | Where-Object { $_ -is [string] -and $_.StartsWith('{') } | ForEach-Object { $_ | ConvertFrom-Json } | Where-Object { $_.step -eq '?loaded XIUIplus' -and $_.loaded })
    if ($Loaded.Count -ne 1) { throw 'The game did not confirm that XIUI+ is loaded. The defaults were not changed.' }
    $Save = (& $GameCli '/xiuiplus save sync','wait 200' -Game $PhoenixInstall) -join "`n"
    if ($Save -notmatch '\[XIUI\+\] Current settings saved\.') { throw "XIUI+ did not confirm a completed save. $Save" }
}
if (-not (Test-Path -LiteralPath $SettingsPath) -or -not (Test-Path -LiteralPath $ThemePath)) { throw 'The settings and theme must both exist.' }
if (-not $SkipBuild) { Invoke-XiuiBuild -Targets xiui_capture_defaults }
$Capture = Join-Path $Root 'bin/tools/xiui_capture_defaults.exe'
if (-not (Test-Path -LiteralPath $Capture)) { throw 'Build xiui_capture_defaults before using -SkipBuild.' }
$Backup = Join-Path $Root ('out/defaults-capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $Backup -Force | Out-Null
Copy-Item -LiteralPath $SettingsPath -Destination (Join-Path $Backup 'settings-source.ini')
Copy-Item -LiteralPath $ThemePath -Destination (Join-Path $Backup 'theme-source.ini')
foreach ($Entry in @(@($OutputPath, 'defaults-before.ini'), @($ThemeOutputPath, 'theme-before.ini'))) {
    if (Test-Path -LiteralPath $Entry[0]) { Copy-Item -LiteralPath $Entry[0] -Destination (Join-Path $Backup $Entry[1]) }
}
$StagedDefaults = Join-Path $Backup 'defaults.ini'
& $Capture (Join-Path $Backup 'settings-source.ini') $StagedDefaults
if ($LASTEXITCODE -ne 0) { throw 'Defaults capture failed. The baseline was not changed.' }
$Rows = Get-Content -LiteralPath $StagedDefaults
# Keep committed metadata free of local paths: record paths relative to the checkout or the game folder.
$PortablePath = {
    param([string]$Path)
    $Full = [IO.Path]::GetFullPath($Path)
    foreach ($Base in @($Root, $PhoenixInstall)) {
        if ($Base -and $Full.StartsWith([IO.Path]::GetFullPath($Base), [StringComparison]::OrdinalIgnoreCase)) {
            return [IO.Path]::GetRelativePath($Base, $Full) -replace '\\', '/'
        }
    }
    return Split-Path -Leaf $Full
}
$Metadata = [ordered]@{
    captured_at = (Get-Date).ToString('o')
    source = if ($FromSaved) { 'saved snapshot' } else { 'confirmed live save' }
    settings_path = & $PortablePath $SettingsPath
    theme_path = & $PortablePath $ThemePath
    sections = @($Rows | Where-Object { $_ -match '^\[' }).Count
    preferences = @($Rows | Where-Object { $_ -match '^[^;\[\s][^=]*=' }).Count
    defaults_sha256 = (Get-FileHash -LiteralPath $StagedDefaults).Hash
    theme_sha256 = (Get-FileHash -LiteralPath (Join-Path $Backup 'theme-source.ini')).Hash
}
Publish-XiuiAtomic -Source (Join-Path $Backup 'theme-source.ini') -Destination ([IO.Path]::GetFullPath($ThemeOutputPath))
Publish-XiuiAtomic -Source $StagedDefaults -Destination ([IO.Path]::GetFullPath($OutputPath))
$Metadata | ConvertTo-Json | Set-Content -LiteralPath ([IO.Path]::ChangeExtension($OutputPath, 'capture.json')) -Encoding utf8
Write-Output "Captured defaults: $OutputPath"
Write-Output "Previous baseline and source snapshot: $Backup"
Write-Output 'Rebuild the standalone DLL to embed this baseline; run the defaults check before installing.'
