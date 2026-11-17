# Capture the existing Canvas HUD styles and create the editable style asset.
param([switch]$UseExistingSpec)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$source = Join-Path $specDir 'world_canvas_hud_style.json'
if (!$UseExistingSpec) {
    if (Test-Path -LiteralPath $source) { Remove-Item -LiteralPath $source }
    & (Join-Path $PSScriptRoot 'Capture.ps1') -Name spec_world_canvas_hud_style `
        -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokWorldMapDebugHud -RokNoNet -RokUiExportWorldCanvasStyle=$source"
}
if (!(Test-Path -LiteralPath $source)) { throw "Missing world Canvas HUD style spec $source" }
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_world_canvas_hud_style `
    -Commands 'Rok.UiWbp.GeneratePage /Game/RokUI/Hud/WBP_RokWorldCanvasHudStyle world_canvas_hud_style'
