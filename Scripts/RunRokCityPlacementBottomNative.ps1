param(
    [switch]$Run,
    [string]$UnityEditor = 'C:\Program Files\Unity\Hub\Editor\2019.4.28f1c1\Editor\Unity.exe'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$probe = Join-Path $repo 'Saved\UnityCityPlacementBottomProbe'
$contractPath = Join-Path $repo 'Saved\RokUnityMigration\BatchReview20261009Gamma\placement_bottom_source_contract.json'
$contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if ($contract.schema -ne 'rok.placement_bottom_source_contract.v1') { throw 'Source placement contract schema differs' }
$files = @(
    'ref/client-unity/Assets/BundleAssetsTest/City/Mesh/building_move_bottom.asset',
    'ref/client-unity/Assets/BundleAssetsTest/City/Mesh/building_move_bottom.asset.meta',
    'ref/client-unity/Assets/BundleAssetsTest/City/Texture2D/sprite_city_building/buiding_bottom2.png',
    'ref/client-unity/Assets/BundleAssetsTest/City/Texture2D/sprite_city_building/buiding_bottom2.png.meta',
    'ref/client-unity/Assets/BundleAssets/Scene/City/city_function/building_move_bottom.prefab',
    'ref/client-unity/Assets/BundleAssets/Scene/City/city_function/building_move_bottom.prefab.meta',
    'ref/client-unity/Assets/BundleAssets/Scene/City/city_function/city_building_move_bottom.mat',
    'ref/client-unity/Assets/BundleAssets/Scene/City/city_function/city_building_move_bottom.mat.meta',
    'ref/client-unity/Assets/BundleAssets/Map/Building/customcitybuilding_move_bottom-4.shader',
    'ref/client-unity/Assets/BundleAssets/Map/Building/customcitybuilding_move_bottom-4.shader.meta',
    'ref/client-unity/Assets/Scripts/Client/Compoment/Map/ChangeSpriteColor.cs',
    'ref/client-unity/Assets/Scripts/Client/Compoment/Map/ChangeSpriteColor.cs.meta'
)
$lookup = @{}
foreach ($entry in $contract.source_hashes) { $lookup[$entry.path] = $entry.sha256.ToLowerInvariant() }
$sourceDir = Join-Path $probe 'Assets\Source'
New-Item -ItemType Directory -Path $sourceDir -Force | Out-Null
$copies = @()
foreach ($relative in $files) {
    $source = Join-Path $repo ($relative.Replace('/', '\'))
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing source: $relative" }
    $actual = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($lookup.ContainsKey($relative) -and $actual -ne $lookup[$relative]) { throw "Source hash differs: $relative" }
    $target = Join-Path $sourceDir (Split-Path $source -Leaf)
    if (Test-Path -LiteralPath $target) {
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $actual) {
            throw "Existing isolated source differs: $target"
        }
    } else { Copy-Item -LiteralPath $source -Destination $target }
    $copies += @{source=$relative; sha256=$actual; probe_asset=('Assets/Source/' + (Split-Path $source -Leaf))}
}
$renderer = Join-Path $PSScriptRoot 'ExportRokCityPlacementBottomNative.cs'
$rendererTarget = Join-Path $probe 'Assets\ExportRokCityPlacementBottomNative.cs'
if (Test-Path -LiteralPath $rendererTarget) {
    $old = (Get-FileHash -LiteralPath $rendererTarget -Algorithm SHA256).Hash
    $new = (Get-FileHash -LiteralPath $renderer -Algorithm SHA256).Hash
    $runs = Join-Path $probe 'OutputRuns'
    if ($old -ne $new -and (Test-Path -LiteralPath $runs) -and
        @(Get-ChildItem -LiteralPath $runs -Directory).Count -gt 0) {
        throw 'Native runs already pin another renderer; use a fresh isolated probe directory'
    }
}
Copy-Item -LiteralPath $renderer -Destination $rendererTarget -Force
New-Item -ItemType Directory -Path (Join-Path $probe 'Packages') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $probe 'ProjectSettings') -Force | Out-Null
$packagePath = Join-Path $probe 'Packages\manifest.json'
$packages = '{"dependencies":{"com.unity.modules.imageconversion":"1.0.0","com.unity.modules.jsonserialize":"1.0.0","com.unity.modules.ui":"1.0.0"}}'
if ((Test-Path -LiteralPath $packagePath) -and ((Get-Content -LiteralPath $packagePath -Raw).Trim() -ne $packages)) { throw 'Isolated package manifest differs' }
[System.IO.File]::WriteAllText($packagePath, $packages + "`n", (New-Object System.Text.UTF8Encoding($false)))
$versionPath = Join-Path $probe 'ProjectSettings\ProjectVersion.txt'
$version = "m_EditorVersion: 2019.4.28f1c1`n"
if (Test-Path -LiteralPath $versionPath) {
    $actualVersion = Get-Content -LiteralPath $versionPath -Raw
    $unityNormalized = $version + "m_EditorVersionWithRevision: 2019.4.28f1c1 (1f7bb6726984)`n"
    if ($actualVersion -ne $version -and $actualVersion -ne $unityNormalized) { throw 'Isolated Unity version differs' }
} else {
    [System.IO.File]::WriteAllText($versionPath, $version, (New-Object System.Text.UTF8Encoding($false)))
}
$probeContract = Join-Path $probe 'source_contract.json'
$contractHash = (Get-FileHash -LiteralPath $contractPath -Algorithm SHA256).Hash.ToLowerInvariant()
$rendererHash = (Get-FileHash -LiteralPath $renderer -Algorithm SHA256).Hash.ToLowerInvariant()
if (Test-Path -LiteralPath $probeContract) {
    $oldContract = Get-Content -LiteralPath $probeContract -Raw | ConvertFrom-Json
    if ($oldContract.schema -ne 'rok.city_placement_bottom_native_probe.v1' -or
        $oldContract.source_contract_sha256 -ne $contractHash -or
        $oldContract.renderer_sha256 -ne $rendererHash) { throw 'Existing isolated probe contract differs' }
} else {
    $prepared = @{schema='rok.city_placement_bottom_native_probe.v1'; source_contract_sha256=$contractHash; renderer_sha256=$rendererHash; source_files=$copies} | ConvertTo-Json -Depth 5
    [System.IO.File]::WriteAllText($probeContract, $prepared + "`n", (New-Object System.Text.UTF8Encoding($false)))
}
Write-Output "Prepared isolated Unity 2019 placement-bottom probe: $probe"
if (!$Run) { return }
if (!(Test-Path -LiteralPath $UnityEditor -PathType Leaf)) { throw "Unity 2019 editor missing: $UnityEditor" }
$logDir = Join-Path $repo 'Saved\Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$log = Join-Path $logDir ('CityPlacementBottomNative_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '.log')
$probeArguments = @('-batchmode', '-projectPath', ('"' + $probe + '"'), '-executeMethod', 'ExportRokCityPlacementBottomNative.Run', '-logFile', ('"' + $log + '"'))
$probeProcess = Start-Process -FilePath $UnityEditor -ArgumentList $probeArguments -WindowStyle Hidden -PassThru
$probeProcess.WaitForExit()
$probeProcess.Refresh()
if ($probeProcess.ExitCode -ne 0) { throw "Unity placement-bottom native probe failed ($($probeProcess.ExitCode)): $log" }
Write-Output "Unity native probe complete: $log"
