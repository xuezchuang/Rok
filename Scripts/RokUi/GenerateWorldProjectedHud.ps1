# Use the legacy world HUD's layouts for projected markers and march portraits.
# GeneratePage is create-only and preserves edits to existing assets.
param([switch]$UseExistingSpec)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specDir = Join-Path $projectRoot 'Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specDir | Out-Null
$sourcePath = Join-Path $specDir 'world_projected_hud.json'
$badgePath = Join-Path $specDir 'world_projected_entity_badge.json'
$marchPath = Join-Path $specDir 'world_march_row.json'
if (!$UseExistingSpec) {
    foreach ($spec in $sourcePath, $badgePath, $marchPath) {
        if (Test-Path -LiteralPath $spec) { Remove-Item -LiteralPath $spec }
    }
    & (Join-Path $PSScriptRoot 'Capture.ps1') -Name spec_world_projected_hud `
        -Map '/Game/Maps/L_RokUnityWorldMapPrototype' `
        -Extra "-RokUiLegacy -RokNoNet -RokUiExportWorldProjected=$specDir"
}
foreach ($spec in $sourcePath, $badgePath, $marchPath) {
    if (!(Test-Path -LiteralPath $spec)) { throw "Missing world HUD spec $spec" }
}

$source = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json -AsHashtable
function Write-MarkerSpec([string]$Name, [string]$RootName) {
    $matches = @($source.nodes | Where-Object { $_.name -ceq $RootName })
    if ($matches.Count -ne 1 -or !$matches[0].children.Count) { throw "No layout children for $RootName" }
    @{
        version = $source.version
        label = $Name
        root = $source.root
        nodes = @($matches[0].children)
        notes = @()
    } | ConvertTo-Json -Depth 100 | Set-Content -Encoding utf8 (Join-Path $specDir "$Name.json")
}
Write-MarkerSpec 'world_projected_city_nameplate' 'WorldMapSelfMarker'
Write-MarkerSpec 'world_projected_city_portrait' 'WorldMapSelfPortrait'
Write-MarkerSpec 'world_projected_home_indicator' 'WorldMapHomeIndicator'

$hud = '/Game/RokUI/Hud'
$commands = @(
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldCityNameplate world_projected_city_nameplate"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldCityPortrait world_projected_city_portrait"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldHomeIndicator world_projected_home_indicator"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldEntityBadge world_projected_entity_badge -Suffix=_0"
    "Rok.UiWbp.GeneratePage $hud/WBP_RokWorldMarchRow world_march_row"
)
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_world_projected_hud -Commands ($commands -join ', ')
