"""UE 5.8 commandlet/editor entrypoint for source Research atlases and Academy menu icon."""
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'Saved/RokUnityMigration/ResearchNative/atlas_manifest.json'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_name = world.get_name() if world else ''
    print('Research texture import identity:', project, world_name)
    if project != ROOT.resolve() or world_name != 'L_RokUnityGameAppCity':
        raise RuntimeError('Research texture import requires exact Rok project and L_RokUnityGameAppCity world')
    report = json.loads(MANIFEST.read_text(encoding='utf-8'))
    assert report['schema'] == 'rok.research.atlas.v1'
    for relative, expected in report['sources'].items():
        assert sha(ROOT / relative) == expected, relative
    imported = []
    rows = [*report['atlases'], report['menuIcon']]
    assert len(rows) == 5 and len({row['ueAssetPath'] for row in rows}) == 5
    existing = [row['ueAssetPath'] for row in rows if unreal.EditorAssetLibrary.does_asset_exist(row['ueAssetPath'])]
    if existing:
        raise RuntimeError('Research import requires five new assets; existing: ' + ', '.join(existing))
    for row in rows:
        src = ROOT / row['output']
        assert src.exists() and sha(src) == row['sha256'], str(src)
        destination = row['ueAssetPath']
        parent, name = destination.rsplit('/', 1)
        task = unreal.AssetImportTask()
        task.filename = str(src)
        task.destination_path = parent
        task.destination_name = name
        task.automated = True
        task.replace_existing = False
        task.save = False
        task.factory = unreal.TextureFactory()
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.EditorAssetLibrary.load_asset(destination)
        if not asset or not isinstance(asset, unreal.Texture2D):
            raise RuntimeError('Research atlas import missing ' + destination)
        for key, value in (
            ('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON),
            ('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
            ('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI),
            ('srgb', True), ('never_stream', True),
            ('filter', unreal.TextureFilter.TF_BILINEAR),
            ('address_x', unreal.TextureAddress.TA_CLAMP),
            ('address_y', unreal.TextureAddress.TA_CLAMP),
        ):
            asset.set_editor_property(key, value)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
            raise RuntimeError('Research atlas save failed: ' + destination)
        expected = row['atlasSize'] if 'atlasSize' in row else {'width': row['rect']['width'], 'height': row['rect']['height']}
        width, height = asset.blueprint_get_size_x(), asset.blueprint_get_size_y()
        if (width, height) != (expected['width'], expected['height']):
            raise RuntimeError(f'Research atlas dimension mismatch {destination}: {(width,height)} != {expected}')
        imported.append({'asset': destination, 'sourceSha256': sha(src), 'width': width, 'height': height,
                         'compression': str(asset.get_editor_property('compression_settings')),
                         'mipGeneration': str(asset.get_editor_property('mip_gen_settings')),
                         'lodGroup': str(asset.get_editor_property('lod_group')),
                         'srgb': asset.get_editor_property('srgb'), 'neverStream': asset.get_editor_property('never_stream'),
                         'filter': str(asset.get_editor_property('filter'))})
    assert len(imported) == 5
    proof = {'schema': 'rok.research.atlas_import.v1', 'project': str(project), 'world': world_name,
             'manifest': str(MANIFEST), 'manifestSha256': sha(MANIFEST), 'imported': imported}
    evidence = ROOT / 'Saved/RokUnityMigration/ResearchNative/ue_import.json'
    evidence.write_text(json.dumps(proof, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(proof, ensure_ascii=False))


main()
