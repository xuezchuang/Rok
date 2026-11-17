# Generate the joined fixture page and reusable office layout, preserving existing assets.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
$sampleOffice = & py -3.12 (Join-Path $PSScriptRoot 'PrepareAllianceJoinedWbpSpecs.py')
if ($LASTEXITCODE -ne 0 -or "$sampleOffice" -notmatch '^\d+$') {
    throw 'Joined alliance layout preparation failed.'
}
if (!$SpecsOnly) {
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceJoined feature_AllianceJoinedPage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_AllianceJoinedOfficeRow feature_AllianceJoinedOfficeRow -Prefix=AllianceOffice -Suffix=_$sampleOffice"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_alliance_joined -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_AllianceJoined', 'WBP_RokFeature_AllianceJoinedOfficeRow') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "Joined alliance generation did not create $asset."
        }
    }
}
