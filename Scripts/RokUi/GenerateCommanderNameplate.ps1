$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderNameplate.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander nameplate spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_nameplate -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderNameplate commander_nameplate_page'
