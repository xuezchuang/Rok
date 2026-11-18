# Create the separate color palette without replacing submitted page/state assets.
param([switch]$SpecsOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& py -3.12 -B (Join-Path $PSScriptRoot 'BuildFeatureTextColorSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'Feature text color spec validation failed.' }
if ($SpecsOnly) { return }
$path = Join-Path $projectRoot 'Content/RokUI/Feature/WBP_RokFeature_FeatureTextColors.uasset'
if (Test-Path -LiteralPath $path) { throw "Create-only generation: asset already exists: $path" }
$command = 'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_FeatureTextColors feature_text_colors -ParentClass=/Script/Rok.RokUiPageWidget -RootName=FeatureTextColorsRoot'
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_text_colors -Commands $command
if (!(Test-Path -LiteralPath $path)) { throw "Feature text color asset missing: $path" }
