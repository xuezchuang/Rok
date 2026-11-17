# Run Rok.Ui.BuildingUpgrade.BarracksAttributesRenderedFlow in the city with
# -RokLocalOnly -RokUiLegacy -RokBarracksAttributesExportSpec before generation.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$spec = Join-Path $project 'Saved/RokUi/Specs/modal_BarracksUpgradeAttributes.json'
if (-not (Test-Path -LiteralPath $spec -PathType Leaf)) {
    throw "Run the legacy barracks attribute replay with spec export first. Missing $spec"
}

$asset = '/Game/RokUI/Modal/Items/WBP_RokModalItem_BarracksUpgradeAttributes'
$commands = "Rok.UiWbp.GeneratePage $asset modal_BarracksUpgradeAttributes -ParentClass=/Script/Rok.RokBarracksUpgradeAttributes -RootName=BarracksUpgradeAttributeRoot"
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name 'gen_barracks_upgrade_attributes' -Commands $commands
if (-not (Test-Path -LiteralPath (Join-Path $project 'Content/RokUI/Modal/Items/WBP_RokModalItem_BarracksUpgradeAttributes.uasset') -PathType Leaf)) {
    throw 'Barracks attribute Widget Blueprint was not generated.'
}
