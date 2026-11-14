param([string]$Name, [string[]]$Arguments = @(), [switch]$NoBuild, [switch]$List)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$Root = Split-Path -Parent $PSScriptRoot
$Catalog = (Get-Content -LiteralPath (Join-Path $Root 'tests/checks.json') -Raw | ConvertFrom-Json).checks
if ($List -or -not $Name) {
    foreach ($Check in $Catalog) { Write-Output "$($Check.name): $($Check.target) $($Check.arguments -join ' ')" }
    return
}
$Selected = @($Catalog | Where-Object { $_.name -eq $Name })
if ($Selected.Count -ne 1) { throw "Unknown check '$Name'. Use ./scripts/check.ps1 -List." }
$Check = $Selected[0]
if ($Check.requires_arguments -and $Arguments.Count -eq 0) { throw "'$Name' needs fixture paths. Pass -Arguments; see tests/README.md and the executable's usage." }
if (-not $NoBuild) { Invoke-XiuiBuild -Targets $Check.target }
$CheckArguments = @($Check.arguments | ForEach-Object { $_.Replace('{root}', $Root) }) + $Arguments
$Executable = Join-Path $Root "bin/tests/$($Check.target).exe"
if (-not (Test-Path -LiteralPath $Executable)) { throw "Build $($Check.target) before using -NoBuild." }
Push-Location $Root
try {
    & $Executable @CheckArguments
    if ($LASTEXITCODE -ne 0) { throw "Check failed: $Name" }
}
finally { Pop-Location }
