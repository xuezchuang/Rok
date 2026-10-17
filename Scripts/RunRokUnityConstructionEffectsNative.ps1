param(
    [switch]$Run,
    [ValidatePattern('^UnityCityConstructionEffectsProbeV[2-9][0-9]*$')]
    [string]$ProbeName = 'UnityCityConstructionEffectsProbeV2',
    [string]$UnityEditor = 'C:\Program Files\Unity\Hub\Editor\2019.4.28f1c1\Editor\Unity.exe'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$saved = [IO.Path]::GetFullPath((Join-Path $repo 'Saved'))
$probe = [IO.Path]::GetFullPath((Join-Path $saved $ProbeName))
if ([IO.Path]::GetDirectoryName($probe) -ne $saved) { throw 'Probe path escaped Saved' }
$inventoryPath = Join-Path $repo 'Saved\RokUnityMigration\BatchReview20261009Construction\construction_effect_source_inventory.json'
$expectedInventoryHash = '01dd55e415bfc8f9284642f22e73f47e111a1b34ccaf126788e7357189844ffe'
if (!(Test-Path -LiteralPath $inventoryPath -PathType Leaf)) { throw 'Construction effect inventory missing' }
$inventoryHash = (Get-FileHash -LiteralPath $inventoryPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($inventoryHash -ne $expectedInventoryHash) { throw 'Sealed construction effect inventory hash differs' }
$inventory = Get-Content -LiteralPath $inventoryPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($inventory.kind -ne 'unity_source_inventory_only' -or
    $inventory.selection.selected -ne 'Build_shelf_5' -or
    @($inventory.sources).Count -lt 40) { throw 'Construction source inventory shape differs' }

$sourcePrefix = 'ref/client-unity/Assets/'
$sourceFiles = @()
$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($item in $inventory.sources) {
    $relative = [string]$item.path
    if (!$relative.StartsWith($sourcePrefix, [System.StringComparison]::Ordinal) -or
        !$seen.Add($relative)) { continue }
    if ($relative.Contains('..')) { throw "Unsafe source path: $relative" }
    $source = Join-Path $repo ($relative.Replace('/', '\'))
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing source: $relative" }
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne ([string]$item.sha256).ToLowerInvariant()) { throw "Source hash differs: $relative" }
    $meta = "$source.meta"
    if (!(Test-Path -LiteralPath $meta -PathType Leaf)) { throw "Missing source meta: $relative" }
    $metaHash = (Get-FileHash -LiteralPath $meta -Algorithm SHA256).Hash.ToLowerInvariant()
    $assetRelative = $relative.Substring($sourcePrefix.Length)
    # Hotfix/client scripts have external dependencies. Preserve hashes in the
    # contract, but let Unity expose missing MonoBehaviours in the isolated copy.
    $copied = !([IO.Path]::GetExtension($relative) -ieq '.cs')
    $probeAsset = if ($copied) { 'Assets/Source/' + $assetRelative } else { '' }
    if ($copied) {
        $target = Join-Path $probe ('Assets\Source\' + $assetRelative.Replace('/', '\'))
        $targetParent = Split-Path -Parent $target
        New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
        foreach ($pair in @(@($source, $target, $hash), @($meta, "$target.meta", $metaHash))) {
            if (Test-Path -LiteralPath $pair[1]) {
                $existingHash = (Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($existingHash -ne $pair[2]) { throw "Existing isolated copy differs: $($pair[1])" }
            } else { Copy-Item -LiteralPath $pair[0] -Destination $pair[1] }
        }
    }
    $sourceFiles += [ordered]@{
        source_path = $relative
        source_sha256 = $hash
        meta_sha256 = $metaHash
        probe_path = $probeAsset
        copied = $copied
    }
}

foreach ($required in @(
    'BundleAssets/Scene/City_building/city_building_1/Build_shelf_5.prefab',
    'BundleAssets/Effect/Prefabs/build_3003.prefab',
    'BundleAssets/Effect/Prefabs/build_3004.prefab')) {
    if (!($sourceFiles | Where-Object { $_.probe_path -eq ('Assets/Source/' + $required) })) {
        throw "Required prefab absent from sealed inventory: $required"
    }
}

$copiedGuids = @{}
$omittedScriptGuids = @{}
$builtinGuids = @('0000000000000000f000000000000000')
foreach ($record in $sourceFiles) {
    $metaPath = Join-Path $repo (($record.source_path + '.meta').Replace('/', '\'))
    $metaText = Get-Content -LiteralPath $metaPath -Raw
    if ($metaText -notmatch '(?m)^guid: ([0-9a-f]{32})') { throw "Source GUID missing: $($record.source_path)" }
    if ($record.copied) { $copiedGuids[$Matches[1]] = $record.source_path }
    else { $omittedScriptGuids[$Matches[1]] = $record.source_path }
}
$referencedOmittedScripts = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$sourceAssetRoot = Join-Path $probe 'Assets\Source'
Get-ChildItem -LiteralPath $sourceAssetRoot -Recurse -File -Force |
    Where-Object { $_.Extension -in @('.prefab', '.anim', '.mat', '.shader') } |
    ForEach-Object {
        $text = Get-Content -LiteralPath $_.FullName -Raw
        foreach ($match in [regex]::Matches($text, 'guid: ([0-9a-f]{32})')) {
            $guid = $match.Groups[1].Value
            if ($guid -in $builtinGuids) { continue } # Explicit Unity built-in shader GUID from sealed source
            if ($copiedGuids.ContainsKey($guid)) { continue }
            if ($omittedScriptGuids.ContainsKey($guid)) {
                [void]$referencedOmittedScripts.Add("$guid $($omittedScriptGuids[$guid])")
                continue
            }
            throw "Unresolved non-script source GUID $guid in $($_.FullName)"
        }
    }

$exporter = Join-Path $PSScriptRoot 'ExportRokUnityConstructionEffectsNative.cs'
if (!(Test-Path -LiteralPath $exporter -PathType Leaf)) { throw 'Native exporter missing' }
$exporterHash = (Get-FileHash -LiteralPath $exporter -Algorithm SHA256).Hash.ToLowerInvariant()
$exporterTarget = Join-Path $probe 'Assets\Editor\ExportRokUnityConstructionEffectsNative.cs'
$runsPath = Join-Path $probe 'OutputRuns'
$hasRuns = (Test-Path -LiteralPath $runsPath) -and
    @((Get-ChildItem -LiteralPath $runsPath -Directory)).Count -gt 0
New-Item -ItemType Directory -Path (Split-Path -Parent $exporterTarget) -Force | Out-Null
if (Test-Path -LiteralPath $exporterTarget) {
    if ((Get-FileHash -LiteralPath $exporterTarget -Algorithm SHA256).Hash.ToLowerInvariant() -ne $exporterHash) {
        if ($hasRuns) { throw 'Existing isolated exporter differs; use a new probe directory instead of changing a pinned capture' }
        Copy-Item -LiteralPath $exporter -Destination $exporterTarget -Force
    }
} else { Copy-Item -LiteralPath $exporter -Destination $exporterTarget }

$packagesPath = Join-Path $probe 'Packages\manifest.json'
$packages = '{"dependencies":{"com.unity.modules.imageconversion":"1.0.0","com.unity.modules.jsonserialize":"1.0.0","com.unity.modules.particlesystem":"1.0.0"}}' + "`n"
New-Item -ItemType Directory -Path (Split-Path -Parent $packagesPath) -Force | Out-Null
if ((Test-Path -LiteralPath $packagesPath) -and
    (Get-Content -LiteralPath $packagesPath -Raw) -ne $packages) { throw 'Isolated package manifest differs' }
if (!(Test-Path -LiteralPath $packagesPath)) {
    [IO.File]::WriteAllText($packagesPath, $packages, [Text.UTF8Encoding]::new($false))
}
$versionPath = Join-Path $probe 'ProjectSettings\ProjectVersion.txt'
$version = "m_EditorVersion: 2019.4.28f1c1`n"
$versionWithRevision = $version + "m_EditorVersionWithRevision: 2019.4.28f1c1 (1f7bb6726984)`n"
New-Item -ItemType Directory -Path (Split-Path -Parent $versionPath) -Force | Out-Null
if (Test-Path -LiteralPath $versionPath) {
    $existingVersion = Get-Content -LiteralPath $versionPath -Raw
    if ($existingVersion -ne $version -and $existingVersion -ne $versionWithRevision) {
        throw 'Isolated Unity project version differs'
    }
} else { [IO.File]::WriteAllText($versionPath, $version, [Text.UTF8Encoding]::new($false)) }

$sourceSettings = Join-Path $repo 'ref\client-unity\ProjectSettings\ProjectSettings.asset'
if (!(Test-Path -LiteralPath $sourceSettings -PathType Leaf) -or
    !(Select-String -LiteralPath $sourceSettings -Pattern '^  m_ActiveColorSpace: 0$' -Quiet)) {
    throw 'Source Unity Gamma color-space setting differs'
}
$contractPath = Join-Path $probe 'source_contract.json'
$contract = [ordered]@{
    schema = 'rok.construction_effect_native_probe_contract.v1'
    repo_root = ($repo.Replace('\', '/'))
    probe_root = ($probe.Replace('\', '/'))
    unity_version = '2019.4.28f1c1'
    source_color_space = 'Gamma'
    source_project_settings_sha256 = (Get-FileHash -LiteralPath $sourceSettings -Algorithm SHA256).Hash.ToLowerInvariant()
    source_inventory_sha256 = $inventoryHash
    exporter_sha256 = $exporterHash
    source_files = $sourceFiles
    omitted_script_guid_refs = @($referencedOmittedScripts | Sort-Object)
    allowed_builtin_guid_refs = $builtinGuids
    lifecycle_limit = 'No GameApp/server/hotfix mediator is loaded; native Animation and ParticleSystem observations only.'
}
$contractText = ($contract | ConvertTo-Json -Depth 8) + "`n"
if (Test-Path -LiteralPath $contractPath) {
    if ((Get-Content -LiteralPath $contractPath -Raw -Encoding UTF8) -ne $contractText) {
        if ($hasRuns) { throw 'Existing isolated source contract differs; do not overwrite a pinned capture' }
        [IO.File]::WriteAllText($contractPath, $contractText, [Text.UTF8Encoding]::new($false))
    }
} else { [IO.File]::WriteAllText($contractPath, $contractText, [Text.UTF8Encoding]::new($false)) }

Write-Output "Prepared isolated Unity construction-effects probe: $probe"
Write-Output "Source records: $($sourceFiles.Count); copied assets: $(($sourceFiles | Where-Object copied).Count); exporter SHA256: $exporterHash"
if (!$Run) { return }
if (!(Test-Path -LiteralPath $UnityEditor -PathType Leaf)) { throw "Unity 2019.4.28f1c1 missing: $UnityEditor" }
$logDirectory = Join-Path $repo 'Saved\Logs'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$log = Join-Path $logDirectory ('UnityCityConstructionEffectsNative_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '.log')
$arguments = @('-batchmode', '-force-d3d11', '-projectPath', ('"' + $probe + '"'),
    '-executeMethod', 'ExportRokUnityConstructionEffectsNative.Run', '-logFile', ('"' + $log + '"'))
$oldProbeName = $env:ROK_CONSTRUCTION_EFFECT_PROBE_NAME
try {
    $env:ROK_CONSTRUCTION_EFFECT_PROBE_NAME = $ProbeName
    $process = Start-Process -FilePath $UnityEditor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Write-Output "Unity construction-effects probe PID=$($process.Id) log=$log"
    $process.WaitForExit()
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Unity construction-effects export failed (exit $($process.ExitCode)): $log" }
} finally {
    if ($null -eq $oldProbeName) { Remove-Item Env:ROK_CONSTRUCTION_EFFECT_PROBE_NAME -ErrorAction SilentlyContinue }
    else { $env:ROK_CONSTRUCTION_EFFECT_PROBE_NAME = $oldProbeName }
}
Write-Output "Unity construction-effects probe completed: $log"
