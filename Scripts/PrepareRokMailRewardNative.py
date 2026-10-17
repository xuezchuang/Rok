"""Prepare an isolated native Unity mail-reward presentation fixture; never start Unity."""
import hashlib
import json
import re
import shutil
import subprocess
import csv
from pathlib import Path

from PrepareRokBarracksLevelNative import parse_languages
from rok_data_paths import DATA_ROOT, label, resolve

ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'ref/client-unity'
TARGET = ROOT / 'Saved/UnityMailRewardNativeProbe'
PACKAGE = DATA_ROOT / 'mail_reward_presentation.json'
MAIL = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_mail/UIMail/'
ITEM = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Item.prefab'
BAG = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_item/UIBag/UI_Item_Bag.prefab'
ROOT_PREFAB = 'Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab'
ATLAS = 'Assets/BundleAssets/UI/UITextureStatic/'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert TARGET.resolve().parent == (ROOT / 'Saved').resolve()
    def out(relative):
        p = (TARGET / relative).resolve()
        assert p.is_relative_to(TARGET.resolve())
        p.parent.mkdir(parents=True, exist_ok=True)
        return p
    sources, copies, adapters, missing = [], [], [], []
    def record(path):
        row = {'path': label(path), 'sha256': sha(path)}
        if row not in sources:
            sources.append(row)
    def copy(path, relative):
        target = out(relative)
        if target.exists():
            import stat
            target.chmod(target.stat().st_mode | stat.S_IWRITE)
            with path.open('rb') as src, target.open('r+b') as dst:
                shutil.copyfileobj(src, dst)
                dst.truncate()
        else:
            shutil.copyfile(path, target)
        assert sha(path) == sha(target)
        record(path)
        copies.append({'source': label(path), 'target': relative, 'sha256': sha(path)})
    fixture = json.loads(PACKAGE.read_text(encoding='utf-8'))
    assert fixture['schema'] == 'rok.mail_reward_presentation.v1'
    assert list(fixture['fixtures']) == ['reference_single', 'mixed_audit', 'empty_alternate']
    for path, expected in fixture['sources'].items():
        actual = resolve(path)
        assert sha(actual) == expected, path
        record(actual)
    record(PACKAGE)
    copy(PACKAGE, 'FixtureSource.json')
    # Keep original Unity GUIDs for every reachable prefab, material, controller,
    # animation, texture, and script. Missing extracted source GUIDs are reported.
    scan_roots = [CLIENT / 'Assets/BundleAssets', CLIENT / 'Assets/Scripts/Client/Compoment',
                  CLIENT / 'Assets/Skyunion/RunTime', CLIENT / 'Assets/Scripts/Hotfix']
    files = subprocess.check_output(['rg', '--files', '--hidden', '--no-ignore', '-g', '*.meta', *map(str, scan_roots)],
                                    text=True, encoding='utf-8').splitlines()
    index = {}
    for name in files:
        meta = Path(name)
        match = re.search(r'^guid: (\w+)', meta.read_text(encoding='utf-8-sig', errors='replace')[:180], re.M)
        if match:
            index[match[1]] = Path(str(meta)[:-5])
    queue = [CLIENT / p for p in (ROOT_PREFAB, MAIL + 'UI_Win_Mail.prefab',
             MAIL + 'UI_Item_MailTypeSystem.prefab', MAIL + 'UI_Model_MailGift.prefab',
             MAIL + 'UI_Item_MailBtn.prefab',
             'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Pop_TextTip.prefab', BAG, ITEM)]
    manual_atlases = [ATLAS + 'ui_res_common/ui_common.png', ATLAS + 'ui_res_troops/troops_head.png',
                      ATLAS + 'ui_res_item/item5.png', ATLAS + 'ui_res_item/item0.png',
                      ATLAS + 'ui_res_mail/ui_mail.png']
    queue += [CLIENT / p for p in manual_atlases]
    seen, scripts, all_yaml = set(), {}, ''
    while queue:
        path = queue.pop()
        if path in seen:
            continue
        seen.add(path)
        if path.suffix == '.cs':
            scripts[path.stem] = path
            record(path)
            continue
        relative = path.relative_to(CLIENT).as_posix()
        copy(path, relative)
        meta = Path(str(path) + '.meta')
        copy(meta, relative + '.meta')
        if path.suffix in {'.prefab', '.mat', '.anim', '.controller', '.asset', '.shader'}:
            body = path.read_text(encoding='utf-8-sig', errors='replace')
            all_yaml += body
            for guid in set(re.findall(r'guid: (\w+)', body)):
                if guid in index:
                    queue.append(index[guid])
                elif not guid.startswith('000000'):
                    row = {'guid': guid, 'referenced_by': relative}
                    if row not in missing:
                        missing.append(row)
    common = {
        'LanguageText': '''using UnityEngine; namespace GameFramework { public static class ReferenceMarker {} } namespace UnityEngine.UI { public class LanguageText : Text { [SerializeField] private int m_languageId = -1; public int SourceLanguageId { get { return m_languageId; } } public override string text { get { return ArabicSupport.ArabicFixer.IsRtl(base.text) ? ArabicSupport.ArabicFixer.FixRtl(ArabicSupport.ArabicFixer.FixWithoutRtl(base.text, false, false, false), false, false, false) : base.text; } set { base.text = value; } } public TextGenerator CaptureGenerator() { var g = cachedTextGenerator; g.Populate(text, GetGenerationSettings(rectTransform.rect.size)); return g; } } }''',
        'ArabLayoutCompment': 'using UnityEngine; public class ArabLayoutCompment : MonoBehaviour { [SerializeField] private int _arab_Layout; public int SourceFlags { get { return _arab_Layout; } } }',
        'GameButton': 'public class GameButton : UnityEngine.UI.Button {}',
        'BtnAnimation': 'public class BtnAnimation : UnityEngine.MonoBehaviour {}',
        'ListView': 'public class ListView : UnityEngine.MonoBehaviour {}',
        'ViewBinder': 'public class ViewBinder : UnityEngine.MonoBehaviour {}',
        'UIZoomCurve': 'namespace Client { public class UIZoomCurve : UnityEngine.MonoBehaviour {} }',
        'InputHandler': 'namespace Skyunion { public class InputHandler : UnityEngine.MonoBehaviour {} }',
        'Empty4Raycast': 'namespace UnityEngine.UI { public class Empty4Raycast : MaskableGraphic { protected override void OnPopulateMesh(VertexHelper vh) { vh.Clear(); } } }',
        'GrayChildrens': 'public class GrayChildrens : UnityEngine.MonoBehaviour {}',
        'UIDefaultValue': 'public class UIDefaultValue : UnityEngine.MonoBehaviour {}',
        'HrefText': 'namespace Client { public class HrefText : UnityEngine.UI.LanguageText {} }',
    }
    for name, path in scripts.items():
        relative = 'Assets/ReferenceComponents/' + name + '.cs'
        copy(Path(str(path) + '.meta'), relative + '.meta')
        if name == 'PolygonImage':
            copy(path, relative)
        else:
            assert name in common, ('Unreviewed source component', name)
            target = out(relative)
            target.write_text(common[name] + '\n', encoding='utf-8')
            adapters.append({'class': name, 'target': relative, 'sha256': sha(target),
                             'source': label(path),
                             'scope': 'Inert isolated presentation adapter; original serialized GUID preserved.'})
    arabic = CLIENT / 'Assets/Scripts/Client/Compoment/UIExtend/ArabicSupport.cs'
    copy(arabic, 'Assets/ReferenceComponents/ArabicSupport.cs')
    copy(Path(str(arabic) + '.meta'), 'Assets/ReferenceComponents/ArabicSupport.cs.meta')
    language_bin = CLIENT / 'Assets/StreamingAssets/Config/Bin/Language.bin'
    language = {int(row[0]): row[1] for row in parse_languages(language_bin)}
    record(language_bin)
    mail_table = ROOT / 'ref/config-tables/编译客户端配置/CSV/Mail.csv'
    activity_table = ROOT / 'ref/config-tables/编译客户端配置/CSV/ActivityCalendar.csv'
    with mail_table.open(encoding='utf-8-sig', newline='') as f:
        mail_row = next(r for r in csv.DictReader(f) if r['ID'] == '100030')
    with activity_table.open(encoding='utf-8-sig', newline='') as f:
        activity_row = next(r for r in csv.DictReader(f) if r['ID'] == '10101')
    assert mail_row['group'] == '1' and mail_row['receiveAuto'] == '0' and mail_row['l_senderID'] == '0'
    assert mail_row['operate'] == '1|2' and mail_row['type'] == '1'
    assert mail_row['poster'] == 'ui_mail[img_mail_title_1]'
    assert activity_row['l_nameID'] == '300229'
    activity_name = language[int(activity_row['l_nameID'])]
    title = language[int(mail_row['l_nameID'])]
    subtitle = language[int(mail_row['l_subheadingID'])].replace('{activity_0}', activity_name)
    body = language[int(mail_row['l_mesID'])].replace('{activity_0}', activity_name)
    assert title == '活动奖励补发' and subtitle == '征服之始-领取宝箱活动奖励补发'
    assert '{' not in body and '</color>' in body
    record(mail_table); record(activity_table)
    email_proxy = CLIENT / 'Assets/Scripts/Hotfix/MVC/Proxy/EmailProxy.cs'
    record(email_proxy)
    template = {'mailDefineId': 100030, 'activityCalendarId': 10101,
                'nameLocaleId': int(mail_row['l_nameID']), 'subtitleLocaleId': int(mail_row['l_subheadingID']),
                'bodyLocaleId': int(mail_row['l_mesID']), 'activityLocaleId': int(activity_row['l_nameID']),
                'claimLocaleId': 570008, 'title': title, 'subtitle': subtitle, 'body': body,
                 'claim': language[570008], 'sendTime': '', 'listIcon': mail_row['icon'],
                 'poster': mail_row['poster'],
                'qualification': 'Source MailDefine + ActivityCalendar localized template paired with synthetic RewardInfo; no EmailInfo/sendTime/server mail.'}
    shared = fixture['mail_template']
    assert shared['mail_id'] == template['mailDefineId'] and shared['activity_id'] == template['activityCalendarId']
    assert shared['locale_ids'] == {'title': template['nameLocaleId'], 'subtitle': template['subtitleLocaleId'],
                                   'body': template['bodyLocaleId'], 'activity_name': template['activityLocaleId'],
                                   'claim': template['claimLocaleId']}
    assert shared['title'] == title and shared['activity_name'] == activity_name
    assert shared['subtitle'] == subtitle and shared['body_rich'] == body and shared['claim_label'] == template['claim']
    assert shared['send_time'] is None
    assert shared['body_plain'] == re.sub(r'<[^>]+>', '', body)
    assert shared['body_greeting'] + shared['body_prefix'] + shared['body_activity_quoted'] + shared['body_suffix'] == shared['body_plain'].replace('\n', '')
    ids = {int(s) for s in re.findall(r'm_languageId: (-?\d+)', all_yaml) if int(s) > 0}
    # Nested prefab instances serialize LanguageText overrides as propertyPath/value,
    # including the six footer buttons in UI_Win_Mail.
    ids |= {int(s) for s in re.findall(r'propertyPath: m_languageId\s+value: (-?\d+)', all_yaml) if int(s) > 0}
    ids |= {570008, 570009, 570006, 300250}
    assert {570016, 570017, 570018, 570019, 570021, 570022}.issubset(ids)
    localized = [{'id': i, 'text': language[i]} for i in sorted(ids) if i in language]
    rows = []
    currencies = {x['field']: x for x in fixture['metadata']['currencies']}
    for key, state in fixture['fixtures'].items():
        payload = state['payload']
        reward_rows = []
        for field in ('food', 'wood', 'stone', 'gold', 'denar'):
            if payload[field] > 0:
                meta = currencies[field]
                reward_rows.append({'kind': 'currency', 'id': meta['id'], 'quantity': payload[field],
                    'name': meta['name'], 'icon': meta['icon'], 'quality': 0, 'tipEnabled': False})
        for soldier in payload['soldiers']:
            meta = fixture['metadata']['soldier']
            assert soldier['id'] == meta['id']
            reward_rows.append({'kind': 'soldier', 'id': meta['id'], 'quantity': soldier['num'],
                'name': meta['name'], 'icon': meta['icon'], 'quality': 0, 'tipEnabled': True})
        for item in payload['items']:
            meta = fixture['metadata']['item']
            assert item['itemId'] == meta['id']
            reward_rows.append({'kind': 'item', 'id': meta['id'], 'quantity': item['itemNum'],
                'name': meta['name'], 'icon': meta['icon'], 'quality': meta['quality'], 'tipEnabled': True})
        rows.append({'key': key, 'role': state['role'], 'rows': reward_rows})
    assert [len(s['rows']) for s in rows] == [1, 7, 0]
    renderer = ROOT / 'Scripts/RenderRokMailRewardNative.cs'
    copy(renderer, 'Assets/Editor/' + renderer.name)
    record(Path(__file__).resolve())
    record(CLIENT / 'Assets/Scripts/Hotfix/MVC/RS.cs')
    record(CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/HelpTips/HelpTipMediator.cs')
    record(CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/HelpTips/HelpTipView.cs')
    record(CLIENT / 'Assets/Scripts/Hotfix/MVC/UI.cs')
    manifest = {
        'schema': 'rok.mail_reward.native_inputs.v1',
        'scope': 'Original UIRoot/UI_Win_Mail/UI_Item_MailTypeSystem/UI_Model_MailGift isolated deterministic source-shaped presentation fixture; no GameApp/login, mailbox, claim, or server mutation.',
        'width': 1440, 'height': 810, 'probeVersion': 1, 'uiRootPrefab': ROOT_PREFAB,
        'windowPrefab': MAIL + 'UI_Win_Mail.prefab', 'systemPrefab': MAIL + 'UI_Item_MailTypeSystem.prefab',
        'listRowPrefab': MAIL + 'UI_Item_MailBtn.prefab',
        'tipPrefab': 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Pop_TextTip.prefab',
        'tipContract': {'style': 'arrowDown', 'sourceUiLayer': 'WindowPopLayer',
                        'rewardIndexesInMixed': [6, 5], 'offsetLocal': 70,
                        'method': 'HelpTip.CreateTip(text,gift.transform) center->Canvas local; HelpTipMediator.SetTipString; SetOffset; each Show closes prior tip'},
        'giftPrefab': MAIL + 'UI_Model_MailGift.prefab', 'fixtureSourceSha256': sha(PACKAGE),
        'fixtures': rows, 'texts': localized, 'mailTemplate': template,
        'qualitySprite': 'item0[img_item_bg4]',
        'atlasPaths': manual_atlases,
        'sources': sources, 'byte_identical_copies': copies, 'adapters': adapters,
        'missingExtractedGuids': sorted(missing, key=lambda r: (r['guid'], r['referenced_by'])),
        'remainingGates': ['Unity PlayMode native capture', 'strict native verifier', 'UE runtime comparison', 'real GameApp/mailbox route']
    }
    out('ReferenceInputs.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    out('Packages/manifest.json').write_text(json.dumps({'dependencies': {k: '1.0.0' for k in
        ['com.unity.ugui', 'com.unity.modules.ui', 'com.unity.modules.imageconversion',
         'com.unity.modules.jsonserialize', 'com.unity.modules.imgui', 'com.unity.modules.animation']}}, indent=2), encoding='utf-8')
    out('ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n', encoding='utf-8')
    unity = Path('C:/Program Files/Unity/Hub/Editor/2019.4.28f1c1/Editor/Data')
    ugui = ROOT / 'Saved/UnityBarracksLevelProbe/Library/ScriptAssemblies/UnityEngine.UI.dll'
    assert ugui.exists(), 'Previously imported Unity2019 UGUI assembly is required for compiler-only check'
    refs = list((unity / 'Managed/UnityEngine').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api').glob('*.dll'))
    refs += list((unity / 'MonoBleedingEdge/lib/mono/4.7.1-api/Facades').glob('*.dll'))
    refs += [unity / 'Managed/UnityEditor.dll', ugui]
    args = ['/nologo', '/target:library', '/define:UNITY_EDITOR', '/out:' + str(out('Validation/ProbeSyntax.dll'))]
    args += ['/reference:' + str(p) for p in refs]
    args += [str(p) for p in (TARGET / 'Assets/ReferenceComponents').glob('*.cs')]
    args += [str(TARGET / 'Assets/Editor/RenderRokMailRewardNative.cs')]
    response = out('Validation/compiler.rsp')
    response.write_text('\n'.join('"' + arg + '"' for arg in args), encoding='utf-8')
    result = subprocess.run([str(unity / 'Tools/Roslyn/csc.exe'), '@' + str(response)], capture_output=True)
    out('Validation/compiler.log').write_bytes(result.stdout + result.stderr)
    out('Validation/compiler_result.json').write_text(json.dumps({'exit_code': result.returncode, 'unity_started': False}), encoding='utf-8')
    if result.returncode:
        raise RuntimeError('Mail reward probe C# syntax failed; see Validation/compiler.log')
    print(json.dumps({'prepared': True, 'unity_started': False, 'copies': len(copies),
                      'adapters': len(adapters), 'missing_guids': len(missing), 'fixture_row_counts': [len(s['rows']) for s in rows]}))


if __name__ == '__main__':
    main()
