"""Verify a fresh 18-body Mountain F runtime audit, eleven decoded mips, and actual day/dusk/night pixels."""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'Saved/RokUnityMigration/MountainFGammaAudit'
SOURCE = ROOT / 'Saved/RokUnityMigration/MountainFGamma'
V2 = ROOT / 'Saved/RokUnityMigration/MountainFGammaV2'
PRODUCT = ROOT / 'Saved/RokUnityMigration/MountainFGammaProduct'
BINDING = ROOT / 'Saved/RokUnityMigration/NextWorldMaterialSlice/ue_binding_probe.json'
MATERIAL = '/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_mountain_F.M_WorldSource_build_mountain_F'
MESH = '/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies/SM_RokWorldBody_build_mountain_F_build_mountain_F.SM_RokWorldBody_build_mountain_F_build_mountain_F'
TEXTURE = '/Game/RokUnityWorldMapPrototype/Textures/T_MountainF_UnityDecodedMips.T_MountainF_UnityDecodedMips'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def key_from_tags(tags):
    tile = next((s for s in tags if s.startswith('UnityTile_')), None)
    index = next((s for s in tags if s.startswith('UnityObjectIndex_')), None)
    return f'{tile}/{index}' if tile and index else None


def erode_two_pixels(mask):
    return np.asarray(Image.fromarray(mask.astype(np.uint8) * 255).filter(ImageFilter.MinFilter(5))) == 255


def numeric_ratio_frames(frames, geometry, colors, failures, label, strict=True):
    """Compare identical pixels; selection never depends on response/error."""
    day = frames[0].astype(np.float64)
    rows = []
    for phase in (1, 2):
        target = frames[phase].astype(np.float64)
        for channel, letter in enumerate('RGB'):
            ratio = colors[phase][channel] / colors[0][channel]
            base = day[:, :, channel]
            actual = target[:, :, channel]
            # 8-bit UNORM budget is chosen before looking at UE residuals:
            # reject low denominator, base or expected output near clipping,
            # and actual saturated output. Source native uses the same rule.
            selected = (geometry & (base >= 64) & (base <= 245)
                        & (ratio * base <= 245) & (actual < 250))
            count = int(selected.sum())
            if count < 300:
                failures.append(f'{label} {phase}/{letter} only {count} unsaturated same-pixel channels')
                continue
            error = actual[selected] - ratio * base[selected]
            abs_error = np.abs(error)
            actual_ratio = actual[selected] / base[selected]
            stats = {'phase': ['day', 'dusk', 'night'][phase], 'channel': letter,
                     'eligible_pixels': count, 'expected_gamma_ratio': float(ratio),
                     'observed_ratio_median': float(np.median(actual_ratio)),
                     'wrong_linear_then_encode_ratio': float(ratio ** (1 / 2.2)),
                     'mae_8bit': float(abs_error.mean()), 'p95_8bit': float(np.percentile(abs_error, 95)),
                     'max_8bit': float(abs_error.max()), 'bias_8bit': float(error.mean()),
                     'over_1_5_8bit': int((abs_error > 1.5).sum())}
            rows.append(stats)
            if strict and (stats['mae_8bit'] > .5 or stats['p95_8bit'] > 1 or stats['max_8bit'] > 1.5):
                failures.append(f'{label} {phase}/{letter} Gamma same-pixel UNORM budget exceeded: '
                                f"MAE={stats['mae_8bit']:.3f} P95={stats['p95_8bit']:.3f} max={stats['max_8bit']:.3f}")
    return rows


def native_numeric(source_native, colors, failures):
    reference = []
    paths = []
    for frame in source_native['frames']:
        path = Path(frame['path']).resolve()
        if not path.is_file():
            failures.append(f'native source frame missing: {path}')
            return {}
        image = np.asarray(Image.open(path).convert('RGB'), dtype=np.int16)
        if image.shape != (720, 1280, 3):
            failures.append(f'native source frame size changed: {path}')
            return {}
        paths.append({'name': frame['name'], 'sha256': sha(path)})
        reference.append(image)
    background = reference[0][0, 0]
    corners = ((0, 0), (0, -1), (-1, 0), (-1, -1))
    if not all(np.array_equal(image[y, x], background) for image in reference for y, x in corners):
        failures.append('native F reference backdrop changed across source colors')
        return {}
    # The isolated source backdrop is constant [33,33,33]. This is an
    # inferred source silhouette, not a projection from Unity mesh geometry.
    silhouette = np.max(np.abs(reference[0] - background), axis=2) >= 16
    geometry = erode_two_pixels(silhouette)
    if int(geometry.sum()) < 20000:
        failures.append('inferred native source F interior too small')
        return {}
    rows = numeric_ratio_frames(reference, geometry, colors, failures, 'native')
    return {'mask_kind': 'inferred isolated F silhouette from constant source backdrop; 2px erosion',
            'background_rgb': background.tolist(), 'background_delta_threshold_8bit': 16,
            'mask_pixels': int(geometry.sum()), 'frames': paths, 'same_pixel_ratios': rows}


def projected_geometry(state, failures, label):
    projection = state.get('mesh_projection') or {}
    vertices = projection.get('lod0_vertices') or []
    triangles = projection.get('lod0_triangles') or []
    if (projection.get('viewport_size') != [1440, 810] or projection.get('mesh') != MESH
            or projection.get('component') != state.get('representative_component')
            or projection.get('vertex_count') != len(vertices) or projection.get('triangle_count') != len(triangles)
            or len(vertices) < 100 or len(triangles) < 1000):
        failures.append(f'{label} actual representative LOD0 projection identity/count invalid')
        return None, None
    point = np.asarray(vertices, dtype=np.float64)
    indices = np.asarray(triangles)
    if (point.shape != (len(vertices), 4) or indices.shape != (len(triangles), 3)
            or not np.all(np.isfinite(point)) or not np.issubdtype(indices.dtype, np.integer)
            or np.any(indices < 0) or np.any(indices >= len(vertices))):
        failures.append(f'{label} projected vertices or triangle indices invalid')
        return None, None
    valid = (point[:, 2] == 1) & (point[:, 3] > 0)
    if int(valid.sum()) != projection.get('valid_vertex_count') or valid.mean() < .9:
        failures.append(f'{label} LOD0 projected-valid/depth proof failed')
        return None, None
    canvas = Image.new('L', (1440, 810), 0)
    draw = ImageDraw.Draw(canvas)
    drawn = 0
    for a, b, c in indices:
        if valid[[a, b, c]].all():
            draw.polygon([tuple(point[i, :2]) for i in (a, b, c)], fill=255)
            drawn += 1
    if drawn < 1000:
        failures.append(f'{label} too few valid projected LOD0 triangles')
        return None, None
    raster = np.asarray(canvas) == 255
    interior = erode_two_pixels(raster)
    # Conservative, predeclared world-HUD corridors; never chosen from color
    # response. This only certifies the central visible representative body.
    interior[:140, :] = False
    interior[690:, :] = False
    interior[:, :140] = False
    interior[:, 1300:] = False
    count = int(interior.sum())
    if count < 1000:
        failures.append(f'{label} representative LOD0 interior/HUD-safe pixels too small: {count}')
    return interior, {'mesh': MESH, 'component': projection['component'],
                      'lod0_vertices': len(vertices), 'valid_vertices': int(valid.sum()),
                      'lod0_triangles': len(triangles), 'valid_triangles_drawn': drawn,
                      'raster_pixels': int(raster.sum()), 'eroded_hud_safe_pixels': count,
                      'erosion_pixels': 2, 'hud_corridors_xy': {'left': [0, 140], 'right': [1300, 1440],
                                                              'top': [0, 140], 'bottom': [690, 810]}}


def check_visual(captures, source_native, colors, diagnostic, failures):
    images = []
    geometry = []
    details = []
    for i, capture in enumerate(captures):
        file = Path(capture.get('path', '')).resolve()
        if not file.is_file() or sha(file) != capture.get('sha256'):
            failures.append(f'capture {i} PNG/hash missing')
            return {}
        image = np.asarray(Image.open(file).convert('RGB'), dtype=np.int16)
        if image.shape != (810, 1440, 3):
            failures.append(f'capture {i} viewport is not actual 1440x810')
            return {}
        images.append(image)
        for stage in ('at_request', 'at_processed'):
            state = capture.get(stage) or {}
            if state.get('representative_mesh') != MESH or state.get('uv_channels', 0) < 1 or state.get('vertices', 0) < 100:
                failures.append(f'capture {i} {stage} mesh/UV state missing')
        mask, detail = projected_geometry(capture.get('at_processed') or {}, failures, f'capture {i}')
        if mask is None:
            return {}
        geometry.append(mask)
        details.append(detail)
    if any(detail['component'] != details[0]['component'] for detail in details):
        failures.append('representative F component changed between source-color frames')
        return {}
    projection0 = captures[0]['at_processed']['mesh_projection']
    verts0 = np.asarray(projection0['lod0_vertices'], dtype=np.float64)
    triangles0 = projection0['lod0_triangles']
    max_projection_drift = 0.0
    for i in (1, 2):
        other = captures[i]['at_processed']['mesh_projection']
        if (other['lod0_triangles'] != triangles0
                or other.get('component_transform') != projection0.get('component_transform')):
            failures.append(f'capture {i} actual F component geometry changed between colors')
            return {}
        vertices = np.asarray(other['lod0_vertices'], dtype=np.float64)
        if vertices.shape != verts0.shape or not np.array_equal(vertices[:, 2], verts0[:, 2]):
            failures.append(f'capture {i} actual F projected vertex identity changed')
            return {}
        drift = float(np.max(np.abs(vertices[:, :2] - verts0[:, :2])))
        max_projection_drift = max(max_projection_drift, drift)
        if drift > .5:
            failures.append(f'capture {i} F projection drift {drift:.3f}px exceeds fixed-pixel budget')
            return {}
    # All phases share one actual LOD0 projection. Intersect its independently
    # rasterized fixed masks; no observed color delta may choose ratio pixels.
    fixed = geometry[0] & geometry[1] & geometry[2]
    native = native_numeric(source_native, colors, failures)
    if int(fixed.sum()) < 1000:
        failures.append('fixed representative F triangle mask too small')
        return {'native': native, 'geometry': details}
    day, dusk, night = images
    responded = int(((np.abs(night[:, :, 0] - day[:, :, 0]) >= 8) & fixed).sum())
    response_required = max(300, int(fixed.sum() * .05))
    response_observed = responded >= response_required
    if not response_observed:
        failures.append(f'only {responded} of {int(fixed.sum())} fixed F pixels respond to night red; require {response_required}')
    rows = numeric_ratio_frames(images, fixed, colors, failures, 'UE isolated') if diagnostic else []
    return {'geometry': details, 'fixed_geometry_hud_safe_pixels': int(fixed.sum()),
            'max_projection_drift_pixels': max_projection_drift,
            'response_observed': response_observed, 'night_red_response_pixels': responded,
            'response_required_pixels': response_required,
            'native': native, 'ue_isolate_same_pixel_ratios': rows,
            'ratio_scope': 'Strict UE same-pixel Gamma ratios only with diagnostic_isolate=true; normal world records actual geometry and material response without claiming unoccluded RGB parity.',
            'numeric_budget': 'On preselected 2px-eroded geometry, per-channel day 64..245, expected target <=245, actual target <250: MAE<=0.5, P95<=1, max<=1.5 8-bit units. Source native three-color frames pass this budget.'}


def verify(path, output=None):
    path = Path(path).resolve()
    assert path.name == 'manifest.json' and path.parent.parent == RUNS.resolve()
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    failures = []
    def check(ok, reason):
        if not ok:
            failures.append(reason)
    source = json.loads((SOURCE / 'source_data.json').read_text(encoding='utf-8-sig'))
    native = json.loads((SOURCE / 'native_render.json').read_text(encoding='utf-8-sig'))
    v2verify = json.loads((V2 / 'verify.json').read_text(encoding='utf-8-sig'))
    v2render = json.loads((V2 / 'native_render.json').read_text(encoding='utf-8-sig'))
    product_path = PRODUCT / 'prepare_manifest.json'
    product = json.loads(product_path.read_text(encoding='utf-8-sig'))
    binding = json.loads(BINDING.read_text(encoding='utf-8-sig'))
    check(data.get('passed') and not data.get('failures'), 'runtime C++ audit failed')
    check(data.get('temporary_state_restored'), 'temporary material/visibility/camera state not restored')
    check(data.get('texture') == TEXTURE and data.get('material') == MATERIAL, 'F texture/material scope changed')
    texture = data.get('texture_state') or {}
    check(texture.get('path') == TEXTURE and texture.get('width') == texture.get('height') == 1024 and
          texture.get('mips') == 11 and texture.get('platform_format') == 'B8G8R8A8' and
          texture.get('srgb') is False and texture.get('filter_bilinear') is True and
          texture.get('leave_existing_mips') is True and texture.get('max_texture_size') == 1024 and
          texture.get('never_stream') is True, 'actual F platform texture flags invalid')
    check(data.get('product_manifest_sha256') == sha(product_path), 'DDS product manifest hash changed')
    check(data.get('source_data_sha256') == sha(SOURCE / 'source_data.json'), 'F source data hash changed')
    check(data.get('source_native_render_sha256') == sha(SOURCE / 'native_render.json'), 'old valid native color frames changed')
    check(data.get('v2_source_verify_sha256') == sha(V2 / 'verify.json'), 'verified GPU mip source changed')
    check(v2verify.get('passed') and v2verify.get('native_checked') and product.get('source_orientation') == 'as_saved'
          and product.get('dds_extra_vertical_flip') is False, 'GPU-decoded mip provenance/orientation invalid')
    expected = {key_from_tags(row['tags']): row for row in binding['actors'] if row['label'].endswith('_build_mountain_F')}
    actors = data.get('fresh_f_bodies') or []
    check(len(expected) == len(actors) == len(source['instances']) == 18, 'exactly 18 F source/binding/runtime actors required')
    check({row.get('key') for row in actors} == set(expected), 'fresh F actor identity mismatch')
    for actor in actors:
        original = expected.get(actor.get('key'))
        if not original:
            continue
        check(actor.get('mesh') == original['mesh'] == MESH and actor.get('base_material') == MATERIAL,
              'F mesh/base material differs from current source binding')
        for field in ('location', 'rotation', 'scale'):
            if len(actor.get(field, [])) != 3 or any(abs(a - b) > (0.1 if field == 'location' else 0.01)
                                                       for a, b in zip(actor[field], original[field])):
                failures.append(f'{actor.get("key")} {field} changed')
    mips = data.get('platform_mips') or []
    check(len(mips) == len(product['mips']) == len(v2render['mips']) == 11, 'all eleven F platform mips required')
    for i, row in enumerate(mips):
        reference = product['mips'][i]
        native_mip = v2render['mips'][i]
        size = max(1, 1024 >> i)
        check(row.get('level') == reference['level'] == i and row.get('width') == row.get('height') == size and
              row.get('bytes') == reference['bytes'] == size * size * 4 and
              row.get('sha256') == reference['bgra8_sha256'], f'F platform mip {i} CPU BGRA bytes')
        check(reference['source_png_sha256'] == sha(native_mip['rgbaPath']) and
              reference['source_bc1_sha256'] == sha(native_mip['bc1Path']) and
              reference['source_orientation'] == 'as_saved', f'F mip {i} native GPU source correspondence')
    captures = data.get('captures') or []
    check(len(captures) == 3 and [c.get('name') for c in captures] == ['stable_day', 'stable_dusk', 'stable_night'],
          'three source-color screenshot callback captures required')
    for i, capture in enumerate(captures):
        for stage in ('at_request', 'at_processed'):
            actual = capture.get(stage, {}).get('actual_sprite_color') or []
            expected_color = source['samples'][i]['spriteColor'] if i < 3 else []
            check(len(actual) == len(expected_color) == 3 and all(math.isfinite(v) and abs(v - e) < 1e-4
                  for v, e in zip(actual, expected_color)), f'capture {i} {stage} source Gamma color')
    diagnostic = bool(data.get('diagnostic_isolate'))
    colors = [sample['spriteColor'] for sample in source['samples']]
    check(len(native.get('frames', [])) == 3 and all(
        len(frame.get('spriteColor', [])) == 3 and all(abs(a - b) < 1e-4 for a, b in zip(frame['spriteColor'], colors[i]))
        for i, frame in enumerate(native.get('frames', []))), 'native three-frame source Gamma colors changed')
    visual = check_visual(captures, native, colors, diagnostic, failures) if len(captures) == 3 else {}
    result = {'passed': not failures, 'failures': failures, 'manifest_sha256': sha(path),
              'visual': visual, 'diagnostic_isolate': diagnostic,
              'normal_world_visual_accepted': False,
              'normal_world_representative_response_accepted': not failures and not diagnostic and bool(visual.get('response_observed')),
              'scope': 'Fresh 18 F body identities/transform/material chain and exact eleven platform BGRA mips. Native three-color Gamma same-pixel ratios use an inferred isolated-source silhouette; UE ratio parity is certified only for the diagnostic-isolate representative LOD0 triangle interior after fixed HUD exclusion. Normal world proves projected geometry and material response only. No full-map or full-shader parity.'}
    target = Path(output) if output else path.parent / 'verify.json'
    target.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest')
    parser.add_argument('--output')
    args = parser.parse_args()
    result = verify(args.manifest, args.output)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result['passed'] else 1)
