# Export the two damage number styles from the native world HUD.
# The generator fills missing content and preserves existing Designer edits.
param([switch]$UseExistingSpec)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$source = Join-Path $specDir 'world_damage_art.json'
if (!$UseExistingSpec) {
    if (Test-Path -LiteralPath $source) { Remove-Item -LiteralPath $source }
    & (Join-Path $PSScriptRoot 'Capture.ps1') -Name spec_world_damage_art `
        -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokNoNet -RokUiExportWorldDamageArt=$source"
}
if (!(Test-Path -LiteralPath $source)) { throw "Missing world damage number spec $source" }
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_world_damage_numbers `
    -Commands 'Rok.UiWbp.GeneratePage /Game/RokUI/Hud/WBP_RokWorldDamageNumbers world_damage_art'
