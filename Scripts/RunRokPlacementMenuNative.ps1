param(
    [switch]$Run,
    [string]$UnityEditor = 'C:\Program Files\Unity\Hub\Editor\2019.4.28f1c1\Editor\Unity.exe'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$probe = Join-Path $repo 'Saved\UnityPlacementMenuProbeV3'
$assetDir = Join-Path $probe 'Assets\Source'
$outputDir = Join-Path $probe 'Artifacts'
$renderer = Join-Path $PSScriptRoot 'RenderRokPlacementMenuNative.cs'
$rendererHash = (Get-FileHash -LiteralPath $renderer -Algorithm SHA256).Hash.ToLowerInvariant()
$runnerHash = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
$sources = @(
    'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.png',
    'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.png.meta',
    'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.tpsheet',
    'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIMainCity/UI_Pop_BuildingMenu.prefab',
    'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UI_Item_CMDBtns.prefab',
    'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_CommandBtn.prefab',
    'ref/client-unity/Assets/BundleAssets/Default/UI_GrayWithMask.mat',
    'ref/client-unity/Assets/BundleAssets/Default/UI_GrayWithMask.mat.meta',
    'ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader',
    'ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader.meta',
    'ref/client-unity/Assets/Scripts/Client/Compoment/UIComponent/GrayChildrens.cs'
)
$pinnedSourceHashes = @{
    'ui_hud.png' = '41ca1bc2e55560b66dc74ca4acd5003a7675125145e3e6f54789a1b3eef2fd50'
    'ui_hud.tpsheet' = '3cceedc5f01d63e6bfe50f29746d44673da31cbcfacd09cd53130336917f49b0'
    'UI_Pop_BuildingMenu.prefab' = '01af7674b301543c101f765a3e6087980ffc5a38faeed8fa6d3d69babc9b331e'
    'UI_Item_CMDBtns.prefab' = '49a63e6cdf2118a7d2924871657bcc980935a671de95e9b14c96612bcb80f8d4'
    'UI_Model_CommandBtn.prefab' = '24324d6c75ba9c4622b0f0f18802cd1b1a1b4075e3a53da33cb4adafad2fa20a'
    'ImageGrayShader.shader' = '3af4b57bb8519c4dadc5f79a34aa2106e0c6f548686b001a016b5e39f608b273'
}
New-Item -ItemType Directory -Path $assetDir, $outputDir, (Join-Path $probe 'Packages'), (Join-Path $probe 'ProjectSettings') -Force | Out-Null
$copies = @()
foreach ($relative in $sources) {
    $source = Join-Path $repo ($relative.Replace('/', '\'))
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing source: $relative" }
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    $leaf = Split-Path $source -Leaf
    if ($pinnedSourceHashes.ContainsKey($leaf) -and $hash -ne $pinnedSourceHashes[$leaf]) { throw "Pinned source changed: $relative" }
    $loadedByUnity = $leaf -in @('ui_hud.png','ui_hud.png.meta','UI_GrayWithMask.mat','UI_GrayWithMask.mat.meta','ImageGrayShader.shader','ImageGrayShader.shader.meta')
    $relativeTarget = if ($loadedByUnity) { "Assets/Source/$leaf" } else { "SourceEvidence/$leaf" }
    $target = Join-Path $probe ($relativeTarget.Replace('/', '\'))
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    if (Test-Path -LiteralPath $target) {
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash) { throw "Isolated source differs: $target" }
    } else { Copy-Item -LiteralPath $source -Destination $target }
    $copies += [ordered]@{path=$relative;sha256=$hash;probeAsset=$relativeTarget}
}
$rendererTarget = Join-Path $probe 'Assets\RenderRokPlacementMenuNative.cs'
if (Test-Path -LiteralPath $rendererTarget) {
    $oldHash = (Get-FileHash -LiteralPath $rendererTarget -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($oldHash -ne $rendererHash -and @(Get-ChildItem -LiteralPath $outputDir -Directory).Count -gt 0) {
        throw 'Existing native runs pin another renderer; use a new isolated project'
    }
}
Copy-Item -LiteralPath $renderer -Destination $rendererTarget -Force
$packagePath = Join-Path $probe 'Packages\manifest.json'
$packageJson = '{"dependencies":{"com.unity.modules.imageconversion":"1.0.0","com.unity.modules.jsonserialize":"1.0.0","com.unity.modules.ui":"1.0.0","com.unity.modules.imgui":"1.0.0","com.unity.ugui":"1.0.0"}}'
if (Test-Path -LiteralPath $packagePath) {
    if ((Get-Content -LiteralPath $packagePath -Raw).Trim() -ne $packageJson -and
        @(Get-ChildItem -LiteralPath $outputDir -Directory).Count -gt 0) { throw 'Existing native runs pin another package manifest' }
}
[System.IO.File]::WriteAllText($packagePath, $packageJson + "`n", (New-Object System.Text.UTF8Encoding($false)))
$versionPath = Join-Path $probe 'ProjectSettings\ProjectVersion.txt'
if (Test-Path -LiteralPath $versionPath) {
    if ((Get-Content -LiteralPath $versionPath -Raw) -notmatch '^m_EditorVersion: 2019\.4\.28f1c1') { throw 'Isolated Unity version differs' }
} else { [System.IO.File]::WriteAllText($versionPath, "m_EditorVersion: 2019.4.28f1c1`n", (New-Object System.Text.UTF8Encoding($false))) }
$lookup = @{}
foreach ($copy in $copies) { $lookup[$copy.path] = $copy.sha256 }
$contract = [ordered]@{
    schema='rok.placement_menu_native_input.v1'
    scope='isolated_source_derived_ugui_fixture_not_gameapp'
    runnerSha256=$runnerHash
    rendererSha256=$rendererHash
    spriteAtlasSha256=$lookup['ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.png']
    spriteSheetSha256=$lookup['ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.tpsheet']
    grayShaderSha256=$lookup['ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader']
    grayMaterialSha256=$lookup['ref/client-unity/Assets/BundleAssets/Default/UI_GrayWithMask.mat']
    width=1440
    height=810
    sourceFiles=$copies
}
$contractPath = Join-Path $probe 'source_contract.json'
$contractJson = ($contract | ConvertTo-Json -Depth 6)
if (Test-Path -LiteralPath $contractPath) {
    $old = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
    if ($old.schema -ne $contract.schema -or $old.runnerSha256 -ne $runnerHash -or $old.rendererSha256 -ne $rendererHash -or
        $old.spriteAtlasSha256 -ne $contract.spriteAtlasSha256 -or $old.grayShaderSha256 -ne $contract.grayShaderSha256) {
        if (@(Get-ChildItem -LiteralPath $outputDir -Directory).Count -gt 0) { throw 'Existing native runs pin another source contract' }
    }
}
[System.IO.File]::WriteAllText($contractPath, $contractJson + "`n", (New-Object System.Text.UTF8Encoding($false)))
Write-Output "Prepared isolated placement menu fixture: $probe"
Write-Output "Source contract SHA256: $((Get-FileHash -LiteralPath $contractPath -Algorithm SHA256).Hash.ToLowerInvariant())"
if (!$Run) { return }
if (!(Test-Path -LiteralPath $UnityEditor -PathType Leaf)) { throw "Unity 2019 editor missing: $UnityEditor" }
$logDir = Join-Path $repo 'Saved\Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$log = Join-Path $logDir ('PlacementMenuNative_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '.log')
$arguments = @('-batchmode','-projectPath',('"'+$probe+'"'),'-executeMethod','RenderRokPlacementMenuNative.Run','-logFile',('"'+$log+'"'))
$process = Start-Process -FilePath $UnityEditor -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit(); $process.Refresh()
if ($process.ExitCode -ne 0) { throw "Unity placement menu fixture failed ($($process.ExitCode)): $log" }
Write-Output "Unity placement menu fixture complete: $log"
