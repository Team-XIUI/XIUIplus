param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
$FixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('xiui-defaults-capture-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $FixtureRoot | Out-Null
try {
    $Source = Join-Path $FixtureRoot 'source.ini'
    $Output = Join-Path $FixtureRoot 'defaults.ini'
    [IO.File]::WriteAllText($Source, "[Player]`r`nenabled=0`r`nlabel=Éclair`r`n[layout.player]`r`nx=123`r`ny=456`r`n[actionbars.slot]`r`ncommand=`r`n[session.gil]`r`nvalue=100`r`n[profiles]`r`nactive=private`r`n[controller.crossbar.device.1]`r`nguid=local`r`n[containers]`r`nenabled=1`r`n[layout.container]`r`nx=9`r`n", [Text.Encoding]::Unicode)
    & $Executable $Source $Output
    if ($LASTEXITCODE -ne 0) { throw 'Capture command failed.' }
    $Text = [IO.File]::ReadAllText($Output)
    foreach ($Required in @('enabled=0', 'label=Éclair', 'x=123', 'y=456', 'command=')) {
        if (-not $Text.Contains($Required)) { throw "Lost saved preference: $Required" }
    }
    if ($Text -match '\[(session\.|profiles|controller\.crossbar\.device|containers|layout\.container)') { throw 'Local/transient/demo data entered the baseline.' }
    Write-Output 'Defaults capture: UTF-16, Unicode, empty actions, positions and portable preference boundaries passed.'
}
finally {
    # The fixture path is explicitly created under the system temporary directory.
    Remove-Item -LiteralPath $Source -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $Output -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $FixtureRoot -Force -ErrorAction SilentlyContinue
}
