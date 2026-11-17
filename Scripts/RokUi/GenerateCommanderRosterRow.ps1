$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderRosterRow.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander roster row spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_roster_row -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderRosterRow commander_roster_row_page'
