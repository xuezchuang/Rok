"""Verify packaged close models against their exact source files and raw exports."""
import hashlib
import json
from pathlib import Path
from PIL import Image
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]

def verify():
    manifest = json.loads((DATA_ROOT / 'world_structure_presentation.json').read_text(encoding='utf-8'))
    seen = set()
    images = set()
    def walk(value):
        if isinstance(value, list):
            for child in value: walk(child)
        elif isinstance(value, dict):
            if 'path' in value and 'sha256' in value:
                path = resolve(value['path'])
                if (str(path),value['sha256']) not in seen:
                    assert path.is_file(),path
                    assert hashlib.sha256(path.read_bytes()).hexdigest() == value['sha256'],path
                    seen.add((str(path),value['sha256']))
                if 'export' in value:
                    exported = resolve(value['export'])
                    assert exported.is_file(),exported
                    with Image.open(path) as original, Image.open(exported) as target:
                        assert target.mode == 'RGBA' and target.size == (value['width'],value['height']),exported
                        assert original.convert('RGBA').tobytes() == target.tobytes(),exported
                    images.add(str(exported))
            for child in value.values(): walk(child)
    walk(manifest)
    models = manifest['models']
    assert len(models)==9
    assert sum(len(m['items']) for m in models.values())==24
    assert all(not m['unsupported'] for m in models.values()),'Unresolved model layers'
    gold=models['build_Rssgold_Big']
    assert len(gold['grid_order'])==13 and gold['native_order_evidence']
    report={'models':9,'sprites':24,'source_hashes_checked':len(seen),'raw_rgba_exports_checked':len(images),'unsupported':0}
    output=ROOT/'Saved/MigrationReadiness/P8CloseVisuals_20261107/SourceVerification.json'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))

if __name__ == '__main__': verify()
