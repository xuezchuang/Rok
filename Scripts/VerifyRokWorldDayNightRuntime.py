"""Verify continuous world/day-night artifacts and eleven current Grove MID bindings.
Usage: python Scripts/VerifyRokWorldDayNightRuntime.py <directory> [--grove-audit <directory>] [--expected-size 1920x912]
Legacy v2 manifests require explicit expected size because they did not record the actual viewport.
--source-only checks the source/contract without claiming any runtime result.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
from rok_data_paths import resolve
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'Saved/RokUnityMigration/GroveCenterScale/manifest.json'
OUTPUT = ROOT/'Saved/RokUnityMigration/WorldDayNightAudit'
PARENT = '/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_GroveCenterScale.M_WorldSource_GroveCenterScale'
NAMES = ['dusk_continuous', 'night_transition_start', 'night_transition_middle', 'night_material_stagger',
         'night_all_layers', 'night_crop1500', 'night_return480', 'night_far4200', 'night_return_from_far',
         'dawn_continuous_transition', 'dawn_materials_restored']


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def values(items, count):
    assert isinstance(items, list) and len(items) == count
    assert all(type(value) in (float, int) and math.isfinite(value) for value in items)
    return items


def near(actual, expected, tolerance):
    assert len(actual) == len(expected)
    assert all(math.isfinite(a) and abs(a-b) <= tolerance for a, b in zip(actual, expected)), (actual, expected)


def dimensions(value):
    assert isinstance(value, (list, tuple)) and len(value) == 2
    assert all(type(component) is int and component > 0 for component in value), value
    return tuple(value)


def parse_dimensions(value):
    try:
        return dimensions([int(component) for component in value.lower().split('x')])
    except (ValueError, AssertionError) as error:
        raise argparse.ArgumentTypeError('Expected positive integer WIDTHxHEIGHT, e.g. 1920x912') from error


def source_contract():
    source = load(SOURCE)
    assert source['schema'] == 'rok.grove_center_scale.v1' and source['prefab'] == 'Grove_co_tree_01_adorning'
    assert len(source['source_files']) == 6
    for item in source['source_files']:
        path = (resolve(item['path'])).resolve()
        assert path.is_relative_to((ROOT/'ref').resolve()) and sha(path) == item['sha256']
    expected = {}
    for entry in source['instances']:
        row = entry['source_instance']
        assert row['prefab_id'] == source['prefab']
        key = (f'UnityTile_{entry["tile"][0]}_{entry["tile"][1]}', f'UnityObjectIndex_{row["template_index"]}')
        assert key not in expected
        yaw = math.radians(row['ue_yaw'])/2
        expected[key] = [row['ue_relative_to_focus'][a] for a in 'xyz']+[row['ue_actor_scale_for_centimeter_asset'][a] for a in 'xyz']+[0, 0, math.sin(yaw), math.cos(yaw)]
    assert len(expected) == 11
    for left, right in zip(source['curve'], source['curve'][1:]):
        if left['weighted_mode'] & 2 or right['weighted_mode'] & 1:
            # The only weighted interval is a constant infinite-tangent step.
            assert left['scale'] == right['scale'] and (math.isinf(float(left['out_slope'])) or math.isinf(float(right['in_slope'])))
    return source, expected


def tree_scale(source, dxf):
    keys = source['curve']
    if dxf <= keys[0]['dxf']:
        return keys[0]['scale']
    if dxf >= keys[-1]['dxf']:
        return keys[-1]['scale']
    for left, right in zip(keys, keys[1:]):
        if left['dxf'] <= dxf <= right['dxf']:
            dt = right['dxf']-left['dxf']
            m0, m1 = float(left['out_slope']), float(right['in_slope'])
            if math.isinf(m0) or math.isinf(m1):
                return left['scale']
            t = (dxf-left['dxf'])/dt
            return (2*t**3-3*t**2+1)*left['scale']+(t**3-2*t**2+t)*dt*m0+(-2*t**3+3*t**2)*right['scale']+(t**3-t**2)*dt*m1
    raise AssertionError('Missing source curve segment')


def static_contract():
    cpp = (ROOT/'Source/Rok/RokWorldDayNightAuditSubsystem.cpp').read_text(encoding='utf-8-sig')
    for token in ['if(!BindGroves())return false;', 'InspectGroves(GroveRows)', 'GetMaterial(0)',
                  'GetEnvironmentMaterials().ContainsByPredicate', 'Weak.Get()==MID',
                  'ActualColor.Equals(S.SpriteColor', 'RokSourceTreeScale', 'EvaluateUnityTreeScale(Camera->GetCurrentDxf())',
                  'GetActorTransform().Equals(Row.SourceTransform', 'GetActorTransform().Equals(Row.InitialActorTransform',
                  'GetComponentTransform().Equals(Row.InitialComponentTransform', 'OnScreenshotRequestProcessed()',
                  'IsWorldDayNightAuditTarget(Cast<UWorld>(Outer))', 'bOwnsTargetWorld=IsWorldDayNightAuditTarget(GetWorld())',
                  'if(bOwnsTargetWorld&&!bFinished)Finish(', 'if(!bOwnsTargetWorld||bFinished||!Pending)',
                  'PC->GetViewportSize(Width,Height)', 'ObservedViewportSize!=ActualViewportSize',
                  'Width!=uint32(ObservedViewportSize.X)||Height!=uint32(ObservedViewportSize.Y)',
                  'TEXT("actual_viewport_size")', 'TEXT("png_size")',
                  'RequestScreenshot(PendingPath,true,false,false,FIntRect(),true)', 'Count!=98', 'Captures.Num()==UE_ARRAY_COUNT(Names)']:
        assert token in cpp, token
    assert cpp.count('if(Count!=11)') == 2


def verify(directory, grove_audit=None, expected_size=None):
    directory = directory.resolve()
    assert directory.is_relative_to(OUTPUT.resolve())
    source, expected = source_contract()
    static_contract()
    path = directory/'manifest.json'
    report = load(path)
    assert report['passed'] and not report['failures'] and report['audit_version'] in (2, 3) and report['owns_target_world']
    recorded_viewport = report['audit_version'] == 3
    if recorded_viewport:
        target_size = dimensions(report['actual_viewport_size'])
        if expected_size is not None:
            assert target_size == dimensions(expected_size), ('Recorded viewport differs from explicit expected size', target_size, expected_size)
    else:
        assert expected_size is not None, 'Legacy v2 did not record viewport dimensions; supply independently observed --expected-size WIDTHxHEIGHT'
        target_size = dimensions(expected_size)
    assert Path(report['project']).resolve() == ROOT.resolve() and report['world'] == '/Game/Maps/L_RokUnityWorldMapPrototype'
    assert report['grove_source_manifest_sha256'] == sha(SOURCE)
    baselines = {}
    actual_mid_paths = set()

    def state_check(state):
        if recorded_viewport:
            assert dimensions(state['actual_viewport_size']) == target_size, 'Actual viewport changed between frames/capture states'
        assert state['actual_cards'] == 98 and state['environment_materials'] >= 11 and state['actual_grove_instances'] == 11
        color = values(state['actual_verified_sprite_color'], 3)
        scale = tree_scale(source, state['dxf'])
        assert len(state['groves']) == 11
        seen = set()
        for grove in state['groves']:
            key = (grove['tile_tag'], grove['index_tag'])
            assert key in expected and key not in seen
            seen.add(key)
            assert grove['material_parent'] == PARENT and grove['current_mid_in_environment_registry']
            assert grove['source_transform_match'] and grove['initial_transform_unchanged']
            actual_mid_paths.add(grove['actual_slot_mid'])
            near(values(grove['actual_sprite_color'], 3), color, 1e-5)
            near([grove['actual_sprite_alpha']], [grove['expected_sprite_alpha']], 1e-5)
            near([grove['actual_tree_scale'], grove['expected_tree_scale']], [scale, scale], 2e-6)
            transform = values(grove['location'], 3)+values(grove['scale'], 3)+values(grove['rotation_quaternion'], 4)
            near(transform[:3], expected[key][:3], .01)
            near(transform[3:], expected[key][3:], 1e-5)
            if key in baselines:
                near(transform, baselines[key], 1e-5)
            else:
                baselines[key] = transform
        assert seen == set(expected)

    frames = report['frames']
    assert len(frames) > 11
    for index, frame in enumerate(frames):
        state_check(frame)
        if index:
            assert frame['frame'] >= frames[index-1]['frame'] and frame['scaled_time'] >= frames[index-1]['scaled_time']
    captures = report['captures']
    assert len(captures) == 11
    artifacts = []
    for index, capture in enumerate(captures):
        assert capture['name'] == NAMES[index]
        file = Path(capture['path']).resolve()
        assert file.parent == directory and file.name == f'{index:02d}_{NAMES[index]}.png'
        data = file.read_bytes()
        assert len(data) >= 24 and len(data) == capture['bytes'] and data[:8] == b'\x89PNG\r\n\x1a\n'
        png_size = struct.unpack('>II', data[16:24])
        assert png_size == target_size, ('PNG size differs from expected actual viewport', file.name, png_size, target_size)
        if recorded_viewport:
            assert dimensions(capture['png_size']) == png_size, ('Recorded PNG size differs from file', file.name)
        for phase in ['at_request', 'at_processed']:
            state = capture[phase]
            state_check(state)
            assert abs(state['dxf']-({5:1500, 7:4200}.get(index, 480))) <= .001
            assert state['village_visible'] == ({5:1, 7:0}.get(index, 13))
            if index == 0:
                assert state['phase'] == 4 and state['night_materials'] == 0
            elif index in [1, 2]:
                assert state['phase'] == 5
            elif index == 3:
                assert state['phase'] == 6 and 0 < state['night_materials'] < 98
            elif index in [4, 5, 6, 7, 8]:
                assert state['phase'] == 6 and state['night_materials'] == 98
            elif index == 9:
                assert state['phase'] == 7 and state['night_materials'] == 98
            else:
                assert state['phase'] == 0 and state['night_materials'] == 0
        assert capture['at_processed']['frame'] >= capture['at_request']['frame']
        artifacts.append({'file':file.name, 'sha256':sha(file), 'png_size':list(png_size)})
    linked = None
    if grove_audit is not None:
        other = grove_audit.resolve()/'manifest.json'
        assert other.parent.is_relative_to((ROOT/'Saved/RokUnityMigration/GroveCenterScaleAudit').resolve())
        grove = load(other)
        assert grove['passed'] and grove['complete'] and not grove['failures'] and len(grove['captures']) == 9
        assert grove['source_manifest_sha256'] == report['grove_source_manifest_sha256']
        assert grove['actual_render_buffers']['buffer_source_match_complete'] and not grove['actual_render_buffers']['nanite_enabled']
        linked = {'manifest':str(other), 'sha256':sha(other), 'shared_source_hash':report['grove_source_manifest_sha256']}
    result = {'passed':True, 'manifest_sha256':sha(path), 'source_manifest_sha256':sha(SOURCE), 'frames_checked':len(frames),
              'dimension_evidence':{'expected_size':list(target_size), 'explicit_expected_size':list(expected_size) if expected_size is not None else None,
                                    'runtime_viewport_recorded':recorded_viewport, 'all_png_dimensions_match':True,
                                    'basis':'runtime viewport at every frame/request/processed' if recorded_viewport else 'explicit independently observed expected size; legacy runtime did not record viewport metadata'},
              'source_grove_instances':11, 'actual_mid_paths_observed':len(actual_mid_paths), 'captures':artifacts, 'linked_grove_zoom_audit':linked,
              'scope':'Continuous runtime current-slot MID registration, simultaneous source scale/day-night color, unchanged source transforms and original 98-card/11-state gates. Source-center GPU displacement and Unity/UE visual equivalence remain in separate image review; this verifier does not replace the nine-zoom render-buffer verifier.'}
    output = directory/'runtime_verify.json'
    output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path, nargs='?')
    parser.add_argument('--grove-audit', type=Path)
    parser.add_argument('--expected-size', type=parse_dimensions, help='Independently observed actual viewport WIDTHxHEIGHT; mandatory for legacy v2, cross-checked for v3')
    parser.add_argument('--source-only', action='store_true')
    args = parser.parse_args()
    if args.source_only:
        source, _ = source_contract()
        static_contract()
        for sample in source['samples']:
            assert abs(tree_scale(source, sample['dxf'])-sample['scale']) < 2e-6
        print('PASS: source placements, curve and cross-audit static gates; no runtime result claimed')
    else:
        assert args.directory is not None
        verify(args.directory, args.grove_audit, args.expected_size)
