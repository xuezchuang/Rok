# Generate layout templates for the projected city indicators from the
# actual game HUD. C++ keeps ownership of the pools and runtime projection.
param([switch]$UseExistingSpec)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$sourcePath = Join-Path $specDir 'city_hud_indicators.json'
if (!$UseExistingSpec) {
    if (Test-Path -LiteralPath $sourcePath) { Remove-Item -LiteralPath $sourcePath }
    & (Join-Path $PSScriptRoot 'Capture.ps1') -Name spec_city_hud_indicators `
        -Extra "-RokUiLegacy -RokNoNet -RokUiExportCityProjected=$sourcePath"
}
if (!(Test-Path -LiteralPath $sourcePath)) { throw "Missing city HUD spec $sourcePath" }

$source = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json -AsHashtable
function Get-Node($Nodes, [string]$Name) {
    $found = @($Nodes | Where-Object { $_.name -ceq $Name })
    if ($found.Count -ne 1) { throw "Expected a single HUD node named $Name, found $($found.Count)" }
    return $found[0]
}
function Write-IndicatorSpec([string]$Name, [object[]]$Nodes) {
    if (!$Nodes.Count) { throw "No nodes for $Name" }
    @{
        version = $source.version
        label = $Name
        root = $source.root
        nodes = $Nodes
        notes = @()
    } | ConvertTo-Json -Depth 100 | Set-Content -Encoding utf8 (Join-Path $specDir "$Name.json")
}
$scale = Get-Node $source.nodes 'UnityExpandScaleBox'
$size = Get-Node $scale.children 'UnityExpandedDesignSize'
$design = Get-Node $size.children 'UnityExpandedDesignCanvas'
Write-IndicatorSpec 'city_hud_construction_bar' (Get-Node $design.children 'ConstructionBar_0').children
Write-IndicatorSpec 'city_hud_harvest_bubble' (Get-Node $design.children 'CityHarvestBubble_0').children
Write-IndicatorSpec 'city_hud_training_indicator' (Get-Node $design.children 'ServerTrainingPop_0').children
Write-IndicatorSpec 'city_hud_harvest_feedback' @((Get-Node $design.children 'CityHarvestFeedback'))
Write-IndicatorSpec 'city_hud_barracks_idle' @(
    (Get-Node $design.children 'BarracksIdleZ')
    (Get-Node $design.children 'BarracksIdleSmallZ')
)

$hud = '/Game/RokUI/Hud'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCityConstructionBar city_hud_construction_bar -Suffix=_0"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCityHarvestBubble city_hud_harvest_bubble -Suffix=_0"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCityTrainingIndicator city_hud_training_indicator -Suffix=_0"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCityHarvestFeedback city_hud_harvest_feedback"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCityBarracksIdle city_hud_barracks_idle"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_city_hud_indicators -Commands ($commands -join ', ')
