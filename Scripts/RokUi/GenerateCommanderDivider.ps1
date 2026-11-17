$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareCommanderDivider.py"
if ($LASTEXITCODE -ne 0) { throw 'Commander divider spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_commander_divider -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_CommanderRosterDivider commander_divider_page'
