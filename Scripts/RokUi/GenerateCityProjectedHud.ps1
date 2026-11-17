# Export the editable parts of projected city HUDs. C++ still projects the
# placement and nameplate roots and binds live values into the task rows.
param([switch]$UseExistingSpec)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$sourcePath = Join-Path $specDir 'city_projected_hud.json'
if (!$UseExistingSpec) {
    if (Test-Path -LiteralPath $sourcePath) { Remove-Item -LiteralPath $sourcePath }
    & (Join-Path $PSScriptRoot 'Capture.ps1') -Name spec_city_projected_hud `
        -Extra "-RokUiLegacy -RokNoNet -RokUiExportCityProjected=$sourcePath"
}
if (!(Test-Path -LiteralPath $sourcePath)) { throw "Missing city HUD spec $sourcePath" }

$source = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json -AsHashtable
function Find-LayoutNode($Nodes, [string]$Name) {
    foreach ($node in $Nodes) {
        if ($node.name -ceq $Name) { $node }
        if ($node.children.Count) { Find-LayoutNode $node.children $Name }
    }
}
function Write-LayoutSpec([string]$Name, [string]$RootName) {
    $roots = @(Find-LayoutNode $source.nodes $RootName)
    if ($roots.Count -ne 1 -or !$roots[0].children.Count) { throw "Expected one non-empty layout called $RootName" }
    @{
        version = $source.version
        label = $Name
        root = $RootName
        nodes = $roots[0].children
        notes = @()
    } | ConvertTo-Json -Depth 100 | Set-Content -Encoding utf8 (Join-Path $specDir "$Name.json")
}
Write-LayoutSpec 'city_hud_placement' 'CityPlacementControls'
Write-LayoutSpec 'city_hud_nameplate' 'PlayerCityWorldMarker'
Write-LayoutSpec 'city_hud_task_row' 'TaskRow_0'

$hud = '/Game/RokUI/Hud'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCityPlacementControls city_hud_placement"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokCityNameplate city_hud_nameplate"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokHudTaskRow city_hud_task_row -Suffix=_0"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_city_projected_hud -Commands ($commands -join ', ')
