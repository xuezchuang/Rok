"""Verify actual city-HUD mail navigation and explicit read-only reward fixtures.

The runtime harness uses both delegate broadcasts and bounded in-app Slate
pointer events. The latter do not prove hardware pointer behavior, a server
mailbox, an inventory grant, or a reward claim request.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path

import numpy as np
from PIL import Image
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'Saved/RokUnityMigration/MailRewardDetailAudit'
FIXTURE = DATA_ROOT / 'mail_reward_presentation.json'
NATIVE_DIR = ROOT / 'Saved/UnityMailRewardNativeProbe/Artifacts/native_20261008_114400_572'
NATIVE = NATIVE_DIR / 'render_evidence.json'
NATIVE_VERIFY = ROOT / 'Saved/RokUnityMigration/MailRewardNative/verified_capture.json'
STEMS = [
    '01_normal_mail_empty', '02_normal_report_empty',
    '03_city_after_normal_close', '04_mixed_reward_selected',
    '05_mail_detail_scrolled_claim_visible', '06_item_help_tip',
    '07_item_tip_body_hit_retained', '08_item_tip_arrow_hit_retained',
    '09_item_tip_blank_retained', '10_troop_help_tip_replaced',
    '11_alternate_mail_empty_tooltip_cleared', '12_mixed_reward_reselected',
    '13_claim_boundary_no_change', '14_item_tip_reopened_after_claim',
    '15_arrow_side_clicked_tip_closed', '16_city_after_fixture_close',
    '17_normal_mail_reentry_empty', '18_legacy_single_attachment',
    '19_city_after_single_close', '20_legacy_single_reopened',
    '21_final_city',
]
PAGES = [
    'normal_empty', 'normal_empty', 'closed', 'mixed_fixture',
    'mixed_fixture', 'mixed_fixture', 'mixed_fixture', 'mixed_fixture',
    'mixed_fixture', 'mixed_fixture', 'empty_alternate', 'mixed_fixture',
    'mixed_fixture', 'mixed_fixture', 'mixed_fixture', 'closed',
    'normal_empty', 'reference_single', 'closed', 'reference_single',
    'closed',
]
STEPS = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 26, 28, 30,
         32, 34, 37, 39, 41, 43]
EXPECTED_ACTIONS = [
    ('MailFeatureHitButton', 'UButton.OnClicked.Broadcast'),
    ('MailTab_1', 'UButton.OnClicked.Broadcast'),
    ('FeatureFrameClose', 'UButton.OnClicked.Broadcast'),
    ('explicit_mixed_fixture', 'URokUnityFeatureScreenWidget.OpenMailReadOnlyRewardFixture'),
    ('MailDetailScroll', 'UScrollBox.ScrollToEnd'),
    ('MailRewardFixtureHit_6', 'UButton.OnClicked.Broadcast'),
    ('MailRewardTooltipBodyHit', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('MailRewardTooltipArrowHit', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('MailTooltipBlankBlocker', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('MailRewardFixtureHit_5', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('MailRowHit_1', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('MailRowHit_0', 'UButton.OnClicked.Broadcast'),
    ('MailDetailScroll', 'UScrollBox.ScrollToEnd'),
    ('MailClaim', 'UButton.OnClicked.Broadcast'),
    ('MailRewardFixtureHit_6', 'UButton.OnClicked.Broadcast'),
    ('MailTooltipArrowSideLeft', 'FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent'),
    ('FeatureFrameClose', 'UButton.OnClicked.Broadcast'),
    ('MailFeatureHitButton', 'UButton.OnClicked.Broadcast'),
    ('FeatureFrameClose', 'UButton.OnClicked.Broadcast'),
    ('explicit_reference_single', 'URokUnityFeatureScreenWidget.OpenMail(true)'),
    ('FeatureFrameClose', 'UButton.OnClicked.Broadcast'),
    ('explicit_reference_single', 'URokUnityFeatureScreenWidget.OpenMail(true)'),
    ('FeatureFrameClose', 'UButton.OnClicked.Broadcast'),
]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rect(obj):
    return tuple(float(obj[key]) for key in ('x', 'y', 'width', 'height'))


def assert_rect(obj, expected, tolerance=.5):
    actual = rect(obj)
    assert all(abs(a - b) <= tolerance for a, b in zip(actual, expected)), (actual, expected)


def visible_geometry(obj):
    return obj['attached'] and obj['visible'] and 'pixel_rect' in obj


def native_node(sample, suffix):
    matches = [node for node in sample['windowGeometry'] if node['path'].endswith(suffix)]
    assert len(matches) == 1, (sample['fixtureKey'], suffix, len(matches))
    return matches[0]


def assert_source_geometry(widget, source, tolerance=1.0, y_shift=0):
    assert visible_geometry(widget), widget
    assert source['activeInHierarchy'], source['path']
    x, y, width, height = rect(source['screenRect'])
    assert_rect(widget['pixel_rect'], (x, y - y_shift, width, height), tolerance)


def assert_source_text(widget, source, color_source=None):
    assert widget['text'] == source['text']
    assert_source_text_style(widget, source, color_source)


def assert_source_text_style(widget, source, color_source=None):
    assert widget['font_points'] == source.get('generatorFontSizeUsed', source['fontSize']) * .75
    assert widget['font_style'] == ('Bold' if source['fontStyle'] == 'Bold' else 'Regular')
    color = (color_source or source)['color']
    assert all(abs(widget['display_srgb'][key] - color[key] * 255) <= 1
               for key in ('r', 'g', 'b', 'a'))


def assert_centered_text(widget, region):
    assert visible_geometry(widget) and visible_geometry(region)
    tx, ty, tw, th = rect(widget['pixel_rect'])
    rx, ry, rw, rh = rect(region['pixel_rect'])
    assert tx >= rx - 1 and tx + tw <= rx + rw + 1
    assert ty >= ry - 1 and ty + th <= ry + rh + 1
    assert abs((ty + th / 2) - (ry + rh / 2)) <= 1


def assert_mail_window(state, sample):
    # These are full UI_Win_Mail runtime geometry checks, rather than reward
    # cell-local slots or a screenshot-only resemblance claim.
    shared = {
        'mail_frame_left': '/pl_view/win/frame1',
        'mail_frame_right': '/pl_view/win/frame2',
        'compose': '/pl_view/title/UI_Item_newMail/btn_newMail',
        'list_panel': '/pl_view/content/mails',
        'list_scroll': '/pl_view/content/mails/sv_list_view',
    }
    for key, suffix in shared.items():
        assert_source_geometry(state[key], native_node(sample, suffix))
    title_source = native_node(sample, '/pl_view/win/lbl_title')
    assert_source_geometry(state['mail_window_title'], title_source)
    assert_source_text(state['mail_window_title'], title_source)
    compose_source = native_node(sample, '/pl_view/title/UI_Item_newMail/btn_newMail/text')
    assert_source_geometry(state['compose_label'], compose_source)
    assert_source_text(state['compose_label'], compose_source)
    tab_names = ('Sys', 'Report', 'Guild', 'Person', 'Send', 'Collect')
    assert len(state['mail_tabs']) == 6
    assert len(state['mail_tab_labels']) == 6
    assert len(state['mail_tab_label_regions']) == 6
    for widget, label, region, name in zip(state['mail_tabs'], state['mail_tab_labels'],
                                           state['mail_tab_label_regions'], tab_names):
        assert_source_geometry(widget, native_node(sample,
            f'/pl_view/title/pageRect/UI_Item_MailPageBtn{name}'))
        source_label = native_node(sample,
            f'/pl_view/title/pageRect/UI_Item_MailPageBtn{name}/lbl_languageText')
        assert_source_geometry(region, source_label)
        assert_centered_text(label, region)
        color_source = source_label
        if state['page'] == 'normal_empty' and state['selected_tab'] == 1:
            if name == 'Sys':
                color_source = native_node(sample,
                    '/pl_view/title/pageRect/UI_Item_MailPageBtnReport/lbl_languageText')
            elif name == 'Report':
                color_source = native_node(sample,
                    '/pl_view/title/pageRect/UI_Item_MailPageBtnSys/lbl_languageText')
        assert_source_text(label, source_label, color_source)
    if state['page'] == 'normal_empty':
        assert not state['detail_panel']['attached']
        assert all(not footer['attached'] for footer in state['mail_footers'])
        assert all(not label['attached'] for label in state['mail_footer_labels'])
        assert all(not region['attached'] for region in state['mail_footer_label_regions'])
        return
    footer_paths = (
        '/pl_view/buttom/pl_all/UI_Item_MailBtnOnButtomDeleteRead',
        '/pl_view/buttom/pl_all/UI_Item_MailBtnOnButtomRead',
        '/pl_view/buttom/group/UI_Item_MailBtnOnButtomCollect',
        '/pl_view/buttom/group/UI_Item_MailBtnOnButtomDel',
    )
    assert len(state['mail_footers']) == len(footer_paths)
    assert len(state['mail_footer_labels']) == len(footer_paths)
    assert len(state['mail_footer_label_regions']) == len(footer_paths)
    for widget, label, region, suffix in zip(state['mail_footers'], state['mail_footer_labels'],
                                              state['mail_footer_label_regions'], footer_paths):
        assert_source_geometry(widget, native_node(sample, suffix))
        source_label = native_node(sample, suffix + '/btn_newMail/text')
        assert_source_geometry(region, source_label)
        assert_centered_text(label, region)
        assert_source_text(label, source_label)
    detail = '/pl_view/content/mailContent'
    assert_source_geometry(state['detail_panel'], native_node(sample, detail + '/pl_mailcontentbg'))
    assert_source_geometry(state['detail_scroll'], native_node(sample, detail + '/sv_haveMail'))
    offset = state['detail_scroll_offset']
    poster_source = native_node(sample,
        detail + '/sv_haveMail/v/c/UI_Item_MailTypeSystem/UI_Item_MailTitle/img_bg')
    assert poster_source['spriteName'] == 'img_mail_title_1'
    assert_source_geometry(state['detail_poster'], poster_source, y_shift=offset)
    assert_atlas(state['detail_poster'],
                 '/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_mail.rok_fullscreen_mail',
                 source_sprite('ui_res_mail/ui_mail.tpsheet', 'img_mail_title_1'),
                 (2048, 1024))
    if state['page'] == 'empty_alternate':
        assert_rect(state['reward_grid']['pixel_rect'],
                    (552, 365 - offset, 820, 0), 1.0)
    assert_source_geometry(state['claim_button'], native_node(sample,
        detail + '/sv_haveMail/v/c/UI_Item_MailTypeSystem/pl_btn/btn_receive'),
        y_shift=offset)
    claim_label_source = native_node(sample,
        detail + '/sv_haveMail/v/c/UI_Item_MailTypeSystem/pl_btn/btn_receive/btn_languageButton/lbl_Text')
    assert_source_geometry(state['claim_label'], claim_label_source, y_shift=offset)
    assert_source_text(state['claim_label'], claim_label_source)
    if state['page'] in ('mixed_fixture', 'reference_single'):
        assert_source_geometry(state['mail_list_rows'][0], sample['leftMailRow'], 1.0)
        row_source = sample['leftMailRowGeometry']
        for widget, suffix in (
            (state['mail_row_titles'][0], '/texts/lbl_title'),
            (state['mail_row_subjects'][0], '/texts/lbl_desc'),
        ):
            matches = [node for node in row_source if node['path'].endswith(suffix)]
            assert len(matches) == 1
            assert_source_geometry(widget, matches[0])
            assert_source_text(widget, matches[0])
        title_root = detail + '/sv_haveMail/v/c/UI_Item_MailTypeSystem/UI_Item_MailTitle'
        title_source = native_node(sample, title_root + '/lbl_title')
        subtitle_source = native_node(sample, title_root + '/lbl_desc')
        assert_source_geometry(state['detail_text']['MailDetailTitle'], title_source,
                               y_shift=offset)
        assert_source_geometry(state['detail_text']['MailDetailSub'], subtitle_source,
                               y_shift=offset)
        assert_source_text(state['detail_text']['MailDetailTitle'], title_source)
        assert_source_text(state['detail_text']['MailDetailSub'], subtitle_source)
        body_source = native_node(sample,
            detail + '/sv_haveMail/v/c/UI_Item_MailTypeSystem/lbl_Content')
        assert '<color=green><b>' in body_source['text'] and '</b></color>' in body_source['text']
        highlight = state['detail_text']['MailDetailHighlight']
        assert highlight['text'] in body_source['text']
        assert highlight['font_style'] == 'Bold'
        assert highlight['display_srgb'] == {'r': 0, 'g': 128, 'b': 0, 'a': 255}
        body = rect(body_source['screenRect'])
        for name in ('MailDetailGreeting', 'MailDetailPrefix', 'MailDetailHighlight',
                     'MailDetailSuffix', 'MailDetailFinalLine'):
            part = rect(state['detail_text'][name]['pixel_rect'])
            assert part[0] >= body[0] - 1 and part[0] + part[2] <= body[0] + body[2] + 1
            assert part[1] >= body[1] - offset - 1
            assert part[1] + part[3] <= body[1] + body[3] - offset + 1
            assert state['detail_text'][name]['font_points'] == body_source['fontSize'] * .75
    if state['page'] in ('mixed_fixture', 'reference_single', 'empty_alternate'):
        # The native unselected probe is deliberately below the viewport, so
        # its text/color/font metadata is authoritative, not its screen rect.
        unselected = 0 if state['selected_mail_index'] == 1 else 1
        for key, suffix in (('mail_row_titles', '/texts/lbl_title'),
                            ('mail_row_subjects', '/texts/lbl_desc')):
            matches = [node for node in sample['unselectedMailRowGeometry']
                       if node['path'].endswith(suffix)]
            assert len(matches) == 1
            widget = state[key][unselected]
            assert visible_geometry(widget)
            assert_source_text_style(widget, matches[0])
        if state['page'] == 'reference_single':
            reward = sample['rewards'][0]
            for key, source_key in {
                'legacy_reward': 'gift', 'legacy_reward_icon': 'icon',
                'legacy_reward_quality': 'qualityBackground',
                'legacy_reward_count': 'countLabel', 'legacy_reward_name': 'nameLabel',
            }.items():
                assert_source_geometry(state[key], reward[source_key], y_shift=offset)
        if state['page'] == 'mixed_fixture':
            assert_rect(state['reward_grid']['pixel_rect'],
                        (552, 365 - offset, 820, 310), 1.0)
            for cell, reward in zip(state['reward_cells'], sample['rewards']):
                assert_source_geometry(cell['root'], reward['gift'], y_shift=offset)
                assert_source_geometry(cell['icon'], reward['icon'], y_shift=offset)


def source_sprite(sheet, name):
    path = ROOT / 'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic' / sheet
    matches = [line.split(';') for line in path.read_text(encoding='utf-8-sig').splitlines()
               if line.split(';', 1)[0] == name]
    assert len(matches) == 1, (sheet, name)
    return tuple(int(value) for value in matches[0][1:5])


def assert_atlas(image, atlas_path, sprite, atlas_size):
    assert visible_geometry(image) and image['texture'] == atlas_path, (image.get('texture'), atlas_path)
    x, y, width, height = sprite
    aw, ah = atlas_size
    expected = (x / aw, (ah - y - height) / ah, (x + width) / aw, (ah - y) / ah)
    observed = tuple(image['uv'][key] for key in ('min_x', 'min_y', 'max_x', 'max_y'))
    assert all(abs(a - b) <= 1e-5 for a, b in zip(observed, expected)), (observed, expected)


def assert_reward_icon(cell, row):
    if row['kind'] == 0:
        name = f'img_com_cur{row["id"] - 99}'
        sprite = source_sprite('ui_res_common/ui_common.tpsheet', name)
        assert_atlas(cell['icon'], '/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common',
                     sprite, (2048, 1024))
    elif row['kind'] == 1:
        sprite = source_sprite('ui_res_troops/troops_head.tpsheet', 'troops_head1_4')
        assert_atlas(cell['icon'], '/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_troops_head.rok_fullscreen_troops_head',
                     sprite, (1024, 1024))
    else:
        sprite = source_sprite('ui_res_item/item5.tpsheet', 'item_5_1031')
        assert_atlas(cell['icon'], '/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5',
                     sprite, (1024, 2048))
        quality = source_sprite('ui_res_item/item0.tpsheet', 'img_item_bg4')
        assert_atlas(cell['quality'], '/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.rok_modal_atlas_item0',
                     quality, (512, 512))


def fixture_source():
    package = json.loads(FIXTURE.read_text(encoding='utf-8-sig'))
    assert package['schema'] == 'rok.mail_reward_presentation.v1'
    assert 'not a player mailbox' in package['origin']
    assert 'positive food/wood/stone/gold/denar' in package['source_behavior']
    for relative, digest in package['sources'].items():
        path = resolve(relative).resolve()
        assert path.is_relative_to(ROOT.resolve()) and path.is_file()
        assert sha(path) == digest, relative
    fixtures = package['fixtures']
    assert set(fixtures) == {'reference_single', 'mixed_audit', 'empty_alternate'}
    single = fixtures['reference_single']['payload']
    mixed = fixtures['mixed_audit']['payload']
    empty = fixtures['empty_alternate']['payload']
    fields = ('food', 'wood', 'stone', 'gold', 'denar')
    assert [single[field] for field in fields] == [0] * 5
    assert single['soldiers'] == [] and single['items'] == [{'itemId': 502090001, 'itemNum': 3}]
    assert [mixed[field] for field in fields] == [101, 202, 303, 404, 505]
    assert mixed['soldiers'] == [{'id': 10304, 'num': 7}]
    assert mixed['items'] == [{'itemId': 502090001, 'itemNum': 3}]
    assert [empty[field] for field in fields] == [0] * 5 and empty['soldiers'] == empty['items'] == []
    metadata = package['metadata']
    template = package['mail_template']
    assert template['mail_id'] == 100030 and template['activity_id'] == 10101
    assert template['send_time'] is None
    assert template['locale_ids'] == {
        'title': 762137, 'subtitle': 762138, 'body': 550111,
        'activity_name': 300229, 'claim': 570008,
    }
    assert template['body_plain'] == (
        template['body_greeting'] + '\n' + template['body_prefix'] +
        template['body_activity_quoted'] + template['body_suffix'])
    assert template['activity_name'] in template['body_activity_quoted']
    assert template['claim_label'] == '领取'
    assert [c['id'] for c in metadata['currencies']] == list(range(100, 105))
    assert metadata['item']['id'] == 502090001 and metadata['item']['quality'] == 4
    assert metadata['item']['icon'] == 'item5[item_5_1031]'
    assert metadata['soldier']['id'] == 10304 and metadata['soldier']['name'] == '弩兵'
    assert metadata['soldier']['level'] == 4 and metadata['soldier']['arms_type'] == 3
    expected = []
    for currency, quantity in zip(metadata['currencies'], (101, 202, 303, 404, 505)):
        expected.append({'kind': 0, 'id': currency['id'], 'quantity': quantity,
                         'quantity_text': str(quantity), 'name': currency['name'],
                         'icon': currency['icon'], 'quality': 0, 'troop_level': 0,
                         'troop_arms_type': 0, 'known_metadata': True})
    soldier = metadata['soldier']
    expected.append({'kind': 1, 'id': soldier['id'], 'quantity': 7,
                     'quantity_text': '7', 'name': soldier['name'], 'icon': soldier['icon'],
                     'quality': 0, 'troop_level': soldier['level'],
                     'troop_arms_type': soldier['arms_type'], 'known_metadata': True})
    item = metadata['item']
    expected.append({'kind': 2, 'id': item['id'], 'quantity': 3,
                     'quantity_text': '3', 'name': item['name'], 'icon': item['icon'],
                     'quality': item['quality'], 'troop_level': 0,
                     'troop_arms_type': 0, 'known_metadata': True})
    return package, expected


def native_source(mixed_rows):
    native = json.loads(NATIVE.read_text(encoding='utf-8-sig'))
    check = json.loads(NATIVE_VERIFY.read_text(encoding='utf-8-sig'))
    assert native['schema'] == 'rok.mail_reward.native.v1'
    assert native['complete'] and native['playMode'] and native['originalUiRoot']
    assert native['originalMailWindow'] and native['originalSystemItem'] and native['originalMailGift']
    assert native['claimNotInvoked'] and native['serverNotInvoked']
    assert (native['width'], native['height']) == (1440, 810)
    assert check['schema'] == 'rok.mail_reward.native_verified.v1'
    assert check['native_evidence_sha256'] == sha(NATIVE)
    assert check['source_fixture_sha256'] == native['fixtureSourceSha256'] == sha(FIXTURE)
    assert [sample['fixtureKey'] for sample in native['samples']] == \
           ['reference_single', 'mixed_audit', 'empty_alternate']
    assert [sample['rowCount'] for sample in native['samples']] == [1, 7, 0]
    expected_sets = [mixed_rows[-1:], mixed_rows, []]
    for sample, expected in zip(native['samples'], expected_sets):
        assert len(sample['rewards']) == len(expected)
        image = NATIVE_DIR / sample['png']
        assert image.is_file() and sha(image) == sample['sha256']
        assert Image.open(image).size == (1440, 810)
        assert_rect(sample['mailViewport']['screenRect'], (547, 128, 830, 590), .1)
        for index, (actual, row) in enumerate(zip(sample['rewards'], expected)):
            assert actual['index'] == index and actual['id'] == row['id']
            assert actual['quantity'] == row['quantity'] and actual['quantityText'] == row['quantity_text']
            assert actual['name'] == row['name'] and actual['iconReference'] == row['icon']
            assert actual['kind'] == ('currency', 'soldier', 'item')[row['kind']]
            assert actual['tipEnabled'] == (row['kind'] != 0)
            assert actual['tipOffsetLocalY'] == (70 if row['kind'] != 0 else 0)
            assert actual['countLabel']['fontSize'] == 26 and actual['nameLabel']['fontSize'] == 18
            assert actual['countLabel']['fontName'] == 'Arial'
            assert actual['countLabel']['fontStyle'] == 'Bold'
            assert actual['countLabel']['generatorFontSizeUsed'] == 26
            assert abs(actual['countLabel']['lossyScale']['x'] /
                       actual['gift']['lossyScale']['x'] - .7) < .001
            assert_rect(actual['gift']['screenRect'],
                        (572 + (index % 5) * 150, 365 + (index // 5) * 170, 130, 140), .1)
            if row['kind'] == 0:
                assert actual['icon']['spriteName'] == f'img_com_cur{row["id"] - 99}'
                assert_rect(actual['icon']['screenRect'],
                            (609 + (index % 5) * 150, 386.7 + (index // 5) * 170, 56, 56), .1)
                assert not actual['qualityBackground']['activeInHierarchy']
            elif row['kind'] == 1:
                assert actual['icon']['spriteName'] == 'troops_head1_4'
                assert not actual['qualityBackground']['activeInHierarchy']
                assert actual['clickMeaning'].startswith('HelpTip.CreateTip(')
            else:
                assert actual['icon']['spriteName'] == 'item_5_1031'
                assert actual['qualityBackground']['spriteName'] == 'img_item_bg4'
                assert actual['qualityBackground']['activeInHierarchy']
                assert actual['clickMeaning'].startswith('HelpTip.CreateTip(')
        if sample['fixtureKey'] == 'mixed_audit':
            assert_rect(sample['giftGrid']['screenRect'], (552, 365, 820, 310), .1)
        elif sample['fixtureKey'] == 'reference_single':
            assert_rect(sample['giftGrid']['screenRect'], (552, 365, 820, 140), .1)
        else:
            assert_rect(sample['giftGrid']['screenRect'], (552, 365, 820, 0), .1)
    assert [tip['rewardKind'] for tip in native['tipSamples']] == ['item', 'soldier']
    for sample in native['samples']:
        claim_label = native_node(sample,
            '/pl_view/content/mailContent/sv_haveMail/v/c/UI_Item_MailTypeSystem/pl_btn/btn_receive/btn_languageButton/lbl_Text')
        assert claim_label['fontBestFit'] and claim_label['fontSize'] == 32
        assert claim_label['generatorFontSizeUsed'] == 30
    for tip, row in zip(native['tipSamples'], (mixed_rows[6], mixed_rows[5])):
        assert tip['rewardIndex'] == (6 if row['kind'] == 2 else 5)
        assert tip['rewardText'] == tip['textLabel']['text'] == row['name']
        assert tip['offsetLocal'] == 70 and tip['textLabel']['fontSize'] == 18
        assert tip['priorTipClosed'] == (row['kind'] == 1)
        image = NATIVE_DIR / tip['png']
        assert image.is_file() and sha(image) == tip['sha256']
        assert Image.open(image).size == (1440, 810)
        gift = rect(tip['rewardGift']['screenRect'])
        assert abs(tip['sourceGiftCenterScreen']['x'] - (gift[0] + gift[2]/2)) <= .1
        assert abs(tip['sourceGiftCenterScreen']['y'] - (gift[1] + gift[3]/2)) <= .1
        assert_rect(tip['background']['screenRect'],
                    (731, 463, 112, 60) if row['kind'] == 2 else (599, 463, 76, 60), .1)
        assert_rect(tip['arrowDown']['screenRect'],
                    (779.5, 521.8, 15, 12) if row['kind'] == 2 else (629.5, 521.8, 15, 12), .1)
    return ({'native_evidence_sha256': sha(NATIVE),
             'native_verified_sha256': sha(NATIVE_VERIFY),
             'scope': check['scope']}, native)


def source_claim_boundary():
    ui_path = ROOT / 'Source/Rok/RokUnityFeatureScreenWidget.cpp'
    text = ui_path.read_text(encoding='utf-8-sig')
    claim_start = text.index('if (ActionId == 240 || ActionId == 241')
    claim_end = text.index('\n\t}', claim_start)
    branch = text[claim_start:claim_end]
    assert 'LastMailBoundaryActionId = ActionId;' in branch
    assert 'ShowBoundaryFeedback(' in branch and '未改变邮件或奖励状态' in branch
    assert 'return;' in branch
    assert all(token not in branch for token in ('Email_TakeEnclosure', 'Request(', 'Grant', 'Inventory'))
    feedback_start = text.index('void URokUnityFeatureScreenWidget::ShowBoundaryFeedback(')
    feedback_end = text.index('\n}', feedback_start)
    feedback = text[feedback_start:feedback_end]
    assert 'FeedbackText->SetText(Message)' in feedback
    assert all(token not in feedback for token in ('Request(', 'Grant', 'Inventory'))
    return sha(ui_path)


def changed_pixels(first, second, area, minimum=100):
    x, y, w, h = (round(v) for v in rect(area))
    x0, y0 = max(x, 0), max(y, 0)
    x1, y1 = min(x + w, first.shape[1]), min(y + h, first.shape[0])
    assert x1 > x0 and y1 > y0
    delta = np.max(np.abs(first[y0:y1, x0:x1] - second[y0:y1, x0:x1]), axis=2)
    changed = int((delta >= 8).sum())
    assert changed > minimum, (area, changed)
    return changed


def stable_pixels(first, second, area, maximum=100):
    x, y, w, h = (round(v) for v in rect(area))
    x0, y0 = max(x, 0), max(y, 0)
    x1, y1 = min(x + w, first.shape[1]), min(y + h, first.shape[0])
    assert x1 > x0 and y1 > y0
    delta = np.max(np.abs(first[y0:y1, x0:x1] - second[y0:y1, x0:x1]), axis=2)
    changed = int((delta >= 8).sum())
    assert changed <= maximum, (area, changed)
    return changed


def white_ink_bbox(image, source):
    # The source TextGenerator quad bounds confine the search. Compare actual
    # white ink pixels, not the layout rect occupied by a text widget.
    assert all(source['color'][key] >= .99 for key in ('r', 'g', 'b'))
    x, y, width, height = rect(source['generatorVertexBoundsScreen'])
    assert width > 0 and height > 0
    x0, y0 = max(0, int(x) - 5), max(0, int(y) - 18)
    x1 = min(image.shape[1], int(x + width) + 6)
    y1 = min(image.shape[0], int(y + height) + 19)
    yy, xx = np.where(np.all(image[y0:y1, x0:x1] >= 220, axis=2))
    assert len(xx) >= 12, (source['path'], len(xx))
    return {'x': int(xx.min() + x0), 'y': int(yy.min() + y0),
            'width': int(xx.max() - xx.min() + 1),
            'height': int(yy.max() - yy.min() + 1), 'pixels': int(len(xx))}


def compare_white_ink(source_image, ue_image, source):
    native_box = white_ink_bbox(source_image, source)
    ue_box = white_ink_bbox(ue_image, source)
    native_mid = native_box['y'] + native_box['height'] / 2
    ue_mid = ue_box['y'] + ue_box['height'] / 2
    assert abs(native_mid - ue_mid) <= 3, (source['path'], native_box, ue_box)
    return {'native': native_box, 'ue': ue_box,
            'vertical_center_delta': ue_mid - native_mid}


def color_core_pixels(image, area, rgb):
    # Count only fully opaque, exact-color interiors of glyphs. Antialiased
    # edge pixels are intentionally outside this comparison.
    x, y, width, height = rect(area)
    x0, y0 = max(0, int(x)), max(0, int(y))
    x1 = min(image.shape[1], int(x + width + 1))
    y1 = min(image.shape[0], int(y + height + 1))
    assert x1 > x0 and y1 > y0, area
    return int(np.all(image[y0:y1, x0:x1] == rgb, axis=2).sum())


def verify(run):
    run = run.resolve()
    assert run.parent == RUNS.resolve()
    path = run / 'manifest.json'
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    assert data['schema'] == 'rok.mail_reward_detail.runtime.v1'
    assert data['complete'] and data['passed'] and data['failures'] == []
    assert data['audit_flag'] == '-RokMailRewardDetailAudit'
    assert Path(data['project']).resolve() == ROOT.resolve()
    assert data['world'] == '/Game/Maps/L_RokUnityGameAppCity'
    assert data['capture_includes_slate_ui'] and data['expected_capture_count'] == len(STEMS)
    assert data['inventory_authority'] == 'unsynced_local_projection_only'
    assert data['fixture_sha256'] == sha(FIXTURE)
    assert data['screen_fixture_fingerprint']
    assert data['claim_reviewed_ui_sha256'] == source_claim_boundary()
    font = data['local_count_font']
    local_font = Path(os.environ['WINDIR']) / 'Fonts/arialbd.ttf'
    assert Path(font['path']).resolve() == local_font.resolve()
    assert local_font.stat().st_size == font['bytes'] and sha(local_font) == font['sha256']
    assert 'not proof of Unity glyph raster equivalence' in font['scope']
    baseline = data['baseline']
    assert baseline['owned_build_queues'] >= 0 and baseline['active_upgrades'] == 0
    for field in ('resources_sha256', 'profile_sha256'):
        assert len(baseline[field]) == 64 and all(c in '0123456789abcdef' for c in baseline[field])
    assert baseline['troop_stack_sha256'] and all(len(value) == 64 for value in baseline['troop_stack_sha256'])
    assert len(baseline['building_state_sha256']) >= 6
    assert all(len(value) == 64 for value in baseline['building_state_sha256'].values())
    package, mixed_rows = fixture_source()
    native_evidence, native = native_source(mixed_rows)
    for relative, info in data['input_hashes'].items():
        file = resolve(relative).resolve()
        assert file.is_relative_to(ROOT.resolve()) and file.is_file(), relative
        assert file.stat().st_size == info['bytes'] and sha(file) == info['sha256'], relative
    assert len(data['input_hashes']) >= 10
    assert [(action['button'], action['method']) for action in data['actions']] == EXPECTED_ACTIONS
    virtual = [action for action in data['actions'] if action['method'].startswith('FSlateApplication.')]
    assert len(virtual) == 6
    for action in virtual:
        assert action['slate_hit_path_valid']
        assert action['screen_in_hit_path'] and len(action['slate_widget_path_types']) >= 3
        if action['button'] == 'MailTooltipArrowSideLeft':
            assert not action['target_in_hit_path']
            assert not action['body_guard_in_hit_path'] and not action['arrow_guard_in_hit_path']
            assert action['right_arrow_side_uncovered']
        else:
            assert action['target_in_hit_path']
            if action['button'] != 'MailTooltipBlankBlocker':
                assert action['target_hovered_after_move']
            assert action['down_handled'] and action['up_handled']
        if action['button'] == 'MailRewardTooltipBodyHit':
            assert action['body_guard_in_hit_path']
        if action['button'] == 'MailRewardTooltipArrowHit':
            assert action['arrow_guard_in_hit_path']
    claim = [action for action in data['actions'] if action['button'] == 'MailClaim']
    assert len(claim) == 1 and claim[0]['claim_local_feedback_source_reviewed']
    assert len(data['captures']) == len(STEMS)
    images = []
    artifacts = []
    for index, capture in enumerate(data['captures']):
        file = Path(capture['file']).resolve()
        assert file.parent == run and file.stem == STEMS[index]
        assert file.stat().st_size == capture['file_bytes'] and sha(file) == capture['png_sha256']
        image = np.asarray(Image.open(file).convert('RGB'), dtype=np.int16)
        assert image.shape == (810, 1440, 3)
        images.append(image)
        artifacts.append({'name': file.name, 'sha256': capture['png_sha256']})
        for stage in ('request', 'processed'):
            state = capture[stage]
            assert state['page'] == PAGES[index] and state['step'] == STEPS[index]
            assert state['state_unchanged']
            assert_rect(state['viewport_pixels'], (0, 0, 1440, 810), .1)
            if state['page'] == 'closed':
                assert 'rendered_rows' not in state
                continue
            assert state['mail_mode']
            assert state['inventory_projection_fingerprint'] == \
                   baseline['inventory_projection_fingerprint']
            sample_index = 1 if state['page'] == 'mixed_fixture' else 2 if state['page'] == 'empty_alternate' else 0
            assert_mail_window(state, native['samples'][sample_index])
            if state['page'] == 'normal_empty':
                assert not state['explicit_reward_fixture']
                assert state['rendered_rows'] == []
                assert visible_geometry(state['empty_state'])
                assert not state['fixture_notice']['attached']
                assert state['selected_tab'] == (1 if index == 1 else 0)
            elif state['page'] == 'empty_alternate':
                assert state['explicit_reward_fixture'] and state['selected_mail_index'] == 1
                assert state['screen_fixture_fingerprint'] == data['screen_fixture_fingerprint']
                assert state['rendered_rows'] == []
                assert state['active_tooltip_index'] == -1
                assert visible_geometry(state['fixture_notice'])
                assert not state['reward_cells'][0]['root']['attached']
                assert state['reward_grid']['attached']
                assert not state['reward_empty']['attached']
                assert state['detail_text']['MailDetailTitle']['text'] == package['mail_template']['title']
                assert not state['detail_text']['MailDetailGreeting']['attached']
            elif state['page'] == 'reference_single':
                assert not state['explicit_reward_fixture'] and state['selected_mail_index'] == 0
                assert state['rendered_rows'] == [mixed_rows[-1]]
                assert visible_geometry(state['fixture_notice'])
                assert visible_geometry(state['legacy_reward'])
                assert visible_geometry(state['legacy_reward_name'])
                assert state['legacy_reward_name']['text'] == package['metadata']['item']['name']
                assert state['legacy_reward_count']['text'] == '3'
                source_count = native['samples'][0]['rewards'][0]['countLabel']
                source_gift = native['samples'][0]['rewards'][0]['gift']
                count_scale = abs(source_count['lossyScale']['x'] /
                                  source_gift['lossyScale']['x'])
                assert abs(state['legacy_reward_count']['font_points'] -
                           source_count['generatorFontSizeUsed'] * count_scale * .75) <= .01
                assert Path(state['legacy_reward_count']['font_filename']).resolve() == local_font.resolve()
                assert all(abs(state['legacy_reward_count']['display_srgb'][key] -
                               source_count['color'][key] * 255) <= 1
                           for key in ('r', 'g', 'b', 'a'))
                assert_reward_icon({'icon': state['legacy_reward_icon'],
                                    'quality': state['legacy_reward_quality']}, mixed_rows[-1])
                assert state['legacy_attachment_pulse']['attached']
                assert state['active_tooltip_index'] == -1
            else:
                assert state['explicit_reward_fixture'] and state['selected_mail_index'] == 0
                assert state['screen_fixture_fingerprint'] == data['screen_fixture_fingerprint']
                assert state['rendered_rows'] == mixed_rows
                assert visible_geometry(state['fixture_notice'])
                assert visible_geometry(state['detail_scroll'])
                assert visible_geometry(state['detail_content'])
                assert len(state['reward_cells']) == 7
                for row, cell in zip(mixed_rows, state['reward_cells']):
                    assert visible_geometry(cell['root']) and visible_geometry(cell['icon'])
                    assert cell['quantity']['text'] == row['quantity_text']
                    assert cell['name']['text'] == row['name']
                    assert cell['quality']['visible'] == (row['kind'] == 2)
                    assert cell['hit']['attached'] == (row['kind'] != 0)
                    slot_index = cell['index']
                    assert_rect(cell['root']['slot_rect'],
                                (20 + (slot_index % 5) * 150, (slot_index // 5) * 170, 130, 140), .1)
                    icon_rect = (37, 21.7, 56, 56) if row['kind'] == 0 else (23, 7.7, 84, 84)
                    assert_rect(cell['icon']['slot_rect'], icon_rect, .1)
                    count_rect = (23.28, 74.27, 80.22, 21) if row['kind'] == 0 else (22.93, 71.47, 80.22, 21)
                    assert_rect(cell['quantity']['slot_rect'], count_rect, .1)
                    assert_rect(cell['name']['slot_rect'], (-5, 112.9, 140, 46.1), .1)
                    source_reward = native['samples'][sample_index]['rewards'][slot_index]
                    source_count = source_reward['countLabel']
                    count_scale = abs(source_count['lossyScale']['x'] /
                                      source_reward['gift']['lossyScale']['x'])
                    assert abs(cell['quantity']['font_points'] -
                               source_count['generatorFontSizeUsed'] * count_scale * .75) <= .01
                    assert Path(cell['quantity']['font_filename']).resolve() == local_font.resolve()
                    assert all(abs(cell['quantity']['display_srgb'][key] -
                                   source_count['color'][key] * 255) <= 1
                               for key in ('r', 'g', 'b', 'a'))
                    assert_source_text(cell['name'], source_reward['nameLabel'])
                    if row['kind'] == 2:
                        assert_rect(cell['quality']['slot_rect'], (18.1, 2.8, 93.8, 93.8), .1)
                    if row['kind'] != 0:
                        assert_rect(cell['hit']['slot_rect'], (18.1, 2.8, 93.8, 93.8), .1)
                    assert_reward_icon(cell, row)
                if index in (4, 5, 6, 7, 8, 9, 12, 13, 14):
                    assert state['detail_scroll_offset'] > 80
                else:
                    assert state['detail_scroll_offset'] <= 1
                scroll = rect(state['detail_scroll']['pixel_rect'])
                last = rect(state['reward_cells'][6]['root']['pixel_rect'])
                assert last[1] >= scroll[1] - .5 and last[1] + last[3] <= scroll[1] + scroll[3] + .5
                claim_rect = rect(state['claim_button']['pixel_rect'])
                if index in (4, 5, 6, 7, 8, 9, 12, 13, 14):
                    assert claim_rect[1] >= scroll[1] - .5
                    assert claim_rect[1] + claim_rect[3] <= scroll[1] + scroll[3] + .5
                else:
                    assert claim_rect[1] >= scroll[1] + scroll[3]
                expected_tip = 6 if index in (5, 6, 7, 8, 13) else 5 if index == 9 else -1
                assert state['active_tooltip_index'] == expected_tip
                assert visible_geometry(state['tooltip_root']) == (expected_tip >= 0)
                if expected_tip >= 0:
                    assert state['tooltip_text']['text'] == mixed_rows[expected_tip]['name']
                    root = rect(state['reward_cells'][expected_tip]['root']['pixel_rect'])
                    anchor = rect(state['tooltip_anchor'])
                    assert abs(anchor[0] - (root[0] + root[2] / 2)) <= .5
                    assert abs(anchor[1] - root[1]) <= .5
                    native_tip = native['tipSamples'][0 if expected_tip == 6 else 1]
                    offset = state['detail_scroll_offset']
                    assert_source_geometry(state['tooltip_background'], native_tip['background'], y_shift=offset)
                    assert_source_geometry(state['tooltip_arrow'], native_tip['arrowDown'], y_shift=offset)
                    assert_source_geometry(state['tooltip_text'], native_tip['textLabel'], y_shift=offset)
                    assert visible_geometry(state['tooltip_body_hit'])
                    assert visible_geometry(state['tooltip_arrow_hit'])
                    assert_rect(state['tooltip_body_hit']['pixel_rect'],
                                rect(state['tooltip_background']['pixel_rect']), 1.0)
                    assert_rect(state['tooltip_arrow_hit']['pixel_rect'],
                                rect(state['tooltip_arrow']['pixel_rect']), 1.0)
                    assert state['tooltip_text']['font_points'] == 13.5
                else:
                    assert not state['tooltip_body_hit']['attached']
                    assert not state['tooltip_arrow_hit']['attached']
                if index == 12:
                    assert state['last_mail_boundary_action_id'] == 240
                    assert visible_geometry(state['feedback'])
                    assert '未改变邮件或奖励状态' in state['feedback']['text']
            if state['page'] in ('mixed_fixture', 'empty_alternate', 'reference_single'):
                assert all(row['attached'] and row['text'] == '' for row in state['mail_row_dates'])
                assert state['mail_row_titles'][0]['text'] == package['mail_template']['title']
                assert state['mail_row_subjects'][0]['text'] == package['mail_template']['subtitle']
            if state['page'] in ('mixed_fixture', 'reference_single'):
                template = package['mail_template']
                detail = state['detail_text']
                assert detail['MailDetailTitle']['text'] == template['title']
                assert detail['MailDetailSub']['text'] == template['subtitle']
                assert detail['MailDetailGreeting']['text'] == template['body_greeting']
                assert detail['MailDetailPrefix']['text'] == template['body_prefix']
                assert detail['MailDetailHighlight']['text'] == template['body_activity_quoted']
                assert (detail['MailDetailSuffix']['text'] + detail['MailDetailFinalLine']['text']) == template['body_suffix']
                assert state['claim_button']['attached']
    panel = data['captures'][3]['processed']['detail_scroll']['pixel_rect']
    visual = {
        'mixed_entry': changed_pixels(images[0], images[3], panel, 200),
        'detail_scroll_to_claim': changed_pixels(images[3], images[4], panel, 200),
        'item_tip': changed_pixels(images[4], images[5],
                                   data['captures'][5]['processed']['tooltip_root']['pixel_rect'], 50),
        'tip_body_stable': stable_pixels(images[5], images[6],
                                        data['captures'][5]['processed']['tooltip_background']['pixel_rect'], 100),
        'tip_arrow_stable': stable_pixels(images[5], images[7],
                                         data['captures'][5]['processed']['tooltip_background']['pixel_rect'], 100),
        'blank_stable': stable_pixels(images[5], images[8],
                                      data['captures'][5]['processed']['tooltip_background']['pixel_rect'], 100),
        'troop_tip': changed_pixels(images[8], images[9],
                                    data['captures'][9]['processed']['tooltip_root']['pixel_rect'], 50),
        'row_switch_empty': changed_pixels(images[3], images[10], panel, 200),
        'tip_after_claim': changed_pixels(images[12], images[13],
                                          data['captures'][13]['processed']['tooltip_background']['pixel_rect'], 50),
        'arrow_side_closes_tip': changed_pixels(images[13], images[14],
                                                data['captures'][13]['processed']['tooltip_background']['pixel_rect'], 50),
    }
    ink_alignment = {}
    footer_paths = (
        '/pl_view/buttom/pl_all/UI_Item_MailBtnOnButtomDeleteRead',
        '/pl_view/buttom/pl_all/UI_Item_MailBtnOnButtomRead',
        '/pl_view/buttom/group/UI_Item_MailBtnOnButtomCollect',
        '/pl_view/buttom/group/UI_Item_MailBtnOnButtomDel',
    )
    for label, ue_index, native_index in (('mixed', 3, 1), ('reference', 17, 0)):
        sample = native['samples'][native_index]
        source_image = np.asarray(Image.open(NATIVE_DIR / sample['png']).convert('RGB'), dtype=np.int16)
        selected_tab = native_node(sample,
            '/pl_view/title/pageRect/UI_Item_MailPageBtnSys/lbl_languageText')
        ink_alignment[f'{label}_selected_tab'] = compare_white_ink(
            source_image, images[ue_index], selected_tab)
        for footer_index, footer_path in enumerate(footer_paths):
            source_label = native_node(sample, footer_path + '/btn_newMail/text')
            ink_alignment[f'{label}_footer_{footer_index}'] = compare_white_ink(
                source_image, images[ue_index], source_label)
    source_color_core = {}
    for label, ue_index, native_index in (('mixed', 3, 1), ('reference', 17, 0),
                                          ('reference_reopened', 19, 0)):
        sample = native['samples'][native_index]
        source_image = np.asarray(Image.open(NATIVE_DIR / sample['png']).convert('RGB'), dtype=np.int16)
        area = data['captures'][ue_index]['processed']['detail_text']['MailDetailHighlight']['pixel_rect']
        native_green = color_core_pixels(source_image, area, (0, 128, 0))
        ue_green = color_core_pixels(images[ue_index], area, (0, 128, 0))
        ue_neon = color_core_pixels(images[ue_index], area, (0, 255, 0))
        assert native_green >= 100 and ue_green >= 100 and ue_neon == 0, \
            (label, native_green, ue_green, ue_neon)
        source_color_core[f'{label}_body_green'] = {
            'roi': area, 'native_rgb': [0, 128, 0], 'native_core_pixels': native_green,
            'ue_core_pixels': ue_green, 'ue_neon_green_pixels': ue_neon,
        }
    for label, ue_index, row_index in (('mixed', 3, 1), ('empty_selected', 10, 0),
                                       ('reference', 17, 1)):
        area = data['captures'][ue_index]['processed']['mail_row_titles'][row_index]['pixel_rect']
        pixels = color_core_pixels(images[ue_index], area, (226, 209, 155))
        assert pixels >= 25, (label, pixels, area)
        source_color_core[f'{label}_unselected_title'] = {
            'row_index': row_index, 'roi': area, 'source_rgb': [226, 209, 155],
            'ue_core_pixels': pixels,
            'source_style_scope': 'Native unselected probe is offscreen; color and font metadata checked above.',
        }
    result = {
        'schema': 'rok.mail_reward_detail.runtime_verify.v1',
        'passed': True,
        'manifest_sha256': sha(path),
        'fixture_sha256': sha(FIXTURE),
        **native_evidence,
        'artifacts': artifacts,
        'visual_changed_pixels': visual,
        'source_text_ink_alignment': ink_alignment,
        'source_text_color_core': source_color_core,
        'scope': 'Actual HUD normal empty mail and explicitly labelled local fixture reward presentation, tooltip and local claim feedback. UI delegate broadcasts plus bounded virtual Slate hit routes; no hardware pointer, account mailbox, inventory authority, grant or request.',
    }
    out = run / 'runtime_verify.json'
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(out)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('run', type=Path)
    verify(parser.parse_args().run)
