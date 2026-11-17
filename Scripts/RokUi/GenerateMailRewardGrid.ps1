# Create only. Source specs come from the existing offline mail reward captures.
$ErrorActionPreference = 'Stop'
& py -3.12 (Join-Path $PSScriptRoot 'BuildMailRewardGridSpec.py')
if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the mail reward grid spec.' }
$asset = '/Game/RokUI/Feature/WBP_RokFeature_MailRewardGrid'
$report = & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name 'gen_mail_reward_grid' `
    -Commands "Rok.UiWbp.GeneratePage $asset feature_MailRewardGridPage" -TimeoutSeconds 180
$report
if (!($report -match '^exit=0 ') -or !($report -match [regex]::Escape("generated $asset widgets=9 "))) {
    throw 'The editor did not save all nine reward grid widgets.'
}
