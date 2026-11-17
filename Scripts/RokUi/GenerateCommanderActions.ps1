$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderActions.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander actions spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_actions -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderActions commander_actions_page'
