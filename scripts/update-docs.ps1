param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$Root = Split-Path -Parent $PSScriptRoot
if (-not $SkipBuild) { Invoke-XiuiBuild -Targets xiui_config_input_smoke }
Push-Location $Root
try {
    & (Join-Path $Root 'bin/tests/xiui_config_input_smoke.exe') --settings-presentation-only (Join-Path $Root 'docs/reference/settings-inventory.md')
    if ($LASTEXITCODE -ne 0) { throw 'Settings inventory generation failed.' }
    python (Join-Path $PSScriptRoot 'check-doc-links.py')
    if ($LASTEXITCODE -ne 0) { throw 'Documentation has missing local targets.' }
}
finally { Pop-Location }
