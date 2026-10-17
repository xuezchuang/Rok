"""Read-only source/integration checks; runtime acceptance requires RokSpeedUpAudit."""
from __future__ import annotations
import csv
import hashlib
import json
import re
import struct
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[1]


def body(text: str, signature: str) -> str:
    start = text.index('{', text.index(signature))
    depth = 0
    for index in range(start, len(text)):
        if text[index] == '{':
            depth += 1
        elif text[index] == '}':
            depth -= 1
            if not depth:
                return text[start:index + 1]
    raise ValueError(f'Unclosed source body: {signature}')


def main() -> None:
    checks, failures, sources = [], [], []

    def read(relative: str) -> str:
        raw = resolve(relative).read_bytes()
        sources.append({'path': relative, 'sha256': hashlib.sha256(raw).hexdigest()})
        return raw.decode('utf-8-sig', errors='replace')

    def check(name: str, result: bool) -> None:
        checks.append({'name': name, 'passed': bool(result)})
        if not result:
            failures.append(name)

    mediator = read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/SpeedUp/AddSpeedMediator.cs')
    timer = body(mediator, 'private void OnTimerComplete()')
    check('Unity timer closes its bound queue at finishTime', all(token in timer for token in (
        'm_speedData.finishTime-ServerTimeModule.Instance.GetServerTime()', 'if (leftTime<=0)', 'OnClose();', 'Timer.Register(1f, OnTimerComplete)')))
    check('Unity baseline uses original duration and authoritative finish', all(token in body(mediator, 'protected override void InitData()') for token in (
        'queue.firstFinishTime - m_speedData.queue.beginTime', 'm_speedData.queue.finishTime')))
    send = body(mediator, 'private void SendSproto(')
    check('Source item use still requires full Role_SpeedUp request', all(token in send for token in (
        'Role_SpeedUp.request', 'req.queueIndex', 'req.itemId', 'req.itemNum', 'req.costDenar', 'req.type', 'SendSproto(req)')))
    worker = read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIWorker/UI_Win_WorkerMediator.cs')
    check('Source worker routes actual building index to speedup', 'AddBuildingUpdateSpeed(BuildingInfo)' in body(worker, 'private void OnSpeedUp1BtnClick()'))
    prefab = read('ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_item/UIAddSpeed/UI_Win_AddSpeed.prefab')
    controller_meta = read('ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/UA_WinAnime_T1.controller.meta')
    check('Exact AddSpeed prefab root Animator is shared T1', 'guid: 79ffb0d19ef4cd34193888a1a1bb63e4' in controller_meta
          and 'm_Controller: {fileID: 9100000, guid: 79ffb0d19ef4cd34193888a1a1bb63e4' in prefab)

    level_csv = read('ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv')
    levels = {row['ID']: row for row in csv.DictReader(level_csv.splitlines()) if row['ID'].isdigit()}
    row = levels['902']
    expected = {'type': '9', 'level': '2', 'buildingTime': '20', 'food': '500', 'wood': '800', 'stone': '0', 'coin': '0', 'power': '10', 'reqType1': '1', 'reqLevel1': '2'}
    check('Actual 901 -> 902 source duration/cost/power and required CityHall2', levels['901']['power'] == '5' and all(row[key] == value for key, value in expected.items()))
    initial = json.loads(read('Saved/RokUnityMigration/unity_initial_city_state.json'))
    buildings = {item['init_id']: item for item in initial['buildings']}
    check('Audit selects existing hall/barracks/hut, never invents an actor', buildings[6]['type_id'] == 9 and buildings[6]['level'] == 1
          and buildings[2]['type_id'] == 7 and buildings[5]['type_id'] == 1 and buildings[5]['level'] == 1)
    hall = levels['102']
    check('Legal prerequisite is source CityHall2 at 2 seconds, 3500/3500 and +14', levels['101']['power'] == '7'
          and all(hall[key] == value for key, value in {'type': '1', 'level': '2', 'buildingTime': '2', 'food': '3500', 'wood': '3500', 'power': '21'}.items())
          and all(hall[f'reqType{slot}'] == '0' and hall[f'reqLevel{slot}'] == '0' for slot in range(1, 4)))
    check('All source prerequisite slots are complete nonnegative integers', all(
        row[f'reqType{slot}'].isdigit() and row[f'reqLevel{slot}'].isdigit()
        and (int(row[f'reqType{slot}']) == 0 or int(row[f'reqLevel{slot}']) > 0)
        for row in levels.values() for slot in range(1, 4)))
    upgrade_source = read('ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs')
    source_refresh = body(upgrade_source, 'private void Refresh3()')
    check('Unity tests three completed-level requirements and ignores zero type slots', all(
        f'GetMaxLevelofType(m_nexbuildingLevelData.reqType{slot}) >= m_nexbuildingLevelData.reqLevel{slot}' in source_refresh
        and f'm_nexbuildingLevelData.reqType{slot} == 0' in source_refresh for slot in range(1, 4)))
    server = read('ref/server/data/work/server/game_server/logic/service/proxy/Build.lua')
    server_upgrade = server.split('function response.UpGradeBuilding( msg )', 1)[1].split('function response.', 1)[0]
    check('Server upgrade rejects unmet reqType/reqLevel with BUILDING_LOCK', all(token in server_upgrade for token in (
        'for i=1,3 do', 'BuildingLogic:getBuildingLv', 'reqType', 'reqLevel', 'ErrorCode.BUILDING_LOCK')))
    subsystem = read('Source/Rok/RokBuildingUpgradeSubsystem.cpp')
    parse_prereq = body(subsystem, 'FRokUnityBuildingLevelConfig::ParsePrerequisites(')
    check('Three-slot parser rejects incomplete, fractional and nonnumeric fields', all(token in parse_prereq for token in (
        'Slot <= 3', 'FMath::IsFinite', 'FMath::FloorToDouble', 'EJson::String', 'Prerequisites.Reset()', 'bPrerequisitesValid = false')))
    completed_levels = body(subsystem, 'URokBuildingUpgradeSubsystem::GetKnownCompletedBuildingLevels(')
    check('Only complete source ownership and canonical current levels supply the maximum', all(token in completed_levels for token in (
        'UnityInitialBuildings', 'UnityInitId_%d', 'Current->UnityBuildingTypeId != Initial.TypeId', 'CompletedLevel = Current->Level', 'FMath::Max(Highest, CompletedLevel)'))
        and 'UpgradeFinishesAt' not in completed_levels and 'UpgradeStartedAt' not in completed_levels)
    can_start = body(subsystem, 'URokBuildingUpgradeSubsystem::CanStartUpgrade(')
    start = body(subsystem, 'URokBuildingUpgradeSubsystem::StartUpgrade(')
    preview = body(subsystem, 'URokBuildingUpgradeSubsystem::GetUpgradePreview(')
    check('Start and preview share prerequisite rejection before spending', 'CheckUpgradePrerequisites(State, OutFailureReason)' in can_start
          and start.index('CanStartUpgrade(') < start.index('SpendResources(') and 'CanStartUpgrade(Actor, OutPreview.FailureReason)' in preview)
    modal = read('Source/Rok/RokUnityModalWidget.cpp')
    upgrade_ui = body(modal, 'URokUnityModalWidget::BuildBuildingUpgrade()')
    check('Actual upgrade UI displays localized gate feedback', all(token in upgrade_ui for token in (
        '!Preview.bCanStartTimedUpgrade', 'Preview.FailureReason', 'FeedbackText->SetText', 'ESlateVisibility::HitTestInvisible')))


    helper = read('Source/Rok/RokSpeedUpQueueLifecycle.cpp')
    observe = body(helper, 'FRokSpeedUpQueueLifecycle::Observe(')
    check('Completion requires bound identity, original deadline, one level and cleared timer', all(token in observe for token in (
        'State->BuildingKey != BuildingKey', 'State->UnityInitId != InitId', 'State->UnityBuildingTypeId != BuildingType',
        'State->UpgradeStartedAt != StartedAt', 'State->UpgradeFinishesAt != FinishesAt',
        'QueueSeconds >= FinishesAt', 'State->Level == StartingLevel + 1', 'State->UpgradeStartedAt == 0', 'State->UpgradeFinishesAt == 0')))
    completion = body(subsystem, 'URokBuildingUpgradeSubsystem::RefreshUpgradeCompletion(')
    check('Lifecycle exactly matches queue float clock and rejects overflow/invalid raw time',
          'const float Now = GetWorld() ? GetWorld()->GetTimeSeconds()' in completion
          and all(token in observe for token in ('!FMath::IsFinite(WorldSeconds)', 'WorldSeconds < 0',
              'const float QueueSeconds = static_cast<float>(WorldSeconds)', '!FMath::IsFinite(QueueSeconds)', 'QueueSeconds < StartedAt')))
    regression = read('Source/Rok/Tests/RokSpeedUpQueueLifecycleTests.cpp')
    f32 = lambda value: struct.unpack('<f', struct.pack('<f', value))[0]
    raw = 30.183331534266472
    deadline = f32(30.183332443237305)
    previous = struct.unpack('<f', struct.pack('<I', struct.unpack('<I', struct.pack('<f', deadline))[0] - 1))[0]
    check('Actual fractional failure reproduces while previous float remains early', raw < deadline and f32(raw) == deadline
          and f32(previous) < deadline and all(token in regression for token in (
              '30.183331534266472', '30.183332443237305f', 'std::nextafter(Fractional.UpgradeFinishesAt, 0.f)',
              'std::numeric_limits<double>::max()', 'Early fractional loss cannot consume completion')))
    check('Preview/lost/replaced cannot silently rebind', 'if (Status != ERokSpeedUpQueueStatus::Active) return Status;' in observe
          and 'bCompletionConsumed' in body(helper, 'FRokSpeedUpQueueLifecycle::ConsumeNaturalCompletion()'))

    feature = read('Source/Rok/RokUnityFeatureScreenWidget.cpp')
    opening = body(feature, 'void URokUnityFeatureScreenWidget::OpenSpeedUp(')
    update = body(feature, 'void URokUnityFeatureScreenWidget::UpdateSpeedUpQueuePresentation()')
    closing = body(feature, 'void URokUnityFeatureScreenWidget::CloseScreen()')
    build = body(feature, 'void URokUnityFeatureScreenWidget::BuildSpeedUp()')
    check('Only screen open binds lifecycle; close resets it', 'SpeedUpQueueLifecycle.Bind(' in opening
          and 'SpeedUpQueueLifecycle.Reset()' in closing and 'SpeedUpQueueLifecycle.Bind(' not in build)
    check('Natural completion uses existing close path and keeps completed progress', 'SpeedUpQueueLifecycle.Observe(' in update
          and 'SpeedUpQueueLifecycle.ConsumeNaturalCompletion()' in update and 'SetPercent(1.0f)' in update
          and 'RequestCloseScreen()' in update and 'WindowAnimation.IsClosing()' in update)
    check('Lost and replaced queue states are explicit', 'ERokSpeedUpQueueStatus::Lost' in update and 'ERokSpeedUpQueueStatus::Replaced' in update)
    item_action = feature.split('if (ActionId >= 1800 && ActionId < 1840)', 1)[1].split('if (ActionId == 1840', 1)[0]
    check('Directory selection remains inventory-unknown, not a speedup command', '未改变倒计时' in item_action and 'RebuildModeContent()' in item_action
          and not re.search(r'->(?:StartUpgrade|SpendResources|SendSproto|SpeedUp)\s*\(', item_action))

    menu_source = read('ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GlobalCityMenuHudMediator.cs')
    open_upgrade = menu_source.split('case "openBuildingUpdata":', 1)[1].split('case "openBuildingSpeedUp":', 1)[0]
    menu_proxy = read('ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/CityBuildingProxy.cs')
    menu_visibility = body(menu_proxy, 'private bool IsShow(long index,int menudata)')
    check('Source menu opens upgrade details without prerequisite/resource gating', 'ShowUI(UI.s_buildingUpdate, null, m_curIndex)' in open_upgrade
          and 'reqType' not in menu_visibility and 'showLevelMin' in menu_visibility and 'IsUpgrading(index)' in menu_visibility)
    panel = read('Source/Rok/RokBuildingPanelWidget.cpp')
    panel_refresh = body(panel, 'void URokBuildingPanelWidget::Refresh()')
    check('Real upgrade-details entry remains enabled while timed command is gated', 'CanStartUpgrade(' not in panel_refresh
          and 'State.Type != ERokBuildingType::Unknown && !State.bUpgradeInProgress' in panel_refresh
          and 'State.Level < UpgradeSubsystem->GetMaxBuildingLevelForActor(' in panel_refresh
          and 'OnUpgradeRequested.Broadcast(SelectedBuildingActor.Get())' in body(panel, 'void URokBuildingPanelWidget::HandleUpgradeClicked()'))

    audit = read('Source/Rok/RokSpeedUpAuditSubsystem.cpp')
    captures = re.findall(r'Capture\(TEXT\("(\d\d_[^"]+)"\)\)', audit)
    check('Exactly seventeen named screenshot states including legal prerequisite', len(captures) == 17 and len(set(captures)) == 17
          and sorted(name[:2] for name in captures) == [f'{index:02}' for index in range(1, 18)])
    check('Runtime uses delegates and screenshot processed verification', all(token in audit for token in (
        'Button->OnClicked.Broadcast()', 'OnScreenshotRequestProcessed()', 'state_at_request', 'state_at_processed', 'FMemory::Memcmp', 'CheckSelectedActor', 'RokAutoSelectBuilding=UnityInitId_6')))
    upgrade_frame = modal.split('case ERokUnityModalMode::BuildingUpgrade:', 1)[1].split('case ERokUnityModalMode::WarWarning:', 1)[0]
    type2_frame = body(modal, 'void URokUnityModalWidget::BuildType2Window(')
    check('Upgrade audit closes the actual Type2 frame through its real button', 'BuildType2Window(' in upgrade_frame
          and 'TEXT("Type2CloseButton")' in type2_frame and '&URokUnityModalWidget::HandleCloseClicked' in type2_frame
          and 'Click(Modal.Get(), TEXT("Type2CloseButton"))' in audit and 'Type3CloseButton' not in audit
          and '!SpeedUpAuditVisible(Close) || !Close->GetIsEnabled()' in body(audit, 'URokSpeedUpAuditSubsystem::Validate()'))
    finish = body(audit, 'void URokSpeedUpAuditSubsystem::Finish(')
    check('Audit failures including manifest save failure exit nonzero', 'RequestExitWithStatus(false, Failures.IsEmpty() ? 0 : 1)' in finish
          and finish.index('Final manifest save failed.') < finish.index('RequestExitWithStatus(')
          and 'RequestExit(false)' not in finish)
    queue_reached = body(audit, 'bool SpeedUpAuditQueueReached(')
    check('Audit uses the same strict float deadline for both queues and events',
          'const float QueueSeconds = static_cast<float>(WorldSeconds)' in queue_reached
          and 'FMath::IsFinite(QueueSeconds) && QueueSeconds >= Deadline' in queue_reached
          and audit.count('!SpeedUpAuditQueueReached(') == 3
          and 'last_prerequisite_observation' in audit and 'last_barracks_observation' in audit
          and 'raw_world_seconds' in audit and 'float_queue_seconds' in audit)
    modal_header = read('Source/Rok/RokUnityModalWidget.h')
    check('Unreflected modal selection uses its read-only getter without dropping actor identity',
          'GetSelectedBuildingActorForAudit()' in modal_header
          and 'BuildingModal->GetSelectedBuildingActorForAudit()' in body(audit, 'URokSpeedUpAuditSubsystem::CheckSelectedActor(')
          and 'Actual != Expected' in body(audit, 'URokSpeedUpAuditSubsystem::CheckSelectedActor('))
    check('Runtime never advances clocks, fabricates actors or directly starts a queue', not re.search(
        r'(?:->|\.)\s*(?:StartUpgrade|AdvanceWindowAnimationForTesting|AdvanceHudAnimations|SetPowerPresentation|SetTimeDilation|SpawnActor|OpenSpeedUp|FlushPendingActionForTesting)\s*\(', audit))
    check('Runtime observes two source power events and actual effects after real time', all(token in audit for token in (
        'OnLocalUpgradeCompleted.AddUObject', 'Result.PreviousPower != (bHallEvent ? 7 : 5)', 'Result.CurrentPower != (bHallEvent ? 21 : 10)',
        'GetLocalCompletedBuildingPowerDelta()', 'Profile.LocalBuildingUpgradeScore', 'PowerGain->GetPlaybackTime()', 'CompletionWall - UpgradeClickedWall < 19.5',
        'HallCompletionWall - HallClickedWall < 1.9', 'CompletionEvents == 2', '(bHallCompleted ? 14 : 0) + (bCompleted ? 5 : 0)', 'BaselineScore = RuntimeState->GetPlayerProfileState().LocalPreviewScore')))

    output = ROOT / 'Saved/RokUnityMigration/speed_up_queue_lifecycle_verify.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({'passed': not failures, 'checks': checks, 'failures': failures, 'source_chain': sources,
        'runtime_verified': False, 'source_queue': {'init_id': 6, 'building_level_id': 902, 'seconds': 20, 'local_power_delta': 5},
        'prerequisite_queue': {'init_id': 5, 'building_level_id': 102, 'seconds': 2, 'local_power_delta': 14},
        'boundary': 'Static source checks only. Real queue/effect/screenshots require -RokSpeedUpAudit; source city-hall prerequisite is checked locally; authoritative inventory/server queue/full server validation remain outside scope.'}, indent=2, ensure_ascii=False), encoding='utf-8')
    print(f'SpeedUp lifecycle source verification: {len(checks)} checks, {len(failures)} failures; {output}')
    if failures:
        raise SystemExit('\n'.join(failures))


if __name__ == '__main__':
    main()
