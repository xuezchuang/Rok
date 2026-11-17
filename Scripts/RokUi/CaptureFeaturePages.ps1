# Open the normal feature routes offscreen for WBP generation or comparison.
# Pass -Only LanguageSettings,SettingsHub to capture a subset.
param(
    [string[]]$Only = @(),
    [switch]$Compare,
    [string]$Set = 'feature_pages'
)

$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$trainReplayRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../Saved/RokUi/Fixtures/TrainArmy')).Replace('\', '/')
$trainReplay = 'train_army -RokTrainArmyPreviewServerBuilding=30 -RokNoNet -RokLocalOnly'
$taskDailyReplayRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../Saved/RokUi/Fixtures/TaskDaily')).Replace('\', '/')
$taskDailyReplay = 'tasks -RokTaskPage=daily -RokNoNet -RokLocalOnly'
$taskMainReplayRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../Saved/RokUi/Fixtures/TaskMain')).Replace('\', '/')
$taskMainReplay = 'tasks -RokNoNet -RokLocalOnly'
$routes = [ordered]@{
    SettingsHub = 'settings'
    GeneralSettings = 'general_settings'
    NotificationSettings = 'notification_settings'
    LanguageSettings = 'language_settings'
    AccountSettings = 'account_settings'
    WorldPositionSearch = 'position'
    MapMarkers = 'markers'
    GemShop = 'gem_shop'
    Charge = 'charge -RokChargePage=107'
    ChargeService = 'charge -RokChargePage=101'
    AllianceWelcome = 'alliance_welcome'
    AllianceOffice = 'alliance_welcome -RokAllianceOfficePreview'
    TaskDailyCatalog = 'tasks -RokTaskPage=daily'
    TaskDailyServerWaiting = "$taskDailyReplay -RokNetReplay=$taskDailyReplayRoot/waiting.json"
    TaskDailyServerEmpty = "$taskDailyReplay -RokNetReplay=$taskDailyReplayRoot/empty.json"
    TaskDailyServer = "$taskDailyReplay -RokNetReplay=$taskDailyReplayRoot/mixed.json"
    TaskMainLocal = 'tasks -RokLocalOnly'
    TaskMainReference = 'tasks_reference -RokLocalOnly'
    TaskMainServerWaiting = "$taskMainReplay -RokNetReplay=$taskMainReplayRoot/waiting.json"
    TaskMainServerEmpty = "$taskMainReplay -RokNetReplay=$taskMainReplayRoot/empty.json"
    TaskMainServer = "$taskMainReplay -RokNetReplay=$taskMainReplayRoot/mixed.json"
    TaskChapterCatalog = 'tasks -RokTaskPage=chapter -RokLocalOnly'
    TaskChapterCatalogGroup2 = 'tasks -RokTaskPage=chapter -RokTaskChapterPreviewGroup=2 -RokLocalOnly'
    TaskChapterCatalogGroup7 = 'tasks -RokTaskPage=chapter -RokTaskChapterPreviewGroup=7 -RokLocalOnly'
    MailLocal = 'mail_local_unread -RokLocalOnly'
    MailEmpty = 'mail -RokLocalOnly'
    MailReportChrome = 'mail -RokMailReportPreview -RokLocalOnly -RokNoNet'
    MailReferenceRow = 'mail_reference -RokLocalOnly -RokNoNet'
    MailReferenceMissing = 'mail_reference -RokLocalOnly -RokNoNet -RokMailReferencePreviewRow=1'
    MailReferenceAlternate = 'mail_reference -RokLocalOnly -RokNoNet -RokMailReferencePreviewTab=2'
    MailReferenceTip = 'mail_reference -RokLocalOnly -RokNoNet -RokMailRewardPreviewTooltip=0'
    MailRewardsMixed = 'mail_rewards_preview -RokLocalOnly -RokNoNet'
    MailRewardsEmpty = 'mail_rewards_preview -RokLocalOnly -RokNoNet -RokMailReferencePreviewRow=1'
    AllianceJoin = 'alliance_welcome -RokAllianceJoinPreview -RokLocalOnly'
    AllianceJoinFixture = 'alliance_welcome -RokAllianceJoinPreview -RokAlliancePreviewFixture -RokLocalOnly'
    AllianceCreate = 'alliance_welcome -RokAllianceCreatePreview -RokLocalOnly'
    ArmyShow = 'army_show -RokLocalOnly'
    ArmyShowWaiting = 'army_show -RokArmyShowFixture=waiting -RokLocalOnly'
    ArmyShowEmpty = 'army_show -RokArmyShowFixture=empty -RokLocalOnly'
    ArmyShowMarches = 'army_show -RokArmyShowFixture=marches -RokLocalOnly'
    ArmyShowRules = 'army_show -RokArmyShowRulesPreview -RokLocalOnly'
    AllianceFlagEditor = 'alliance_welcome -RokAllianceCreatePreview -RokAllianceFlagEditorPreview=1 -RokLocalOnly'
    AllianceFlagClothColor = 'alliance_welcome -RokAllianceCreatePreview -RokAllianceFlagEditorPreview=2 -RokLocalOnly'
    AllianceFlagEmblem = 'alliance_welcome -RokAllianceCreatePreview -RokAllianceFlagEditorPreview=3 -RokLocalOnly'
    AllianceFlagEmblemColor = 'alliance_welcome -RokAllianceCreatePreview -RokAllianceFlagEditorPreview=4 -RokLocalOnly'
    ResourceSearch = 'resource_search -RokLocalOnly'
    ResourceSearchFood = 'resource_search -RokResourceSearchTypePreview=1 -RokLocalOnly'
    ResourceSearchGuide = 'resource_search -RokResourceSearchGuideLevel=5 -RokLocalOnly'
    WorkerQueue = 'worker_queue -RokLocalOnly'
    WorkerQueueHire = 'worker_queue -RokWorkerQueueFixture=1 -RokLocalOnly'
    WorkerQueueBusy = 'worker_queue -RokWorkerQueueFixture=2 -RokLocalOnly'
    WorkerQueueRenew = 'worker_queue -RokWorkerQueueFixture=3 -RokLocalOnly'
    ArmyStatistics = 'army_statistics -RokLocalOnly'
    ArmyStatisticsInside = 'army_statistics -RokArmyStatisticsTab=1 -RokLocalOnly'
    ArmyStatisticsOutside = 'army_statistics -RokArmyStatisticsTab=2 -RokLocalOnly'
    ArmyStatisticsWaiting = 'army_statistics -RokArmyStatisticsFixture=1 -RokLocalOnly'
    ArmyStatisticsSynced = 'army_statistics -RokArmyStatisticsFixture=2 -RokLocalOnly'
    ArmyStatisticsSyncedOutside = 'army_statistics -RokArmyStatisticsFixture=2 -RokArmyStatisticsTab=2 -RokLocalOnly'
    ActivityCenter = 'activity -RokActivityPreviewTab=1 -RokLocalOnly'
    ActivityLordsWarAlternate = 'activity -RokActivityPreviewTab=2 -RokLocalOnly'
    ActivityCalendar = 'activity -RokActivityPreviewTab=0 -RokLocalOnly'
    ActivityCalendarSketch = 'activity -RokActivityPreviewTab=0 -RokActivityCalendarEntry=40201 -RokLocalOnly'
    ActivityCalendarNeutral = 'activity -RokActivityPreviewTab=0 -RokActivityCalendarEntry=100001 -RokLocalOnly'
    CityManager = 'city_manager -RokLocalOnly'
    CityManagerWar = 'city_manager -RokCityManagerPreviewGroup=201 -RokLocalOnly'
    CityManagerShield = 'city_manager -RokCityManagerPreviewGroup=301 -RokLocalOnly'
    CityManagerGather = 'city_manager -RokCityManagerPreviewGroup=601 -RokLocalOnly'
    Hospital = 'hospital -RokAutoSelectBuilding=UnityBuildingType_14 -RokLocalOnly'
    HospitalEmpty = 'hospital -RokAutoSelectBuilding=UnityBuildingType_14 -RokHospitalPreviewState=1 -RokLocalOnly'
    HospitalWounded = 'hospital -RokAutoSelectBuilding=UnityBuildingType_14 -RokHospitalPreviewState=2 -RokLocalOnly'
    HospitalTreating = 'hospital -RokAutoSelectBuilding=UnityBuildingType_14 -RokHospitalPreviewState=3 -RokLocalOnly'
    HospitalReady = 'hospital -RokAutoSelectBuilding=UnityBuildingType_14 -RokHospitalPreviewState=4 -RokLocalOnly'
    HospitalConfirm = 'hospital -RokAutoSelectBuilding=UnityBuildingType_14 -RokHospitalPreviewState=5 -RokLocalOnly'
    SpeedUp = 'speed_up -RokLocalOnly'
    SpeedUpSecondPage = 'speed_up -RokSpeedUpPreviewPage=1 -RokLocalOnly'
    SpeedUpEmpty = 'speed_up -RokSpeedUpCatalogEmpty=1 -RokLocalOnly'
    SpeedUpServer = 'worker_queue -RokSpeedUpServerPreview=1 -RokLocalOnly'
    SpeedUpServerConfirm = 'worker_queue -RokSpeedUpServerPreview=2 -RokLocalOnly'
    SpeedUpServerOverrun = 'worker_queue -RokSpeedUpServerPreview=3 -RokLocalOnly'
    TrainArmyDetail = 'train_army -RokAutoSelectBuilding=UnityInitId_6 -RokTrainArmyPreviewDetail=1 -RokLocalOnly'
    TrainArmyDetailTier2 = 'train_army -RokAutoSelectBuilding=UnityInitId_6 -RokTrainArmyPreviewDetail=2 -RokLocalOnly'
    TrainArmyDetailAttributeTip = 'train_army -RokAutoSelectBuilding=UnityInitId_6 -RokTrainArmyPreviewDetail=1 -RokTrainArmyPreviewTip=1 -RokLocalOnly'
    TrainArmyDetailSkillTip = 'train_army -RokAutoSelectBuilding=UnityInitId_6 -RokTrainArmyPreviewDetail=2 -RokTrainArmyPreviewTip=2 -RokLocalOnly'
    TrainArmyParameter = 'train_army -RokAutoSelectBuilding=UnityInitId_6 -RokLocalOnly'
    TrainArmyParameterLocked = 'train_army -RokAutoSelectBuilding=UnityInitId_6 -RokTrainArmyPreviewBrowse=2 -RokLocalOnly'
    TrainArmyParameterIdle = "$trainReplay -RokNetReplay=$trainReplayRoot/idle.json"
    TrainArmyParameterBusy = "$trainReplay -RokNetReplay=$trainReplayRoot/busy.json"
    TrainArmyParameterReady = "$trainReplay -RokNetReplay=$trainReplayRoot/ready.json"
    TrainArmyParameterConfirm = "$trainReplay -RokNetReplay=$trainReplayRoot/idle.json -RokTrainArmyPreviewConfirm=1"
}
$entries = foreach ($pair in $routes.GetEnumerator()) {
    if ($Only.Count -ne 0 -and $pair.Key -notin $Only) { continue }
    $legacy = if ($Compare) { '' } else { ' -RokUiLegacy' }
    "feature_$($pair.Key)|-RokOpenModal=$($pair.Value)$legacy"
}
if (-not $entries) { throw 'No matching feature pages' }
if ($entries -match '/Fixtures/TrainArmy/') {
    & py -3.12 "$PSScriptRoot/BuildTrainArmyReplayFixtures.py"
    if ($LASTEXITCODE -ne 0) { throw 'Preparing training replay fixtures failed.' }
}
if ($entries -match '/Fixtures/TaskDaily/') {
    & py -3.12 "$PSScriptRoot/BuildTaskDailyReplayFixtures.py"
    if ($LASTEXITCODE -ne 0) { throw 'Preparing daily task replay fixtures failed.' }
}
if ($entries -match '/Fixtures/TaskMain/') {
    & py -3.12 "$PSScriptRoot/BuildTaskMainReplayFixtures.py"
    if ($LASTEXITCODE -ne 0) { throw 'Preparing main task replay fixtures failed.' }
}
if ($Compare) {
    & "$PSScriptRoot/CompareBase.ps1" -Set $Set -Entries $entries
}
else {
    & "$PSScriptRoot/CaptureSpecs.ps1" -Entries $entries
}
