# Export the four world HUD panels through the actual legacy world map and
# generate their Widget Blueprints. The generator never overwrites a WBP.
param([switch]$UseExistingSpec)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$sourcePath = Join-Path $specDir 'world_panels.json'
if (!$UseExistingSpec) {
    if (Test-Path -LiteralPath $sourcePath) { Remove-Item -LiteralPath $sourcePath }
    & (Join-Path $PSScriptRoot 'Capture.ps1') -Name spec_world_hud_panels `
        -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokUiExportWorldPanels=$sourcePath"
}
if (!(Test-Path -LiteralPath $sourcePath)) { throw "Missing HUD panel spec $sourcePath" }

$source = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json -AsHashtable
function Get-Node($Nodes, [string]$Name) {
    $found = @($Nodes | Where-Object { $_.name -ceq $Name })
    if ($found.Count -ne 1) { throw "Expected one HUD node named $Name, found $($found.Count)" }
    return $found[0]
}
function Write-PanelSpec([string]$Name, [object[]]$Nodes) {
    if (!$Nodes.Count) { throw "No nodes for $Name" }
    @{
        version = $source.version
        label = $Name
        root = $source.root
        nodes = $Nodes
        notes = @()
    } | ConvertTo-Json -Depth 100 | Set-Content -Encoding utf8 (Join-Path $specDir "$Name.json")
}
$worldObject = Get-Node $source.nodes 'WorldObjectInfoRoot'
$gather = Get-Node $source.nodes 'WorldGatherDispatch'
Write-PanelSpec 'world_hud_kingdoms' @((Get-Node $source.nodes 'WorldMapServerListOverlay'))
Write-PanelSpec 'world_hud_barbarian' @((Get-Node $worldObject.children 'WorldBarbarianInfo'))
Write-PanelSpec 'world_hud_march_queue' @((Get-Node $source.nodes 'WorldMarchQueue'))
Write-PanelSpec 'world_hud_gather_dispatch' @($gather.children | Where-Object { $_.name -notlike 'WorldGatherRouteDot_*' })

$hud = '/Game/RokUI/Hud'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldKingdoms world_hud_kingdoms"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldBarbarianInfo world_hud_barbarian -SkipChildren=WorldBarbarianRewards"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldMarchQueue world_hud_march_queue -SkipChildren=WorldMarchQueueList"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldGatherDispatch world_hud_gather_dispatch -SkipChildren=WorldAttackArmyChoices"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_world_hud_panels -Commands ($commands -join ', ')
