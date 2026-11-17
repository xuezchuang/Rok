# Generate the extra HUD layouts from legacy game widgets without modifying the
# city, tactical, or world HUD assets. Task rows and live world-object state are
# attached to the new pages by the HUD controllers.
param([switch]$UseExistingSpecs)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$city = Join-Path $specDir 'city_hud.json'
$worldObject = Join-Path $specDir 'world_object_hud.json'
if (!$UseExistingSpecs) {
    foreach ($spec in $city, $worldObject) {
        if (Test-Path -LiteralPath $spec) { Remove-Item -LiteralPath $spec }
    }
    $capture = Join-Path $PSScriptRoot 'Capture.ps1'
    & $capture -Name spec_city_hud_sections -Extra "-RokUiLegacy -RokUiExportMainHud=$city"
    & $capture -Name spec_world_object_hud -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokUiExportWorldObjectHud=$worldObject"
}
foreach ($spec in $city, $worldObject) {
    if (!(Test-Path -LiteralPath $spec)) { throw "Missing HUD layout spec $spec" }
}

$chrome = 'PositionGroup+AgeAndRuntimeGroup+ChatBackground+ChatWorldMarker+ChatAllianceMarkerBase+ChatAllianceMarker+ChatButton+BuildCityButton+WorldSearchButton+CityWorldButton+WorldGlobeButton'
$task = 'HotTaskGroup+ArmyQueueEntry'
$feature = 'FeatureBackground+FeatureButtonsClip+MoreFeatureButton+MailFeature+MailFeatureHitButton+ActivityButton+ActivityBadge+ChargeEventsButton+ChargeEventsBadge'
$tip = 'ExpeditionUnlockTip'
$hud = '/Game/RokUI/Hud'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokHudTasks city_hud -Root=UnityExpandedDesignCanvas -Exclude=$chrome+$feature+$tip -SkipChildren=TaskList"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokHudFeatures city_hud -Root=UnityExpandedDesignCanvas -Exclude=$chrome+$task+$tip"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokHudExpeditionTip city_hud -Root=UnityExpandedDesignCanvas -Exclude=$chrome+$task+$feature"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldObjectInfo world_object_hud"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_hud_sections -Commands ($commands -join ', ')
