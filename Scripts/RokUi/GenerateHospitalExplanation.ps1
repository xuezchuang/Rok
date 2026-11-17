# Create the hospital explanation WBP from its source-layout manifest.
param()

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$specBuilder = Join-Path $PSScriptRoot 'BuildHospitalExplanationSpec.py'
& py -3.12 $specBuilder
if ($LASTEXITCODE -ne 0) { throw 'BuildHospitalExplanationSpec.py failed' }

$asset = '/Game/RokUI/Hospital/WBP_RokHospitalExplanation'
$contentAsset = Join-Path $projectRoot 'Content/RokUI/Hospital/WBP_RokHospitalExplanation.uasset'
if (Test-Path -LiteralPath $contentAsset) {
    throw "Refusing to overwrite existing WBP: $contentAsset"
}

& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_hospital_explanation -Commands `
    "Rok.UiWbp.GeneratePage $asset hospital_explanation -ParentClass=/Script/Rok.RokHospitalExplanationWidget -RootName=HospitalExplanationRoot"
