param(
    [switch]$SkipInstall,
    [switch]$RuntimeOnly,
    [switch]$Development,
    [Alias('SkipTests')][switch]$SkipDevelopmentTests,
    [string]$SdkPath,
    [string]$InstallPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$Root = Split-Path -Parent $PSScriptRoot
if ($RuntimeOnly -and -not $Development) { throw '-RuntimeOnly requires the optional -Development host. Normal Phoenix installs use one standalone DLL.' }
if ($SkipDevelopmentTests -and -not $Development) { throw '-SkipDevelopmentTests (legacy -SkipTests) applies only to -Development. Standalone packaging retains its required checks.' }
if (-not $Development) {
    & (Join-Path $PSScriptRoot 'package.ps1') -SdkPath $SdkPath
    if ($SkipInstall) { return }
    $PhoenixInstall = Resolve-XiuiInstallPath -PreferredPath $InstallPath
    try { Install-XiuiStandalone -Source (Join-Path $Root 'dist/XIUIplus.dll') -InstallPath $PhoenixInstall }
    catch { throw "Could not install the standalone DLL. Unload XIUI+ with '/unload XIUIplus' (or '/unload xiuinative' before upgrading), then retry. $($_.Exception.Message)" }
    Write-Output "Installed standalone XIUI+ into $PhoenixInstall/plugins/XIUIplus.dll"
    return
}
if (-not $SkipInstall -and [string]::IsNullOrWhiteSpace($InstallPath)) { throw '-Development requires an explicit -InstallPath for its isolated install.' }
$Sdk = Resolve-XiuiSdk -PreferredPath $SdkPath
$VisualStudio = Resolve-XiuiVisualStudio
$CMake = Resolve-XiuiCMake -VisualStudioPath $VisualStudio
$VcVars = Join-Path $VisualStudio 'VC\Auxiliary\Build\vcvarsall.bat'

function Get-Sha256 {
    param([string]$Path)

    $Stream = [IO.File]::OpenRead($Path)
    try {
        $Sha = [Security.Cryptography.SHA256]::Create()
        try {
            return ([BitConverter]::ToString($Sha.ComputeHash($Stream))).Replace('-', '')
        }
        finally {
            $Sha.Dispose()
        }
    }
    finally {
        $Stream.Dispose()
    }
}

$TargetArgument = if ($RuntimeOnly) { ' --target XIUIplus_runtime' } else { '' }
$cmd = @"
cd /d "$Root" && call "$VcVars" x86 && set "ASHITA4_SDK_PATH=$Sdk" && "$CMake" --preset msvc-x86-release && "$CMake" --build --preset release$TargetArgument
"@
cmd /d /s /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "Native build failed with exit code $LASTEXITCODE."
}

$BuiltHost = Join-Path $Root 'bin\XIUIplus.dll'
$BuiltRuntime = Join-Path $Root 'bin\XIUIplus-runtime.dll'
$RendererSmoke = Join-Path $Root 'bin\tests\xiui_renderer_smoke.exe'
$FoundationsSmoke = Join-Path $Root 'bin\tests\xiui_foundations_smoke.exe'
$CallbackGateSmoke = Join-Path $Root 'bin\tests\xiui_callback_gate_smoke.exe'
$ConfigInputSmoke = Join-Path $Root 'bin\tests\xiui_config_input_smoke.exe'
$SettingsSmoke = Join-Path $Root 'bin\tests\xiui_settings_smoke.exe'
$ThemeReloadSmoke = Join-Path $Root 'bin\tests\xiui_theme_reload_smoke.exe'
$HotReloadSmoke = Join-Path $Root 'bin\tests\xiui_hot_reload_smoke.exe'
$RejectedRuntime = Join-Path $Root 'bin\tests\xiui-rejected-runtime.dll'
$StatefulRuntime = Join-Path $Root 'bin\tests\xiui-stateful-runtime.dll'
$ActivationFailureRuntime = Join-Path $Root 'bin\tests\xiui-activation-failure-runtime.dll'
$ExitFailureRuntime = Join-Path $Root 'bin\tests\xiui-exit-failure-runtime.dll'
$RenderFailureRuntime = Join-Path $Root 'bin\tests\xiui-render-failure-runtime.dll'
$PublishSmoke = Join-Path $Root 'tests\host\publish_smoke.ps1'

if (-not (Test-Path $BuiltRuntime)) {
    throw "Expected runtime output was not found: $BuiltRuntime"
}
if (-not $RuntimeOnly -and -not (Test-Path $BuiltHost)) {
    throw "Expected stable host output was not found: $BuiltHost"
}

if (-not $SkipDevelopmentTests -and -not $RuntimeOnly) {
    if (-not (Test-Path $ConfigInputSmoke) -or -not (Test-Path $SettingsSmoke)) {
        throw 'Expected config input and settings persistence smoke-test files were not found.'
    }
    if (-not (Test-Path $FoundationsSmoke) -or -not (Test-Path $RendererSmoke) -or -not (Test-Path $CallbackGateSmoke) -or -not (Test-Path $ThemeReloadSmoke) -or -not (Test-Path $HotReloadSmoke) -or -not (Test-Path $RejectedRuntime) -or -not (Test-Path $StatefulRuntime) -or -not (Test-Path $ActivationFailureRuntime) -or -not (Test-Path $ExitFailureRuntime) -or -not (Test-Path $RenderFailureRuntime) -or -not (Test-Path $PublishSmoke)) {
        throw 'Expected smoke-test files were not found.'
    }
    & $FoundationsSmoke
    if ($LASTEXITCODE -ne 0) { throw "Foundations smoke test failed with exit code $LASTEXITCODE." }
    & $RendererSmoke
    if ($LASTEXITCODE -ne 0) { throw "Renderer smoke test failed with exit code $LASTEXITCODE." }
    & $CallbackGateSmoke
    if ($LASTEXITCODE -ne 0) { throw "Callback gate smoke test failed with exit code $LASTEXITCODE." }
    & $ConfigInputSmoke
    if ($LASTEXITCODE -ne 0) { throw "Config input smoke test failed with exit code $LASTEXITCODE." }
    & $SettingsSmoke
    if ($LASTEXITCODE -ne 0) { throw "Settings persistence smoke test failed with exit code $LASTEXITCODE." }
    & $ThemeReloadSmoke
    if ($LASTEXITCODE -ne 0) { throw "Theme reload smoke test failed with exit code $LASTEXITCODE." }
    & $HotReloadSmoke $BuiltRuntime $RejectedRuntime $StatefulRuntime $ActivationFailureRuntime $ExitFailureRuntime $RenderFailureRuntime
    if ($LASTEXITCODE -ne 0) { throw "Hot-reload smoke test failed with exit code $LASTEXITCODE." }
    & $PublishSmoke -ProjectRoot $Root
}

$RuntimeHash = Get-Sha256 $BuiltRuntime
$HostHash = if (Test-Path $BuiltHost) { Get-Sha256 $BuiltHost } else { '' }
if ($SkipInstall) {
    if (-not $RuntimeOnly) {
        Write-Host "Built stable host $BuiltHost"
        Write-Host "Host SHA-256 $HostHash"
    }
    Write-Host "Built hot runtime $BuiltRuntime"
    Write-Host "Runtime SHA-256 $RuntimeHash (publishing skipped)"
    return
}

$AshitaRoot = Resolve-XiuiInstallPath -PreferredPath $InstallPath
$PluginDir = Join-Path $AshitaRoot 'plugins'
$InstalledHost = Join-Path $PluginDir 'XIUIplus.dll'
if ($RuntimeOnly -and ((-not (Test-Path -LiteralPath $InstalledHost)) -or (Get-Sha256 $InstalledHost) -ne $HostHash)) {
    throw 'The selected install does not contain the matching development host. Use the standalone build, or set up an explicit development install first.'
}
$PublishedRuntime = Join-Path $PluginDir 'XIUIplus-runtime.dll'
if (-not $RuntimeOnly) {
    $InstalledHostHash = if (Test-Path $InstalledHost) { Get-Sha256 $InstalledHost } else { '' }
    if ($InstalledHostHash -ne $HostHash) {
        try {
            Copy-Item -Path $BuiltHost -Destination $InstalledHost -Force -ErrorAction Stop
        }
        catch {
            throw "Stable host changed and is locked. No runtime was published. Run '/unload XIUIplus' once, then rerun this build. $($_.Exception.Message)"
        }
    }
    $InstalledHostHash = Get-Sha256 $InstalledHost
    if ($InstalledHostHash -ne $HostHash) {
        throw "Installed host hash mismatch. Built=$HostHash Installed=$InstalledHostHash"
    }
    Write-Host "Installed stable host $InstalledHost"
    Write-Host "Host SHA-256 $InstalledHostHash"
}

Publish-XiuiAtomic -Source $BuiltRuntime -Destination $PublishedRuntime
$PublishedRuntimeHash = Get-Sha256 $PublishedRuntime
if ($PublishedRuntimeHash -ne $RuntimeHash) {
    throw "Published runtime hash mismatch. Built=$RuntimeHash Published=$PublishedRuntimeHash"
}
Write-Host "Published hot runtime $PublishedRuntime"
Write-Host "Runtime SHA-256 $PublishedRuntimeHash"
