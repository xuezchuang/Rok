$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderDetails.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander detail spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_details -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderSkillDetail commander_skill_page, Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderTalentDetail commander_talent_page, Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderBiography commander_biography_page, Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderSortMenu commander_sort_page'
