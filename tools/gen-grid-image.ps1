# Shared helper for bitmap sprite generation from ASCII grids.
#
# Usage from a generator script:
#   . (Join-Path $PSScriptRoot "gen-grid-image.ps1")
#   Write-GridImagePng -Width 24 -Height 24 -Palette $palette -Grid $grid -RelativeOutput "..\assets\sprites\foo.png"

function Write-GridImagePng {
    param(
        [Parameter(Mandatory = $true)]
        [int]$Width,

        [Parameter(Mandatory = $true)]
        [int]$Height,

        [Parameter(Mandatory = $true)]
        [hashtable]$Palette,

        [Parameter(Mandatory = $true)]
        [string[]]$Grid,

        [Parameter(Mandatory = $true)]
        [string]$RelativeOutput
    )

    Add-Type -AssemblyName System.Drawing

    $bmp = New-Object System.Drawing.Bitmap $Width, $Height
    try {
        for ($y = 0; $y -lt $Height; $y++) {
            $row = $Grid[$y]
            if ($row.Length -gt $Width) {
                throw "row $y is $($row.Length) chars, max $Width"
            }

            $row = $row.PadRight($Width, '.')
            for ($x = 0; $x -lt $Width; $x++) {
                $key = $row[$x].ToString()
                if (-not $Palette.ContainsKey($key)) {
                    throw "palette does not define '$key' at row $y, column $x"
                }

                $bmp.SetPixel($x, $y, $Palette[$key])
            }
        }

        $out = Join-Path $PSScriptRoot $RelativeOutput
        $out = [System.IO.Path]::GetFullPath($out)
        $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "written: $out"
    } finally {
        $bmp.Dispose()
    }
}
