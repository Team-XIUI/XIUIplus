param(
    [Parameter(Mandatory)][string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $ProjectRoot 'scripts\common.ps1')

$Directory = Join-Path ([IO.Path]::GetTempPath()) "xiui-publish-smoke-$PID"
$Source = Join-Path $Directory 'source.bin'
$Destination = Join-Path $Directory 'published.bin'
[IO.Directory]::CreateDirectory($Directory) | Out-Null
try {
    [IO.File]::WriteAllText($Source, 'generation-one')
    Publish-XiuiAtomic -Source $Source -Destination $Destination
    if ([IO.File]::ReadAllText($Destination) -ne 'generation-one') {
        throw 'Initial publish content mismatch.'
    }

    [IO.File]::WriteAllText($Source, 'generation-two')
    Publish-XiuiAtomic -Source $Source -Destination $Destination
    if ([IO.File]::ReadAllText($Destination) -ne 'generation-two') {
        throw 'Replacement publish content mismatch.'
    }
    Write-Host 'publish smoke passed'
}
finally {
    Remove-Item $Directory -Recurse -Force -ErrorAction SilentlyContinue
}
