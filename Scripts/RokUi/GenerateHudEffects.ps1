# Export the power effect's original slot and the world overlay art samples.
# The Blueprint generator creates missing assets and preserves Designer edits.
param([switch]$UseExistingSpecs)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$city = Join-Path $specDir 'city_hud_effect_source.json'
$world = Join-Path $specDir 'world_overlay_art.json'
if (!$UseExistingSpecs) {
    foreach ($spec in $city, $world) {
        if (Test-Path -LiteralPath $spec) { Remove-Item -LiteralPath $spec }
    }
    $capture = Join-Path $PSScriptRoot 'Capture.ps1'
    & $capture -Name spec_hud_power_gain -Extra "-RokUiLegacy -RokNoNet -RokUiExportMainHud=$city"
    & $capture -Name spec_world_overlay_art -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokNoNet -RokUiExportWorldOverlayArt=$world"
}
foreach ($spec in $city, $world) {
    if (!(Test-Path -LiteralPath $spec)) { throw "Missing HUD effect spec $spec" }
}

$source = Get-Content -LiteralPath $city -Raw | ConvertFrom-Json -AsHashtable
function Find-Node($Nodes, [string]$Name) {
    foreach ($node in $Nodes) {
        if ($node.name -ceq $Name) { $node }
        if ($node.children) { Find-Node $node.children $Name }
    }
}
$effect = @(Find-Node $source.nodes 'PlayerPowerGain')
if ($effect.Count -ne 1) { throw "Expected one PlayerPowerGain layout" }
@{
    version = $source.version
    label = 'hud_power_gain'
    root = $source.root
    nodes = $effect
    notes = @()
} | ConvertTo-Json -Depth 100 | Set-Content -Encoding utf8 (Join-Path $specDir 'hud_power_gain.json')

$hud = '/Game/RokUI/Hud'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokHudPowerGain hud_power_gain"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldOverlayArt world_overlay_art"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_hud_effects -Commands ($commands -join ', ')
