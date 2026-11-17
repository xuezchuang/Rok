$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderChrome.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander chrome spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_chrome -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderChrome commander_chrome_page'
