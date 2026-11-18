# Create-only. Existing Designer edits are preserved by Rok.UiWbp.GeneratePage.
# Prerequisite: building_info, resource_building_info, building_upgrade and
# building_upgrade_barracks captures in Saved/RokUi/Specs.
$ErrorActionPreference = 'Stop'
& py -3.12 (Join-Path $PSScriptRoot 'BuildBuildingPreviewLayoutSpec.py')
if ($LASTEXITCODE -ne 0) { throw 'BuildBuildingPreviewLayoutSpec.py failed' }
$asset = '/Game/RokUI/Modal/Pages/WBP_RokModal_BuildingPreviewLayout'
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_building_preview_layout -Commands "Rok.UiWbp.GeneratePage $asset building_preview_layout"
if ($LASTEXITCODE -ne 0) { throw 'Building preview layout WBP generation failed' }
