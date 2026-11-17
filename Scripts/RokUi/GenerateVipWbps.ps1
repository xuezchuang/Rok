# Generate VIP page, benefit and help layouts while preserving existing assets.
param([switch]$SpecsOnly, [switch]$ReplayOnly)

$ErrorActionPreference = 'Stop'
if ($ReplayOnly) {
    $repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
    $directory = Join-Path $repo 'Saved/RokUi/Fixtures'
    New-Item -ItemType Directory -Force $directory | Out-Null
    $path = Join-Path $directory 'vip_role_replay.json'
    @{
        rid = 9007
        replay_server_time_ms = 1794099043000
        state_by_protocol = @{ Role_RoleInfo = @{ roleInfo = @{
            rid = 9007; name = 'VIP read-only audit'; country = 101; level = 1
            vip = 150; continuousLoginDay = 1; vipExpFlag = $false; vipFreeBox = $false
        } } }
    } | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $path
    Write-Output $path
    return
}

& py -3.12 (Join-Path $PSScriptRoot 'PrepareVipWbpSpecs.py')
if ($LASTEXITCODE -ne 0) { throw 'VIP layout preparation failed.' }
if (!$SpecsOnly) {
    $feature = '/Game/RokUI/Feature'
    $commands = @(
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_Vip feature_VipPage"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_VipBenefitRow feature_VipBenefitRow -Prefix=VipBenefit -Suffix=_0"
        "Rok.UiWbp.GeneratePage $feature/WBP_RokFeature_VipDailyPointHelpTip feature_VipDailyPointHelpTip"
    )
    & (Join-Path $PSScriptRoot 'EditorCmd.ps1') -Name gen_feature_vip -Commands ($commands -join ', ')
    foreach ($asset in 'WBP_RokFeature_Vip', 'WBP_RokFeature_VipBenefitRow', 'WBP_RokFeature_VipDailyPointHelpTip') {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot "../../Content/RokUI/Feature/$asset.uasset"))) {
            throw "VIP generation did not create $asset."
        }
    }
}
