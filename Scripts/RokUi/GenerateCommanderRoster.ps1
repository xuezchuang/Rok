$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderRoster.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander roster spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_roster -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderRoster commander_roster_page'
