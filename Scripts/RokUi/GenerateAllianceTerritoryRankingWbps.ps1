# Run Rok.Ui.Alliance.TerritoryRankingRenderedFlow with -RokUiLegacy -RokAllianceTerritoryRankingExportSpecs first.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
& py -3.12 (Join-Path $PSScriptRoot 'PrepareAllianceTerritoryRankingWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'Alliance territory/ranking layout preparation failed.' }
if (!$SpecsOnly) {
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceTerritoryReference feature_AllianceTerritoryReferencePage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceRankingReference feature_AllianceRankingReferencePage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceRankingReferenceCard feature_AllianceRankingReferenceCard -Prefix=AllianceRanking -Suffix=_300"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_alliance_territory_ranking -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_AllianceTerritoryReference', 'WBP_RokFeature_AllianceRankingReference', 'WBP_RokFeature_AllianceRankingReferenceCard') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "Alliance territory/ranking generation did not create $asset."
        }
    }
}
