# Capture the source HUD layouts in their real maps and generate the three HUD
# Widget Blueprints. The editor generator preserves existing Designer assets.
param(
    [switch]$UseExistingSpecs
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$city = Join-Path $specDir 'city_hud.json'
$world = Join-Path $specDir 'world_hud.json'
$tactical = Join-Path $specDir 'tactical_hud.json'

if (!$UseExistingSpecs) {
    foreach ($spec in $city, $world, $tactical) {
        if (Test-Path -LiteralPath $spec) { Remove-Item -LiteralPath $spec }
    }
    $capture = Join-Path $PSScriptRoot 'Capture.ps1'
    & $capture -Name spec_city_hud -Extra "-RokUiLegacy -RokUiExportMainHud=$city"
    & $capture -Name spec_world_hud -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokUiExportWorldHud=$world -RokUiExportMainHud=$tactical"
}
foreach ($spec in $city, $world, $tactical) {
    if (!(Test-Path -LiteralPath $spec)) { throw "Missing HUD layout spec $spec" }
}

$mainParent = '-ParentClass=/Script/Rok.RokMainHudWidget -RootName=RokMainHudRoot'
$worldParent = '-ParentClass=/Script/Rok.RokWorldMapHudWidget -RootName=RokWorldMapHudRoot'
$hud = '/Game/RokUI/Hud'
# These groups still get their runtime task rows and fold geometry from C++.
$mainRuntime = 'HotTaskGroup+ArmyQueueEntry+FeatureBackground+FeatureButtonsClip+ExpeditionUnlockTip+MoreFeatureButton+MailFeature+MailFeatureHitButton+ActivityButton+ActivityBadge+ChargeEventsButton+ChargeEventsBadge'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCityHud city_hud $mainParent -Exclude=$mainRuntime+PlayerPowerGain"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokTacticalHud tactical_hud $mainParent -Exclude=$mainRuntime"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldMapHud world_hud $worldParent -Exclude=WorldMapSelectionInput+WorldMapMiniMapInput+WorldMapPowerGain"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_hud -Commands ($commands -join ', ')
