"""Verify completed isolated hospital audit artifacts; never starts an engine.
Usage: python Scripts/VerifyRokHospitalExplanationRuntime.py <audit-directory>
"""
import argparse, hashlib, json, re, struct
from pathlib import Path
import numpy as np
from PIL import Image
from rok_data_paths import DATA_ROOT, resolve
ROOT = Path(__file__).resolve().parents[1]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def explanation_pixels(path, source):
    # Derive the viewport from the source Rect, crop its interior to exclude frame edges.
    x,y,w,h = source['nodes']['Rect/Right/pl_explain']['rect']
    box = (int(x+24), int(y+8), int(x+w-12), int(y+h-8))
    rgb = np.asarray(Image.open(path).convert('RGB').crop(box)).astype(np.int16)
    r,g,b = rgb[:,:,0],rgb[:,:,1],rgb[:,:,2]
    yellow = (r>150)&(g>140)&(b<110)&(abs(r-g)<65)
    white = (r>160)&(g>160)&(b>160)&((rgb.max(axis=2)-rgb.min(axis=2))<40)
    ink = yellow|white
    bands = np.flatnonzero(ink.sum(axis=1)>8)
    runs = 0 if len(bands)==0 else 1+int(np.count_nonzero(np.diff(bands)>1))
    assert yellow.sum()>60, 'Explanation viewport has no rendered yellow text'
    assert white.sum()>200 and runs>=8, 'Explanation viewport lacks multiple rendered text lines'
    assert ink.mean()<.35, 'Solid/color region cannot stand in for glyph evidence'
    return ink, {'source_crop':box,'yellow_pixels':int(yellow.sum()),'white_pixels':int(white.sum()),'text_bands':runs}
def overlap(a,b):
    return float(np.count_nonzero(a&b)/max(1,np.count_nonzero(a|b)))
def verify(directory):
    directory = directory.resolve()
    assert directory.is_relative_to((ROOT/'Saved/RokUnityMigration/HospitalExplanationAudit').resolve()), 'Unexpected output directory'
    manifest = directory/'manifest.json'
    m = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert m.get('capture_includes_slate_ui') is True
    audit_source=(ROOT/'Source/Rok/RokHospitalExplanationAuditSubsystem.cpp').read_text(encoding='utf-8-sig')
    assert 'RequestScreenshot(PendingPath, true, false)' in audit_source, 'Source regressed to scene-only screenshot'
    assert m['complete'] and m['passed'] and not m['failures'], 'Runtime failed/incomplete'
    assert Path(m['project']).resolve() == ROOT.resolve() and m['world'] == '/Game/Maps/L_RokUnityGameAppCity'
    catalog = DATA_ROOT / 'hospital_explanation.json'
    source = json.loads(catalog.read_text(encoding='utf-8-sig'))
    assert source['schema'] == 'rok.hospital.explanation.v1' and source['language_id'] == 181001
    assert len(re.findall(r'(?m)^[1-7]\.', source['source_text'])) == 7
    assert len(source['yellow_segments']) == m['yellow_spans'] == 13
    for item in source['sources']:
        path = resolve(item['path']).resolve()
        assert path.is_relative_to((ROOT/'ref').resolve()) and sha(path) == item['sha256']
    captures = m['captures']; assert len(captures) == m['expected_capture_count'] == 8
    expected = [('01_hospital_unknown','hospital'),('02_explanation_top','explanation'),('03_explanation_bottom','explanation'),('04_hospital_return_unknown','hospital'),('05_explanation_retained_bottom','explanation'),('06_city_closed','city'),('07_reopened_explanation_top','explanation'),('08_city_final','city')]
    artifacts = []; pixel_masks = {}; pixel_evidence = {}
    for capture, (name, page) in zip(captures, expected):
        file = Path(capture['file']).resolve(); assert file.parent == directory and file.stem == name
        data = file.read_bytes(); assert data[:8] == b'\x89PNG\r\n\x1a\n' and len(data) == capture['file_bytes']
        dimensions = struct.unpack('>II', data[16:24]); assert dimensions == (1440, 810), dimensions
        for key in ['request','processed']:
            state = capture[key]
            assert state['page'] == page and state['read_only_state_unchanged']
            assert state['feature_open'] == (page != 'city')
            assert state['explanation_visible'] == (page == 'explanation')
            if page == 'city':
                assert not state['explanation_attached'] and not state['capacity_attached'], 'Closed layout still attached'
            if page == 'explanation':
                assert state['rich_text'] == source['rich_text'] and state['yellow_style_exact']
                if name in ['03_explanation_bottom','05_explanation_retained_bottom']:
                    assert state['scroll_offset'] > 0 and abs(state['scroll_offset']-state['scroll_end']) <= .1
                else: assert abs(state['scroll_offset']) <= .1
        assert capture['processed']['frame'] >= capture['request']['frame']
        if page == 'explanation':
            pixel_masks[name], pixel_evidence[name] = explanation_pixels(file,source)
        artifacts.append({'name':file.name,'sha256':sha(file),'dimensions':dimensions})
    bottom, reopened = captures[2]['processed'], captures[4]['processed']
    assert bottom['scroll_id'] == reopened['scroll_id'] and abs(bottom['scroll_offset']-reopened['scroll_offset']) <= .1
    top=pixel_masks['02_explanation_top']; bottom_pixels=pixel_masks['03_explanation_bottom']
    retained_iou=overlap(bottom_pixels,pixel_masks['05_explanation_retained_bottom'])
    reset_iou=overlap(top,pixel_masks['07_reopened_explanation_top'])
    changed_iou=overlap(top,bottom_pixels)
    assert retained_iou>.85 and reset_iou>.85, 'Rendered text does not follow retained/reset scroll state'
    assert changed_iou<.85, 'Top and bottom screenshots show the same rendered text region'
    actions = [a['button'] for a in m['actions']]
    assert actions == ['BuildingFeatureButton','HospitalCapacityInfo','HospitalExplanationBackButton','HospitalCapacityInfo','FeatureFrameClose','BuildingFeatureButton','HospitalCapacityInfo','FeatureFrameClose']
    result = {'passed':True,'manifest_sha256':sha(manifest),'source_catalog_sha256':sha(catalog),'artifacts':artifacts,'viewport_pixels':pixel_evidence,'scroll_mask_iou':{'retained':retained_iou,'reset':reset_iou,'top_vs_bottom':changed_iou},'scope':'Real delegate/tick state and screenshot integrity, exact source text/yellow style and scroll lifecycle. Manual visual/native comparison remains separate; no physical pointer or authoritative treatment validation.'}
    output = directory/'runtime_verify.json'; output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(output)
if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('directory',type=Path);verify(p.parse_args().directory)
