# Generate the store page and product cell without overwriting designer changes.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
& py -3.12 (Join-Path $PSScriptRoot 'PrepareStoreWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'Store layout preparation failed.' }

if (!$SpecsOnly) {
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_Store feature_StorePage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_StoreCell feature_StoreCell -Prefix=Store -Suffix=_0"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_store -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_Store', 'WBP_RokFeature_StoreCell') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "Store generation did not create $asset."
        }
    }
}
