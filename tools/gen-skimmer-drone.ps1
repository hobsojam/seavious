# Generates assets/sprites/skimmer_drone.png (24x16) from an ASCII pixel grid.
# Skimmer Drone per README roster: tiny dart/diamond, one glowing core, minimal
# wing area — deliberately plain so it parses instantly in swarms. Nose points
# LEFT (drones fly right-to-left toward the player). The scheme inverts the
# player's: dark gunmetal-violet hull, so the magenta energy stripe and single
# pale core carry the color read at swarm scale. Shade sits toward the tail
# (lit from the leading edge), so the grid is vertically symmetric and is
# authored in full rather than mirrored.
#
# The core's pulse animation is done in code (per-drone phase), not in frames.
#
# Run from anywhere:  powershell -File tools/gen-skimmer-drone.ps1

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$palette = @{
    '.' = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)          # transparent
    'O' = [System.Drawing.Color]::FromArgb(255, 20, 16, 32)     # outline dark violet-navy
    'H' = [System.Drawing.Color]::FromArgb(255, 58, 52, 72)     # hull gunmetal violet
    'S' = [System.Drawing.Color]::FromArgb(255, 42, 36, 56)     # hull shade (tail faces)
    'M' = [System.Drawing.Color]::FromArgb(255, 216, 72, 192)   # magenta energy #d848c0
    'W' = [System.Drawing.Color]::FromArgb(255, 255, 216, 248)  # glowing core near-white pink
}

# 24 columns x 16 rows; short rows are padded with transparency. Body is 11
# rows centered on row 8 (the sprite's vertical center when drawn at
# pos - size/2). Core cluster sits forward of body center — widest point
# forward, longer taper behind, for directional read without breaking the
# diamond silhouette.
$grid = @(
    "",
    "",
    "",
    ".........OO",
    ".......OOSSO",
    ".....OOHHHSSOO",
    "...OOHHHHHHSSSOOO",
    ".OOHHMMWWMMHHSSSSOOOO",
    "OMMMMMWWWWMMMMMMMMMMOO",
    ".OOHHMMWWMMHHSSSSOOOO",
    "...OOHHHHHHSSSOOO",
    ".....OOHHHSSOO",
    ".......OOSSO",
    ".........OO",
    "",
    ""
)

$width = 24
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

$out = Join-Path $PSScriptRoot "..\assets\sprites\skimmer_drone.png"
$out = [System.IO.Path]::GetFullPath($out)
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "written: $out"
