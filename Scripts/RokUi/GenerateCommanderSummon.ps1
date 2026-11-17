$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderSummon.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander summon spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_summon -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderSummon commander_summon_page'
