# Export the legacy reward cell and army choice layouts from the world HUD.
# Existing Widget Blueprints are preserved by the create-only generator.
param([switch]$UseExistingSpec)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$specs = @('world_barbarian_reward', 'world_attack_army_choice')
if (!$UseExistingSpec) {
    foreach ($name in $specs) {
        $path = Join-Path $specDir "$name.json"
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    & (Join-Path $PSScriptRoot 'Capture.ps1') -Name spec_world_battle_rows `
        -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokNoNet -RokUiExportWorldBattleRows=$specDir"
}
foreach ($name in $specs) {
    $path = Join-Path $specDir "$name.json"
    if (!(Test-Path -LiteralPath $path)) { throw "Missing world battle row spec $path" }
}

$hud = '/Game/RokUI/Hud'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldBarbarianReward world_barbarian_reward -Index=_0"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldAttackArmyChoice world_attack_army_choice -Suffix=_0"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_world_battle_rows -Commands ($commands -join ', ')
