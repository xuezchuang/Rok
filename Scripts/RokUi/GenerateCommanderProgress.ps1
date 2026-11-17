$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderProgress.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander progress spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_progress -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderProgress commander_progress_page'
