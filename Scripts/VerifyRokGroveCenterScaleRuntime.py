"""Verify actual Grove zoom artifacts without starting UE or Unity.

Usage: python Scripts/VerifyRokGroveCenterScaleRuntime.py <audit-directory>
--source-only validates existing source/native inputs and this audit's static gates.
Buffer/center checks are CPU metadata. They are not a GPU WPO readback or visual parity claim.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve
import struct

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Saved/RokUnityMigration/GroveCenterScale'
AUDITS = ROOT / 'Saved/RokUnityMigration/GroveCenterScaleAudit'
DXFS = [480, 900, 1509, 3000, 4200, 7000, 4200, 1509, 480]
MESH = '/Game/RokUnityWorldMapPrototype/Meshes/grove/StaticMeshes/SM_RokGroveCenterScale.SM_RokGroveCenterScale'
MATERIAL = '/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_GroveCenterScale.M_WorldSource_GroveCenterScale'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def finite(values, shape):
    result = np.asarray(values, dtype=np.float64)
    assert result.shape == shape and np.isfinite(result).all(), ('Invalid numeric array', shape)
    return result


def native_values(items, keys):
    return [[row[key] for key in keys] for row in items]


def triangle(t):
    values = tuple(int(v) for v in t)
    index = values.index(min(values))
    return values[index:] + values[:index]


def source_data():
    source, native = load(SOURCE/'manifest.json'), load(SOURCE/'native_render.json')
    assert source['schema'] == 'rok.grove_center_scale.v1' and source['prefab'] == 'Grove_co_tree_01_adorning'
    assert native['unityVersion'] == '2019.4.28f1c1' and native['shader'] == 'custom/landmap/tree-16-nomask' and native['colorSpace'] == 'Gamma'
    assert len(source['source_files']) == 6
    for item in source['source_files']:
        path = (resolve(item['path'])).resolve()
        assert path.is_relative_to((ROOT/'ref').resolve()) and sha(path) == item['sha256']
    positions = finite(source['positions'], (377, 3)).astype(np.float32)
    colors = finite(source['source_color_rgba'], (377, 4)).astype(np.float32)
    uv0 = finite(source['uv0'], (377, 2)).astype(np.float32)
    assert np.max(np.abs(finite(native_values(native['positions'], 'xyz'), (377, 3))-positions)) < 1e-6
    assert np.array_equal(finite(native_values(native['colors'], 'rgba'), (377, 4)).astype(np.float32), colors)
    assert np.array_equal(finite(native_values(native['uv0'], 'xy'), (377, 2)).astype(np.float32), uv0)
    indices = finite(source['triangles'], (309, 3))
    assert np.array_equal(indices, indices.astype(np.int32)) and indices.min() >= 0 and indices.max() < 377
    assert np.array_equal(np.asarray(native['triangles']).reshape(309, 3), indices)
    centers = colors[:, :3]*np.float32(180)
    assert len(np.unique(centers, axis=0)) == 16
    baked = centers+(positions-centers)*np.float32(source['bake_scale'])
    ue_positions = baked[:, [0, 2, 1]]*np.float32(100)
    assert len(source['instances']) == 11 and len(native['frames']) == len(source['samples']) == 9
    for index, (sample, frame, dxf) in enumerate(zip(source['samples'], native['frames'], DXFS)):
        assert sample['name'] == frame['name'] == f'{index:02d}_dxf{dxf}' and sample['dxf'] == frame['dxf'] == dxf
        assert np.isfinite(frame['scale']) and 0 < frame['scale'] <= 10 and abs(sample['scale']-frame['scale']) < 2e-6
        # Native JSON explicitly labels these positions as CPU reference formula.
        assert np.max(np.abs(finite(native_values(frame['evaluatedLocalPositions'], 'xyz'), (377, 3))-(centers+(positions-centers)*np.float32(frame['scale'])))) < 2e-6
        native_png = Path(frame['path']).resolve()
        assert native_png == (SOURCE/f'native_{frame["name"]}.png').resolve()
        with Image.open(native_png) as image:
            assert image.size == (1280, 720)
            image.verify()
    return source, native, ue_positions, uv0, colors, indices


def static_gates():
    cpp = (ROOT/'Source/Rok/RokGroveCenterScaleAuditSubsystem.cpp').read_text(encoding='utf-8-sig')
    for token in ['RokGroveCenterScaleV1', 'UE_BUILD_SHIPPING', 'Type == EWorldType::Game', 'LoadSource()',
                  'GetUseFullPrecisionUVs()', 'bUseFullPrecisionUVs', 'bAllowCPUAccess', 'GetScalarParameterValue',
                  'RokSourceTreeScale', 'GetViewTarget() != Camera.Get()', 'GetPawn() != Camera.Get()',
                  'GetComponentTransform().Equals(Row.OriginalComponentTransform',
                  'RequestScreenshot(PendingPath, true, false, false, FIntRect(), true)',
                  'OnScreenshotRequestProcessed()', 'RestoreVisibility()', 'RequestExitWithStatus(false, Failures.IsEmpty() ? 0 : 1)',
                  'IsGroveTargetWorld(Cast<UWorld>(Outer))', 'bOwnsTargetWorld = IsGroveTargetWorld(GetWorld())', 'bOwnsTargetWorld && !bFinished',
                  'gpu_vertex_readback', 'FIntVector(int32(T[0]), int32(T[2]), int32(T[1]))']:
        assert token in cpp, token
    for token in ['SetActorScale3D(', 'SetActorTransform(', 'SetActorLocation(', 'SetScalarParameterValue(', 'CreateDynamicMaterialInstance(', 'TestingAdvance', 'SetTimeDilation(', 'FlushRenderingCommands(']:
        assert token not in cpp, ('Forbidden audit substitution', token)


def verify(directory):
    directory = directory.resolve()
    assert directory.is_relative_to(AUDITS.resolve())
    source, native, positions, uv0, colors, indices = source_data()
    static_gates()
    manifest_path = directory/'manifest.json'
    report = load(manifest_path)
    assert report['audit_version'] == 1 and report['complete'] and report['passed'] and not report['failures']
    assert Path(report['project']).resolve() == ROOT.resolve() and report['world'] == '/Game/Maps/L_RokUnityWorldMapPrototype'
    assert report['source_manifest_sha256'] == sha(SOURCE/'manifest.json') and report['native_manifest_sha256'] == sha(SOURCE/'native_render.json')
    assert report['capture_includes_slate_ui'] and report['visibility_restored'] and report['owns_target_world']
    assert report['gpu_vertex_readback'] is False and report['unity_visual_comparison_complete'] is False
    mesh = report['actual_render_buffers']
    assert mesh['mesh'] == MESH and mesh['full_precision_uvs'] and mesh['allow_cpu_access'] and mesh['uv_channels'] == 3
    rows = np.asarray(mesh['render_vertices_source_mapping'], dtype=np.float64)
    assert rows.ndim == 2 and rows.shape[1] == 10 and 377 <= len(rows) <= 927 and np.isfinite(rows).all()
    mapping = rows[:, 0].astype(np.int32)
    assert np.array_equal(mapping, rows[:, 0]) and set(mapping) == set(range(377))
    assert np.max(np.abs(rows[:, 1:4]-positions[mapping])) <= .005
    assert np.array_equal(rows[:, 4:6].astype(np.float32), uv0[mapping])
    assert np.array_equal(rows[:, 6:10].astype(np.float32), colors[mapping])
    actual_triangles = finite(mesh['render_triangles_source_mapping'], (309, 3))
    assert Counter(triangle(t) for t in actual_triangles) == Counter(triangle(t[[0, 2, 1]]) for t in indices)
    expected_actors = {}
    for instance in source['instances']:
        item = instance['source_instance']
        key = (f'UnityTile_{instance["tile"][0]}_{instance["tile"][1]}', f'UnityObjectIndex_{item["template_index"]}')
        assert key not in expected_actors
        expected_actors[key] = item
    captures = report['captures']
    assert len(captures) == 9
    actor_baselines = {}
    center_baseline = None
    artifacts, crops = [], []
    for index, capture in enumerate(captures):
        path = Path(capture['file']).resolve()
        assert path.parent == directory and path.name == f'{index:02d}_dxf{DXFS[index]}.png'
        data = path.read_bytes()
        assert data[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', data[16:24]) == (1440, 810)
        assert len(data) == capture['file_bytes'] and sha(path) == capture['sha256']
        for phase in ['request', 'processed']:
            state = capture[phase]
            assert state['sample_index'] == index and state['sample_name'] == native['frames'][index]['name'] and abs(state['dxf']-DXFS[index]) < .001
            assert abs(state['native_scale']-native['frames'][index]['scale']) <= 2e-6
            assert 'RokWorldMapCameraPawn' in state['view_target']
            assert len(state['instances']) == 11
            actual_keys = set()
            for actor in state['instances']:
                key = (actor['tile_tag'], actor['index_tag'])
                assert key not in actual_keys and key in expected_actors
                actual_keys.add(key)
                expected = expected_actors[key]
                transform = finite(actor['location']+actor['scale']+actor['rotation_quaternion'], (10,))
                assert np.max(np.abs(transform[:3]-[expected['ue_relative_to_focus'][a] for a in 'xyz'])) <= .01
                assert np.max(np.abs(transform[3:6]-[expected['ue_actor_scale_for_centimeter_asset'][a] for a in 'xyz'])) <= .01
                half_yaw = np.deg2rad(expected['ue_yaw'])/2
                assert np.max(np.abs(transform[6:]-[0, 0, np.sin(half_yaw), np.cos(half_yaw)])) <= .00001
                assert actor['source_transform_unchanged'] and actor['material_parent'] == MATERIAL and abs(actor['source_tree_scale']-state['native_scale']) <= 2e-6
                if not report['isolated_existing_asset'] or actor['component'] == state['focus_component']:
                    assert actor['visible_parent_chain']
                if key in actor_baselines:
                    assert np.max(np.abs(actor_baselines[key]-transform)) < 1e-5
                else:
                    actor_baselines[key] = transform
            assert actual_keys == set(expected_actors)
            centers = state['source_center_projections']
            assert len(centers) == 16
            world_centers = finite([c['world_center'] for c in centers], (16, 3))
            finite([c['screen_center'] for c in centers], (16, 2))
            if center_baseline is None:
                center_baseline = world_centers
            else:
                assert np.max(np.abs(world_centers-center_baseline)) <= 1e-6, 'CPU source center anchors moved'
        assert capture['processed']['frame'] >= capture['request']['frame']
        if index:
            assert capture['request']['frame'] >= captures[index-1]['processed']['frame']+3
            assert capture['request']['elapsed']-captures[index-1]['processed']['elapsed'] >= .49
        # Actual pixels, with a repeatable crop derived from projected stable centers.
        screen = np.asarray([c['screen_center'] for c in capture['processed']['source_center_projections']])
        low, high = np.floor(screen.min(axis=0)-24).astype(int), np.ceil(screen.max(axis=0)+24).astype(int)
        box = (max(0, int(low[0])), max(0, int(low[1])), min(1440, int(high[0])), min(810, int(high[1])))
        assert box[2]-box[0] >= 8 and box[3]-box[1] >= 8
        with Image.open(path) as image:
            rgb = np.asarray(image.convert('RGB').crop(box))
        assert float(rgb.std()) > 1, 'Actual grove center region is a flat/empty frame'
        crops.append(rgb)
        artifacts.append({'file':path.name, 'sha256':sha(path), 'center_crop':box, 'crop_rgb_std':float(rgb.std())})
    repeat_differences = []
    for first, repeat in [(0, 8), (2, 7), (4, 6)]:
        assert crops[first].shape == crops[repeat].shape, 'Repeated camera sample changed projection'
        delta = np.abs(crops[first].astype(np.int16)-crops[repeat].astype(np.int16))
        repeat_differences.append({'samples':[first,repeat], 'mean_rgb_difference':float(delta.mean()), 'fraction_over_12':float(np.any(delta>12, axis=2).mean())})
    result = {'passed':True, 'manifest_sha256':sha(manifest_path), 'source_manifest_sha256':sha(SOURCE/'manifest.json'),
              'artifacts':artifacts, 'repeat_actual_pixel_diagnostics':repeat_differences,
              'scope':'Actual renderer screenshots and exact source render-buffer/MID/transform checks. CPU projected centers remain fixed; this is not GPU vertex readback. Repeat-image differences are diagnostics because normal map actors can animate or occlude the grove. Native reference has a fixed camera, UE uses changing real source camera. Manual native/UE visual comparison remains an open acceptance gate.'}
    output = directory/'runtime_verify.json'
    output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path, nargs='?')
    parser.add_argument('--source-only', action='store_true')
    args = parser.parse_args()
    if args.source_only:
        source_data()
        static_gates()
        print('PASS: source/native data and audit static gates; no runtime acceptance claimed')
    else:
        assert args.directory is not None, 'Supply the actual completed audit directory'
        verify(args.directory)
