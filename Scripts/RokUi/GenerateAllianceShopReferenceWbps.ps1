# Run Rok.Ui.Alliance.ShopReferenceRenderedFlow with -RokUiLegacy -RokAllianceShopExportSpecs before generating.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
$sampleId = & py -3.12 (Join-Path $PSScriptRoot 'PrepareAllianceShopReferenceWbpSpecs.py')
if ($LASTEXITCODE -ne 0 -or "$sampleId" -notmatch '^\d+$') {
    throw 'Alliance shop reference layout preparation failed.'
}
if (!$SpecsOnly) {
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceShopReference feature_AllianceShopReferencePage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceShopReferenceRow feature_AllianceShopReferenceRow -Prefix=AllianceShop -Suffix=_$sampleId"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_alliance_shop_reference -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_AllianceShopReference', 'WBP_RokFeature_AllianceShopReferenceRow') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "Alliance shop reference generation did not create $asset."
        }
    }
}
