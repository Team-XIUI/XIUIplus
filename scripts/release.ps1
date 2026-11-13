param(
    [Parameter(Mandatory)][string]$NotesFile,
    [switch]$SkipBuild,
    [switch]$WhatIf
)

# Publishes the version named in VERSION. The tag, title and channel all derive from that one line.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Root = Split-Path -Parent $PSScriptRoot
$PublicRepo = 'Team-XIUI/xiuiplus-releases'
$Version = (Get-Content -LiteralPath (Join-Path $Root 'VERSION') -Raw).Trim()
if ($Version -notmatch '^\d+\.\d\d(-[a-z]+)?$') { throw "VERSION must look like 0.21 or 0.21-alpha, not '$Version'." }
$Tag = "v$Version"
$Title = "XIUI+ $Version"
$Notes = (Resolve-Path -LiteralPath $NotesFile).Path

Push-Location $Root
try {
    if (git status --porcelain) { throw 'Commit all changes before releasing.' }
    if ((git rev-parse --abbrev-ref HEAD) -ne 'main') { throw 'Release from main.' }
    git fetch --quiet origin main
    if ((git rev-parse HEAD) -ne (git rev-parse origin/main)) { throw 'Push main before releasing.' }
    if (git tag --list $Tag) { throw "Source tag $Tag already exists. Change VERSION for a new release." }
    gh release view $Tag --repo $PublicRepo *> $null
    if ($LASTEXITCODE -eq 0) { throw "Public release $Tag already exists. Change VERSION for a new release." }

    if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'package.ps1') }
    $Dll = Join-Path $Root 'dist/XIUIplus.dll'
    $Sums = Join-Path $Root 'dist/SHA256SUMS.txt'
    $Built = (Get-Item -LiteralPath $Dll).VersionInfo.ProductVersion
    if ($Built -ne $Version) { throw "dist/XIUIplus.dll is $Built, not $Version. Run without -SkipBuild." }

    Write-Output "Version $Version -> source tag $Tag, public release '$Title'"
    if ($WhatIf) { return }

    git tag -a $Tag -m $Title
    git push origin $Tag
    # Every release is marked Latest; the channel is part of the name only.
    gh release create $Tag $Dll $Sums --repo $PublicRepo --target main --latest --title $Title --notes-file $Notes
    if ($LASTEXITCODE -ne 0) { throw 'Public release failed; the source tag is already pushed.' }
    $Expected = (Get-FileHash -LiteralPath $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    $Uploaded = gh release view $Tag --repo $PublicRepo --json assets --jq '.assets[] | select(.name == "XIUIplus.dll") | .digest'
    if ($Uploaded -ne "sha256:$Expected") { throw "Uploaded DLL hash $Uploaded does not match $Expected." }
    Write-Output "Published https://github.com/$PublicRepo/releases/tag/$Tag"
}
finally { Pop-Location }
