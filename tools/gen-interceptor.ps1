# Generates assets/sprites/interceptor.png (32x16) from an ASCII pixel grid.
# Interceptor per README roster: the first enemy that shoots back. Elongated
# fuselage with swept-back wings (stealth-fighter-ish), nose LEFT (flies
# right-to-left, faster and straighter than the Skimmer Drone). Same air-
# family language as the drone — dark gunmetal-violet hull, magenta energy —
# but the full-length spine stripe (bright top / dark bottom half) and the
# pale weapon core at the nose mark it as armed. Wingtip glints echo the
# stripe color.
#
# Run from anywhere:  powershell -File tools/gen-interceptor.ps1

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$palette = @{
    '.' = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)          # transparent
    'O' = [System.Drawing.Color]::FromArgb(255, 20, 16, 32)     # outline dark violet-navy
    'H' = [System.Drawing.Color]::FromArgb(255, 58, 52, 72)     # hull gunmetal violet
    'S' = [System.Drawing.Color]::FromArgb(255, 42, 36, 56)     # hull shade
    'M' = [System.Drawing.Color]::FromArgb(255, 216, 72, 192)   # magenta energy #d848c0
    'm' = [System.Drawing.Color]::FromArgb(255, 150, 50, 134)   # magenta energy, dark half
    'W' = [System.Drawing.Color]::FromArgb(255, 255, 216, 248)  # glowing core near-white pink
}

$grid = @(
    ".........................OOO",
    ".......................OOHMHO",
    ".....................OOHHHOO",
    "...................OOHHHOO",
    "...........OOOOOOOOHHHOO",
    "......OOOOOHHHHHHHHHHOOOOO",
    "..OOOOHHHHHHHHHHHHHHHHHHHHOOOO",
    ".OHMWWWWMMMMMMMMMMMMMMMMMMMMMHO",
    ".OSmWWWWmmmmmmmmmmmmmmmmmmmmmSO",
    "..OOOOSSSSSSSSSSSSSSSSSSSSOOOO",
    "......OOOOOSSSSSSSSSSOOOOO",
    "...........OOOOOOOOSSSOO",
    "...................OOSSSOO",
    ".....................OOSSSOO",
    ".......................OOSMSO",
    ".........................OOO"
)

$width = 32
$height = 16
$bmp = New-Object System.Drawing.Bitmap $width, $height
for ($y = 0; $y -lt $height; $y++) {
    $row = $grid[$y]
    if ($row.Length -gt $width) { throw "row $y is $($row.Length) chars, max $width" }
    $row = $row.PadRight($width, '.')
    for ($x = 0; $x -lt $width; $x++) {
        $bmp.SetPixel($x, $y, $palette[$row[$x].ToString()])
    }
}

$out = Join-Path $PSScriptRoot "..\assets\sprites\interceptor.png"
$out = [System.IO.Path]::GetFullPath($out)
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "written: $out"
