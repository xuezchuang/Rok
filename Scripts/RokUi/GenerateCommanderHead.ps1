$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderHead.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander head spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_head -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderHead commander_head_page -Suffix=_0'
