param(
    [string]$ProjectRoot = (Split-Path $PSScriptRoot -Parent),
    [string]$SceneName = "Map_TTS"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$scenePath = Join-Path $ProjectRoot "ref\client-unity\Assets\BundleAssets\Map\Scenes\$SceneName.unity"
$outDir = Join-Path $ProjectRoot "Saved\RokUnitySpriteScene\$SceneName"
$manifestPath = Join-Path $outDir "sprites.json"
$referenceSpriteDir = Join-Path $ProjectRoot "ref\resources\Sprite"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$fallbackNameToSource = @{
    "Building_MainCity_01" = "Castle_1_5.png"
    "Building_MainCity_02" = "Castle_1_5.png"
    "Building_Castle_01" = "Castle_1_5.png"
    "Building_Castle_02" = "Castle_6_5.png"
    "City_Main" = "city.png"
    "Building_Tower_01" = "GuardTowerUI_1_5.png"
    "Building_Tower_02" = "GuardTowerUI_1_4.png"
    "Building_Farm" = "FarmWindmill_ani_5_00.png"
    "Building_Hospital" = "Hospital_1_5.png"
    "Building_WorkMan_01" = "Barracks_1_5.png"
    "Building_WorkMan_02" = "Barracks_1_5.png"
    "Building_Military_Camp_01" = "Barracks_1_5.png"
    "Building_Military_Camp_02" = "Stable_1_5.png"
    "Building_Book_02" = "Tavern_1_5.png"
    "Building_Wood" = "Lumbermill_1_5.png"
    "Building_Stone" = "Quarry_1_5.png"
    "Building_Gold_02" = "Goldmine_1_5.png"
    "Building_Storehouse_01" = "Shop_1_4.png"
    "Building_Storehouse_02" = "Shop_1_4.png"
    "Building_Hero_01" = "Monument_1_4.png"
    "Building_Hero_02" = "Monument_6_4.png"
    "Building_Arc_01" = "Archery_1_5.png"
    "Building_Arc_02" = "Archery_6_5.png"
    "Building_Patrol_01" = "ScoutCamp_1_4.png"
    "Building_Patrol_02" = "ScoutCamp_6_4.png"
    "World_2" = "CityWall_1_4_c.png"
    "World_4" = "CityWall_1_4_c.png"
    "zhongxinchengbao" = "Castle_10_5.png"
    "zhedang02" = "CityWall_1_4_shadow.png"
}

function ConvertTo-SafeName([string]$Value) {
    $safe = [regex]::Replace($Value.Trim(), '[^0-9A-Za-z_]+', '_')
    $safe = [regex]::Replace($safe, '_+', '_').Trim('_')
    if ([string]::IsNullOrWhiteSpace($safe)) { return "Sprite" }
    return $safe
}

function Parse-Vector3([string]$Body, [string]$Field, [double[]]$Fallback) {
    $pattern = [regex]::Escape($Field) + ': \{x: ([^,}]+), y: ([^,}]+), z: ([^,}]+)\}'
    $m = [regex]::Match($Body, $pattern)
    if (-not $m.Success) { return ,$Fallback }
    return ,@([double]$m.Groups[1].Value, [double]$m.Groups[2].Value, [double]$m.Groups[3].Value)
}

function Get-YamlChunks([string]$Text) {
    $headers = [regex]::Matches($Text, '(?m)^--- !u!(\d+) &(-?\d+)\r?\n')
    $chunks = @()
    for ($i = 0; $i -lt $headers.Count; $i++) {
        $start = $headers[$i].Index + $headers[$i].Length
        $end = if ($i + 1 -lt $headers.Count) { $headers[$i + 1].Index } else { $Text.Length }
        $chunks += [pscustomobject]@{
            Type = $headers[$i].Groups[1].Value
            Id = $headers[$i].Groups[2].Value
            Body = $Text.Substring($start, $end - $start)
        }
    }
    return $chunks
}

function Get-MetaInfo([string]$MetaPath) {
    $text = Get-Content -Raw -Encoding UTF8 $MetaPath
    $guidMatch = [regex]::Match($text, '(?m)^guid: ([0-9a-f]+)$')
    if (-not $guidMatch.Success) { return $null }

    $idToName = @{}
    foreach ($m in [regex]::Matches($text, '(?s)- first:\s*\r?\n\s*213: (-?\d+)\s*\r?\n\s*second: ([^\r\n]+)')) {
        $idToName[$m.Groups[1].Value] = $m.Groups[2].Value.Trim()
    }

    $rectById = @{}
    $spritePattern = '(?s)- serializedVersion: 2\s*\r?\n\s*name: ([^\r\n]+).*?rect:\s*\r?\n\s*serializedVersion: 2\s*\r?\n\s*x: ([0-9.]+)\s*\r?\n\s*y: ([0-9.]+)\s*\r?\n\s*width: ([0-9.]+)\s*\r?\n\s*height: ([0-9.]+).*?internalID: (-?\d+)'
    foreach ($m in [regex]::Matches($text, $spritePattern)) {
        $rectById[$m.Groups[6].Value] = [pscustomobject]@{
            Name = $m.Groups[1].Value.Trim()
            X = [int][double]$m.Groups[2].Value
            Y = [int][double]$m.Groups[3].Value
            Width = [int][double]$m.Groups[4].Value
            Height = [int][double]$m.Groups[5].Value
        }
    }

    return [pscustomobject]@{
        Guid = $guidMatch.Groups[1].Value
        PngPath = $MetaPath.Substring(0, $MetaPath.Length - 5)
        IdToName = $idToName
        RectById = $rectById
    }
}

function Resolve-FallbackSprite([string]$Name) {
    $normalized = ($Name -replace ' \(.+\)$', '')
    $lookup = $normalized
    if (-not $fallbackNameToSource.ContainsKey($lookup)) {
        $withoutOrdinal = ($normalized -replace '_\d+$', '')
        if ($fallbackNameToSource.ContainsKey($withoutOrdinal)) {
            $lookup = $withoutOrdinal
        }
    }
    if (-not $fallbackNameToSource.ContainsKey($lookup)) { return $null }
    $filename = $fallbackNameToSource[$lookup]
    $path = Join-Path $referenceSpriteDir $filename
    if (-not (Test-Path $path)) { return $null }
    $stem = [System.IO.Path]::GetFileNameWithoutExtension($path)
    $extension = [System.IO.Path]::GetExtension($path)
    $maskPath = Join-Path $referenceSpriteDir ($stem + "_mask" + $extension)
    return [pscustomobject]@{
        PngPath = $path
        MaskPath = if (Test-Path $maskPath) { $maskPath } else { $null }
        SpriteName = $stem
    }
}

function Save-CroppedSpritePng(
    [System.Drawing.Bitmap]$Image,
    [System.Drawing.Bitmap]$Mask,
    [int]$CropX,
    [int]$CropY,
    [int]$CropW,
    [int]$CropH,
    [string]$OutPath
) {
    $dest = New-Object System.Drawing.Bitmap $CropW, $CropH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($x = 0; $x -lt $CropW; $x++) {
        for ($y = 0; $y -lt $CropH; $y++) {
            $sourceColor = $Image.GetPixel($CropX + $x, $CropY + $y)
            if ($Mask) {
                $maskColor = $Mask.GetPixel([Math]::Min($CropX + $x, $Mask.Width - 1), [Math]::Min($CropY + $y, $Mask.Height - 1))
                $maskValue = [Math]::Max($maskColor.R, [Math]::Max($maskColor.G, $maskColor.B))
                $colorValue = [Math]::Max($sourceColor.R, [Math]::Max($sourceColor.G, $sourceColor.B))
                $alpha = if ($maskValue -lt 24 -or $colorValue -lt 24) { 0 } else { 255 }
                $dest.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, $sourceColor.R, $sourceColor.G, $sourceColor.B))
            }
            else {
                $dest.SetPixel($x, $y, $sourceColor)
            }
        }
    }
    $dest.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $dest.Dispose()
}

$metaByGuid = @{}
$roots = @(
    (Join-Path $ProjectRoot "ref\client-unity\Assets\BundleAssets"),
    (Join-Path $ProjectRoot "ref\resources")
)
foreach ($root in $roots) {
    if (-not (Test-Path $root)) { continue }
    foreach ($meta in Get-ChildItem -Path $root -Recurse -File -Filter "*.png.meta") {
        $info = Get-MetaInfo $meta.FullName
        if ($info -and -not $metaByGuid.ContainsKey($info.Guid)) {
            $metaByGuid[$info.Guid] = $info
        }
    }
}

$sceneText = Get-Content -Raw -Encoding UTF8 $scenePath
$chunks = Get-YamlChunks $sceneText
$gameObjectNames = @{}
$gameObjectTransforms = @{}
$transforms = @{}
$spriteRows = @()

foreach ($chunk in $chunks) {
    $goMatch = [regex]::Match($chunk.Body, 'm_GameObject: \{fileID: (-?\d+)\}')
    if ($chunk.Type -eq "1") {
        $nameMatch = [regex]::Match($chunk.Body, '(?m)^\s*m_Name: (.*)$')
        if ($nameMatch.Success) { $gameObjectNames[$chunk.Id] = $nameMatch.Groups[1].Value.Trim() }
    }
    elseif ($chunk.Type -eq "4") {
        if ($goMatch.Success) { $gameObjectTransforms[$goMatch.Groups[1].Value] = $chunk.Id }
        $fatherMatch = [regex]::Match($chunk.Body, 'm_Father: \{fileID: (-?\d+)\}')
        $transforms[$chunk.Id] = [pscustomobject]@{
            Position = Parse-Vector3 $chunk.Body "m_LocalPosition" @(0.0, 0.0, 0.0)
            Scale = Parse-Vector3 $chunk.Body "m_LocalScale" @(1.0, 1.0, 1.0)
            Euler = Parse-Vector3 $chunk.Body "m_LocalEulerAnglesHint" @(0.0, 0.0, 0.0)
            Father = if ($fatherMatch.Success) { $fatherMatch.Groups[1].Value } else { "0" }
        }
    }
    elseif ($chunk.Type -eq "212") {
        if (-not $goMatch.Success) { continue }
        $spriteMatch = [regex]::Match($chunk.Body, 'm_Sprite: \{fileID: ([^,}]+), guid: ([0-9a-f]+), type: 3\}')
        if (-not $spriteMatch.Success) { continue }
        $orderMatch = [regex]::Match($chunk.Body, 'm_SortingOrder: (-?\d+)')
        $sizeMatch = [regex]::Match($chunk.Body, 'm_Size: \{x: ([^,}]+), y: ([^,}]+)\}')
        $goId = $goMatch.Groups[1].Value
        $spriteRows += [pscustomobject]@{
            Name = if ($gameObjectNames.ContainsKey($goId)) { $gameObjectNames[$goId] } else { $goId }
            GameObjectId = $goId
            Transform = if ($gameObjectTransforms.ContainsKey($goId)) { $gameObjectTransforms[$goId] } else { $null }
            FileId = $spriteMatch.Groups[1].Value.Trim()
            Guid = $spriteMatch.Groups[2].Value
            SortingOrder = if ($orderMatch.Success) { [int]$orderMatch.Groups[1].Value } else { 0 }
            SizeX = if ($sizeMatch.Success) { [double]$sizeMatch.Groups[1].Value } else { 1.0 }
            SizeY = if ($sizeMatch.Success) { [double]$sizeMatch.Groups[2].Value } else { 1.0 }
        }
    }
}

$worldCache = @{}
function Get-ScalarNumber($Value) {
    while ($Value -is [array] -and $Value.Count -eq 1) {
        $Value = $Value[0]
    }
    return [double]$Value
}

function Get-WorldTransform([string]$TransformId) {
    if ([string]::IsNullOrWhiteSpace($TransformId) -or $TransformId -eq "0" -or -not $transforms.ContainsKey($TransformId)) {
        return ,([pscustomobject]@{ Position = @(0.0, 0.0, 0.0); Scale = @(1.0, 1.0, 1.0); Euler = @(0.0, 0.0, 0.0) })
    }
    if ($worldCache.ContainsKey($TransformId)) { return $worldCache[$TransformId] }
    $local = $transforms[$TransformId]
    $parent = Get-WorldTransform $local.Father
    $pp0 = [double](Get-ScalarNumber ($parent.Position[0]))
    $pp1 = [double](Get-ScalarNumber ($parent.Position[1]))
    $pp2 = [double](Get-ScalarNumber ($parent.Position[2]))
    $ps0 = [double](Get-ScalarNumber ($parent.Scale[0]))
    $ps1 = [double](Get-ScalarNumber ($parent.Scale[1]))
    $ps2 = [double](Get-ScalarNumber ($parent.Scale[2]))
    $lp0 = [double](Get-ScalarNumber ($local.Position[0]))
    $lp1 = [double](Get-ScalarNumber ($local.Position[1]))
    $lp2 = [double](Get-ScalarNumber ($local.Position[2]))
    $ls0 = [double](Get-ScalarNumber ($local.Scale[0]))
    $ls1 = [double](Get-ScalarNumber ($local.Scale[1]))
    $ls2 = [double](Get-ScalarNumber ($local.Scale[2]))
    $position0 = [double]$pp0 + ([double]$lp0 * [double]$ps0)
    $position1 = [double]$pp1 + ([double]$lp1 * [double]$ps1)
    $position2 = [double]$pp2 + ([double]$lp2 * [double]$ps2)
    $scale0 = [double]$ps0 * [double]$ls0
    $scale1 = [double]$ps1 * [double]$ls1
    $scale2 = [double]$ps2 * [double]$ls2
    $position = @($position0, $position1, $position2)
    $scale = @($scale0, $scale1, $scale2)
    $result = [pscustomobject]@{ Position = $position; Scale = $scale; Euler = $local.Euler }
    $worldCache[$TransformId] = $result
    return ,$result
}

$manifest = @()
$index = 0
foreach ($row in $spriteRows) {
    $meta = if ($metaByGuid.ContainsKey($row.Guid)) { $metaByGuid[$row.Guid] } else { $null }
    $fallback = if (-not $meta) { Resolve-FallbackSprite $row.Name } else { $null }
    if (-not $meta -and -not $fallback) {
        Write-Warning "Missing PNG meta/fallback for guid $($row.Guid) used by $($row.Name)"
        continue
    }
    $pngPath = if ($meta) { $meta.PngPath } else { $fallback.PngPath }
    if (-not (Test-Path $pngPath)) {
        Write-Warning "Missing PNG file $pngPath"
        continue
    }

    $image = [System.Drawing.Bitmap]::FromFile($pngPath)
    $mask = $null
    if ($fallback -and $fallback.MaskPath) {
        $mask = [System.Drawing.Bitmap]::FromFile($fallback.MaskPath)
    }
    try {
        $rect = $null
        $spriteName = if ($fallback) { $fallback.SpriteName } else { [System.IO.Path]::GetFileNameWithoutExtension($meta.PngPath) }
        if ($meta -and $row.FileId -ne "21300000" -and $meta.RectById.ContainsKey($row.FileId)) {
            $rect = $meta.RectById[$row.FileId]
            $spriteName = $rect.Name
        }
        if ($rect) {
            $cropX = [Math]::Max(0, [Math]::Min($rect.X, $image.Width - 1))
            $cropY = [Math]::Max(0, [Math]::Min($image.Height - $rect.Y - $rect.Height, $image.Height - 1))
            $cropW = [Math]::Max(1, [Math]::Min($rect.Width, $image.Width - $cropX))
            $cropH = [Math]::Max(1, [Math]::Min($rect.Height, $image.Height - $cropY))
        }
        else {
            $cropX = 0
            $cropY = 0
            $cropW = $image.Width
            $cropH = $image.Height
        }

        $safe = ConvertTo-SafeName "$($row.Name)_$spriteName"
        $outName = "{0:D3}_{1}.png" -f $index, $safe
        $outPath = Join-Path $outDir $outName
        Save-CroppedSpritePng $image $mask $cropX $cropY $cropW $cropH $outPath

        $transformId = if ($row.Transform) { $row.Transform } elseif ($gameObjectTransforms.ContainsKey($row.GameObjectId)) { $gameObjectTransforms[$row.GameObjectId] } else { $null }
        $world = Get-WorldTransform $transformId
        $manifest += [pscustomobject]@{
            index = $index
            name = $row.Name
            sprite_name = $spriteName
            guid = $row.Guid
            file_id = $row.FileId
            source_png = $pngPath
            processed_png = $outPath
            crop = @{ x = $cropX; y = $cropY; width = $cropW; height = $cropH }
            position = @{ x = $world.Position[0]; y = $world.Position[1]; z = $world.Position[2] }
            scale = @{ x = $world.Scale[0]; y = $world.Scale[1]; z = $world.Scale[2] }
            euler = @{ x = $world.Euler[0]; y = $world.Euler[1]; z = $world.Euler[2] }
            sorting_order = $row.SortingOrder
            size = @{ x = $row.SizeX; y = $row.SizeY }
        }
        $index++
    }
    finally {
        $image.Dispose()
        if ($mask) { $mask.Dispose() }
    }
}

$manifest | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $manifestPath
Write-Host "Prepared $($manifest.Count) sprites from $SceneName"
Write-Host $manifestPath
