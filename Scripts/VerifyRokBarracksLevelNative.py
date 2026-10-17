"""Verify isolated source-prefab barracks all-level PlayMode evidence."""
import csv
import hashlib
import json
import math
import sys
from pathlib import Path
from rok_data_paths import resolve

from PIL import Image, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / 'Saved/UnityBarracksLevelProbe/Artifacts'
TABLES = ROOT / 'ref/config-tables/编译客户端配置/CSV'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def csv_rows(name):
    with (TABLES / (name + '.csv')).open(encoding='utf-8-sig', newline='') as stream:
        return [row for row in csv.DictReader(stream) if row['ID'].isdigit()]


def rect(row):
    value = row['screenRect']
    return tuple(float(value[k]) for k in ('x', 'y', 'width', 'height'))


def verify(evidence_path):
    evidence_path = Path(evidence_path).resolve()
    assert evidence_path.name == 'render_evidence.json' and evidence_path.parent.parent == ARTIFACTS.resolve()
    folder = evidence_path.parent
    report = json.loads(evidence_path.read_text(encoding='utf-8-sig'))
    inputs_path = folder / 'ReferenceInputs.json'
    inputs = json.loads(inputs_path.read_text(encoding='utf-8-sig'))
    failures = []

    def check(ok, message):
        if not ok:
            failures.append(message)

    check(report.get('complete') and not report.get('failure') and report.get('playMode'), 'native PlayMode complete')
    check(report.get('unityVersion') == '2019.4.28f1c1', 'Unity version')
    check(report.get('inputSha256') == sha(inputs_path), 'input hash')
    check(inputs.get('probeVersion') == 1 and (inputs.get('width'), inputs.get('height')) == (1440, 810), 'probe contract')
    check(report.get('fixture') == 'isolated_barracks_level_presentation', 'honest isolated fixture')
    check(report.get('nativeAnimatorEnabled') and report.get('projectionInitialized'), 'original parent animator and camera')
    check(report.get('nativeCameraClearFlags') == 'Depth' and report.get('clearPolicy') == 'GL.Clear(true,true,Color.clear)', 'source camera clear')
    check(report.get('levelRowCount') == 25 and report.get('currentLevel') == inputs.get('currentLevel') == 1, '25 rows/current level')
    check([row['id'] for row in inputs.get('headers', [])] == [180310, 180329, 180352, 180314], 'source header IDs')
    check(inputs.get('titlePrefab', '').endswith('/UI_Item_BuildingLevelData_Title_Four.prefab'), 'source Four title prefab')
    check(inputs.get('rowPrefab', '').endswith('/UI_Item_BuildingLevelData_Info_Four.prefab'), 'source Four row prefab')
    check(len(inputs.get('levels', [])) == 25, 'all source level values')
    power = {(int(r['type']), int(r['level'])): int(r['power']) for r in csv_rows('BuildingLevelData')}
    barracks = csv_rows('BuildingBarracks')
    expected = [(int(r['level']), int(r['infantryTrainNumber']), int(r['infantryAttackMulti']), power[(9, int(r['level']))]) for r in barracks]
    actual = [(r['level'], r['train'], r['attack'], r['power']) for r in inputs.get('levels', [])]
    check(actual == expected and [row[0] for row in actual] == list(range(1, 26)), 'source Barracks/LevelData rows')
    check(all((r['attackText'] == '' if r['attack'] == 0 else r['attackText'].endswith('%')) for r in inputs.get('levels', [])), 'source zero/positive attack formatting')
    for entry in inputs.get('sources', []):
        if entry['path'].startswith('Source/Rok/'):
            continue  # Concurrent UE implementation is outside this isolated source capture.
        path = resolve(entry['path'])
        check(path.is_file() and sha(path) == entry['sha256'], 'source hash: ' + entry['path'])
    for entry in inputs.get('byte_identical_copies', []):
        copied = ROOT / 'Saved/UnityBarracksLevelProbe' / entry['target']
        check(copied.is_file() and sha(copied) == entry['sha256'], 'copied source hash: ' + entry['target'])
    setup_path = folder / 'setup_evidence.json'
    check(setup_path.is_file() and report.get('setupSha256') == sha(setup_path), 'setup hash')
    if setup_path.is_file():
        setup = json.loads(setup_path.read_text(encoding='utf-8-sig'))
        check(setup.get('fixtureTitleApplied') == inputs.get('fixtureTitle'), 'source title fixture')
        check(bool(setup.get('languageBindings')) and bool(setup.get('layoutFlagsApplied')), 'source LanguageText/Arab layout')
        check(all(abs((setup.get('rootAfterLoad') or {}).get('localScale', {}).get(k, math.inf)) < 1e-5 for k in 'xyz'), 'original serialized UIRoot scale')
    samples = report.get('samples') or []
    check(len(samples) == 9 and [s.get('phase') for s in samples] == ['parent'] * 4 + ['detail'] * 3 + ['back'] * 2, 'parent/detail/back phases')
    check(report.get('clearCount', 0) >= 9, 'independent native render target clears')
    pictures = []
    for i, sample in enumerate(samples):
        image_path = folder / sample.get('png', '')
        check(image_path.is_file() and image_path.parent == folder, 'frame exists ' + str(i))
        if image_path.is_file():
            check(sha(image_path) == sample.get('sha256'), 'frame hash ' + str(i))
            image = Image.open(image_path).convert('RGBA')
            pictures.append(image)
            check(image.size == (1440, 810) and max(ImageStat.Stat(image).stddev[:3]) > 3, 'visible frame ' + str(i))
        check(sample.get('stateName') == 'Open', 'original parent Open animation ' + str(i))
    if len(pictures) == 9:
        check(ImageChops.difference(pictures[3], pictures[6]).getbbox() is not None, 'detail visibly differs from parent')
        check(ImageChops.difference(pictures[6], pictures[8]).getbbox() is not None, 'back visibly differs from detail')
    if len(samples) == 9:
        check(samples[3].get('animatorNormalizedTime', 0) >= .99, 'parent Open terminal')
        check(all(s.get('levelActive') and s.get('backActive') and not s.get('contentActive') for s in samples[4:7]), 'level navigation visibility')
        check(samples[6].get('detailStateName') == 'Show' and samples[6].get('detailNormalizedTime', 0) >= .99, 'native detail Show terminal')
        check(all(s.get('contentActive') and not s.get('levelActive') and not s.get('backActive') for s in samples[7:9]), 'source back visibility')
        check(samples[8].get('contentStateName') == 'Show', 'native content Show on back')
    parent = report.get('terminalWindowGeometry') or []
    detail = report.get('detailWindowGeometry') or []
    returned = report.get('returnWindowGeometry') or []
    check(len(parent) > 20 and len(detail) > len(parent) + 25 and len(returned) == len(detail), 'complete geometry inventories')
    def one(rows, suffix):
        matches = [r for r in rows if r.get('path', '').endswith(suffix)]
        check(len(matches) == 1, 'unique source node ' + suffix)
        return matches[0] if matches else {}
    for stage, rows in [('parent', parent), ('detail', detail), ('return', returned)]:
        for row in rows:
            corners = row.get('screenCorners') or []
            check(len(corners) == 4, stage + ' actual corners ' + row.get('path', ''))
            if len(corners) == 4:
                x = min(p['x'] for p in corners); y = min(p['y'] for p in corners)
                bounds = (x, y, max(p['x'] for p in corners) - x, max(p['y'] for p in corners) - y)
                check(all(abs(a-b) < .02 for a, b in zip(bounds, rect(row))), stage + ' corner/rect consistency ' + row.get('path', ''))
    for name in ('/Rect/pl_sv_level', '/Rect/pl_sv_level/pl_tilte', '/Rect/pl_sv_level/sv_levelData', '/UI_Model_Window_Type1/title/btn_back'):
        node = one(detail, name)
        check(node.get('activeInHierarchy'), 'visible detail control ' + name)
    check(not one(detail, '/Rect/pl_content').get('activeInHierarchy'), 'main content hidden in detail')
    check(not one(returned, '/Rect/pl_sv_level').get('activeInHierarchy') and one(returned, '/Rect/pl_content').get('activeInHierarchy'), 'return mode geometry')
    title = one(detail, '/Rect/pl_sv_level/pl_tilte/UI_Item_BuildingLevelData_Title_Four(Clone)')
    check(title.get('activeInHierarchy'), 'original Four header instantiated')
    for i, header in enumerate(inputs.get('headers', []), 1):
        item = one(detail, '/pl_tilte/UI_Item_BuildingLevelData_Title_Four(Clone)/lbl_name' + str(i))
        check(item.get('text') == header['text'], 'source header text ' + str(i))
    rows = [r for r in detail if r.get('path', '').endswith('/UI_Item_BuildingLevelData_Info_Four(Clone)')]
    check(len(rows) == 25 and all(r.get('activeInHierarchy') for r in rows), '25 original Four row prefabs')
    if len(rows) == 25:
        check(all(abs(rect(r)[3] - 50) < .1 for r in rows), 'source 50px row height')
        check(all(abs(rect(rows[i+1])[1] - rect(rows[i])[1] - 50) < .2 for i in range(24)), 'source row pitch/scroll layout')
    selections = [r for r in detail if '/UI_Item_BuildingLevelData_Info_Four(Clone)/img_select' in r.get('path', '')]
    check(len(selections) == 25 and sum(bool(r.get('activeSelf')) for r in selections) == 1, 'exactly one current row highlight')
    if len(selections) == 25:
        check(selections[0].get('activeInHierarchy'), 'level 1 highlight')
    row_roots = [i for i, r in enumerate(detail) if r.get('path', '').endswith('/UI_Item_BuildingLevelData_Info_Four(Clone)')]
    if len(row_roots) == 25:
        for i, start in enumerate(row_roots):
            end = row_roots[i + 1] if i + 1 < len(row_roots) else len(detail)
            group = detail[start:end]
            rendered = {r['path'].rsplit('/', 1)[-1]: r.get('text') for r in group
                        if r.get('path', '').rsplit('/', 1)[-1] in {'lbl_name1', 'lbl_name2', 'lbl_name3', 'lbl_name4'}}
            source = inputs['levels'][i]
            values = [str(source['level']), f"{source['train']:,}", source['attackText'], f"{source['power']:,}"]
            check([rendered.get('lbl_name' + str(j)) for j in range(1, 5)] == values,
                  'native visible text matches all four source columns at level ' + str(i + 1))
    result = {'schema': 'rok.barracks_level_native_verify.v1', 'passed': not failures, 'failures': failures,
              'scope': inputs.get('scope'), 'evidence_sha256': sha(evidence_path), 'source_level_count': len(actual),
              'parent_geometry_count': len(parent), 'detail_geometry_count': len(detail),
              'detail_rects': {name: rect(one(detail, name)) for name in
                               ('/Rect/pl_sv_level', '/Rect/pl_sv_level/pl_tilte', '/Rect/pl_sv_level/sv_levelData',
                                '/UI_Model_Window_Type1/title/btn_back') if one(detail, name)},
              'header_rect': rect(title) if title else None,
              'first_row_rect': rect(rows[0]) if rows else None,
              'current_highlight_rect': rect(selections[0]) if selections else None}
    (folder / 'verify_level_native.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if result['passed']:
        summary = ROOT / 'Saved/RokUnityMigration/BarracksLevelNative'
        summary.mkdir(parents=True, exist_ok=True)
        (summary / 'verified_capture.json').write_text(json.dumps({
            'schema': 'rok.barracks_level_native_capture.v1', 'scope': result['scope'],
            'evidence': evidence_path.relative_to(ROOT).as_posix(), 'evidence_sha256': result['evidence_sha256'],
            'verify': (folder / 'verify_level_native.json').relative_to(ROOT).as_posix(),
            'reference_inputs_sha256': sha(inputs_path), 'headers': inputs['headers'],
            'levels': inputs['levels'], 'geometry': {k: result[k] for k in
                ('detail_rects', 'header_rect', 'first_row_rect', 'current_highlight_rect')},
            'remaining_gate': 'Full GameApp/login and actual source ListView service were not run in this isolated presentation fixture.'
        }, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return result


if __name__ == '__main__':
    result = verify(sys.argv[1])
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result['passed'] else 1)
