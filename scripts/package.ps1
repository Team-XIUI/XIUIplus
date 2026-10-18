param(
    [switch]$SkipBuild,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
$Root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $OutputDirectory = Join-Path $Root 'dist' }
if (-not $SkipBuild) {
    $VisualStudio = Resolve-XiuiVisualStudio
    $CMake = Resolve-XiuiCMake -VisualStudioPath $VisualStudio
    $VcVars = Join-Path $VisualStudio 'VC/Auxiliary/Build/vcvarsall.bat'
    $Sdk = Resolve-XiuiSdk
    $BuildCommand = 'call "' + $VcVars + '" x86 && set "ASHITA4_SDK_PATH=' + $Sdk + '" && "' + $CMake + '" --preset msvc-x86-release && "' + $CMake + '" --build --preset release --target xiuinative_release xiui_single_dll_smoke xiui_resource_smoke'
    Push-Location $Root
    try {
        cmd /d /s /c $BuildCommand
        if ($LASTEXITCODE -ne 0) { throw 'Single DLL release build failed.' }
        & (Join-Path $Root 'bin/tests/xiui_resource_smoke.exe') (Join-Path $Root 'bin/release/xiuinative.dll') $Root
        if ($LASTEXITCODE -ne 0) { throw 'Compressed resource check failed.' }
        & (Join-Path $Root 'bin/tests/xiui_single_dll_smoke.exe') (Join-Path $Root 'bin/release/xiuinative.dll')
        if ($LASTEXITCODE -ne 0) { throw 'Single DLL release check failed.' }
    }
    finally { Pop-Location }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$Destination = Join-Path (Resolve-Path -LiteralPath $OutputDirectory).Path 'xiuinative.dll'
Publish-XiuiAtomic -Source (Join-Path $Root 'bin/release/xiuinative.dll') -Destination $Destination
Write-Output "Release $((Get-Content -LiteralPath (Join-Path $Root 'VERSION') -Raw).Trim()): $Destination"
Write-Output "SHA256: $((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash)"
