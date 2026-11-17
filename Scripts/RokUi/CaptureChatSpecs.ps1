# Capture offline legacy chat states used to generate its designer page.
$ErrorActionPreference = 'Stop'
$specRoot = 'D:/ue/Rok/Saved/RokUi/Specs'
New-Item -ItemType Directory -Force $specRoot | Out-Null
$states = @(
    @{ Name = 'chat_empty'; Args = '-RokOpenModal=chat -RokChatCaptureChannel=0' },
    @{ Name = 'chat_fixture_world'; Args = '-RokOpenModal=chat_reference -RokChatCaptureChannel=0' },
    @{ Name = 'chat_fixture_alliance'; Args = '-RokOpenModal=chat_reference -RokChatCaptureChannel=1' },
    @{ Name = 'chat_fixture_private'; Args = '-RokOpenModal=chat_reference -RokChatCaptureChannel=2' },
    @{ Name = 'chat_fixture_emoji'; Args = '-RokOpenModal=chat_reference -RokChatCaptureChannel=0 -RokChatEmojiOpen' }
)
foreach ($state in $states) {
    $spec = "$specRoot/$($state.Name).json"
    & "$PSScriptRoot/Capture.ps1" -Name "$($state.Name)_legacy" -Extra "-RokLocalOnly -RokUiLegacy $($state.Args) -RokUiExportSpec=$spec"
    if (-not (Test-Path $spec)) { throw "Chat spec not written: $spec" }
}
