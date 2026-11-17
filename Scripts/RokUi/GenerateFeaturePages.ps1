# Generate create-only feature page Widget Blueprints from CaptureFeaturePages.ps1.
param([string[]]$Only = @())

$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$pages = @(
    'SettingsHub',
    'GeneralSettings',
    'NotificationSettings',
    'LanguageSettings',
    'AccountSettings',
    'WorldPositionSearch',
    'MapMarkers',
    'GemShop',
    'Charge',
    'AllianceWelcome',
    'AllianceOffice',
    'TaskDailyCatalog',
    'TaskDailyServer',
    'TaskMainLocal',
    'TaskMainServer',
    'TaskChapterCatalog',
    'MailLocal',
    'MailReference',
    'MailRewards',
    'AllianceJoin',
    'AllianceCreate',
    'ArmyShow',
    'AllianceFlagEditor',
    'ResourceSearch',
    'WorkerQueue',
    'ArmyStatistics',
    'ActivityCenter',
    'CityManager',
    'Hospital',
    'SpeedUp',
    'TrainArmyDetail',
    'TrainArmyParameter',
    'TrainArmyFrame'
)
$commands = foreach ($page in $pages) {
    if ($Only.Count -ne 0 -and $page -notin $Only) { continue }
    $spec = switch ($page) {
        'TrainArmyFrame' { 'feature_TrainArmyParameter' }
        'MailReference' { 'feature_MailReferenceRow' }
        'MailRewards' { 'feature_MailRewardsMixed' }
        default { "feature_$page" }
    }
    if (-not (Test-Path "$PSScriptRoot/../../Saved/RokUi/Specs/$spec.json")) {
        throw "Missing $spec.json. Run CaptureFeaturePages.ps1 first."
    }
    $assetRoot = '/Game/RokUI/Feature/WBP_RokFeature'
    if ($page -in @('MapMarkers', 'GemShop', 'Charge', 'AllianceOffice', 'TaskDailyCatalog', 'TaskDailyServer', 'TaskMainLocal', 'TaskMainServer', 'TaskChapterCatalog', 'MailLocal', 'MailReference', 'MailRewards', 'AllianceJoin', 'ArmyShow', 'AllianceFlagEditor', 'ResourceSearch', 'WorkerQueue', 'ArmyStatistics', 'ActivityCenter', 'CityManager', 'Hospital', 'SpeedUp', 'TrainArmyDetail', 'TrainArmyParameter', 'TrainArmyFrame')) {
        & py -3.12 "$PSScriptRoot/PrepareFeaturePageSpecs.py" $page
        if ($LASTEXITCODE -ne 0) { throw "Preparing $page specs failed." }
        if ($page -eq 'MapMarkers') {
            "${assetRoot}_MapMarkers|feature_MapMarkersPage"
            "${assetRoot}_MapMarkerRow|feature_MapMarkerRow -Prefix=MapMarker -Suffix=_1"
            "${assetRoot}_MapMarkerRowSelected|feature_MapMarkerRowSelected -Prefix=MapMarker -Suffix=_0"
        }
        elseif ($page -eq 'GemShop') {
            "${assetRoot}_GemShop|feature_GemShopPage"
            "${assetRoot}_GemShopPackage|feature_GemShopPackage -Prefix=GemShop -Suffix=_0"
        }
        elseif ($page -eq 'Charge') {
            "${assetRoot}_Charge|feature_ChargePage feature_ChargeServicesOnly"
            "${assetRoot}_ChargeGemPackage|feature_ChargeGemPackage -Prefix=ChargeGemPackage -Suffix=_0"
        }
        elseif ($page -eq 'AllianceOffice') {
            "${assetRoot}_AllianceOffice|feature_AllianceOfficePage"
            "${assetRoot}_AllianceOfficeRow|feature_AllianceOfficeRow -Prefix=AllianceOfficeReference -Suffix=_2001"
            "${assetRoot}_AllianceOfficeRowSelected|feature_AllianceOfficeRowSelected -Prefix=AllianceOfficeReference -Suffix=_1001"
        }
        elseif ($page -eq 'TaskDailyCatalog') {
            "${assetRoot}_TaskDailyCatalog|feature_TaskDailyCatalogPage"
            "${assetRoot}_TaskDailyCatalogRow|feature_TaskDailyCatalogRow -Prefix=TaskDailyCatalog -Suffix=_0"
        }
        elseif ($page -eq 'TaskDailyServer') {
            "${assetRoot}_TaskDailyServer|feature_TaskDailyServerPage"
            "${assetRoot}_TaskDailyServerRow|feature_TaskDailyServerRow -Prefix=Task -Suffix=_0"
            "${assetRoot}_TaskDailyServerReward|feature_TaskDailyServerReward -Prefix=Task -Suffix=_0_0"
            "${assetRoot}_TaskDailyServerMilestone|feature_TaskDailyServerMilestone -Prefix=TaskDaily -Suffix=_0"
            "${assetRoot}_TaskDailyServerClaimMilestone|feature_TaskDailyServerClaimMilestone -Prefix=TaskDaily -Suffix=_0"
        }
        elseif ($page -eq 'TaskMainLocal') {
            "${assetRoot}_TaskMainLocal|feature_TaskMainLocalPage"
            "${assetRoot}_TaskMainLocalRow|feature_TaskMainLocalRow -Prefix=Task -Suffix=_0"
        }
        elseif ($page -eq 'TaskMainServer') {
            "${assetRoot}_TaskMainServer|feature_TaskMainServerPage"
        }
        elseif ($page -eq 'TaskChapterCatalog') {
            "${assetRoot}_TaskChapterCatalog|feature_TaskChapterCatalogPage"
            "${assetRoot}_TaskChapterCatalogRow|feature_TaskChapterCatalogRow -Prefix=TaskChapter -Suffix=_0"
            "${assetRoot}_TaskChapterCatalogAggregateReward|feature_TaskChapterCatalogAggregateReward -Prefix=TaskChapter -Suffix=_0_0"
            "${assetRoot}_TaskChapterCatalogTaskReward|feature_TaskChapterCatalogTaskReward -Prefix=TaskChapter -Suffix=_0_0"
        }
        elseif ($page -eq 'MailLocal') {
            "${assetRoot}_MailLocal|feature_MailLocalPage feature_MailEmptyOnly"
            "${assetRoot}_MailLocalRow|feature_MailLocalRow -Prefix=Mail -Suffix=_1"
            "${assetRoot}_MailLocalRowSelected|feature_MailLocalRowSelected -Prefix=Mail -Suffix=_0"
        }
        elseif ($page -eq 'MailReference') {
            "${assetRoot}_MailReference|feature_MailReferencePage"
            "${assetRoot}_MailReferenceRow|feature_MailReferenceNormalPresentationRow -Prefix=Mail -Suffix=_2"
            "${assetRoot}_MailReferenceSelectedRow|feature_MailReferenceSelectedPresentationRow -Prefix=Mail -Suffix=_0"
        }
        elseif ($page -eq 'MailRewards') {
            "${assetRoot}_MailRewardReferenceCard|feature_MailRewardReferenceCard"
            "${assetRoot}_MailRewardItemCard|feature_MailRewardItemCard -Prefix=MailReward -Suffix=_6"
            "${assetRoot}_MailRewardCurrencyCard|feature_MailRewardCurrencyCard -Prefix=MailReward -Suffix=_0"
            "${assetRoot}_MailRewardTooltip|feature_MailRewardTooltip"
        }
        elseif ($page -eq 'AllianceJoin') {
            "${assetRoot}_AllianceJoin|feature_AllianceJoinPage feature_AllianceJoinOrdinaryOnly"
            "${assetRoot}_AllianceJoinRow|feature_AllianceJoinRow -Prefix=AllianceJoin -Suffix=_1"
            "${assetRoot}_AllianceJoinRowSelected|feature_AllianceJoinRowSelected -Prefix=AllianceJoin -Suffix=_0"
        }
        elseif ($page -eq 'ArmyShow') {
            "${assetRoot}_ArmyShow|feature_ArmyShow"
            "${assetRoot}_ArmyShowRules|feature_ArmyShowRules"
            "${assetRoot}_ArmyShowMarchRow|feature_ArmyShowMarchRow -Prefix=ArmyShowMarch -Suffix=_0"
            "${assetRoot}_ArmyShowMessage|feature_ArmyShowMessage"
        }
        elseif ($page -eq 'AllianceFlagEditor') {
            "${assetRoot}_AllianceFlagEditor|feature_AllianceFlagEditorPage"
            "${assetRoot}_AllianceFlagSpriteOption|feature_AllianceFlagSpriteOption -Prefix=AllianceFlagItem -Suffix=_320"
            "${assetRoot}_AllianceFlagColorOption|feature_AllianceFlagColorOption -Prefix=AllianceFlagItem -Suffix=_201"
        }
        elseif ($page -eq 'ResourceSearch') {
            "${assetRoot}_ResourceSearch|feature_ResourceSearchPage"
            "${assetRoot}_ResourceSearchType|feature_ResourceSearchType -Prefix=ResourceSearch -Suffix=_0"
        }
        elseif ($page -eq 'WorkerQueue') {
            "${assetRoot}_WorkerQueue|feature_WorkerQueuePage"
            "${assetRoot}_WorkerQueuePrimaryRow|feature_WorkerQueuePrimaryRow"
            "${assetRoot}_WorkerQueueSecondaryRow|feature_WorkerQueueSecondaryRow"
        }
        elseif ($page -eq 'ArmyStatistics') {
            "${assetRoot}_ArmyStatistics|feature_ArmyStatisticsPage"
            "${assetRoot}_ArmyStatisticsRow|feature_ArmyStatisticsRow -Prefix=ArmyStatistics -Suffix=_0"
            "${assetRoot}_ArmyStatisticsMessage|feature_ArmyStatisticsMessage"
        }
        elseif ($page -eq 'ActivityCenter') {
            "${assetRoot}_ActivityCenter|feature_ActivityCenterPage"
            "${assetRoot}_ActivityMenuRow|feature_ActivityMenuRow -Prefix=ActivityMenu -Suffix=_3"
            "${assetRoot}_ActivityCalendarRow|feature_ActivityCalendarRow -Prefix=ActivityCalendar -Suffix=_0"
        }
        elseif ($page -eq 'CityManager') {
            "${assetRoot}_CityManager|feature_CityManagerPage"
            "${assetRoot}_CityManagerGroupRow|feature_CityManagerGroupRow -Prefix=CityManager -Suffix=_201"
            "${assetRoot}_CityManagerWarLevelRow|feature_CityManagerWarLevelRow -Prefix=CityManagerWar -Suffix=_0"
            "${assetRoot}_CityManagerDetailRow|feature_CityManagerDetailRow -Prefix=CityManagerDetail -Suffix=_0"
        }
        elseif ($page -eq 'Hospital') {
            "${assetRoot}_Hospital|feature_HospitalPage"
            "${assetRoot}_HospitalTroopRow|feature_HospitalTroopRow -Prefix=Hospital -Suffix=_10101"
            "${assetRoot}_HospitalConfirm|feature_HospitalConfirmPage"
        }
        elseif ($page -eq 'SpeedUp') {
            "${assetRoot}_SpeedUp|feature_SpeedUpPage"
            "${assetRoot}_SpeedUpCatalogCard|feature_SpeedUpCatalogCard -Prefix=SpeedUp -Suffix=_0"
            "${assetRoot}_BuildingSpeedUp|feature_BuildingSpeedUpPage"
            "${assetRoot}_BuildingSpeedUpOwnedRow|feature_BuildingSpeedUpOwnedRow -Prefix=SpeedUp -Suffix=_0"
            "${assetRoot}_BuildingSpeedUpBuyRow|feature_BuildingSpeedUpBuyRow -Prefix=SpeedUp -Suffix=_0"
            "${assetRoot}_BuildingSpeedUpConfirm|feature_BuildingSpeedUpConfirmPage"
        }
        elseif ($page -eq 'TrainArmyDetail') {
            "${assetRoot}_TrainArmyDetail|feature_TrainArmyDetailPage"
            "${assetRoot}_TrainArmyDetailAttribute|feature_TrainArmyDetailAttribute -Prefix=TrainArmyDetail -Suffix=_0"
            "${assetRoot}_TrainArmyDetailSkill|feature_TrainArmyDetailSkill -Prefix=TrainArmyDetail -Suffix=_0"
            "${assetRoot}_TrainArmyDetailTip|feature_TrainArmyDetailTip"
        }
        elseif ($page -eq 'TrainArmyParameter') {
            "${assetRoot}_TrainArmyParameter|feature_TrainArmyParameterPage"
            "${assetRoot}_TrainArmyTierCard|feature_TrainArmyTierCard -Prefix=TrainArmy -Suffix=_0"
            "${assetRoot}_TrainArmyInstantConfirm|feature_TrainArmyParameterConfirmPage"
        }
        elseif ($page -eq 'TrainArmyFrame') {
            "${assetRoot}_TrainArmyFrame|feature_TrainArmyFramePage"
        }
    }
    else {
        "${assetRoot}_$page|$spec"
    }
}
if (-not $commands) { throw 'No matching feature pages' }
& "$PSScriptRoot/EditorCmd.ps1" -Name gen_feature_pages -Commands (($commands | ForEach-Object {
    $asset, $inputs = $_ -split '\|', 2
    "Rok.UiWbp.GeneratePage $asset $inputs"
}) -join ', ')
