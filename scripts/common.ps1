Set-StrictMode -Version Latest

function Resolve-XiuiInstallPath {
    param([string]$PreferredPath)
    $Candidate = if (-not [string]::IsNullOrWhiteSpace($PreferredPath)) { $PreferredPath }
        elseif (-not [string]::IsNullOrWhiteSpace($env:XIUI_INSTALL_PATH)) { $env:XIUI_INSTALL_PATH }
        else { 'C:/Games/PhoenixXI-dev/PhoenixXI' }
    if (-not (Test-Path -LiteralPath (Join-Path $Candidate 'plugins') -PathType Container)) {
        throw 'Phoenix install not found. Pass -InstallPath pointing to the Phoenix folder containing plugins.'
    }
    return (Resolve-Path -LiteralPath $Candidate).Path
}

function Resolve-XiuiSdk {
    param([string]$PreferredPath)

    $Candidates = [Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($PreferredPath)) { $Candidates.Add($PreferredPath) }
    if (-not [string]::IsNullOrWhiteSpace($env:ASHITA4_SDK_PATH)) { $Candidates.Add($env:ASHITA4_SDK_PATH) }
    $Candidates.Add((Join-Path $env:USERPROFILE 'Desktop\Ashita-v4beta-main\plugins\sdk'))
    $Desktop = Join-Path $env:USERPROFILE 'Desktop'
    if (Test-Path $Desktop) {
        Get-ChildItem $Desktop -Directory -Filter 'Ashita*' -ErrorAction SilentlyContinue | ForEach-Object {
            $Candidates.Add((Join-Path $_.FullName 'plugins\sdk'))
        }
    }

    foreach ($Candidate in $Candidates | Select-Object -Unique) {
        if (Test-Path (Join-Path $Candidate 'Ashita.h')) {
            return (Resolve-Path $Candidate).Path
        }
    }
    throw 'Ashita v4 SDK not found. Set ASHITA4_SDK_PATH or pass -SdkPath.'
}

function Resolve-XiuiVisualStudio {
    $VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $VsWhere)) {
        throw 'vswhere.exe was not found. Install Visual Studio 2022 with x86 C++ tools.'
    }
    $Install = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ([string]::IsNullOrWhiteSpace($Install)) {
        throw 'Visual Studio with x86 C++ tools was not found.'
    }
    return $Install.Trim()
}

function Resolve-XiuiCMake {
    param([string]$VisualStudioPath)

    $FromPath = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $FromPath) {
        return $FromPath.Source
    }
    $Bundled = Join-Path $VisualStudioPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path $Bundled) {
        return $Bundled
    }
    throw 'cmake.exe was not found on PATH or in the selected Visual Studio installation.'
}

function Publish-XiuiAtomic {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination
    )

    if (-not ('XiuiPublishNativeMethods' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class XiuiPublishNativeMethods
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool MoveFileEx(string existingName, string newName, uint flags);
}
'@
    }

    $Directory = Split-Path -Parent $Destination
    [IO.Directory]::CreateDirectory($Directory) | Out-Null
    $Leaf = Split-Path -Leaf $Destination
    $Staging = Join-Path $Directory ".$Leaf.staging.$PID.$([Guid]::NewGuid().ToString('N'))"
    try {
        Copy-Item -Path $Source -Destination $Staging -Force
        $ReplaceExisting = 0x1
        $WriteThrough = 0x8
        if (-not [XiuiPublishNativeMethods]::MoveFileEx($Staging, $Destination, $ReplaceExisting -bor $WriteThrough)) {
            throw [ComponentModel.Win32Exception]::new([Runtime.InteropServices.Marshal]::GetLastWin32Error())
        }
    }
    finally {
        Remove-Item $Staging -Force -ErrorAction SilentlyContinue
    }
}
