# Generates assets/sprites/mine.png (14x14) from an ASCII pixel grid.
# Mine per README roster: a positioning check, not a shooter — small
# spiked-sphere/urchin seen top-down, kept dim and low-key on purpose (dark
# rust spikes, dark warm body, small dim amber core). It should not visually
# announce itself the way glowing threats do.
#
# Run from anywhere:  powershell -File tools/gen-mine.ps1

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "gen-grid-image.ps1")

$palette = @{
    '.' = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)          # transparent
    'o' = [System.Drawing.Color]::FromArgb(255, 18, 14, 10)     # outline dark warm
    'h' = [System.Drawing.Color]::FromArgb(255, 46, 38, 30)     # hull dark warm
    'a' = [System.Drawing.Color]::FromArgb(255, 232, 148, 44)   # amber energy #e8942c
    'k' = [System.Drawing.Color]::FromArgb(255, 96, 62, 30)     # spike dark rust
}

$grid = @(
    "",
    "......o",
    "...o.oko..o",
    "..okooooooko",
    "...okhkhhko",
    "..oohhhhhho.o",
    ".okokhaahhkoko",
    "..oohhaahho.o",
    "...ohhhhhho",
    "...okhhhhko",
    "..okookoooko",
    "...o..o...o",
    ".....oko",
    "......o"
)
Write-GridImagePng -Width 14 -Height 14 -Palette $palette -Grid $grid -RelativeOutput "..\assets\sprites\mine.png"
