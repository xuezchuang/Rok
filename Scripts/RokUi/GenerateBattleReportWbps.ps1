# Create the independent editable mail/report assets; existing assets are never deleted.
param([switch]$SpecsOnly)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& py -3.12 (Join-Path $PSScriptRoot 'BuildBattleReportWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'BuildBattleReportWbpSpecs.py failed' }
& py -3.12 (Join-Path $PSScriptRoot 'VerifyMailWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'VerifyMailWbpSpecs.py failed' }
if ($SpecsOnly) { return }

$pages = @(
    @('BattleReportOverview', 'battle_report_screen', 'BattleReportRoot'),
    @('BattleReportInboxRow', 'battle_report_row', 'BattleReportRowRoot'),
    @('BattleReportParticipantCard', 'battle_report_army_row', 'BattleReportArmyRoot'),
    @('BattleReportLootCard', 'battle_report_reward_row', 'BattleReportRewardRoot'),
    @('ServerMailScreen', 'server_mail_screen', 'ServerMailRoot'),
    @('ServerMailRow', 'server_mail_row', 'ServerMailRowRoot'),
    @('ServerMailRewardCard', 'server_mail_reward_card', 'ServerMailRewardRoot')
)
foreach ($page in $pages) {
    $target = Join-Path $projectRoot "Content/RokUI/Feature/WBP_RokFeature_$($page[0]).uasset"
    if (Test-Path -LiteralPath $target) { throw "Create-only mail generation: asset already exists: $target" }
}
$commands = @($pages | ForEach-Object {
    "Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_$($_[0]) $($_[1]) -ParentClass=/Script/Rok.RokUiPageWidget -RootName=$($_[2])"
})
& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_mail_report_wbps -Commands ($commands -join ', ')
foreach ($page in $pages) {
    $target = Join-Path $projectRoot "Content/RokUI/Feature/WBP_RokFeature_$($page[0]).uasset"
    if (!(Test-Path -LiteralPath $target)) { throw "Mail generation did not create $target" }
}
