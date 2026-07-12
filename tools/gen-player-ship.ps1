# Generates assets/sprites/player_ship.png (48x24) from an ASCII pixel grid.
# Top-down Skimmer per README design: dart fuselage with pointed nose (right),
# twin rear ski pontoons (wide stance), raised centerline spine with cyan glow,
# canopy, engine glow. Only the top half is authored; the bottom half is
# mirrored with highlight/shade swapped (light from the top edge).
#
# Run from anywhere:  powershell -File tools/gen-player-ship.ps1

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$palette = @{
    '.' = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)          # transparent
    'O' = [System.Drawing.Color]::FromArgb(255, 14, 32, 40)     # outline navy
    'H' = [System.Drawing.Color]::FromArgb(255, 232, 248, 248)  # hull #e8f8f8
    'W' = [System.Drawing.Color]::FromArgb(255, 255, 255, 255)  # highlight
    'S' = [System.Drawing.Color]::FromArgb(255, 175, 201, 211)  # hull shade
    'C' = [System.Drawing.Color]::FromArgb(255, 76, 224, 232)   # cyan #4ce0e8
    'k' = [System.Drawing.Color]::FromArgb(255, 46, 143, 160)   # canopy cyan
    'G' = [System.Drawing.Color]::FromArgb(255, 200, 250, 252)  # pale glow
}

# 48 columns x 12 rows (top half, y = 0..11; short rows are padded with
# transparency). Ship faces right; skis attach directly to the hull sides.
$topHalf = @(
    "",
    "",
    "",
    "",
    "",
    "..OOOOOOOOOOOOOOOOOOOO",
    ".OGHHHHHHHHHHHHHHHHHOOO",
    "..OCCCCCCCCCCCCCCCCCO",
    "..................OOOOOOO",
    "...............OWWWWWWWWWOOOOO",
    "............OHHHHHHHHHHHHHkkkkHHOOOOO",
    ".........OGGHHCCCCCCCCCCCCkkkkkkHHHHHHHO"
)

$width = 48
$height = 24
$bmp = New-Object System.Drawing.Bitmap $width, $height

# swap highlight/shade when mirroring so the light stays at the top
$mirrorMap = @{ 'W' = 'S'; 'S' = 'W' }

for ($y = 0; $y -lt 12; $y++) {
    $row = $topHalf[$y]
    if ($row.Length -gt $width) { throw "row $y is $($row.Length) chars, max $width" }
    $row = $row.PadRight($width, '.')
    for ($x = 0; $x -lt $width; $x++) {
        $ch = $row[$x].ToString()
        $bmp.SetPixel($x, $y, $palette[$ch])
        $mch = if ($mirrorMap.ContainsKey($ch)) { $mirrorMap[$ch] } else { $ch }
        $bmp.SetPixel($x, 23 - $y, $palette[$mch])
    }
}

$out = Join-Path $PSScriptRoot "..\assets\sprites\player_ship.png"
$out = [System.IO.Path]::GetFullPath($out)
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "written: $out"
