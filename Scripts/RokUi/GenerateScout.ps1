# Rebuild the scout page from the legacy runtime capture in Saved/RokUi/Specs/scout.json.
# Capture with Capture.ps1 -Name scout_base -Extra
# '-RokOpenModal=scout -RokUiLegacy -RokUiExportScoutSpec=D:/ue/Rok/Saved/RokUi/Specs/scout.json'.
# The generator only creates a missing asset and preserves edits made in the Designer.
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_scout -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Scout/WBP_RokScoutScreen scout'
