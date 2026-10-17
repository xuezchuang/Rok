"""Strict native Slate and original-PNG audit for local Alliance flag editing."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import struct
import tempfile
from pathlib import Path
from rok_data_paths import resolve

ROOT = Path(__file__).resolve().parents[3]
STAGE = Path(__file__).resolve().parents[1]
PRODUCT = ROOT / 'Saved/PendingAllianceFlagLocalPreviewV1'
SCHEMA = 'rok.alliance_flag.local_preview.runtime_audit.v2'
ATLAS = '/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance_flag.rok_fullscreen_alliance_flag'
PHASES = ('00_city_hud', '01_alliance_welcome', '02_create_default',
          '03_editor_cloth_default', '04_cloth_all16', '05_cloth_colors_all16',
          '06_emblems_all24', '07_emblem_colors_all7', '08_cancel_restores_create',
          '09_editor_reopened_default', '10_editor_four_first_ids',
          '11_create_draft_confirmed', '12_hud_closed', '13_welcome_reopened',
          '14_create_reset', '15_final_hud')
PRODUCT_READY = '2cf2d8e4a9d351f296e5d3d3482d61f4b5dcd0214222ed98d80495f244e3074e'
PRODUCT_RELEASE = 'b49f0d5b42f6658f715b39ef97a00fd19e3ea51a1bba70fbb5ff227b8c38e085'
PRODUCT_REVIEW = '17cbba774d65880d74620e0b56e76de905a989dcc91d9de6474ab0f57506ed9d'
PRODUCT_CPP = '55ede9000d689b2bcd23979f1db9081b0d06ae79881511a878623c2a95ce00a4'
FRIEND_H = '7819bfe968c28308c2d88974b2bca9f742039ae38885d7603d20f0859f0a76cd'
CAMERA = ('camera_x', 'camera_y', 'camera_z', 'camera_pitch', 'camera_yaw', 'camera_roll',
          'camera_scale_x', 'camera_scale_y', 'camera_scale_z')
DEFAULT = {'1': 102, '2': 207, '3': 324, '4': 406}
TEXT_FIELDS = (('type_create_name', 'AllianceCreateNameInput', 'FlagAuditCity'),
               ('type_create_tag', 'AllianceCreateTagInput', 'FAG'),
               ('type_create_notice', 'AllianceCreateNoticeInput', 'Local preview only'))


def need(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def contained_file(path: Path, root: Path) -> Path:
    p = path.resolve(strict=True)
    need(p.is_file() and root.resolve() in p.parents and not p.is_symlink(), 'file outside expected root')
    return p


def source_rows() -> dict[int, list[dict]]:
    manifest = json.loads((PRODUCT/'source_manifest.json').read_text(encoding='utf-8'))
    need(manifest['schema'] == 'rok.alliance_flag.local_preview.source.v1'
         and manifest['type_counts'] == {'1': 16, '2': 16, '3': 24, '4': 7}
         and manifest['default_ids'] == [102, 207, 324, 406]
         and manifest['atlas_size'] == [1024, 2048], 'frozen source manifest shape')
    rows = {i: [] for i in range(1, 5)}
    for row in manifest['rows']:
        rows[row['type']].append(row)
    need([len(rows[i]) for i in range(1, 5)] == [16, 16, 24, 7]
         and len({r['id'] for group in rows.values() for r in group}) == 63,
         'source row count and IDs')
    return rows


def expected_pins() -> dict[str, str]:
    cpp = (STAGE/'Source/Rok/RokAllianceFlagLocalPreviewAuditSubsystem.cpp').read_text(encoding='utf-8-sig')
    pins = dict(re.findall(r'\{TEXT\("([^"\r\n]+)"\), TEXT\("([0-9a-f]{64})"\)\}', cpp))
    need(len(pins) == 17 and pins['Source/Rok/RokUnityFeatureScreenWidget.cpp'] == PRODUCT_CPP
         and pins['Source/Rok/RokUnityFeatureScreenWidget.h'] == FRIEND_H
         and pins['Source/Rok/RokAllianceFlagLocalPreviewData.cpp'] == '488d145991bb1ccbbe8fc406f81ebd6bd12629ee9ed594824df7382ca7a419ba'
         and pins['Source/Rok/RokAllianceFlagLocalPreviewData.h'] == 'face9b9d15a39faef0528c0eedd113b806e2ccf238ab3dd822d69c0c5ea8106d',
         'exact native source pin table')
    need(sha(PRODUCT/'ready_for_root_review.json') == PRODUCT_READY
         and sha(PRODUCT/'release_report.json') == PRODUCT_RELEASE
         and sha(ROOT/'Saved/PendingAllianceFlagLocalPreviewIndependentReviewV1/review.json') == PRODUCT_REVIEW,
         'product freeze or independent review drift')
    ready = json.loads((PRODUCT/'ready_for_root_review.json').read_text(encoding='utf-8'))
    for rel, row in ready['candidate_files'].items():
        need(sha(PRODUCT/rel) == row['sha256'] and (PRODUCT/rel).stat().st_size == row['bytes'],
             'product staged file drift ' + rel)
    friend = STAGE/'Source/Rok/RokUnityFeatureScreenWidget.h'
    need(sha(friend) == FRIEND_H, 'final friend header drift')
    base_h = (PRODUCT/'Source/Rok/RokUnityFeatureScreenWidget.h').read_text(encoding='utf-8-sig')
    final_h = friend.read_text(encoding='utf-8-sig')
    need(final_h.replace('\tfriend class URokAllianceFlagLocalPreviewAuditSubsystem;\n', '') == base_h,
         'friend header includes more than one declared friend')
    for name in ('URokAllianceFixtureSearchAuditSubsystem', 'URokAllianceOfficeReadonlyAuditSubsystem',
                 'URokAllianceTechnologyReadonlyAuditSubsystem', 'URokTaskCompletedRowAnimationAuditSubsystem'):
        need(final_h.count('friend class ' + name + ';') >= 1, 'preserved predecessor friend ' + name)
    base_cpp = (ROOT/'Saved/PendingAllianceFixtureSearchV2/Source/Rok/RokUnityFeatureScreenWidget.cpp').read_text(encoding='utf-8-sig')
    candidate_cpp = (PRODUCT/'Source/Rok/RokUnityFeatureScreenWidget.cpp').read_text(encoding='utf-8-sig')
    a, b = 'if (ActionId == 623)', 'if (ActionId == 621 || ActionId == 622)'
    need(base_cpp.split(a, 1)[1].split(b, 1)[0] == candidate_cpp.split(a, 1)[1].split(b, 1)[0],
         'server create action 623 no longer same-base')
    for rel, digest in pins.items():
        if rel.startswith('Saved/') or rel.startswith('ref/'):
            need(sha(resolve(rel)) == digest, 'source/reference SHA drift ' + rel)
    return pins


def static_guard() -> None:
    expected_pins()
    cpp = (STAGE/'Source/Rok/RokAllianceFlagLocalPreviewAuditSubsystem.cpp').read_text(encoding='utf-8-sig')
    h = (STAGE/'Source/Rok/RokAllianceFlagLocalPreviewAuditSubsystem.h').read_text(encoding='utf-8-sig')
    for name in ('ProcessMouseButtonDownEvent', 'ProcessMouseButtonUpEvent', 'LocateWindowUnderMouse',
                 'pending_after', 'processed_frame', 'next_frame', 'FScreenshotRequest::RequestScreenshot',
                 'GetHttpDispatchAttemptCount', 'GetSuppressedLocalRequestCount',
                 'RokAllianceFlagLocalPreview::Rows()', 'Actions.Num() == 83', 'TextInputs.Num() == 3',
                 'Captures.Num() == 16', 'ProcessKeyCharEvent',
                 'Indexed->ActionId == 623', 'GFrameCounter == static_cast<uint64>(Last->GetNumberField(TEXT("next_frame"))) + 1'):
        need(name in cpp or name in h, 'native audit route missing ' + name)
    for forbidden in ('->SetText(', '->HandleAction(', '->ExecuteIndexedAction(',
                      'FlushPendingActionForTesting(', 'AdvanceWorldTime(', 'SearchAlliance('):
        need(forbidden not in cpp, 'native audit shortcut ' + forbidden)
    need('UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS' in cpp, 'shipping gate')
    need('PendingAfter == Indexed->ActionId' in cpp and 'Down && Up && bDeferredCorrect' in cpp,
         'indexed deferred Slate action gate')
    source_rows()


def expected_config() -> list[dict]:
    result = []
    for typ, group in source_rows().items():
        for index, row in enumerate(group):
            def rect(raw):
                return dict(zip(('min_x', 'min_y', 'max_x', 'max_y'),
                                (raw[0], raw[1], raw[0] + raw[2], raw[1] + raw[3]))) if raw else dict.fromkeys(('min_x', 'min_y', 'max_x', 'max_y'), 0)
            result.append({'id': row['id'], 'type': typ, 'index': index,
                           'action_id': 19000 + typ * 100 + index,
                           'show_rect': rect(row['showIconRect']),
                           'reality_rect': rect(row['realityIconRect']),
                           'has_sprite': row['rgb'] is None, 'has_color': row['rgb'] is not None,
                           'rgb': row['rgb']})
    return result


def specs() -> tuple[tuple[tuple[str, str, int, dict | None], ...], ...]:
    rows = {t: [r for r in expected_config() if r['type'] == t] for t in range(1, 5)}
    item = lambda r: (f"select_{r['type']}_{r['index']}_{r['id']}",
                      f"AllianceFlagItemHit_{r['id']}", r['action_id'], r)
    tab = lambda t: (f'switch_tab_{t}', f'AllianceFlagTab_{t}', 19000 + t, None)
    return (
        (('open_alliance_from_hud', 'AllianceFeatureButton', 0, None),),
        (('open_create_601', 'AllianceWelcomeCreate', 601, None),),
        (('open_flag_editor_625', 'AllianceFlagEdit', 625, None),),
        tuple(map(item, rows[1])),
        (tab(2), *map(item, rows[2])),
        (tab(3), *map(item, rows[3])),
        (tab(4), *map(item, rows[4])),
        (('cancel_local_draft', 'AllianceFlagEditorCancel', 19010, None),),
        (('reopen_flag_editor', 'AllianceFlagEdit', 625, None),),
        (item(rows[1][0]), tab(2), item(rows[2][0]), tab(3), item(rows[3][0]), tab(4), item(rows[4][0])),
        (('confirm_local_draft', 'AllianceFlagEditorConfirm', 19011, None),),
        (('close_create_to_hud', 'FeatureFrameClose', 1, None),),
        (('reopen_alliance', 'AllianceFeatureButton', 0, None),),
        (('reopen_create_reset', 'AllianceWelcomeCreate', 601, None),),
        (('final_close_to_hud', 'FeatureFrameClose', 1, None),),
    )


def check_business(b: dict) -> None:
    need(b['server_local_only'] is True and b['http_dispatch_attempts'] == 0
         and b['http_suppressed_requests'] == 0 and b['active_upgrades'] == 0
         and b['active_construction'] == 0 and b['pending_completed_upgrades'] == 0
         and b['completed_upgrade_events'] == 0 and b['construction_ticket_1_exists'] is False
         and b['profile_kind'] == 'local_preview' and b['profile_has_role_id'] is False
         and b['profile_role_id'] == '' and b['profile_has_civilization'] is False
         and len(b['starter_heroes']) > 0 and len(b['starter_troops']) > 0
         and len(b['task_chain']) > 0 and len(b['city_actor_paths']) > 0,
         'anonymous business/account/wallet/server boundary')


def png(path: Path, digest: str, size: int) -> None:
    data = path.read_bytes()
    need(len(data) == size and hashlib.sha256(data).hexdigest() == digest
         and len(data) > 24 and data[:8] == b'\x89PNG\r\n\x1a\n'
         and data[12:16] == b'IHDR' and struct.unpack('>II', data[16:24]) == (1440, 810),
         'original 1440x810 PNG bytes/hash')


def close_num(actual: float, expected: float, tolerance: float = .002) -> bool:
    return abs(actual - expected) <= tolerance


def same_color(a: dict, b: dict, tolerance: float = .002) -> bool:
    return all(close_num(a[k], b[k], tolerance) for k in ('r', 'g', 'b', 'a'))


def source_color(rgb: list[int] | None) -> dict:
    if rgb is None:
        return {'r': 1.0, 'g': 1.0, 'b': 1.0, 'a': 1.0}
    def linear(channel: int) -> float:
        value = channel / 255.0
        return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4
    return dict(zip(('r', 'g', 'b', 'a'), (*map(linear, rgb), 1.0)))


def check_input_map(actual: dict, expected: dict) -> None:
    need(actual == expected, 'exact input hash map')


def shown(w: dict) -> bool:
    return w['exists'] is True and w['visible'] is True and w['enabled'] is True


def check_rect_image(w: dict, rect: dict, color: dict, image_size: tuple[int, int]) -> None:
    need(shown(w) and w['texture'] == ATLAS and same_color(w['color'], color)
         and close_num(w['brush_image_size']['x'], image_size[0])
         and close_num(w['brush_image_size']['y'], image_size[1]), 'preview texture/color/size')
    left, top = rect['min_x'] / 1024, (2048 - rect['max_y']) / 2048
    right, bottom = rect['max_x'] / 1024, (2048 - rect['min_y']) / 2048
    need(close_num(w['uv_min']['x'], left) and close_num(w['uv_min']['y'], top)
         and close_num(w['uv_max']['x'], right) and close_num(w['uv_max']['y'], bottom),
         'Unity bottom-origin source rect to UMG atlas UV')


def selected_expected(phase: int, config: dict[int, list[dict]]) -> tuple[dict, dict]:
    edit = dict(DEFAULT)
    create = dict(DEFAULT)
    if 4 <= phase <= 7:
        for typ in range(1, phase - 2):
            edit[str(typ)] = config[typ][-1]['id']
    if phase in (10, 11):
        edit = {str(t): config[t][0]['id'] for t in range(1, 5)}
        if phase == 11:
            create = dict(edit)
    return edit, create


def check_state(s: dict, phase: int, baseline: dict, camera: tuple, primary: int,
                selection: str, pid: int, window: str, config: dict[int, list[dict]]) -> None:
    need(s['pid'] == s['window_owner_pid'] == s['foreground_owner_pid'] == pid
         and s['window_foreground'] is True
         and s['own_window_identity'] == s['foreground_window_identity'] == window
         and s['client_width'] == 1440 and s['client_height'] == 810
         and s['modal_open'] is False and s['primary_press_count'] == primary
         and s['selected_actor'] == selection and tuple(s[k] for k in CAMERA) == camera
         and s['business'] == baseline, 'frame business/window/camera boundary')
    widgets = s['widgets']
    hud = phase in (0, 12, 15)
    welcome = phase in (1, 13)
    editor = phase in (3, 4, 5, 6, 7, 9, 10)
    create = phase in (2, 8, 11, 14)
    draft_text = phase in (3, 4, 5, 6, 7, 8, 9, 10, 11)
    for _, target, value in TEXT_FIELDS:
        key = {'AllianceCreateNameInput': 'create_name_draft',
               'AllianceCreateTagInput': 'create_tag_draft',
               'AllianceCreateNoticeInput': 'create_notice_draft'}[target]
        need(s[key] == (value if draft_text else ''), 'three create text drafts kept/reset')
    need(s['screen_open'] is (not hud)
         and s['pending_action_id'] == -1
         and shown(widgets['AllianceFeatureButton'])
         and (hud or (s['active_mode'] == 6 and shown(widgets['FeatureFrameClose']))),
         'screen phase/entry/close')
    if hud:
        need(not s['flag_editor_open'] and not s['visible_items'], 'HUD retains flag editor')
        return
    ui = s['alliance_ui_settled']
    need(ui['ready'] is True and ui['direct_children'] > 5
         and ui['unsettled_children'] == 0 and ui['remaining_texture_compilations'] == 0
         and ui['texture_count'] == 5 and ui['texture_rhi_ready'] is True,
         'Alliance flag atlas/UMG layout stable')
    need(s['alliance_preview_state'] == (0 if welcome else 3)
         and s['flag_editor_open'] is editor, 'Alliance welcome/create/editor route')
    if welcome:
        need(shown(widgets['AllianceWelcomeCreate']) and not widgets['AllianceCreateFlagPanel']['exists'],
             'welcome create entry')
        return
    edit, draft = selected_expected(phase, config)
    need(s['edit_draft'] == edit and s['create_draft'] == draft,
         'cancel/confirm/default draft identity')
    if editor:
        need(all(not shown(widgets[target]) for _, target, _ in TEXT_FIELDS),
             'editor rebuild hides create text widgets but retains draft')
        typ = 1 if phase in (3, 9) else 4 if phase == 10 else phase - 3
        need(s['selected_type'] == typ and shown(widgets['AllianceFlagEditorConfirm'])
             and shown(widgets['AllianceFlagEditorCancel'])
             and shown(widgets['AllianceFlagEditorPreviewPanel'])
             and len(s['visible_items']) == len(config[typ]), 'selected tab/grid count')
        for t in range(1, 5):
            need(shown(widgets[f'AllianceFlagTab_{t}'])
                 and shown(widgets[f'AllianceFlagTabPanel_{t}']), 'four reachable tabs')
        for row, expected in zip(s['visible_items'], config[typ]):
            need(all(row[k] == expected[k] for k in ('id', 'type', 'index', 'action_id',
                                                      'show_rect', 'reality_rect', 'has_sprite', 'has_color'))
                 and shown(row['Hit']) and shown(row['Panel']) and shown(row['Id']),
                 'visible config row identity/action/widget')
            hit = row['Hit']
            a, b = hit['absolute_top_left'], hit['absolute_bottom_right']
            origin = s['viewport_origin']
            need(a['x'] >= origin['x'] and a['y'] >= origin['y']
                 and b['x'] <= origin['x'] + 1440 and b['y'] <= origin['y'] + 810
                 and hit['local_size']['x'] >= 75 and hit['local_size']['y'] >= 75,
                 'all visible options fit 1440x810 without clipping')
            selected = row['id'] == edit[str(typ)]
            target = {'r': .67 if selected else .20, 'g': .48 if selected else .20,
                      'b': .13 if selected else .19, 'a': 1.0}
            need(same_color(row['Panel']['brush_color'], target), 'selected item highlight')
            if row['has_sprite']:
                check_rect_image(row['Sprite'], row['show_rect'],
                                 {'r': 1, 'g': 1, 'b': 1, 'a': 1}, (64, 64))
                need(not row['Swatch']['exists'], 'sprite row swatch leaked')
            else:
                need(shown(row['Swatch']) and not row['Sprite']['exists']
                     and same_color(row['Swatch']['brush_color'], expected['color']),
                     'source color swatch row')
        prefix, size = 'AllianceFlagEditor', ((184, 216), (100, 100))
        identity = widgets['AllianceFlagEditorSourceIds']['text']
        need(identity.endswith(f"{edit['1']} / {edit['2']} / {edit['3']} / {edit['4']}"),
             'visible source IDs differ from actual draft')
    else:
        need(create and shown(widgets['AllianceCreateFlagPanel'])
             and shown(widgets['AllianceFlagEdit']) and shown(widgets['AllianceCreateConfirm'])
             and widgets['AllianceCreateConfirm']['name'] == 'AllianceCreateConfirm'
             and not s['visible_items'], 'create form/623 boundary')
        for _, target, value in TEXT_FIELDS:
            need(shown(widgets[target]) and widgets[target]['text'] == ('' if phase == 14 else value),
                 'real create text input preserved or reset')
        prefix, size = 'AllianceCreateFlag', ((184, 216), (100, 100))
    ids = edit if editor else draft
    cloth = next(r for r in config[1] if r['id'] == ids['1'])
    cloth_color = next(r for r in config[2] if r['id'] == ids['2'])
    emblem = next(r for r in config[3] if r['id'] == ids['3'])
    emblem_color = next(r for r in config[4] if r['id'] == ids['4'])
    check_rect_image(widgets[prefix + 'Cloth'], cloth['reality_rect'], cloth_color['color'], size[0])
    check_rect_image(widgets[prefix + 'Emblem'], emblem['reality_rect'], emblem_color['color'], size[1])
    cloth_box = widgets[prefix + 'Cloth']['absolute_top_left']
    emblem_box = widgets[prefix + 'Emblem']['absolute_top_left']
    need(emblem_box['x'] > cloth_box['x'] and emblem_box['y'] > cloth_box['y'],
         'cloth/emblem layered position')


def final_progression(last: dict, final: dict) -> None:
    need(final['frame'] == last['frame'] + 1 and final['utc_ticks'] > last['utc_ticks'],
         'Finish exactly next frame with strictly later UTC')
    a, b = copy.deepcopy(last), copy.deepcopy(final)
    for row in (a, b):
        del row['frame']; del row['utc_ticks']
    need(a == b, 'Finish drift beyond frame and observation UTC')


def check_action(a: dict, spec: tuple, baseline: dict, camera: tuple, primary: int,
                 selection: str, pid: int, window: str, config: dict[int, list[dict]]) -> None:
    label, target, action_id, item = spec
    need(a['label'] == label and a['target'] == target and a['action_id'] == action_id
         and action_id != 623 and a['kind'] == ('item' if item else 'button')
         and a['method'] == 'ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent'
         and a['exact_button_in_path'] is True and a['umg_path_present'] is True
         and a['target_hovered'] is True and a['own_game_window'] is True
         and a['foreground'] is True and a['own_window_pid'] == pid
         and a['own_window_identity'] == a['foreground_window_identity'] == window
         and a['down_handled'] is True and a['up_handled'] is True
         and a['stable_frames'] >= 2 and a['pending_before'] == -1
         and a['pending_after'] == (action_id if action_id else -1)
         and a['primary_before'] == a['primary_after'] == primary
         and a['business_before'] == a['business_after'] == baseline
         and a['failure'] == '', 'actual Slate action/deferred pending ' + label)
    need(a['target_slate_identity'] in [n['slate_identity'] for n in a['hit_path_nodes']]
         and a['target_widget_identity'].startswith('0x'), 'exact button identity ' + label)
    need(a['before_state']['frame'] == a['requested_state']['frame'] == a['frame']
         and a['before_state']['pending_action_id'] == -1
         and a['requested_state']['pending_action_id'] == (action_id if action_id else -1)
         and a['processed_frame'] > a['frame']
         and a['processed_state']['frame'] == a['processed_frame']
         and a['processed_state']['pending_action_id'] == -1
         and a['next_frame'] == a['processed_frame'] + 1
         and a['next_frame_state']['frame'] == a['next_frame']
         and a['next_frame_state']['pending_action_id'] == -1,
         'actual before/request/processed/next tick ' + label)
    states = (a['before_state'], a['requested_state'], a['processed_state'], a['next_frame_state'])
    need(all(s['pid'] == pid and s['business'] == baseline
             and s['window_foreground'] is True and s['own_window_identity'] == window
             and s['foreground_window_identity'] == window
             and s['primary_press_count'] == primary and s['selected_actor'] == selection
             and tuple(s[k] for k in CAMERA) == camera for s in states),
         'action window/world/business unchanged ' + label)
    times = [a['utc_ticks']] + [s['utc_ticks'] for s in states]
    need(times[0] <= times[1] <= times[2] < times[3] < times[4],
         'action UTC progression ' + label)
    if item:
        need((a['selected_type'], a['selected_id'], a['item_index']) ==
             (item['type'], item['id'], item['index'])
             and a['source_show_rect'] == item['show_rect']
             and a['source_reality_rect'] == item['reality_rect']
             and a['processed_state']['flag_editor_open'] is True
             and a['processed_state']['selected_type'] == item['type']
             and a['processed_state']['edit_draft'][str(item['type'])] == item['id'],
             '63 actual item selections/source IDs ' + label)
        processed = a['processed_state']
        ids = processed['edit_draft']
        cloth = next(r for r in config[1] if r['id'] == ids['1'])
        cloth_color = next(r for r in config[2] if r['id'] == ids['2'])
        emblem = next(r for r in config[3] if r['id'] == ids['3'])
        emblem_color = next(r for r in config[4] if r['id'] == ids['4'])
        widgets = processed['widgets']
        check_rect_image(widgets['AllianceFlagEditorCloth'], cloth['reality_rect'],
                         cloth_color['color'], (184, 216))
        check_rect_image(widgets['AllianceFlagEditorEmblem'], emblem['reality_rect'],
                         emblem_color['color'], (100, 100))
        need(widgets['AllianceFlagEditorSourceIds']['text'].endswith(
                 f"{ids['1']} / {ids['2']} / {ids['3']} / {ids['4']}")
             and any(r['id'] == item['id'] and same_color(r['Panel']['brush_color'],
                 {'r': .67, 'g': .48, 'b': .13, 'a': 1}) for r in processed['visible_items']),
             'actual selected preview, source IDs, and highlight ' + label)


def check_text_input(receipt: dict, spec: tuple[str, str, str], baseline: dict,
                     primary: int, pid: int, window: str) -> None:
    label, target, value = spec
    need(receipt['label'] == label and receipt['target'] == target
         and receipt['method'] == 'ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent+ProcessKeyCharEvent'
         and receipt['requested_text'] == receipt['text_after'] == value
         and receipt['text_before'] == '' and receipt['stable_frames'] >= 2
         and receipt['exact_input_in_path'] is True and receipt['editable_text_in_path'] is True
         and receipt['umg_path_present'] is True and receipt['target_hovered'] is True
         and receipt['own_game_window'] is True and receipt['foreground'] is True
         and receipt['own_window_pid'] == pid
         and receipt['own_window_identity'] == receipt['foreground_window_identity'] == window
         and receipt['target_widget_identity'].startswith('0x')
         and receipt['target_slate_identity'] in [n['slate_identity'] for n in receipt['hit_path_nodes']]
         and receipt['focus_down_handled'] is True and receipt['focus_up_handled'] is True
         and receipt['keyboard_focus_in_hit_path'] is True
         and receipt['keyboard_focused_identity'].startswith('0x')
         and receipt['focus_retained'] is True and receipt['all_characters_handled'] is True
         and ''.join(event['character'] for event in receipt['character_events']) == value
         and all(event['handled'] is True for event in receipt['character_events'])
         and receipt['primary_before'] == receipt['primary_after'] == primary
         and receipt['business_before'] == receipt['business_after'] == baseline
         and receipt['failure'] == '', 'actual Slate three-field typing ' + label)
    before, requested = receipt['before_state'], receipt['requested_state']
    need(before['frame'] == requested['frame'] == receipt['frame']
         and receipt['utc_ticks'] <= before['utc_ticks'] <= requested['utc_ticks']
         and before['pid'] == requested['pid'] == pid
         and before['widgets'][target]['text'] == ''
         and requested['widgets'][target]['text'] == value
         and before['business'] == requested['business'] == baseline
         and before['pending_action_id'] == requested['pending_action_id'] == -1,
         'serialized before/request text field state ' + label)


def validate(d: dict, run: Path, *, live: bool = True) -> None:
    pins = expected_pins() if live else {}
    if live:
        static_guard()
    need(d['schema'] == SCHEMA and d['complete'] is True and d['passed'] is True
         and d['failures'] == [] and d['attempts'] == [] and d['completion_events'] == 0
         and d['baseline_phase'] == PHASES[0] and d['engine_major'] == 5
         and d['engine_minor'] == 8 and d['world'] == '/Game/Maps/L_RokUnityGameAppCity'
         and Path(d['project']).resolve() == ROOT.resolve()
         and Path(d['engine_dir']).resolve() == Path('D:/ue/UE_5.8/Engine').resolve()
         and d['pid'] > 0 and d['utc_ticks'] > 0, 'native complete/project/world/engine')
    for flag in ('world_time_advanced_by_audit', 'alliance_membership_accepted',
                 'server_create_accepted', 'unity_pixel_parity_accepted', 'full_migration_accepted'):
        need(d[flag] is False, 'scope claim ' + flag)
    need(re.fullmatch(r'[0-9]+_' + str(d['pid']), run.name), 'fresh UTCticks_PID directory')
    if live:
        expected = dict(pins)
        for rel in ('Source/Rok/RokAllianceFlagLocalPreviewAuditSubsystem.h',
                    'Source/Rok/RokAllianceFlagLocalPreviewAuditSubsystem.cpp',
                    'Scripts/VerifyRokAllianceFlagLocalPreviewRuntime.py'):
            expected[rel] = sha(STAGE/rel)
        check_input_map(d['inputs_sha256'], expected)
        for rel, digest in expected.items():
            need(sha(contained_file(resolve(rel), ROOT)) == digest, 'live input changed ' + rel)
    expected = expected_config()
    rows = d['source_config']['rows']
    need(d['source_config']['atlas'] == ATLAS
         and d['source_config']['atlas_size'] == {'x': 1024, 'y': 2048}
         and len(rows) == 63, 'native atlas/config count')
    for row, ref in zip(rows, expected):
        need(all(row[k] == ref[k] for k in ('id', 'type', 'index', 'action_id',
                                           'show_rect', 'reality_rect', 'has_sprite', 'has_color')),
             'native 63 source rows/order')
        need(same_color(row['color'], source_color(ref['rgb'])),
             'source SRGB-derived config color')
    config = {t: [r for r in rows if r['type'] == t] for t in range(1, 5)}
    intervals = specs()
    need(len(intervals) == 15 and sum(map(len, intervals)) == 83,
         'fixed 16-phase/83-action contract')
    captures, actions, typed = d['captures'], d['actions'], d['text_inputs']
    need(len(captures) == 16 and len(actions) == 83
         and len(typed) == 3
         and tuple(c['phase'] for c in captures) == PHASES, 'native capture/action sequence')
    baseline = d['baseline_business']; check_business(baseline)
    first = captures[0]['requested_state']
    camera = tuple(first[k] for k in CAMERA)
    primary, selection, window = first['primary_press_count'], first['selected_actor'], first['own_window_identity']
    previous_frame, previous_time = 0, 0
    for phase, c in enumerate(captures):
        frames = (c['requested_frame'], c['processed_frame'], c['next_frame'])
        states = (c['requested_state'], c['processed_state'], c['next_frame_state'])
        times = tuple(s['utc_ticks'] for s in states)
        need(previous_frame < frames[0] <= frames[1] < frames[2]
             and previous_time <= times[0] <= times[1] < times[2]
             and all(s['frame'] == frame for s, frame in zip(states, frames)),
             'screenshot requested/processed/next frame and UTC')
        previous_frame, previous_time = frames[2], times[2]
        image = contained_file(Path(c['png']), run)
        need(image == (run/(PHASES[phase] + '.png')).resolve(strict=True)
             and c['png_width'] == 1440 and c['png_height'] == 810,
             'native original PNG path/extent')
        png(image, c['png_sha256'], c['png_bytes'])
        for state in states:
            check_state(state, phase, baseline, camera, primary, selection,
                        d['pid'], window, config)
    need(len({c['png_sha256'] for c in captures}) >= 8, 'visual phase diversity')
    prior = actions[1]['next_frame']
    for receipt, spec in zip(typed, TEXT_FIELDS):
        need(prior < receipt['frame'] < captures[2]['requested_frame'],
             'three real text events between Create and editor capture')
        check_text_input(receipt, spec, baseline, primary, d['pid'], window)
        prior = receipt['frame']
    n = 0
    for i, between in enumerate(intervals):
        prior_frame = captures[i]['next_frame']
        prior_time = captures[i]['next_frame_state']['utc_ticks']
        for spec in between:
            a = actions[n]
            need(prior_frame < a['frame'] and a['next_frame'] < captures[i + 1]['requested_frame']
                 and prior_time <= a['utc_ticks'], 'action between actual screenshots')
            check_action(a, spec, baseline, camera, primary, selection, d['pid'], window, config)
            prior_frame, prior_time = a['next_frame'], a['next_frame_state']['utc_ticks']
            n += 1
    need(all(a['action_id'] != 623 for a in actions), 'server create click forbidden')
    need(len({(a['selected_type'], a['selected_id']) for a in actions if a['kind'] == 'item'}) == 63,
         'all 63 distinct source options actually clicked')
    last = captures[-1]['next_frame_state']; final = d['final_state']
    final_progression(last, final)
    check_state(final, 15, baseline, camera, primary, selection, d['pid'], window, config)


def reject(fn, label: str) -> None:
    try:
        fn()
    except (ValueError, KeyError, TypeError, OSError):
        return
    raise RuntimeError('negative accepted: ' + label)


def self_test() -> dict:
    static_guard()
    cases = []
    last = {'frame': 100, 'utc_ticks': 200, 'business': {'http_dispatch_attempts': 0}}
    final = {'frame': 101, 'utc_ticks': 201, 'business': {'http_dispatch_attempts': 0}}
    final_progression(last, final); cases.append('final_exact_next_tick_positive')
    for label, change in (
        ('same_finish_frame', {'frame': 100}),
        ('extra_finish_frame', {'frame': 102}),
        ('equal_finish_utc', {'utc_ticks': 200}),
        ('reversed_finish_utc', {'utc_ticks': 199}),
        ('finish_business_drift', {'business': {'http_dispatch_attempts': 1}}),
    ):
        bad = copy.deepcopy(final); bad.update(change)
        reject(lambda b=bad: final_progression(last, b), label); cases.append(label)
    # The same fields are emitted by the native click writer: one request tick,
    # one deferred processed tick, one next tick, and a pending action identity.
    action = {'label': 'open_create_601', 'target': 'AllianceWelcomeCreate', 'action_id': 601,
              'kind': 'button', 'method': 'ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent',
              'exact_button_in_path': True, 'umg_path_present': True, 'target_hovered': True,
              'own_game_window': True, 'foreground': True, 'own_window_pid': 77,
              'own_window_identity': '0xA', 'foreground_window_identity': '0xA',
              'down_handled': True, 'up_handled': True, 'stable_frames': 2,
              'pending_before': -1, 'pending_after': 601, 'primary_before': 0,
              'primary_after': 0, 'business_before': {'http_dispatch_attempts': 0},
              'business_after': {'http_dispatch_attempts': 0}, 'failure': '',
              'target_slate_identity': '0xB', 'target_widget_identity': '0xC',
              'hit_path_nodes': [{'slate_identity': '0xB'}], 'frame': 10, 'utc_ticks': 100}
    state = {'pid': 77, 'business': action['business_before'], 'window_foreground': True,
             'own_window_identity': '0xA', 'foreground_window_identity': '0xA',
             'primary_press_count': 0, 'selected_actor': '', **{k: 0 for k in CAMERA}}
    action['before_state'] = {**state, 'frame': 10, 'utc_ticks': 100, 'pending_action_id': -1}
    action['requested_state'] = {**state, 'frame': 10, 'utc_ticks': 101, 'pending_action_id': 601}
    action['processed_frame'] = 11
    action['processed_state'] = {**state, 'frame': 11, 'utc_ticks': 102, 'pending_action_id': -1}
    action['next_frame'] = 12
    action['next_frame_state'] = {**state, 'frame': 12, 'utc_ticks': 103, 'pending_action_id': -1}
    spec = ('open_create_601', 'AllianceWelcomeCreate', 601, None)
    def check(a): check_action(a, spec, action['business_before'], tuple(0 for _ in CAMERA), 0, '', 77, '0xA', {})
    action = json.loads(json.dumps(action, sort_keys=True))
    check(action); cases.append('serialized_writer_shape_click_positive')
    for label, field, value in (
        ('wrong_pending_after', 'pending_after', 600),
        ('missing_pending_after', 'pending_after', None),
        ('unhandled_pointer_up', 'up_handled', False),
        ('foreign_window', 'own_window_pid', 88),
        ('server_create_action', 'action_id', 623),
        ('processed_same_frame', 'processed_frame', 10),
        ('next_frame_gap', 'next_frame', 13),
        ('business_mutation', 'business_after', {'http_dispatch_attempts': 1}),
    ):
        bad = copy.deepcopy(action)
        if value is None: del bad[field]
        else: bad[field] = value
        reject(lambda b=bad: check(b), label); cases.append(label)
    pin_baseline = expected_pins()
    check_input_map(json.loads(json.dumps(pin_baseline)), pin_baseline)
    cases.append('serialized_input_pin_positive')
    for label, path in (
        ('wrong_source_pin', 'Source/Rok/RokUnityFeatureScreenWidget.cpp'),
        ('wrong_friend_pin', 'Source/Rok/RokUnityFeatureScreenWidget.h'),
    ):
        bad = copy.deepcopy(pin_baseline); bad[path] = '0'*64
        reject(lambda b=bad: check_input_map(b, pin_baseline), label); cases.append(label)
    with tempfile.TemporaryDirectory() as directory:
        image = Path(directory) / '00_city_hud.png'
        data = b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 1440, 810) + b'\x08\x06\x00\x00\x00'
        image.write_bytes(data)
        digest = hashlib.sha256(data).hexdigest()
        png(image, digest, len(data)); cases.append('serialized_png_header_hash_positive')
        reject(lambda: png(image, '0'*64, len(data)), 'wrong_png_sha'); cases.append('wrong_png_sha')
        reject(lambda: png(image, digest, len(data)+1), 'wrong_png_bytes'); cases.append('wrong_png_bytes')
        bad = bytearray(data); bad[16:20] = struct.pack('>I', 1439)
        image.write_bytes(bad)
        reject(lambda: png(image, hashlib.sha256(bad).hexdigest(), len(bad)), 'wrong_png_width')
        cases.append('wrong_png_width')
    rect = {'min_x': 1, 'min_y': 305, 'max_x': 185, 'max_y': 521}
    uv = {'x': 1/1024, 'y': (2048-521)/2048}
    widget = {'exists': True, 'visible': True, 'enabled': True, 'texture': ATLAS,
              'color': source_color(None), 'brush_image_size': {'x': 184, 'y': 216},
              'uv_min': uv, 'uv_max': {'x': 185/1024, 'y': (2048-305)/2048}}
    widget = json.loads(json.dumps(widget, sort_keys=True))
    check_rect_image(widget, rect, source_color(None), (184, 216))
    cases.append('serialized_atlas_preview_positive')
    for label, key, value in (('wrong_atlas', 'texture', '/Game/Wrong'),
                              ('wrong_uv', 'uv_max', {'x': 0, 'y': 0}),
                              ('wrong_tint', 'color', {'r': 0, 'g': 0, 'b': 0, 'a': 1})):
        bad = copy.deepcopy(widget); bad[key] = value
        reject(lambda b=bad: check_rect_image(b, rect, source_color(None), (184, 216)), label)
        cases.append(label)
    bad = copy.deepcopy(final); bad['widgets'] = {'AllianceCreateConfirm': {'visible': True}}
    reject(lambda: final_progression(last, bad), 'finish_ui_drift'); cases.append('finish_ui_drift')
    target = TEXT_FIELDS[0]
    typed = {'label': target[0], 'target': target[1], 'requested_text': target[2],
             'text_before': '', 'text_after': target[2],
             'method': 'ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent+ProcessKeyCharEvent',
             'stable_frames': 2, 'exact_input_in_path': True,
             'editable_text_in_path': True, 'umg_path_present': True,
             'target_hovered': True, 'own_game_window': True, 'foreground': True,
             'own_window_pid': 77, 'own_window_identity': '0xA',
             'foreground_window_identity': '0xA', 'target_widget_identity': '0xC',
             'target_slate_identity': '0xB', 'hit_path_nodes': [{'slate_identity': '0xB'}],
             'focus_down_handled': True, 'focus_up_handled': True,
             'keyboard_focus_in_hit_path': True, 'keyboard_focused_identity': '0xD',
             'focus_retained': True, 'all_characters_handled': True,
             'character_events': [{'character': ch, 'handled': True} for ch in target[2]],
             'primary_before': 0, 'primary_after': 0,
             'business_before': action['business_before'], 'business_after': action['business_before'],
             'failure': '', 'frame': 20, 'utc_ticks': 200,
             'before_state': {'frame': 20, 'utc_ticks': 201, 'pid': 77,
                              'business': action['business_before'], 'pending_action_id': -1,
                              'widgets': {target[1]: {'text': ''}}},
             'requested_state': {'frame': 20, 'utc_ticks': 202, 'pid': 77,
                                 'business': action['business_before'], 'pending_action_id': -1,
                                 'widgets': {target[1]: {'text': target[2]}}}}
    typed = json.loads(json.dumps(typed, sort_keys=True))
    def check_typed(t): check_text_input(t, target, action['business_before'], 0, 77, '0xA')
    check_typed(typed); cases.append('serialized_real_slate_text_positive')
    for label, field, value in (
        ('wrong_text_target', 'target', 'AllianceCreateTagInput'),
        ('unhandled_character', 'all_characters_handled', False),
        ('wrong_text_value', 'text_after', 'NotTheName'),
        ('missing_focus', 'keyboard_focus_in_hit_path', False),
        ('text_business_mutation', 'business_after', {'http_dispatch_attempts': 1}),
    ):
        bad = copy.deepcopy(typed); bad[field] = value
        reject(lambda b=bad: check_typed(b), label); cases.append(label)
    need(sum(map(len, specs())) == 83, '83 action fixture')
    return {'schema': 'rok.alliance_flag.local_preview.verifier_selftest.v2',
            'passed': True, 'negative_cases': len(cases) - 6, 'cases': cases,
            'real_runtime': False}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        print(json.dumps(self_test(), sort_keys=True)); return
    need(args.manifest is not None, '--manifest required')
    path = contained_file(args.manifest, STAGE/'RuntimeAudit')
    run = path.parent
    need(path.name == 'manifest.json', 'native manifest name')
    obj = json.loads(path.read_text(encoding='utf-8-sig'))
    validate(obj, run)
    print(json.dumps({'actual_runtime_verified': True, 'manifest': str(path),
                      'png_count': 16, 'action_count': 83}, sort_keys=True))


if __name__ == '__main__':
    main()
