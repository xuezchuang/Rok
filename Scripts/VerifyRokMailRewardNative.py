"""Verify source hashes and an actual isolated Unity mail-reward native capture."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'Saved/UnityMailRewardNativeProbe'
STAGE = ROOT / 'Saved/RokUnityMigration/MailRewardNative'
DATA = DATA_ROOT / 'mail_reward_presentation.json'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def valid_rect(node, visible=True):
    assert node and isinstance(node['path'], str) and node['path']
    rect = node['screenRect']
    assert all(math.isfinite(rect[k]) for k in ('x', 'y', 'width', 'height')), node['path']
    assert rect['width'] >= 0 and rect['height'] >= 0, node['path']
    if visible:
        assert node['activeInHierarchy'] and rect['width'] > 0 and rect['height'] > 0, node['path']
    assert len(node['screenCorners']) == 4
    return rect


def close(a, b, tol=.6):
    assert abs(a - b) <= tol, (a, b)


def text_metric(node):
    assert node['fontName'] == 'Arial' and node['fontDynamic']
    assert node['fontSize'] > 0 and node['generatorFontSizeUsed'] > 0, node['path']
    assert node['generatorCharacterCountVisible'] > 0, node['path']
    assert node['generatorVertexCount'] > 0 and node['generatorVertexCount'] % 4 == 0, node['path']
    assert len(node['generatorVertices']) == node['generatorVertexCount'], node['path']
    assert node['generatorVertexBoundsLocal']['width'] > 0
    assert node['generatorVertexBoundsScreen']['width'] > 0
    if node['fontBestFit']:
        assert node['fontMinSize'] <= node['generatorFontSizeUsed'] <= node['fontMaxSize']
    return {'path': node['path'], 'text': node['text'], 'font_name': node['fontName'],
            'font_asset_path': node['fontAssetPath'], 'font_size': node['fontSize'],
            'font_size_used': node['generatorFontSizeUsed'], 'font_style': node['fontStyle'],
            'font_best_fit': node['fontBestFit'], 'font_min_size': node['fontMinSize'],
            'font_max_size': node['fontMaxSize'], 'alignment': node['alignment'],
            'color': node['color'], 'local_scale': node['localScale'],
            'lossy_scale': node['lossyScale'], 'screen_rect': node['screenRect'],
            'vertex_count': node['generatorVertexCount'],
            'vertex_bounds_local': node['generatorVertexBoundsLocal'],
            'vertex_bounds_screen': node['generatorVertexBoundsScreen']}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('artifact', nargs='?', type=Path)
    parser.add_argument('--prepared-only', action='store_true')
    args = parser.parse_args()
    inputs = json.loads((PROJECT / 'ReferenceInputs.json').read_text(encoding='utf-8'))
    fixture = json.loads(DATA.read_text(encoding='utf-8'))
    assert inputs['schema'] == 'rok.mail_reward.native_inputs.v1'
    assert fixture['schema'] == 'rok.mail_reward_presentation.v1'
    assert inputs['fixtureSourceSha256'] == sha(DATA)
    assert [r['key'] for r in inputs['fixtures']] == ['reference_single', 'mixed_audit', 'empty_alternate']
    assert [len(r['rows']) for r in inputs['fixtures']] == [1, 7, 0]
    for src in inputs['sources']:
        assert sha(resolve(src['path'])) == src['sha256'], src['path']
    for copied in inputs['byte_identical_copies']:
        assert sha(resolve(copied['source'])) == sha(PROJECT / copied['target']) == copied['sha256'], copied['target']
    for adapter in inputs['adapters']:
        assert sha(PROJECT / adapter['target']) == adapter['sha256'], adapter['target']
    assert json.loads((PROJECT / 'Validation/compiler_result.json').read_text(encoding='utf-8'))['exit_code'] == 0
    expected_icons = ['ui_common[img_com_cur1]', 'ui_common[img_com_cur2]',
                      'ui_common[img_com_cur3]', 'ui_common[img_com_cur4]',
                      'ui_common[img_com_cur5]', 'troops_head[troops_head1_4]', 'item5[item_5_1031]']
    assert [r['icon'] for r in inputs['fixtures'][1]['rows']] == expected_icons
    assert inputs['qualitySprite'] == 'item0[img_item_bg4]'
    assert inputs['tipPrefab'].endswith('/UI_Pop_TextTip.prefab')
    assert inputs['tipContract'] == {'style': 'arrowDown', 'sourceUiLayer': 'WindowPopLayer',
                                     'rewardIndexesInMixed': [6, 5], 'offsetLocal': 70,
                                     'method': 'HelpTip.CreateTip(text,gift.transform) center->Canvas local; HelpTipMediator.SetTipString; SetOffset; each Show closes prior tip'}
    template = inputs['mailTemplate']
    assert (template['mailDefineId'], template['activityCalendarId'], template['activityLocaleId']) == (100030, 10101, 300229)
    assert (template['nameLocaleId'], template['subtitleLocaleId'], template['bodyLocaleId'], template['claimLocaleId']) == (762137, 762138, 550111, 570008)
    assert template['title'] == '活动奖励补发' and template['subtitle'] == '征服之始-领取宝箱活动奖励补发'
    assert template['claim'] == '领取' and template['sendTime'] == ''
    assert template['listIcon'] == 'ui_mail[icon_mail_1004]'
    assert template['poster'] == 'ui_mail[img_mail_title_1]'
    assert '{activity_0}' not in template['body'] and '尊敬的城主' in template['body']
    localized = {row['id']: row['text'] for row in inputs['texts']}
    footer_locales = {570016: '一键已读&领取', 570017: '删除已读', 570018: '删除',
                      570019: '收藏', 570021: '翻译', 570022: '回复'}
    assert all(localized.get(key) == value for key, value in footer_locales.items())
    if args.prepared_only:
        print(json.dumps({'prepared_verified': True, 'native_capture': False, 'source_count': len(inputs['sources']),
                          'copies': len(inputs['byte_identical_copies']), 'fixture_rows': [1, 7, 0]}))
        return
    artifacts = sorted((PROJECT / 'Artifacts').glob('native_*'))
    artifact = args.artifact or (artifacts[-1] if artifacts else None)
    assert artifact and (artifact / 'render_evidence.json').exists(), 'Unity native capture missing'
    evidence = json.loads((artifact / 'render_evidence.json').read_text(encoding='utf-8'))
    assert evidence['schema'] == 'rok.mail_reward.native.v1' and evidence['complete'], evidence.get('failure')
    assert not evidence.get('failure')
    assert evidence['unityVersion'] == '2019.4.28f1c1' and evidence['playMode']
    assert evidence['width'] == evidence['cameraPixelWidth'] == 1440
    assert evidence['height'] == evidence['cameraPixelHeight'] == 810
    assert evidence['inputSha256'] == sha(artifact / 'ReferenceInputs.json') == sha(PROJECT / 'ReferenceInputs.json')
    assert evidence['fixtureSourceSha256'] == sha(DATA)
    assert evidence['mailTemplate'] == template
    for key in ('originalUiRoot', 'originalMailWindow', 'originalSystemItem', 'originalMailGift', 'claimNotInvoked', 'serverNotInvoked'):
        assert evidence[key], key
    assert [s['fixtureKey'] for s in evidence['samples']] == [r['key'] for r in inputs['fixtures']]
    assert len(evidence['tipSamples']) == 2
    try:
        from PIL import Image
    except ImportError:
        Image = None
    summaries = []
    for sample, fixture_case in zip(evidence['samples'], inputs['fixtures']):
        assert sample['rowCount'] == len(sample['rewards']) == len(fixture_case['rows'])
        assert sample['windowAnimatorState'] == 'Open' and sample['windowAnimatorNormalizedTime'] >= .99
        assert sample['canvasScaleFactor'] > 0
        states = {n['path'].split('/')[-1]: n for n in sample['selectedMailState']}
        assert set(states) == {'pl_mailNone', 'pl_mailcontentbg', 'sv_haveMail', 'UI_Item_MailFightReport',
                               'lbl_Content_link', 'lbl_Content', 'lbl_Sender', 'pl_res', 'pl_gift',
                               'pl_already', 'btn_receive', 'lbl_Text',
                               'UI_Item_MailBtnOnButtomDeleteRead', 'UI_Item_MailBtnOnButtomRead',
                               'UI_Item_MailBtnOnButtomDel', 'UI_Item_MailBtnOnButtomCollect',
                               'UI_Item_MailBtnOnButtomTrans', 'UI_Item_MailBtnOnButtomRe'}
        for key in ('pl_mailcontentbg', 'sv_haveMail', 'lbl_Content', 'pl_gift', 'btn_receive', 'lbl_Text',
                    'UI_Item_MailBtnOnButtomDeleteRead', 'UI_Item_MailBtnOnButtomRead',
                    'UI_Item_MailBtnOnButtomDel', 'UI_Item_MailBtnOnButtomCollect'):
            assert states[key]['activeInHierarchy'], (sample['fixtureKey'], key)
        for key in ('pl_mailNone', 'UI_Item_MailFightReport', 'lbl_Content_link', 'lbl_Sender', 'pl_res', 'pl_already',
                    'UI_Item_MailBtnOnButtomTrans', 'UI_Item_MailBtnOnButtomRe'):
            assert not states[key]['activeInHierarchy'], (sample['fixtureKey'], key)
        tabs = sample['pageTabState']
        assert len(tabs) == 18
        for i in range(6):
            root_tab, active_image, label = tabs[i * 3:(i + 1) * 3]
            assert root_tab['activeInHierarchy'] and root_tab['path'] + '/img_active' == active_image['path']
            assert root_tab['path'] + '/lbl_languageText' == label['path']
            assert active_image['activeInHierarchy'] == (i == 0)
            assert abs(label['color']['r'] - (1 if i == 0 else 164 / 255)) < .002
            assert label['fontName'] == 'Arial' and label['fontSize'] == 22
            assert label['fontStyle'] == 'Normal' and label['alignment'] == 'MiddleCenter'
        assert states['lbl_Content']['text'] == template['body']
        assert states['lbl_Text']['text'] == template['claim']
        labels = {n['path'].split('/')[-1]: n for n in sample['systemGeometry'] if '/UI_Item_MailTitle/' in n['path']}
        assert labels['lbl_title']['text'] == template['title']
        assert labels['lbl_desc']['text'] == template['subtitle']
        assert labels['lbl_time']['text'] == template['sendTime']
        assert labels['lbl_title']['fontName'] == 'Arial'
        assert labels['lbl_title']['fontSize'] == 26 and labels['lbl_title']['fontStyle'] == 'Bold'
        assert labels['lbl_title']['alignment'] == 'LowerLeft'
        poster = labels['img_bg']
        assert poster['activeInHierarchy'] and poster['spriteName'] == 'img_mail_title_1'
        assert poster['spriteAssetPath'].endswith('ui_mail.png')
        valid_rect(poster)
        close(poster['screenRect']['width'], 821, .25)
        close(poster['screenRect']['height'], 123, .25)
        window_title = next(n for n in sample['windowGeometry']
                            if n['path'].endswith('/UI_Win_Mail/pl_view/win/lbl_title'))
        assert window_title['fontName'] == 'Arial' and window_title['fontSize'] == 30
        assert window_title['fontStyle'] == 'Normal' and window_title['alignment'] == 'MiddleCenter'
        footer = {key: next(n for n in sample['windowGeometry'] if n['path'].endswith('/' + key))
                  for key in ('UI_Item_MailBtnOnButtomDeleteRead', 'UI_Item_MailBtnOnButtomRead',
                              'UI_Item_MailBtnOnButtomCollect', 'UI_Item_MailBtnOnButtomDel')}
        for key, locale_id in (('UI_Item_MailBtnOnButtomDeleteRead', 570017),
                               ('UI_Item_MailBtnOnButtomRead', 570016),
                               ('UI_Item_MailBtnOnButtomCollect', 570019),
                               ('UI_Item_MailBtnOnButtomDel', 570018)):
            label = next(n for n in sample['windowGeometry'] if n['path'] == footer[key]['path'] + '/btn_newMail/text')
            assert label['activeInHierarchy'] and label['text'] == localized[locale_id], (sample['fixtureKey'], key)
            assert label['fontSize'] == 22 and label['fontStyle'] == 'Normal'
        collect_rect = valid_rect(footer['UI_Item_MailBtnOnButtomCollect'])
        del_rect = valid_rect(footer['UI_Item_MailBtnOnButtomDel'])
        close(del_rect['x'] - collect_rect['x'], collect_rect['width'] + 24, .25)
        png = artifact / sample['png']
        assert png.is_file() and sample['sha256'] == sha(png)
        if Image:
            with Image.open(png) as image:
                assert image.size == (1440, 810) and image.mode in ('RGB', 'RGBA')
                assert len(image.getcolors(maxcolors=1_000_000) or []) > 100, 'Native capture is flat/blank'
        for name in ('uiRoot', 'windowLayer', 'mailWindow', 'leftListScroll', 'leftListViewport',
                     'leftListContent', 'leftMailRow', 'mailScroll', 'mailViewport',
                     'mailContent', 'systemItem', 'giftGrid', 'claimButton'):
            valid_rect(sample[name], visible=name not in ('claimButton', 'mailContent') and
                       not (name == 'giftGrid' and sample['fixtureKey'] == 'empty_alternate'))
        grid = sample['giftGrid']
        assert grid['path'].endswith('/UI_Item_MailTypeSystem/pl_gift')
        assert sample['mailScroll']['path'].endswith('/sv_haveMail')
        assert sample['mailViewport']['path'].endswith('/sv_haveMail/v')
        assert sample['leftListScroll']['path'].endswith('/sv_list_view')
        assert sample['leftListViewport']['path'].endswith('/sv_list_view/v')
        assert sample['leftMailRow']['path'].startswith(sample['leftListContent']['path'] + '/')
        assert 450 <= sample['leftMailRow']['screenRect']['width'] <= 465
        assert 110 <= sample['leftMailRow']['screenRect']['height'] <= 120
        left = {n['path'].split('/')[-1]: n for n in sample['leftMailRowGeometry']}
        unselected = {n['path'].split('/')[-1]: n for n in sample['unselectedMailRowGeometry']}
        left_icon = next(n for n in sample['leftMailRowGeometry'] if n['path'] == sample['leftMailRow']['path'] + '/img_icon')
        assert left['img_bgActive']['activeInHierarchy'] and left['img_bag']['activeInHierarchy']
        assert not left['img_redpot']['activeInHierarchy']
        assert left_icon['spriteName'] == 'icon_mail_1004'
        assert left_icon['spriteAssetPath'].endswith('ui_mail.png')
        assert left['lbl_title']['text'] == template['title']
        assert left['lbl_time']['text'] == ''
        assert left['lbl_title']['color']['r'] == 0 and left['lbl_desc']['color']['r'] == 0
        assert not unselected['img_bgActive']['activeInHierarchy']
        assert unselected['lbl_title']['activeInHierarchy'] and unselected['lbl_desc']['activeInHierarchy']
        assert abs(unselected['lbl_title']['color']['r'] - 226 / 255) < .002
        assert abs(unselected['lbl_title']['color']['g'] - 209 / 255) < .002
        assert abs(unselected['lbl_title']['color']['b'] - 155 / 255) < .002
        assert all(unselected['lbl_desc']['color'][channel] == 1 for channel in 'rgb')
        assert unselected['UI_Item_MailBtn_UnselectedProbe']['screenRect']['y'] > 810
        window = sample['windowGeometry']
        assert len(window) > 100 and all(n['path'].startswith(sample['mailWindow']['path']) for n in window)
        names = [n['path'].split('/')[-1] for n in window]
        for required in ('UI_Item_MailPageBtnSys', 'UI_Item_MailPageBtnReport', 'UI_Item_MailPageBtnGuild',
                         'UI_Item_MailPageBtnCollect', 'UI_Item_MailPageBtnPerson', 'UI_Item_MailPageBtnSend',
                         'UI_Item_newMail', 'btn_close', 'sv_list_view', 'sv_haveMail', 'pl_mailcontentbg',
                         'UI_Item_MailBtnOnButtomDeleteRead', 'UI_Item_MailBtnOnButtomRead'):
            assert required in names, required
        assert sample['systemItem']['path'].startswith(sample['mailContent']['path'] + '/')
        assert len(sample['systemGeometry']) > 10
        def one_suffix(nodes, suffix):
            found = [n for n in nodes if n['path'].endswith(suffix)]
            assert len(found) == 1, (sample['fixtureKey'], suffix, len(found))
            return found[0]
        text_nodes = [window_title]
        text_nodes += [tabs[i * 3 + 2] for i in range(6)]
        text_nodes += [one_suffix(window, '/UI_Item_newMail/btn_newMail/text')]
        text_nodes += [left[key] for key in ('lbl_title', 'lbl_desc')]
        text_nodes += [unselected[key] for key in ('lbl_title', 'lbl_desc')]
        text_nodes += [labels[key] for key in ('lbl_title', 'lbl_desc')]
        text_nodes += [states['lbl_Content'], states['lbl_Text']]
        text_nodes += [one_suffix(window, '/' + key + '/btn_newMail/text')
                       for key in ('UI_Item_MailBtnOnButtomDeleteRead', 'UI_Item_MailBtnOnButtomRead',
                                   'UI_Item_MailBtnOnButtomCollect', 'UI_Item_MailBtnOnButtomDel')]
        text_evidence = [text_metric(node) for node in text_nodes]
        records = []
        for index, (item, source) in enumerate(zip(sample['rewards'], fixture_case['rows'])):
            assert item['index'] == index
            for field in ('kind', 'id', 'quantity', 'quality', 'name', 'tipEnabled'):
                assert item[field] == source[field], (sample['fixtureKey'], field)
            assert item['iconReference'] == source['icon']
            gift = valid_rect(item['gift'])
            icon = valid_rect(item['icon'])
            count = valid_rect(item['countLabel'])
            valid_rect(item['nameLabel'])
            assert item['countLabel']['text'] == format(item['quantity'], ',')
            assert item['countLabel']['fontName'] == 'Arial'
            assert item['countLabel']['fontSize'] == 26 and item['countLabel']['fontStyle'] == 'Bold'
            expected_count_color = (1, 1, 1) if source['kind'] == 'currency' else (217 / 255, 212 / 255, 186 / 255)
            assert all(abs(item['countLabel']['color'][channel] - expected) < .002
                       for channel, expected in zip('rgb', expected_count_color))
            assert item['nameLabel']['text'] == item['name']
            assert item['icon']['spriteName'] == source['icon'].split('[')[1][:-1]
            assert item['icon']['spriteAssetPath'].endswith(source['icon'].split('[')[0] + '.png')
            assert 100 <= gift['width'] <= 140 and 110 <= gift['height'] <= 150
            assert icon['width'] > 15 and icon['height'] > 15
            assert count['width'] > 0
            text_evidence.append(text_metric(item['countLabel']))
            text_evidence.append(text_metric(item['nameLabel']))
            close(item['tipAnchorScreenRect']['x'], gift['x'])
            close(item['tipAnchorScreenRect']['y'], gift['y'])
            if source['tipEnabled']:
                assert item['tipText'] == source['name'] and 'HelpTip.CreateTip' in item['clickMeaning']
                close(item['tipOffsetLocalY'], item['gift']['localRect']['height'] / 2, .01)
                assert item['clickButton']['buttonType'].endswith('GameButton')
            else:
                assert not item['tipText'] and item['tipOffsetLocalY'] == 0
            if source['kind'] == 'currency':
                assert not item['qualityBackground'] or not item['qualityBackground']['path']
            else:
                assert item['qualityBackground'] is not None
                assert item['qualityBackground']['activeInHierarchy'] == (source['kind'] == 'item')
                if source['kind'] == 'item':
                    assert item['qualityBackground']['spriteName'] == 'img_item_bg4'
                    assert item['qualityBackground']['spriteAssetPath'].endswith('item0.png')
            records.append({'index': item['index'], 'kind': item['kind'], 'id': item['id'], 'quantity': item['quantity'],
                            'gift_rect': gift, 'icon_rect': icon, 'count_rect': count,
                            'tip_offset_local_y': item['tipOffsetLocalY']})
        # The first five cells occupy one row and sixth/seventh wrap, as source GridLayoutGroup declares.
        if sample['fixtureKey'] == 'mixed_audit':
            xs = [r['gift_rect']['x'] for r in records]
            ys = [r['gift_rect']['y'] for r in records]
            assert all(xs[i + 1] > xs[i] for i in range(4))
            close(ys[0], ys[4]); assert ys[5] > ys[4] and ys[6] == ys[5]
        summaries.append({'fixture': sample['fixtureKey'], 'png': str(png), 'png_sha256': sha(png),
                           'poster': poster['screenRect'],
                          'left_list_scroll': sample['leftListScroll']['screenRect'],
                          'left_list_viewport': sample['leftListViewport']['screenRect'],
                          'left_mail_row': sample['leftMailRow']['screenRect'],
                           'text_evidence': text_evidence,
                          'mail_viewport': sample['mailViewport']['screenRect'],
                          'gift_grid': grid['screenRect'], 'claim': sample['claimButton']['screenRect'],
                          'rows': records})
    tips = []
    mixed = inputs['fixtures'][1]['rows']
    for i, tip in enumerate(evidence['tipSamples']):
        index = (6, 5)[i]
        source = mixed[index]
        assert tip['rewardIndex'] == index and tip['rewardKind'] == source['kind']
        assert tip['rewardText'] == source['name'] and tip['style'] == 'arrowDown'
        assert tip['sourcePrefab'] == inputs['tipPrefab'] and tip['sourcePrefabInstantiated']
        assert tip['priorTipClosed'] == (i == 1)
        close(tip['offsetLocal'], 70, .01)
        for field in ('rewardGift', 'tipRoot', 'plPos', 'background', 'arrowDown', 'textLabel'):
            valid_rect(tip[field], visible=field not in ('plPos', 'tipRoot'))
        assert tip['tipRoot']['path'].startswith('UIRoot/Container/WindowPopLayer/UI_Pop_TextTip')
        assert tip['arrowDown']['activeInHierarchy']
        assert all(not tip[field]['activeInHierarchy'] for field in ('arrowUp', 'arrowLeft', 'arrowRight'))
        assert tip['background']['spriteName'] and tip['arrowDown']['spriteName']
        assert tip['textLabel']['text'] == source['name'] and tip['textLabel']['fontSize'] == 18
        assert tip['textLabel']['fontName'] and tip['textLabel']['color']['a'] > .99
        tip_text_evidence = text_metric(tip['textLabel'])
        close(tip['background']['localRect']['height'] / 2, tip['backgroundHalfHeightLocal'], .01)
        close(tip['arrowDown']['localRect']['height'], tip['arrowHeightLocal'], .01)
        close(tip['background']['localRect']['width'] - tip['textLabel']['localRect']['width'], 40, .2)
        close(tip['background']['localRect']['height'] - tip['textLabel']['localRect']['height'], 40, .2)
        gift = tip['rewardGift']['screenRect']
        close(tip['sourceGiftCenterScreen']['x'], gift['x'] + gift['width'] / 2)
        close(tip['sourceGiftCenterScreen']['y'], gift['y'] + gift['height'] / 2)
        close(tip['plPos']['anchoredPosition']['y'] - tip['sourceCanvasLocalPosition']['y'],
              tip['offsetLocal'] + tip['backgroundHalfHeightLocal'] + tip['arrowHeightLocal'], .1)
        bg = tip['background']['screenRect']
        assert 0 < bg['x'] and 0 < bg['y'] and bg['x'] + bg['width'] < 1440 and bg['y'] + bg['height'] < 810
        png = artifact / tip['png']
        assert png.is_file() and tip['sha256'] == sha(png)
        if Image:
            with Image.open(png) as image:
                assert image.size == (1440, 810) and len(image.getcolors(maxcolors=1_000_000) or []) > 100
        tips.append({'kind': tip['rewardKind'], 'png': str(png), 'png_sha256': sha(png),
                     'text_evidence': tip_text_evidence,
                     'gift_rect': gift, 'background_rect': bg,
                     'arrow_rect': tip['arrowDown']['screenRect'], 'text_rect': tip['textLabel']['screenRect'],
                     'anchor_screen': tip['sourceGiftCenterScreen'], 'offset_local': tip['offsetLocal']})
    STAGE.mkdir(parents=True, exist_ok=True)
    final = {'schema': 'rok.mail_reward.native_verified.v1', 'scope': inputs['scope'],
             'artifact': str(artifact), 'input_sha256': sha(PROJECT / 'ReferenceInputs.json'),
             'source_fixture_sha256': sha(DATA), 'native_evidence_sha256': sha(artifact / 'render_evidence.json'),
             'source_count': len(inputs['sources']), 'copy_count': len(inputs['byte_identical_copies']),
             'mail_template': template, 'samples': summaries, 'tip_samples': tips,
             'limits': ['isolated deterministic fixture; source MailDefine 100030/ActivityCalendar 10101 pairing with synthetic RewardInfo', 'no live mailbox/login or claim click',
                                              '1440x810 capture only; no 1600x1000 native measurement']}
    target = STAGE / 'verified_capture.json'
    target.write_text(json.dumps(final, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'verified': True, 'artifact': str(artifact), 'output': str(target),
                      'row_counts': [s['rowCount'] for s in evidence['samples']]}))


if __name__ == '__main__':
    main()
