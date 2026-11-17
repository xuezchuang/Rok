# Generate the bag page, row and stack assets without overwriting designer changes.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
& py -3.12 (Join-Path $PSScriptRoot 'PrepareItemsWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'Items layout preparation failed.' }

if (!$SpecsOnly) {
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_Items feature_ItemsPage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_ItemsInventoryRow feature_ItemsInventoryRow"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_ItemsCell feature_ItemsCell -Prefix=Items -Suffix=_0"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_items -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_Items', 'WBP_RokFeature_ItemsInventoryRow', 'WBP_RokFeature_ItemsCell') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "Items generation did not create $asset."
        }
    }
}
