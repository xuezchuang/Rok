# First run Rok.Ui.Alliance.TechnologyReferenceRenderedFlow with -RokUiLegacy -RokAllianceTechnologyExportSpecs.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
$sampleId = & py -3.12 (Join-Path $PSScriptRoot 'PrepareAllianceTechnologyReferenceWbpSpecs.py')
if ($LASTEXITCODE -ne 0 -or "$sampleId" -notmatch '^\d+$') {
    throw 'Alliance technology reference layout preparation failed.'
}
if (!$SpecsOnly) {
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceTechnologyReference feature_AllianceTechnologyReferencePage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceTechnologyReferenceRow feature_AllianceTechnologyReferenceRow -Prefix=AllianceTechnology -Suffix=_$sampleId"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_alliance_technology_reference -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_AllianceTechnologyReference', 'WBP_RokFeature_AllianceTechnologyReferenceRow') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "Alliance technology reference generation did not create $asset."
        }
    }
}
