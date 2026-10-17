"""Prepare, but never launch, an isolated source-prefab native UGUI reference.

Original prefab, textures, importer metadata, PolygonImage and GameSlider remain
byte-identical. LanguageText/ArabLayout use documented isolated adapters because
their original code depends on the full client services. No product data changes.
"""
import hashlib
import json
import re
import shutil
from pathlib import Path
from BuildRokUnityEconomyCatalog import read_language_lookup, localized_text, LANGUAGE_WORKBOOK
from VerifyRokLeaderboardOverviewLayout import documents, node, vector

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / 'Saved/UnityGraphicsNativeReferenceProbe'
CLIENT = ROOT / 'ref/client-unity'
PREFAB = 'Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIPlayerData/UI_Item_GraphicsSetting.prefab'
RENDERER = ROOT / 'Scripts/RenderRokGraphicsNativeReference.cs'
GUIDS = {
    'LanguageText': '38b39f04615db8f4ca3b00ca5cd7b06c',
    'ArabLayoutCompment': '3f49ded9ad7716046b5f0d429a8015c9',
    'GameSlider': '9511fff753887a74897a45815bc55106',
    'PolygonImage': 'a9b6e432158eb304e8c8722aef745589',
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def safe(path):
    path = path.resolve()
    if not path.is_relative_to(TARGET.resolve()):
        raise RuntimeError('Write escaped isolated graphics reference: ' + str(path))
    return path


def main():
    if TARGET.resolve().parent != (ROOT / 'Saved').resolve():
        raise RuntimeError('Unexpected probe target')
    # Preserve the first successful render execution whose framing was rejected.
    # It remains evidence of the bug, not an accepted whole-card reference.
    original_evidence = TARGET / 'Artifacts/render_evidence.json'
    rejection = safe(TARGET / 'Artifacts/rejected_initial_crop/framing_rejection.json')
    if original_evidence.exists() and not rejection.exists():
        rejection.parent.mkdir(parents=True, exist_ok=True)
        preserved = []
        for relative in ['ReferenceInputs.json', 'Assets/Editor/RenderRokGraphicsNativeReference.cs']:
            old = TARGET / relative
            if not old.exists():
                raise RuntimeError('Cannot preserve original framing inputs: ' + relative)
            destination = safe(rejection.parent / Path(relative).name)
            shutil.copy2(old, destination)
            preserved.append({'original': relative, 'preserved': destination.relative_to(TARGET).as_posix(), 'sha256': digest(old)})
        artifacts = [original_evidence] + sorted((TARGET / 'Artifacts').glob('graphics_native_level_*.png'))
        rejection.write_text(json.dumps({
            'accepted_whole_card_reference': False,
            'reason': 'Original source root list placement anchoredY=-104.100006 was attached directly to canvas; top was 104.1 rather than 20 and card bottom exceeded 407px capture.',
            'original_artifacts_unchanged': [{'path': p.relative_to(TARGET).as_posix(), 'sha256': digest(p)} for p in artifacts],
            'preserved_inputs': preserved,
            'correction': 'Translate a capture-only parent using native camera projection; retain all original card RectTransform values and font sizes. New renders use separate session folders.'
        }, ensure_ascii=False, indent=2), encoding='utf-8')
    source = CLIENT / PREFAB
    text = source.read_text(encoding='utf-8-sig')
    docs = documents(text)
    if any(v.startswith('PrefabInstance:') for v in docs.values()):
        raise RuntimeError('Unexpected nested prefab requires expanded dependency traversal')
    assert vector(node(docs, 'UI_Item_GraphicsSetting')[0], 'm_SizeDelta') == (1000, 367)
    script_guids = set(re.findall(r'm_Script:.*?guid: (\w+)', text))
    assert script_guids == set(GUIDS.values())
    assert set(re.findall(r'_arab_Layout: (\d+)', text)) == {'10', '32'}
    assert not re.search(r'm_Material: \{fileID: (?!0\})', text)
    assert not re.search(r'm_fakeSprite: \{fileID: (?!0\})', text)
    assert set(re.findall(r'm_Font: \{fileID: (\d+)', text)) == {'10102'}

    sources, copies, adapters = [], [], []

    def record(path):
        sources.append({'path': path.relative_to(ROOT).as_posix(), 'sha256': digest(path)})

    def copy(path, destination):
        destination = safe(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        assert digest(path) == digest(destination)
        copies.append({'source': path.relative_to(ROOT).as_posix(),
                       'target': destination.relative_to(TARGET).as_posix(), 'sha256': digest(path)})
        record(path)

    # Keep original importer/GUID identities; no texture cropping or alpha edits.
    copy(source, TARGET / 'Assets/Source/UI_Item_GraphicsSetting.prefab')
    copy(Path(str(source) + '.meta'), TARGET / 'Assets/Source/UI_Item_GraphicsSetting.prefab.meta')
    atlas_guids = set(re.findall(r'm_Sprite: \{fileID: -?\d+, guid: (\w+)', text))
    remaining = set(atlas_guids)
    for meta in (CLIENT / 'Assets/BundleAssets/UI/UITextureStatic').rglob('*.meta'):
        header = meta.read_text(encoding='utf-8-sig')[:150]
        match = re.search(r'^guid: (\w+)', header, re.M)
        if not match or match[1] not in remaining:
            continue
        atlas = Path(str(meta)[:-5])
        assert atlas.suffix == '.png', atlas
        copy(atlas, TARGET / 'Assets/Source' / atlas.name)
        copy(meta, TARGET / 'Assets/Source' / meta.name)
        remaining.remove(match[1])
    if remaining:
        raise RuntimeError('Missing source atlas GUIDs: ' + ','.join(sorted(remaining)))

    script_paths = {}
    for name, guid in GUIDS.items():
        matches = list((CLIENT / 'Assets/Scripts').rglob(name + '.cs.meta'))
        matches = [p for p in matches if 'guid: ' + guid in p.read_text(encoding='utf-8-sig')]
        assert len(matches) == 1, name
        script = Path(str(matches[0])[:-5])
        script_paths[name] = script
        record(script)
        copy(matches[0], TARGET / 'Assets/ReferenceComponents' / (name + '.cs.meta'))

    # These two source classes require no client behavior. PolygonImage's runtime
    # mesh modifiers are empty; its editor fake sprite is unused by this prefab.
    for name in ['PolygonImage', 'GameSlider']:
        copy(script_paths[name], TARGET / 'Assets/ReferenceComponents' / (name + '.cs'))
    language_adapter = '''using UnityEngine;
namespace GameFramework { public static class GraphicsReferenceNamespaceMarker {} }
namespace UnityEngine.UI {
// Isolated adapter: original Text serialized appearance retained; localization
// is assigned explicitly from source language IDs by the native renderer.
public class LanguageText : Text {
 [SerializeField] private int m_languageId = -1;
 public int SourceLanguageId { get { return m_languageId; } }
}
}
'''
    arab_adapter = '''using UnityEngine;
// Isolated adapter: serialized source flags retained. The renderer performs
// only the source Chinese Awake branches used by this prefab (10 and 32).
public class ArabLayoutCompment : MonoBehaviour {
 [SerializeField] private int _arab_Layout;
 public int SourceFlags { get { return _arab_Layout; } }
}
'''
    for name, code in [('LanguageText', language_adapter), ('ArabLayoutCompment', arab_adapter)]:
        destination = safe(TARGET / 'Assets/ReferenceComponents' / (name + '.cs'))
        destination.write_text(code, encoding='utf-8')
        adapters.append({'class': name, 'source': script_paths[name].relative_to(ROOT).as_posix(),
                         'original_guid_retained': GUIDS[name], 'adapter_sha256': digest(destination),
                         'not_original_runtime_script': True})
    copy(RENDERER, TARGET / 'Assets/Editor/RenderRokGraphicsNativeReference.cs')
    language, _ = read_language_lookup()
    record(LANGUAGE_WORKBOOK)
    record(Path(__file__).resolve())
    ids = sorted(set(map(int, re.findall(r'm_languageId: (-?\d+)', text))))
    entries = [{'id': i, 'text': localized_text(language, i, 'cn')} for i in ids if i >= 0]
    assert all(e['text'] for e in entries)
    logic = CLIENT / 'Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_GraphicsSetting_SubView.cs'
    quality = CLIENT / 'Assets/Scripts/Hotfix/Common/Utils/QualitySetting.cs'
    record(logic); record(quality)
    assert 'LanguageUtils.getTextFormat(300227' in logic.read_text(encoding='utf-8-sig')
    assert 'systemMemorySize > 1000 * 3' in quality.read_text(encoding='utf-8-sig')
    settings = CLIENT / 'ProjectSettings/ProjectSettings.asset'
    record(settings)
    assert re.search(r'm_ActiveColorSpace: (\d+)', settings.read_text(encoding='utf-8-sig'))[1] == '0'
    manifest = {'scope': 'Source-derived isolated native UGUI reference. Original prefab/atlas/import metadata; '
                'two explicit adapters replace full-client LanguageText and ArabLayout services. '
                'Not a full mediator, original running game, or saved user graphics preference.',
                'project': str(TARGET), 'unity_version': '2019.4.28f1c1',
                'prefab': 'Assets/Source/UI_Item_GraphicsSetting.prefab',
                'width': 1040, 'height': 407, 'framing_padding_pixels': 20,
                'background': 'transparent; no invented surrounding window or card panel',
                'nodes': sum(v.startswith('RectTransform:') for v in docs.values()),
                'texts': entries, 'sources': sources, 'byte_identical_copies': copies, 'adapters': adapters,
                'remaining_gates': ['Unity compile/import/native capture not yet executed',
                    'Compare generated native pixels and bounds with actual UE card',
                    'No audio service, quality application, mediator lifecycle or full GameView']}
    safe(TARGET / 'ReferenceInputs.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    packages = safe(TARGET / 'Packages'); packages.mkdir(exist_ok=True)
    (packages / 'manifest.json').write_text(json.dumps({'dependencies': {
        'com.unity.ugui': '1.0.0', 'com.unity.modules.ui': '1.0.0',
        'com.unity.modules.imageconversion': '1.0.0', 'com.unity.modules.jsonserialize': '1.0.0',
        'com.unity.modules.imgui': '1.0.0'}}), encoding='utf-8')
    settings_out = safe(TARGET / 'ProjectSettings'); settings_out.mkdir(exist_ok=True)
    (settings_out / 'ProjectVersion.txt').write_text('m_EditorVersion: 2019.4.28f1c1\n', encoding='utf-8')
    print(json.dumps({'prepared': True, 'unity_started': False, 'project': str(TARGET),
                      'rect_nodes': manifest['nodes'], 'copied_files': len(copies), 'adapters': len(adapters)}))


if __name__ == '__main__':
    main()
