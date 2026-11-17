$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderTalentEditor.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander talent editor spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_talent_editor -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderTalentEditor commander_talent_editor_page, Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderTalentNode commander_talent_node_page -Prefix=CommanderTalentNode_1, Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderConfirm commander_confirm_page'
