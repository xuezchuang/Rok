$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderStatus.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander status spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_status -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderStatus commander_status_page'
