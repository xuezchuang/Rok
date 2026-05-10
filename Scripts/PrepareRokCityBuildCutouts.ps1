param(
    [string]$ProjectRoot = (Split-Path $PSScriptRoot -Parent),
    [string]$OutDir = ""
)

Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $ProjectRoot "Saved\RokCityBuildCutouts"
}
$sourceDir = Join-Path $ProjectRoot "ref\resources\Texture2D"
$null = New-Item -ItemType Directory -Force -Path $OutDir

$atlasNames = @(
    "city_build_1_5_0.png",
    "city_build_6_5_0.png",
    "city_build_9_5_0.png",
    "city_build_10_5_0.png",
    "city_build_1_4_0.png",
    "city_build_6_4_0.png",
    "city_build_9_4_0.png"
)

function Save-Cutout(
    [System.Drawing.Bitmap]$Image,
    [System.Drawing.Bitmap]$Mask,
    [int]$X,
    [int]$Y,
    [int]$W,
    [int]$H,
    [string]$OutPath
) {
    $dest = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($xx = 0; $xx -lt $W; $xx++) {
        for ($yy = 0; $yy -lt $H; $yy++) {
            $sx = $X + $xx
            $sy = $Y + $yy
            $maskColor = $Mask.GetPixel($sx, $sy)
            $maskValue = [Math]::Max($maskColor.R, [Math]::Max($maskColor.G, $maskColor.B))
            if ($maskValue -lt 18) {
                $dest.SetPixel($xx, $yy, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
            }
            else {
                $sourceColor = $Image.GetPixel($sx, $sy)
                $dest.SetPixel($xx, $yy, [System.Drawing.Color]::FromArgb(255, $sourceColor.R, $sourceColor.G, $sourceColor.B))
            }
        }
    }
    $dest.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $dest.Dispose()
}

$manifest = @()
$globalIndex = 0
foreach ($atlasName in $atlasNames) {
    $imagePath = Join-Path $sourceDir $atlasName
    $maskPath = Join-Path $sourceDir ($atlasName -replace '^city_build_', 'city_build_mask_')
    if (-not (Test-Path $imagePath) -or -not (Test-Path $maskPath)) {
        Write-Warning "Missing city build atlas or mask: $atlasName"
        continue
    }

    $image = [System.Drawing.Bitmap]::FromFile($imagePath)
    $mask = [System.Drawing.Bitmap]::FromFile($maskPath)
    try {
        $w = $mask.Width
        $h = $mask.Height
        $solid = New-Object 'bool[]' ($w * $h)
        $visited = New-Object 'bool[]' ($w * $h)
        for ($y = 0; $y -lt $h; $y++) {
            for ($x = 0; $x -lt $w; $x++) {
                $c = $mask.GetPixel($x, $y)
                $v = [Math]::Max($c.R, [Math]::Max($c.G, $c.B))
                if ($v -gt 18) {
                    $solid[$y * $w + $x] = $true
                }
            }
        }

        for ($start = 0; $start -lt $solid.Length; $start++) {
            if (-not $solid[$start] -or $visited[$start]) { continue }
            $queue = New-Object System.Collections.Generic.Queue[int]
            $queue.Enqueue($start)
            $visited[$start] = $true
            $minX = $w
            $minY = $h
            $maxX = 0
            $maxY = 0
            $area = 0
            while ($queue.Count -gt 0) {
                $idx = $queue.Dequeue()
                $x = $idx % $w
                $y = [Math]::Floor($idx / $w)
                $area++
                if ($x -lt $minX) { $minX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -gt $maxY) { $maxY = $y }

                for ($dy = -1; $dy -le 1; $dy++) {
                    for ($dx = -1; $dx -le 1; $dx++) {
                        if ($dx -eq 0 -and $dy -eq 0) { continue }
                        $nx = $x + $dx
                        $ny = $y + $dy
                        if ($nx -lt 0 -or $ny -lt 0 -or $nx -ge $w -or $ny -ge $h) { continue }
                        $n = $ny * $w + $nx
                        if ($solid[$n] -and -not $visited[$n]) {
                            $visited[$n] = $true
                            $queue.Enqueue($n)
                        }
                    }
                }
            }

            $boxW = $maxX - $minX + 1
            $boxH = $maxY - $minY + 1
            if ($area -lt 2200 -or $boxW -lt 48 -or $boxH -lt 48) { continue }

            $pad = 4
            $cropX = [Math]::Max(0, $minX - $pad)
            $cropY = [Math]::Max(0, $minY - $pad)
            $cropW = [Math]::Min($w - $cropX, $boxW + $pad * 2)
            $cropH = [Math]::Min($h - $cropY, $boxH + $pad * 2)
            $stem = [System.IO.Path]::GetFileNameWithoutExtension($atlasName)
            $outName = "{0:D3}_{1}_{2}x{3}.png" -f $globalIndex, $stem, $cropW, $cropH
            $outPath = Join-Path $OutDir $outName
            Save-Cutout $image $mask $cropX $cropY $cropW $cropH $outPath
            $manifest += [pscustomobject]@{
                index = $globalIndex
                atlas = $atlasName
                processed_png = $outPath
                crop = @{ x = $cropX; y = $cropY; width = $cropW; height = $cropH }
                area = $area
            }
            $globalIndex++
        }
    }
    finally {
        $image.Dispose()
        $mask.Dispose()
    }
}

$manifestPath = Join-Path $OutDir "city_build_cutouts.json"
$manifest | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $manifestPath
Write-Output "Prepared $($manifest.Count) city build cutouts"
Write-Output $manifestPath
