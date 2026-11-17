# Run Rok.Ui.Alliance.GiftReferenceRenderedFlow with -RokUiLegacy -RokAllianceGiftExportSpecs before generating.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
$sampleId = & py -3.12 (Join-Path $PSScriptRoot 'PrepareAllianceGiftReferenceWbpSpecs.py')
if ($LASTEXITCODE -ne 0 -or "$sampleId" -notmatch '^\d+$') {
    throw 'Alliance gift reference layout preparation failed.'
}
if (!$SpecsOnly) {
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceGiftReference feature_AllianceGiftReferencePage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceGiftReferenceRow feature_AllianceGiftReferenceRow -Prefix=AllianceGiftReference -Suffix=_$sampleId"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_alliance_gift_reference -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_AllianceGiftReference', 'WBP_RokFeature_AllianceGiftReferenceRow') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "Alliance gift reference generation did not create $asset."
        }
    }
}
