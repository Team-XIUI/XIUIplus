param(
    [string]$AssetDirectory = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'assets\cursors')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

foreach ($Name in @('pointer', 'hand', 'move', 'resize-diagonal', 'resize-horizontal')) {
    $Source = Join-Path $AssetDirectory "$Name.png"
    $Destination = Join-Path $AssetDirectory "$Name.bgra"
    $Bitmap = [Drawing.Bitmap]::new($Source)
    try {
        if ($Bitmap.Width -ne 64 -or $Bitmap.Height -ne 64) {
            throw "$Source must be a 64x64 cursor image."
        }
        $Stream = [IO.File]::Open($Destination, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try {
            for ($Y = 0; $Y -lt $Bitmap.Height; ++$Y) {
                for ($X = 0; $X -lt $Bitmap.Width; ++$X) {
                    $Pixel = $Bitmap.GetPixel($X, $Y)
                    $Stream.WriteByte($Pixel.B)
                    $Stream.WriteByte($Pixel.G)
                    $Stream.WriteByte($Pixel.R)
                    $Stream.WriteByte($Pixel.A)
                }
            }
        }
        finally {
            $Stream.Dispose()
        }
    }
    finally {
        $Bitmap.Dispose()
    }
}
