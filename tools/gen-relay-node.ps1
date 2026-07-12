# Generates assets/sprites/relay_node.png (24x24) from an ASCII pixel grid.
# Relay Node per README roster: stationary surface spire that launches
# Skimmer Drones — seen top-down as concentric structure: amber waterline
# glow ring (shared ground-family cue with the Turret Platform), dark warm
# platform disc lit from upper-left, three radial masts at 120 degrees
# ("broadcasting", not "shooting"), and a pale beacon core that flashes
# brighter in code when a drone launches.
#
# Run from anywhere:  powershell -File tools/gen-relay-node.ps1

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$palette = @{
    '.' = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)          # transparent
    'o' = [System.Drawing.Color]::FromArgb(255, 18, 14, 10)     # outline dark warm
    'h' = [System.Drawing.Color]::FromArgb(255, 46, 38, 30)     # hull dark warm
    's' = [System.Drawing.Color]::FromArgb(255, 34, 28, 22)     # hull shade
    'a' = [System.Drawing.Color]::FromArgb(255, 232, 148, 44)   # amber energy #e8942c
    'b' = [System.Drawing.Color]::FromArgb(255, 255, 224, 168)  # beacon core pale amber
}

$grid = @(
    "...........oo",
    ".......ooooaaoooo",
    "......oaaaaaaaaaao",
    ".....oaaooooooooaao",
    "....oaooosssassoooao",
    "...oaoossssssssssooao",
    "..oaoossssssasssssooao",
    ".oaaohhssssssssssssoaao",
    ".oaoohhhssssassssssooao",
    ".oaohhhhhsssssssssssoao",
    ".oaohhhhhhaaasssssssoao",
    "oaaohhhhhhabbsssssssoaao",
    "oaaohhhhhhabbsssssssoaao",
    ".oaohhhhhahhhsasssssoao",
    ".oaohhaaahhhhhsaasssoao",
    ".oaooahhhhhhhhhssaaooao",
    ".oaaoahhhhhhhhhhssaoaao",
    "..oaoohhhhhhhhhhhsooao",
    "...oaoohhhhhhhhhhooao",
    "....oaooohhhhhhoooao",
    ".....oaaooooooooaao",
    "......oaaaaaaaaaao",
    ".......ooooaaoooo",
    "...........oo"
)

$width = 24
$height = 24
$bmp = New-Object System.Drawing.Bitmap $width, $height
for ($y = 0; $y -lt $height; $y++) {
    $row = $grid[$y]
    if ($row.Length -gt $width) { throw "row $y is $($row.Length) chars, max $width" }
    $row = $row.PadRight($width, '.')
    for ($x = 0; $x -lt $width; $x++) {
        $bmp.SetPixel($x, $y, $palette[$row[$x].ToString()])
    }
}

$out = Join-Path $PSScriptRoot "..\assets\sprites\relay_node.png"
$out = [System.IO.Path]::GetFullPath($out)
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "written: $out"
