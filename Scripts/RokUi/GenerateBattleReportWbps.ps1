# Generate the editable battle-report pane and indexed list row.
param([switch]$Recreate)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& py -3.12 (Join-Path $PSScriptRoot 'BuildBattleReportWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'BuildBattleReportWbpSpecs.py failed' }

$assets = @(
    'Content/RokUI/Feature/WBP_RokFeature_BattleReportScreen.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_BattleReportRow.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_BattleReportArmyRow.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_BattleReportRewardRow.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_BattleReportRoundRow.uasset'
)
$existing = @($assets | ForEach-Object { Join-Path $projectRoot $_ } | Where-Object { Test-Path -LiteralPath $_ })
if ($existing.Count -gt 0) {
    if (-not $Recreate) { throw "Refusing to overwrite existing WBP: $($existing -join ', ')" }
    $assetDir = (Resolve-Path (Join-Path $projectRoot 'Content/RokUI/Feature')).Path
    $expectedDir = Join-Path $projectRoot 'Content\RokUI\Feature'
    if (-not $assetDir.Equals($expectedDir, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected battle-report WBP target directory: $assetDir"
    }
    foreach ($path in $existing) {
        if (-not $path.StartsWith($assetDir + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove WBP outside the verified Feature directory: $path"
        }
    }
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $backupDir = Join-Path $projectRoot "Saved\RokUi\Backups\BattleReportWbpBeforeRecreate_$stamp"
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    foreach ($path in $existing) { Copy-Item -LiteralPath $path -Destination $backupDir }
    foreach ($path in $existing) { Remove-Item -LiteralPath $path }
    Write-Output "Backed up previous generated assets to $backupDir"
}

$commands = @(
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_BattleReportScreen battle_report_screen -ParentClass=/Script/Rok.RokUiPageWidget -RootName=BattleReportRoot',
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_BattleReportRow battle_report_row -ParentClass=/Script/Rok.RokUiPageWidget -RootName=BattleReportRowRoot',
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_BattleReportArmyRow battle_report_army_row -ParentClass=/Script/Rok.RokUiPageWidget -RootName=BattleReportArmyRowRoot',
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_BattleReportRewardRow battle_report_reward_row -ParentClass=/Script/Rok.RokUiPageWidget -RootName=BattleReportRewardRowRoot',
    'Rok.UiWbp.GeneratePage /Game/RokUI/Feature/WBP_RokFeature_BattleReportRoundRow battle_report_round_row -ParentClass=/Script/Rok.RokUiPageWidget -RootName=BattleReportRoundRowRoot'
) -join ', '

& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_battle_report_wbps -Commands $commands
