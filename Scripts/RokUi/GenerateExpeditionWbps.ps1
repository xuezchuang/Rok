param([switch]$Recreate)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& py -3.12 (Join-Path $PSScriptRoot 'BuildExpeditionWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'BuildExpeditionWbpSpecs.py failed' }
& (Join-Path $PSScriptRoot 'ImportExpeditionWbpTextures.ps1')

$relativeAssets = @(
    'Content/RokUI/Feature/WBP_RokFeature_Expedition.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionTask.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionArmyComposer.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionStageRow.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionMapRewardRow.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionStarConditionRow.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionEnemyHeroRow.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionHeroChoiceRow.uasset',
    'Content/RokUI/Feature/WBP_RokFeature_ExpeditionSoldierRow.uasset'
)
$existing = @($relativeAssets | ForEach-Object { Join-Path $projectRoot $_ } | Where-Object { Test-Path -LiteralPath $_ })
if ($existing.Count -gt 0) {
    if (-not $Recreate) { throw "Refusing to overwrite existing expedition WBP: $($existing -join ', ')" }
    $assetDir = (Resolve-Path (Join-Path $projectRoot 'Content/RokUI/Feature')).Path
    $expectedDir = Join-Path $projectRoot 'Content\RokUI\Feature'
    if (-not $assetDir.Equals($expectedDir, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected expedition WBP target directory: $assetDir"
    }
    foreach ($path in $existing) {
        if (-not $path.StartsWith($assetDir + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove WBP outside the verified Feature directory: $path"
        }
    }
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $backupDir = Join-Path $projectRoot "Saved\RokUi\Backups\ExpeditionWbpBeforeRecreate_$stamp"
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    foreach ($path in $existing) { Copy-Item -LiteralPath $path -Destination $backupDir }
    foreach ($path in $existing) { Remove-Item -LiteralPath $path }
    Write-Output "Backed up previous generated assets to $backupDir"
}

$assetRoot = '/Game/RokUI/Feature/WBP_RokFeature_Expedition'
$parent = '-ParentClass=/Script/Rok.RokUiPageWidget'
$commands = @(
    "Rok.UiWbp.GeneratePage ${assetRoot} expedition_map $parent -RootName=ExpeditionRoot",
    "Rok.UiWbp.GeneratePage ${assetRoot}Task expedition_task $parent -RootName=ExpeditionTaskRoot",
    "Rok.UiWbp.GeneratePage ${assetRoot}ArmyComposer expedition_army_composer $parent -RootName=ExpeditionArmyComposerRoot",
    "Rok.UiWbp.GeneratePage ${assetRoot}StageRow expedition_stage_row $parent -Prefix=ExpeditionStage -Suffix=_1 -RootName=ExpeditionStageRowRoot",
    "Rok.UiWbp.GeneratePage ${assetRoot}MapRewardRow expedition_map_reward_row $parent -Prefix=ExpeditionReward -Suffix=_0 -RootName=ExpeditionMapRewardRowRoot",
    "Rok.UiWbp.GeneratePage ${assetRoot}StarConditionRow expedition_star_condition_row $parent -Prefix=ExpeditionStarCondition -Suffix=_0 -RootName=ExpeditionStarConditionRowRoot",
    "Rok.UiWbp.GeneratePage ${assetRoot}EnemyHeroRow expedition_enemy_hero_row $parent -Prefix=ExpeditionEnemyHero -Suffix=_0 -RootName=ExpeditionEnemyHeroRowRoot",
    "Rok.UiWbp.GeneratePage ${assetRoot}HeroChoiceRow expedition_hero_choice_row $parent -Prefix=ExpeditionHeroChoice -Suffix=_0 -RootName=ExpeditionHeroChoiceRowRoot",
    "Rok.UiWbp.GeneratePage ${assetRoot}SoldierRow expedition_soldier_row $parent -Prefix=ExpeditionSoldier -Suffix=_10101 -RootName=ExpeditionSoldierRowRoot"
) -join ', '

& (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_expedition_wbps -Commands $commands
