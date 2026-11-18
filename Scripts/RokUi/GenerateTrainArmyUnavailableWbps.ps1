# Create-only generation for three real training missing-data states.
param([switch]$SpecsOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& py -3.12 (Join-Path $PSScriptRoot 'BuildTrainArmyUnavailableSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'Training unavailable-state spec validation failed.' }
if ($SpecsOnly) { return }
$pages = @(
    @('TrainArmyDetailUnavailable', 'train_army_detail_unavailable'),
    @('TrainArmyCatalogUnavailable', 'train_army_catalog_unavailable'),
    @('TrainArmyModelUnavailable', 'train_army_model_unavailable')
)
foreach ($page in $pages) {
    $path = Join-Path $projectRoot "Content/RokUI/Feature/WBP_RokFeature_$($page[0]).uasset"
    if (Test-Path -LiteralPath $path) { throw "Create-only generation: asset already exists: $path" }
}
$commands = @($pages | ForEach-Object {
    "Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_$($_[0]) $($_[1]) -ParentClass=/Script/Rok.RokUiPageWidget -RootName=TrainArmyUnavailableRoot"
})
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_train_army_unavailable -Commands ($commands -join ', ')
foreach ($page in $pages) {
    $path = Join-Path $projectRoot "Content/RokUI/Feature/WBP_RokFeature_$($page[0]).uasset"
    if (!(Test-Path -LiteralPath $path)) { throw "Training unavailable-state asset missing: $path" }
}
