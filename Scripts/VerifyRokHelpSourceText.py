"""Validate a completed native Help glyph export; no engine or asset writes."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from PIL import Image


def verify(directory):
    root = Path(directory).resolve()
    load = lambda name: json.loads((root / name).read_text(encoding='utf-8'))
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    data, inputs = load('source_text_meshes.json'), load('ReferenceInputs.json')
    assert data['schema'] == 'rok.help.source_text.v1'
    assert data['inputSha256'] == sha(root / 'ReferenceInputs.json') == load('render_evidence.json')['inputSha256']
    expected = {x['key']: x for x in inputs['sourceTexts']}
    assert len(expected) == len(data['texts']) == 222
    seen, atlases = set(), {}
    for item in data['texts']:
        key = item['key']
        assert key in expected and key not in seen
        seen.add(key)
        source = expected[key]
        assert item['text'] == source['text'] and item['text'] and '\ufffd' not in item['text']
        assert item['fontSize'] == source['fontSize'] and item['style'] == source['style'] and item['alignment'] == source['alignment']
        assert abs(item['width'] - source['width']) < .01
        assert all(math.isfinite(item[n]) and item[n] > 0 for n in ['width', 'height', 'preferredWidth', 'preferredHeight', 'pixelsPerUnit'])
        xy, uv, triangles = item['xy'], item['uv'], item['triangles']
        assert 0 < len(xy) <= 10000 and len(xy) % 4 == 0 and len(uv) == len(xy)
        assert len(triangles) == len(xy) // 4 * 6
        assert all(type(i) is int and 0 <= i < len(xy) for i in triangles)
        assert all(math.isfinite(v[axis]) and abs(v[axis]) < 10000 for v in xy for axis in ['x', 'y'])
        assert all(math.isfinite(v[axis]) and 0 <= v[axis] <= 1 for v in uv for axis in ['x', 'y'])
        assert {g['codepoint'] for g in item['glyphs']} == {ord(c) for c in item['text'] if ord(c) >= 32}
        path = (root / item['atlas']).resolve()
        assert path.is_relative_to(root / 'SourceTextAtlases') and path.suffix == '.png'
        if path not in atlases:
            assert sha(path) == item['atlasSha256'] == path.stem
            image = Image.open(path).convert('RGBA')
            assert image.getchannel('A').getextrema()[1] > 0
            assert max(image.size) <= 8192
            atlases[path] = sha(path)
    result = {'passed': True, 'texts': len(seen), 'atlases': len(atlases), 'input_sha256': data['inputSha256'], 'ue_implementation_or_visual_accepted': False}
    (root / 'source_text_verify.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory')
    verify(parser.parse_args().directory)
