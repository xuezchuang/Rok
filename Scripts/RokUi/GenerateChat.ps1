# Recreate the chat page from five deterministic offline legacy captures.
# The generator is create-only and does not overwrite Designer edits.
$ErrorActionPreference = 'Stop'
python "$PSScriptRoot/PrepareChatSpec.py"
if ($LASTEXITCODE -ne 0) { throw 'Chat spec preparation failed' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_chat_page -Commands `
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_Chat chat_page'
