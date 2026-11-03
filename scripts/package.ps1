param(
    [switch]$SkipBuild,
    [string]$OutputDirectory,
    [string]$SdkPath
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
    $Sdk = Resolve-XiuiSdk -PreferredPath $SdkPath
    $BuildCommand = 'call "' + $VcVars + '" x86 && set "ASHITA4_SDK_PATH=' + $Sdk + '" && "' + $CMake + '" --preset msvc-x86-release && "' + $CMake + '" --build --preset release --target XIUIplus_release xiui_single_dll_smoke xiui_resource_smoke xiui_ashita_compat_smoke xiui_ashita_4_16_fixture xiui_ashita_4_30_fixture xiui_ashita_facade_fixture'
    Push-Location $Root
    try {
        cmd /d /s /c $BuildCommand
        if ($LASTEXITCODE -ne 0) { throw 'Single DLL release build failed.' }
        & (Join-Path $Root 'bin/tests/xiui_resource_smoke.exe') (Join-Path $Root 'bin/release/XIUIplus.dll') $Root
        if ($LASTEXITCODE -ne 0) { throw 'Compressed resource check failed.' }
        & (Join-Path $Root 'bin/tests/xiui_single_dll_smoke.exe') (Join-Path $Root 'bin/release/XIUIplus.dll')
        if ($LASTEXITCODE -ne 0) { throw 'Single DLL release check failed.' }
        foreach ($Interface in @('4.16', '4.30')) {
            & (Join-Path $Root 'bin/tests/xiui_ashita_compat_smoke.exe') (Join-Path $Root "bin/tests/ashita-$Interface/Ashita.dll") (Join-Path $Root 'bin/release/XIUIplus.dll')
            if ($LASTEXITCODE -ne 0) { throw "Ashita $Interface compatibility check failed." }
            & (Join-Path $Root 'bin/tests/xiui_ashita_compat_smoke.exe') (Join-Path $Root "bin/tests/ashita-$Interface/Ashita.dll") (Join-Path $Root 'bin/release/XIUIplus.dll') --renamed
            if ($LASTEXITCODE -ne 0) { throw "Renamed Ashita $Interface compatibility check failed." }
            foreach ($Mode in @('--mapped', '--core-handle')) {
                & (Join-Path $Root 'bin/tests/xiui_ashita_compat_smoke.exe') (Join-Path $Root "bin/tests/ashita-$Interface/Ashita.dll") (Join-Path $Root 'bin/release/XIUIplus.dll') $Mode
                if ($LASTEXITCODE -ne 0) { throw "Ashita $Interface $Mode startup check failed." }
            }
        }
        & (Join-Path $Root 'bin/tests/xiui_ashita_compat_smoke.exe') (Join-Path $Root 'bin/tests/ashita-facade/CoreFacade.dll') (Join-Path $Root 'bin/release/XIUIplus.dll') --facade
        if ($LASTEXITCODE -ne 0) { throw 'SDK facade without Ashita version exports failed initialization.' }
    }
    finally { Pop-Location }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$Destination = Join-Path (Resolve-Path -LiteralPath $OutputDirectory).Path 'XIUIplus.dll'
Publish-XiuiAtomic -Source (Join-Path $Root 'bin/release/XIUIplus.dll') -Destination $Destination
$LegacyDll = Join-Path $OutputDirectory 'xiuinative.dll'
if (Test-Path -LiteralPath $LegacyDll -PathType Leaf) { Remove-Item -LiteralPath $LegacyDll }
$Checksum = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'SHA256SUMS.txt'), "$Checksum  XIUIplus.dll`r`n", [Text.UTF8Encoding]::new($false))
Write-Output "Release $((Get-Content -LiteralPath (Join-Path $Root 'VERSION') -Raw).Trim()): $Destination"
Write-Output "SHA256: $Checksum"
