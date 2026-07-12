# Generates assets/tiles/ocean_overlay.png (160x96, seamless both axes): the
# foam-glint parallax layer. Transparent except for 3 dim foam marks
# (#cfeff0 at varying alpha, 1-2px) — an earlier 14-mark version was too
# busy in playtest. It scrolls slightly faster than the
# base water (OCEAN_OVERLAY_SPEED_SCALE), so the glints drift relative to
# the water body — reading as propagating wavelets — and because 160x96
# never aligns with the 128x64 base tile, the combined pattern's visible
# repeat period is far longer than either texture alone. This is why glints
# live here and not in the base tile: baked landmarks made the water repeat
# obviously trackable.
#
# Run from anywhere:  powershell -File tools/gen-ocean-overlay.ps1

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$width = 160
$height = 96
$bmp = New-Object System.Drawing.Bitmap $width, $height

# x, y, width-in-px, alpha — irregular spread so no rows/columns form
$marks = @(
    @(14,  8, 2, 170),
    @(95, 34, 2, 130),
    @(48, 91, 1, 130)
)

foreach ($m in $marks) {
    $x, $y, $w, $a = $m
    for ($i = 0; $i -lt $w; $i++) {
        $bmp.SetPixel($x + $i, $y, [System.Drawing.Color]::FromArgb($a, 207, 239, 240))
    }
}

$out = Join-Path $PSScriptRoot "..\assets\tiles\ocean_overlay.png"
$out = [System.IO.Path]::GetFullPath($out)
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "written: $out"
