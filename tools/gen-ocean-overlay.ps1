# Generates assets/tiles/ocean_overlay.png (160x96, seamless both axes): the
# foam-glint parallax layer. Transparent except for ~14 dim foam marks
# (#cfeff0 at varying alpha, 1-2px). It scrolls slightly faster than the
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
    @(14,  8, 2, 170), @(66, 15, 1, 120), @(118, 5, 2, 150),
    @(38, 28, 1, 190), @(95, 34, 2, 130), @(147,25, 1, 160),
    @(10, 47, 2, 140), @(57, 55, 1, 170), @(129,50, 2, 120),
    @(80, 68, 1, 150), @(30, 76, 2, 120), @(152,72, 1, 190),
    @(108,84, 2, 160), @(48, 91, 1, 130)
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
