# Generate the construction drawer and reusable cards with create-only semantics.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
$sampleIds = & py -3.12 (Join-Path $PSScriptRoot 'PrepareBuildCityWbpSpecs.py')
if ($LASTEXITCODE -ne 0 -or $sampleIds -notmatch '^\d+ \d+$') { throw 'BuildCity layout preparation failed.' }

if (!$SpecsOnly) {
    $availableId, $lockedId = $sampleIds -split ' '
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_BuildCity feature_BuildCityPage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_BuildCityCardAvailable feature_BuildCityCardAvailable -Prefix=BuildCityCard -Suffix=_$availableId"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_BuildCityCardLocked feature_BuildCityCardLocked -Prefix=BuildCityCard -Suffix=_$lockedId"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_build_city -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_BuildCity', 'WBP_RokFeature_BuildCityCardAvailable', 'WBP_RokFeature_BuildCityCardLocked') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "BuildCity generation did not create $asset."
        }
    }
}
