$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderServerState.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander server state spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_server_state -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderServerState commander_server_state_page'
