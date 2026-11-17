# Capture the original panel tree, then create its Widget Blueprint if absent.
$ErrorActionPreference = 'Stop'
$spec = 'D:/ue/Rok/Saved/RokUi/Specs/building_panel.json'
& (Join-Path $PSScriptRoot 'Capture.ps1') -Name building_panel_base -Extra `
    "-RokUiLegacy -RokAutoSelectBuilding=UnityInitId_5 -RokUiExportBuildingPanel=$spec"
if (-not (Test-Path -LiteralPath $spec)) { throw "Missing building panel spec $spec" }
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_building_panel -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Building/WBP_RokBuildingPanel building_panel -ParentClass=/Script/Rok.RokBuildingPanelWidget -RootName=BuildingPanelCanvas'
