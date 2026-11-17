$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderPortrait.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander portrait spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_portrait -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderPortrait commander_portrait_page'
