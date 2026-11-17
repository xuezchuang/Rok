$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderSkillItem.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander skill item spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_skill_item -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderSkillItem commander_skill_item_page -Suffix=_0'
