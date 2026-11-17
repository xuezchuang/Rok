# Export the native city selection status and world object card arrow.
# GeneratePage creates missing assets and preserves existing Designer edits.
param([switch]$UseExistingSpecs)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$city = Join-Path $specDir 'city_selection_status_source.json'
$world = Join-Path $specDir 'world_object_info_arrow_source.json'
if (!$UseExistingSpecs) {
    foreach ($spec in $city, $world) {
        if (Test-Path -LiteralPath $spec) { Remove-Item -LiteralPath $spec }
    }
    $capture = Join-Path $PSScriptRoot 'Capture.ps1'
    & $capture -Name spec_city_selection_status -Extra "-RokUiLegacy -RokNoNet -RokUiExportCityProjected=$city"
    & $capture -Name spec_world_object_info_arrow -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokNoNet -RokUiExportWorldObjectInfoArrow=$world"
}
function Find-Node($Nodes, [string]$Name) {
    foreach ($node in $Nodes) {
        if ($node.name -ceq $Name) { $node }
        if ($node.children) { Find-Node $node.children $Name }
    }
}
function Write-LayoutSpec([string]$Path, [string]$WidgetName, [string]$SpecName) {
    if (!(Test-Path -LiteralPath $Path)) { throw "Missing HUD part spec $Path" }
    $source = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable
    $nodes = @(Find-Node $source.nodes $WidgetName)
    if ($nodes.Count -ne 1) { throw "Expected one $WidgetName layout" }
    @{
        version = $source.version
        label = $SpecName
        root = $source.root
        nodes = $nodes
        notes = @()
    } | ConvertTo-Json -Depth 100 | Set-Content -Encoding utf8 (Join-Path $specDir "$SpecName.json")
}
Write-LayoutSpec $city 'SelectionStatusText' 'city_selection_status'
Write-LayoutSpec $world 'WorldObjectInfoArrow' 'world_object_info_arrow'
$hud = '/Game/RokUI/Hud'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCitySelectionStatus city_selection_status"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldObjectInfoArrow world_object_info_arrow"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_hud_selection_details -Commands ($commands -join ', ')
