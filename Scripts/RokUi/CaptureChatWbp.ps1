$ErrorActionPreference = 'Stop'
$states = @(
    @{ Name = 'chat_empty'; Args = '-RokOpenModal=chat -RokChatCaptureChannel=0' },
    @{ Name = 'chat_fixture_world'; Args = '-RokOpenModal=chat_reference -RokChatCaptureChannel=0' },
    @{ Name = 'chat_fixture_alliance'; Args = '-RokOpenModal=chat_reference -RokChatCaptureChannel=1' },
    @{ Name = 'chat_fixture_private'; Args = '-RokOpenModal=chat_reference -RokChatCaptureChannel=2' },
    @{ Name = 'chat_fixture_emoji'; Args = '-RokOpenModal=chat_reference -RokChatCaptureChannel=0 -RokChatEmojiOpen' }
)
foreach ($state in $states) {
    & "$PSScriptRoot/Capture.ps1" -Name "$($state.Name)_wbp" -Extra "-RokLocalOnly $($state.Args)"
}
