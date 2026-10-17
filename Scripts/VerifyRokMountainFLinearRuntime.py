"""Verify representative Mountain F material arithmetic in pre-tonemap RGBA16F.

The day capture is the same-pixel texture anchor. It is not an independent
absolute texture/UV/mip oracle; the independent inputs are Unity source colors,
the material's documented sRGB transfer, and a geometry-only pixel mask.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'Saved/RokUnityMigration/MountainFLinearAudit'
SOURCE = ROOT / 'Saved/RokUnityMigration/MountainFGamma/source_data.json'
PRODUCT = ROOT / 'Saved/RokUnityMigration/MountainFGammaProduct/prepare_manifest.json'
BINDING = ROOT / 'Saved/RokUnityMigration/NextWorldMaterialSlice/ue_binding_probe.json'
MATERIAL = '/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_mountain_F.M_WorldSource_build_mountain_F'
MESH = '/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies/SM_RokWorldBody_build_mountain_F_build_mountain_F.SM_RokWorldBody_build_mountain_F_build_mountain_F'
TEXTURE = '/Game/RokUnityWorldMapPrototype/Textures/T_MountainF_UnityDecodedMips.T_MountainF_UnityDecodedMips'
SIZE = (1440, 900)
NAMES = ('stable_day', 'stable_dusk', 'stable_night')
BUDGET = {'mae_8bit': 0.5, 'p95_8bit': 1.0, 'max_8bit': 1.5}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def decode_srgb(value):
    value = np.asarray(value, dtype=np.float64)
    return np.where(value <= 0.04045, value / 12.92,
                    np.power((value + 0.055) / 1.055, 2.4))


def encode_srgb(value):
    value = np.asarray(value, dtype=np.float64)
    return np.where(value <= 0.0031308, 12.92 * value,
                    1.055 * np.power(np.maximum(value, 0), 1 / 2.4) - 0.055)


def required(ok, failure, failures):
    if not ok:
        failures.append(failure)
    return bool(ok)


def fixed_geometry(geometry, failures):
    if geometry.get('schema') != 'rok.mountain_f_linear_geometry.v1' or geometry.get('size') != list(SIZE):
        failures.append('Geometry schema or capture dimensions differ')
        return None
    vertices = np.asarray(geometry.get('vertices_xy_depth_uv', []), dtype=np.float64)
    triangles = np.asarray(geometry.get('triangles', []), dtype=np.int64)
    if (vertices.ndim != 2 or vertices.shape[1] != 5 or vertices.shape[0] < 100 or
            triangles.ndim != 2 or triangles.shape[1] != 3 or triangles.shape[0] < 1000 or
            not np.isfinite(vertices).all() or np.any(triangles < 0) or
            np.any(triangles >= len(vertices))):
        failures.append('Invalid geometry topology or projection')
        return None
    if geometry.get('valid_vertices', 0) < int(len(vertices) * .9):
        failures.append('Too many invalid projected F vertices')
        return None
    canvas = Image.new('L', SIZE, 0)
    draw = ImageDraw.Draw(canvas)
    drawn = 0
    for tri in triangles:
        points = vertices[tri]
        if np.all(points[:, 2] > 0):
            draw.polygon([tuple(xy) for xy in points[:, :2]], fill=255)
            drawn += 1
    if drawn < 1000:
        failures.append('Too few visible-depth F triangles')
        return None
    # Fixed before reading any capture RGB; erode two pixels to avoid edge coverage.
    mask = np.asarray(canvas.filter(ImageFilter.MinFilter(5))) == 255
    if int(mask.sum()) < 1000:
        failures.append('Predeclared F interior is too small')
        return None
    return mask


def load_raw(frame, failures):
    path = Path(frame.get('path', '')).resolve()
    if (not path.is_file() or path.parent.parent != RUNS.resolve() or
            path.suffix != '.rgba16f' or sha(path) != frame.get('sha256') or
            path.stat().st_size != SIZE[0] * SIZE[1] * 8 or
            frame.get('bytes') != SIZE[0] * SIZE[1] * 8):
        failures.append(f'{frame.get("name")} raw capture path/hash/layout differs')
        return None
    # FFloat16Color is R,G,B,A; lossless half-float bytes are retained on disk.
    return np.fromfile(path, dtype='<f2').reshape(SIZE[1], SIZE[0], 4).astype(np.float64)


def verify(path, output=None):
    path = Path(path).resolve()
    if path.name != 'manifest.json' or path.parent.parent != RUNS.resolve():
        raise ValueError('Pass a manifest under Saved/RokUnityMigration/MountainFLinearAudit/<run>/')
    manifest = json.loads(path.read_text(encoding='utf-8-sig'))
    source = json.loads(SOURCE.read_text(encoding='utf-8-sig'))
    product = json.loads(PRODUCT.read_text(encoding='utf-8-sig'))
    binding = json.loads(BINDING.read_text(encoding='utf-8-sig'))
    failures = []
    required(manifest.get('schema') == 'rok.mountain_f_linear_audit.v1', 'Audit schema differs', failures)
    required(manifest.get('capture_completed') and not manifest.get('failures') and
             manifest.get('temporary_state_restored'), 'C++ capture/restoration failed', failures)
    required(manifest.get('world') == '/Game/Maps/L_RokUnityWorldMapPrototype' and
             Path(manifest.get('project', '')).resolve() == ROOT.resolve(), 'Wrong project/world', failures)
    required(manifest.get('capture_source') == 'SCS_SceneColorHDRNoAlpha' and
             manifest.get('target_format') == 'PF_FloatRGBA' and manifest.get('size') == list(SIZE) and
             manifest.get('render_mode') == 'PRM_UseShowOnlyList, representative F mesh only',
             'Linear HDR capture mode differs', failures)
    settings = manifest.get('capture_settings') or {}
    required(settings.get('capture_every_frame') is False and
             settings.get('capture_on_movement') is False and
             settings.get('persist_render_state') is False and
             settings.get('anti_aliasing') is False and
             settings.get('temporal_aa') is False and
             settings.get('eye_adaptation') is False and
             settings.get('motion_blur') is False and
             settings.get('show_only_components') == 1 and
             manifest.get('target_force_linear_gamma') is True,
             'SceneCapture actual flags differ', failures)
    required(manifest.get('source_data_sha256') == sha(SOURCE) and
             manifest.get('product_manifest_sha256') == sha(PRODUCT) and
             manifest.get('platform_mip0_sha256') == product['mips'][0]['bgra8_sha256'] and
             manifest.get('platform_mip_sha256') == [m['bgra8_sha256'] for m in product['mips']],
             'Native source/product/platform mip provenance differs', failures)
    required(manifest.get('material') == MATERIAL and manifest.get('texture') == TEXTURE and
             manifest.get('representative_mesh') == MESH and
             manifest.get('representative_original_material') == MATERIAL,
             'Representative F original material/texture/mesh differs', failures)
    key = manifest.get('representative_source_key')
    bindings = {f"{next((x for x in row.get('tags', []) if x.startswith('UnityTile_')), '')}/"
                f"{next((x for x in row.get('tags', []) if x.startswith('UnityObjectIndex_')), '')}": row
                for row in binding.get('actors', []) if row.get('label', '').endswith('_build_mountain_F')}
    required(len(bindings) == 18 and key in bindings and bindings.get(key, {}).get('mesh') == MESH,
             'Representative source identity absent from 18 F bindings', failures)
    geometry_path = Path(manifest.get('geometry_path', '')).resolve()
    if (not geometry_path.is_file() or geometry_path.parent != path.parent or
            sha(geometry_path) != manifest.get('geometry_sha256')):
        failures.append('Independent geometry file/hash missing')
        mask = None
    else:
        mask = fixed_geometry(json.loads(geometry_path.read_text(encoding='utf-8-sig')), failures)
    colors = [row.get('spriteColor') for row in source.get('samples', [])]
    required(len(colors) == 3 and [row.get('name') for row in source.get('samples', [])] == list(NAMES),
             'Unity native color samples differ', failures)
    frames = manifest.get('frames') or []
    required(len(frames) == 3 and [row.get('name') for row in frames] == list(NAMES),
             'Three HDR captures required', failures)
    arrays = [load_raw(row, failures) for row in frames] if len(frames) == 3 else []
    if len(frames) == 3 and len(colors) == 3:
        ref_camera = [frames[0].get(x) for x in ('camera_position', 'camera_rotation_pyr', 'camera_horizontal_fov', 'camera_aspect', 'dxf')]
        for i, frame in enumerate(frames):
            actual_color = frame.get('source_color') or []
            required(len(actual_color) == 3 and all(abs(float(a) - float(b)) < 1e-5
                      for a, b in zip(actual_color, colors[i])),
                     f'{NAMES[i]} source color differs', failures)
            required(frame.get('pre_exposure_override') == 1.0, f'{NAMES[i]} pre-exposure differs', failures)
            required([frame.get(x) for x in ('camera_position', 'camera_rotation_pyr', 'camera_horizontal_fov', 'camera_aspect', 'dxf')] == ref_camera,
                     f'{NAMES[i]} camera drifted', failures)
        required(abs(float(frames[0].get('camera_aspect', 0)) - 1.6) < 1e-4,
                 'Capture/source camera aspect not 16:10', failures)

    rows = []
    if mask is not None and len(arrays) == 3 and all(a is not None for a in arrays) and not failures:
        rgb = [a[:, :, :3] for a in arrays]
        for i, values in enumerate(rgb):
            if not np.isfinite(values[mask]).all() or np.any(values[mask] < -0.001):
                failures.append(f'{NAMES[i]} fixed F interior contains invalid/negative HDR')
        if not failures:
            day = rgb[0]
            if np.count_nonzero(np.max(day[mask], axis=1) > 0.05) < max(300, int(mask.sum() * .05)):
                failures.append('Fixed geometry lacks representative F material signal')
            for phase in (1, 2):
                for channel, letter in enumerate('RGB'):
                    ratio = float(colors[phase][channel]) / float(colors[0][channel])
                    expected_linear = decode_srgb(encode_srgb(np.maximum(day[:, :, channel], 0)) * ratio)
                    actual_linear = rgb[phase][:, :, channel]
                    linear_delta = (actual_linear - expected_linear)[mask]
                    # No SDR clipping or saturation filter: transfer extends above 1.
                    # Convert only the residual to 8-bit gamma units for the frozen
                    # original MAE/P95/max budget; raw HDR values remain untouched.
                    encoded_delta = (encode_srgb(np.maximum(actual_linear, 0)) -
                                     encode_srgb(expected_linear))[mask] * 255
                    abs_error = np.abs(encoded_delta)
                    stats = {'phase': NAMES[phase], 'channel': letter,
                             'geometry_pixels': int(mask.sum()), 'source_gamma_ratio': ratio,
                             'mae_8bit': float(abs_error.mean()),
                             'p95_8bit': float(np.percentile(abs_error, 95)),
                             'max_8bit': float(abs_error.max()),
                             'linear_mae': float(np.abs(linear_delta).mean()),
                             'linear_max': float(np.abs(linear_delta).max()),
                             'hdr_above_one_pixels': int(np.count_nonzero(actual_linear[mask] > 1))}
                    rows.append(stats)
                    if any(stats[name] > budget for name, budget in BUDGET.items()):
                        failures.append(f'{NAMES[phase]}/{letter} linear HDR arithmetic exceeds frozen 8-bit residual budget: '
                                        f'MAE={stats["mae_8bit"]:.3f} P95={stats["p95_8bit"]:.3f} max={stats["max_8bit"]:.3f}')
    result = {'schema': 'rok.mountain_f_linear_verify.v1', 'passed': not failures,
              'failures': failures, 'manifest_sha256': sha(path),
              'geometry_pixels': int(mask.sum()) if mask is not None else 0,
              'rows': rows, 'budget': BUDGET,
              'scope': 'Same-pixel representative F color multiplication and exact sRGB transfer in pre-tonemap SceneColor. Day is an observed texture anchor; absolute source texture UV/mip, all-18 shader parity, and final viewport parity are not certified.'}
    destination = Path(output).resolve() if output else path.parent / 'verify.json'
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest')
    parser.add_argument('--output')
    args = parser.parse_args()
    result = verify(args.manifest, args.output)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result['passed'] else 1)
