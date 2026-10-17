"""Check the scoped Mountain F viewport output-chain experiment.

Normal SDR retains the historical strict Unity-ratio budget. GammaOnly has a
separate, predeclared oracle from matched scene-color HDR and display gamma;
it is an output-chain diagnostic, not Unity final-presentation acceptance.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

from VerifyRokMountainFGammaRuntime import projected_geometry


ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'Saved/RokUnityMigration/MountainFGammaAudit'
SOURCE = ROOT / 'Saved/RokUnityMigration/MountainFGamma/source_data.json'
PRODUCT = ROOT / 'Saved/RokUnityMigration/MountainFGammaProduct/prepare_manifest.json'
HISTORICAL_FRAMING = RUNS / '20260925_120204_30176/manifest.json'
NAMES = ('stable_day', 'stable_dusk', 'stable_night')
MODES = ('normal', 'gamma_only')
SIZE = (1440, 810)
STRICT = {'mae_8bit': 0.5, 'p95_8bit': 1.0, 'max_8bit': 1.5}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def decode_srgb(value):
    value = np.asarray(value, dtype=np.float64)
    return np.where(value <= 0.04045, value / 12.92,
                    np.power((value + 0.055) / 1.055, 2.4))


def encode_srgb(value):
    value = np.maximum(np.asarray(value, dtype=np.float64), 0)
    return np.where(value <= 0.0031308, 12.92 * value,
                    1.055 * np.power(value, 1 / 2.4) - 0.055)


def add_check(failures, condition, message):
    if not condition:
        failures.append(message)
    return bool(condition)


def cvar_number(row):
    try:
        return float(row['value'])
    except (KeyError, TypeError, ValueError):
        return math.nan


def errors(actual, expected, selected, phase, channel, label, failures):
    count = int(selected.sum())
    if count < 300:
        failures.append(f'{label} {phase}/{channel}: only {count} preselected pixels')
        return None
    delta = (actual - expected)[selected]
    absolute = np.abs(delta)
    row = {'mode': label, 'phase': phase, 'channel': channel, 'pixels': count,
           'mae_8bit': float(absolute.mean()),
           'p95_8bit': float(np.percentile(absolute, 95)),
           'max_8bit': float(absolute.max()), 'bias_8bit': float(delta.mean()),
           'over_1_5_8bit': int(np.count_nonzero(absolute > 1.5))}
    if any(row[key] > limit for key, limit in STRICT.items()):
        failures.append(f'{label} {phase}/{channel} exceeds frozen {STRICT} budget: '
                        f'MAE={row["mae_8bit"]:.3f} P95={row["p95_8bit"]:.3f} '
                        f'max={row["max_8bit"]:.3f}')
    return row


def load_frame(capture, run, index, failures):
    image_path = Path(capture.get('path', '')).resolve()
    raw = capture.get('pre_png_fcolor') or {}
    raw_path = Path(raw.get('path', '')).resolve()
    hdr = capture.get('aligned_scene_color') or {}
    hdr_path = Path(hdr.get('path', '')).resolve()
    if not (image_path.parent == raw_path.parent == hdr_path.parent == run and
            image_path.suffix == '.png' and raw_path.suffix == '.bgra8' and
            hdr_path.suffix == '.rgba16f' and
            image_path.is_file() and raw_path.is_file() and hdr_path.is_file()):
        failures.append(f'capture {index}: raw/PNG/HDR path or file missing')
        return None
    if not (sha(image_path) == capture.get('sha256') and
            sha(raw_path) == raw.get('sha256') and sha(hdr_path) == hdr.get('sha256')):
        failures.append(f'capture {index}: raw/PNG/HDR hash mismatch')
        return None
    if raw_path.stat().st_size != SIZE[0] * SIZE[1] * 4 or raw.get('bytes') != SIZE[0] * SIZE[1] * 4:
        failures.append(f'capture {index}: BGRA8 byte count differs')
        return None
    offset = hdr.get('view_rect_offset') or []
    size = hdr.get('size') or []
    if (len(offset) != 2 or len(size) != 2 or not all(isinstance(x, int) for x in offset + size) or
            size[0] <= 0 or size[1] <= 0 or offset[0] < 0 or offset[1] < 0 or
            offset[0] + size[0] > SIZE[0] or offset[1] + size[1] > SIZE[1] or
            hdr_path.stat().st_size != size[0] * size[1] * 8 or hdr.get('bytes') != size[0] * size[1] * 8):
        failures.append(f'capture {index}: aligned HDR extent/bytes differ')
        return None
    if (hdr.get('capture_source') != 'SCS_SceneColorHDRNoAlpha' or
            hdr.get('target_format') != 'PF_FloatRGBA' or
            hdr.get('show_only_representative') is not True or
            hdr.get('aa_disabled') is not True or hdr.get('eye_adaptation_disabled') is not True or
            hdr.get('pre_exposure_override') != 1.0 or
            not math.isfinite(hdr.get('max_projection_drift_pixels', math.inf)) or
            hdr['max_projection_drift_pixels'] > .25 or hdr.get('compared_vertices', 0) < 100):
        failures.append(f'capture {index}: aligned HDR source/projection settings differ')
        return None
    bgra = np.fromfile(raw_path, dtype=np.uint8).reshape(SIZE[1], SIZE[0], 4)
    rgba = bgra[:, :, [2, 1, 0, 3]]
    with Image.open(image_path) as image:
        png = np.asarray(image.convert('RGBA'))
    if png.shape != rgba.shape or not np.array_equal(png, rgba):
        failures.append(f'capture {index}: PNG differs from pre-encoder viewport FColor')
        return None
    if not np.all(bgra[:, :, 3] == 255):
        failures.append(f'capture {index}: FColor delegate alpha is not opaque')
    scene = np.fromfile(hdr_path, dtype='<f2').reshape(size[1], size[0], 4).astype(np.float64)
    return rgba[:, :, :3].astype(np.float64), scene[:, :, :3], (offset[0], offset[1], size[0], size[1])


def verify(path, output=None):
    path = Path(path).resolve()
    if path.name != 'manifest.json' or path.parent.parent != RUNS.resolve():
        raise ValueError('Pass a MountainFGammaAudit/<run>/manifest.json')
    manifest = json.loads(path.read_text(encoding='utf-8-sig'))
    source = json.loads(SOURCE.read_text(encoding='utf-8-sig'))
    product = json.loads(PRODUCT.read_text(encoding='utf-8-sig'))
    historical = json.loads(HISTORICAL_FRAMING.read_text(encoding='utf-8-sig'))
    failures = []
    historical_first = (historical.get('captures') or [{}])[0].get('at_request') or {}
    historical_dxf = historical_first.get('dxf')
    framing = manifest.get('source_camera_framing') or {}
    add_check(failures, historical.get('passed') is True and
              historical.get('diagnostic_isolate') is True and
              historical_first.get('mesh_projection', {}).get('viewport_size') == list(SIZE) and
              historical_first.get('representative_component', '').endswith('StaticMeshActor_97.StaticMeshComponent0') and
              isinstance(historical_dxf, (int, float)) and
              framing.get('method') ==
              'original F Gamma audit Stage0: start dxf 1509, multiply 1.15 only before first framed screenshot' and
              framing.get('locked_before_first_capture') is True and
              framing.get('pre_capture_steps') == 12 and
              abs(framing.get('initial_dxf', math.inf) - 1509) < .01 and
              abs(framing.get('locked_dxf', math.inf) - historical_dxf) < .01,
              'source-camera framing differs from original fixed 1440x810 strict-SDR run')
    add_check(failures, manifest.get('passed') and not manifest.get('failures') and
              manifest.get('temporary_state_restored') and manifest.get('sdr_output_diagnostic') is True and
              manifest.get('diagnostic_isolate') is True, 'C++ six-frame diagnostic/restoration failed')
    recorded_lut_dimension = manifest.get('sdr_configured_lut_dimension')
    legacy_lut_metadata = recorded_lut_dimension is None
    configured_lut_dimension = 32 if legacy_lut_metadata else recorded_lut_dimension
    add_check(failures, isinstance(configured_lut_dimension, (int, float)) and
              not isinstance(configured_lut_dimension, bool) and
              configured_lut_dimension in (32, 64),
              'SDR diagnostic configured LUT dimension must be 32 or 64')
    add_check(failures, manifest.get('source_data_sha256') == sha(SOURCE) and
              manifest.get('product_manifest_sha256') == sha(PRODUCT) and
              manifest.get('texture_state', {}).get('platform_format') == 'B8G8R8A8' and
              len(manifest.get('platform_mips') or []) == 11 and
              [row.get('sha256') for row in manifest.get('platform_mips', [])] ==
              [row.get('bgra8_sha256') for row in product.get('mips', [])],
              'source/mip platform provenance changed')
    add_check(failures, len(manifest.get('fresh_f_bodies') or []) == 18 and
              manifest.get('representative_source_key') in
              {row.get('key') for row in manifest.get('fresh_f_bodies', [])},
              'representative source identity or 18 F actors missing')
    body_keys = {row.get('key') for row in manifest.get('fresh_f_bodies', [])}
    hidden_keys = manifest.get('temporarily_hidden_f_source_keys') or []
    hidden_records = [row for row in manifest.get('temporarily_hidden_components', [])
                      if row.get('reason') == 'match representative-only HDR capture']
    add_check(failures, len(hidden_keys) == len(set(hidden_keys)) == len(hidden_records) == 17 and
              set(hidden_keys) == body_keys - {manifest.get('representative_source_key')} and
              {row.get('source_key') for row in hidden_records} == set(hidden_keys) and
              all(row.get('was_visible') is True and row.get('was_hidden_in_game') is False
                  for row in hidden_records),
              'viewport/HDR representative-only F geometry isolation differs')
    colors = [row.get('spriteColor') for row in source.get('samples', [])]
    add_check(failures, len(colors) == 3 and
              [row.get('name') for row in source.get('samples', [])] == list(NAMES),
              'Unity source colors missing')
    captures = manifest.get('captures') or []
    add_check(failures, len(captures) == 6 and
              [row.get('name') for row in captures] == list(NAMES) * 2 and
              [row.get('output_mode') for row in captures] == ['normal'] * 3 + ['gamma_only'] * 3,
              'normal/GammaOnly capture sequence changed')
    if len(captures) != 6 or len(colors) != 3:
        result = {'passed': False, 'failures': failures, 'manifest_sha256': sha(path)}
        destination = Path(output).resolve() if output else path.parent / 'sdr_output_verify.json'
        destination.write_text(json.dumps(result, indent=2), encoding='utf-8')
        return result

    arrays, masks, details, states = [], [], [], []
    baseline_output_state = None
    for i, capture in enumerate(captures):
        mode = MODES[i // 3]
        phase = i % 3
        add_check(failures, Path(capture.get('path', '')).name ==
                  f'{i:02d}_{mode}_{NAMES[phase]}.png', f'capture {i}: PNG identity changed')
        add_check(failures, capture.get('screenshot_show_ui') is False,
                  f'capture {i}: screenshot was not explicit game-viewport path')
        loaded = load_frame(capture, path.parent, i, failures)
        arrays.append(loaded)
        before, after = capture.get('at_request') or {}, capture.get('at_processed') or {}
        states.append(after)
        for stage, state in (('request', before), ('processed', after)):
            actual = state.get('actual_sprite_color') or []
            add_check(failures, len(actual) == 3 and all(
                abs(float(a) - float(b)) < 1e-4 for a, b in zip(actual, colors[phase])),
                f'capture {i} {stage}: source color changed')
            view = state.get('sdr_viewport') or {}
            flags = view.get('game_viewport_show_flags') or {}
            rect = view.get('constrained_view_rect') or []
            add_check(failures, view.get('render_target_size') == list(SIZE) and
                      rect == manifest.get('effective_view_rect') and
                      (view.get('configured_lut_dimension') == configured_lut_dimension
                       if not legacy_lut_metadata else
                       view.get('configured_lut_dimension') is None) and
                      view.get('effective_lut_pixel_format') == 'not_captured_private_view_state' and
                      view.get('display_output_format_enum') == 0 and
                      view.get('backbuffer_source') == 'Slate viewport provider, queried on render thread' and
                      view.get('actual_backbuffer_size') == list(SIZE) and
                      view.get('actual_backbuffer_pixel_format') in ('B8G8R8A8', 'R8G8B8A8', 'A2B10G10R10') and
                      (view.get('render_target_pixel_format') in ('B8G8R8A8', 'R8G8B8A8', 'A2B10G10R10')
                       if view.get('game_thread_render_target_present') else
                       view.get('render_target_pixel_format') == 'unavailable_direct_to_window') and
                      (view.get('shader_resource_pixel_format') in ('B8G8R8A8', 'R8G8B8A8', 'A2B10G10R10')
                       if view.get('game_thread_shader_resource_present') else
                       view.get('shader_resource_pixel_format') == 'unavailable_optional') and
                      view.get('scene_hdr_enabled') is False and
                      math.isfinite(view.get('display_gamma', math.nan)) and
                      1.8 <= view['display_gamma'] <= 2.6 and
                      flags.get('tonemapper') is (mode == 'normal') and
                      flags.get('post_processing') is True and
                      flags.get('anti_aliasing') is False and
                      flags.get('eye_adaptation') is False,
                      f'capture {i} {stage}: actual viewport output/showflags differ')
            cvars = view.get('cvars') or {}
            for name, expected in (('r.AntiAliasingMethod', 0), ('r.ScreenPercentage', 100),
                                   ('r.BackbufferQuantizationDitheringOverride', 16),
                                   ('r.TonemapperGamma', 0),
                                   ('r.EyeAdaptation.PreExposureOverride', 1),
                                   ('r.DefaultBackBufferPixelFormat', 4)):
                add_check(failures, abs(cvar_number(cvars.get(name) or {}) - expected) < .00001,
                          f'capture {i} {stage}: {name} changed')
            lut_cvar = cvars.get('r.LUT.Size') or {}
            add_check(failures,
                      cvar_number(lut_cvar) == configured_lut_dimension and
                      isinstance(lut_cvar.get('flags'), (int, float)) and
                      not isinstance(lut_cvar.get('flags'), bool) and
                      lut_cvar['flags'] == int(lut_cvar['flags']) and
                      (int(lut_cvar['flags']) & 0xff000000) in (0x0d000000, 0x10000000),
                      f'capture {i} {stage}: r.LUT.Size differs from explicitly locked 32/64 dimension')
            fixed_output = {'render_target_size': view.get('render_target_size'),
                            'configured_lut_dimension': view.get('configured_lut_dimension'),
                            'constrained_view_rect': rect,
                            'render_target_pixel_format': view.get('render_target_pixel_format'),
                            'shader_resource_pixel_format': view.get('shader_resource_pixel_format'),
                            'game_thread_render_target_present': view.get('game_thread_render_target_present'),
                            'game_thread_shader_resource_present': view.get('game_thread_shader_resource_present'),
                            'backbuffer_source': view.get('backbuffer_source'),
                            'actual_backbuffer_size': view.get('actual_backbuffer_size'),
                            'actual_backbuffer_pixel_format': view.get('actual_backbuffer_pixel_format'),
                            'display_output_format_enum': view.get('display_output_format_enum'),
                            'display_gamma': view.get('display_gamma'),
                            'scene_hdr_enabled': view.get('scene_hdr_enabled'),
                            'cvars': cvars,
                            'show_flags_except_tonemapper': {k: v for k, v in flags.items()
                                                             if k != 'tonemapper'},
                            'camera_post_process': state.get('render_inputs', {}).get('camera_post_process')}
            if baseline_output_state is None:
                baseline_output_state = fixed_output
            else:
                add_check(failures, fixed_output == baseline_output_state,
                          f'capture {i} {stage}: output control besides Tonemapper changed')
        mask, detail = projected_geometry(after, failures, f'capture {i}')
        masks.append(mask)
        details.append(detail)
        if loaded is not None:
            hdr = capture['aligned_scene_color']
            view = after.get('sdr_viewport') or {}
            rect = view.get('constrained_view_rect') or []
            add_check(failures, len(rect) == 4 and loaded[2] ==
                      (rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1]),
                      f'capture {i}: HDR size/offset do not match effective ViewRect')
            add_check(failures, hdr.get('camera_position') == after.get('camera_position') and
                      abs(hdr.get('camera_horizontal_fov', math.inf) -
                          (captures[0]['aligned_scene_color'].get('camera_horizontal_fov') or -math.inf)) < 1e-6,
                      f'capture {i}: HDR camera position/FOV changed')

    rows_normal, rows_gamma, rows_hdr, rows_mode_delta = [], [], [], []
    fixed_count = 0
    max_vertex_drift = math.nan
    max_normal_gamma_hdr_delta = math.nan
    if all(row is not None for row in arrays) and all(mask is not None for mask in masks):
        first = states[0]
        add_check(failures, first.get('representative_component') ==
                  historical_first.get('representative_component') and
                  abs(first.get('dxf', math.inf) - historical_dxf) < .01 and
                  first.get('mesh_projection', {}).get('viewport_size') == list(SIZE) and
                  len(first.get('camera_position') or []) == 3 and
                  max(abs(a - b) for a, b in zip(first['camera_position'],
                                                  historical_first['camera_position'])) < .01,
                  'first capture differs from historical source-camera pose/identity')
        baseline = states[0]['mesh_projection']
        base_vertices = np.asarray(baseline['lod0_vertices'], dtype=np.float64)
        max_vertex_drift = 0.0
        for i, state in enumerate(states):
            projection = state['mesh_projection']
            vertices = np.asarray(projection['lod0_vertices'], dtype=np.float64)
            if (projection.get('lod0_triangles') != baseline.get('lod0_triangles') or
                    projection.get('component_transform') != baseline.get('component_transform') or
                    state.get('camera_position') != states[0].get('camera_position') or
                    state.get('dxf') != states[0].get('dxf') or
                    vertices.shape != base_vertices.shape):
                failures.append(f'capture {i}: camera, geometry or LOD0 topology changed')
                continue
            drift = float(np.max(np.abs(vertices[:, :2] - base_vertices[:, :2])))
            max_vertex_drift = max(max_vertex_drift, drift)
            if drift > .25:
                failures.append(f'capture {i}: projected geometry moved {drift:.6f}px')
        fixed = np.logical_and.reduce(masks)
        fixed_count = int(fixed.sum())
        add_check(failures, fixed_count >= 1000, 'fixed F geometry interior too small')
        view_rects = [row[2] for row in arrays]
        add_check(failures, all(rect == view_rects[0] for rect in view_rects),
                  'aligned HDR view rect changed')
        x, y, width, height = view_rects[0]
        hdr_mask = fixed[y:y + height, x:x + width]
        hdr = [row[1] for row in arrays]
        sdr = [row[0] for row in arrays]
        if not all(np.isfinite(frame[hdr_mask]).all() and
                   np.all(frame[hdr_mask] >= -0.001) for frame in hdr):
            failures.append('fixed F interior contains invalid/negative scene color')
        max_normal_gamma_hdr_delta = float(max(np.max(np.abs(hdr[i] - hdr[i + 3])[hdr_mask])
                                               for i in range(3)))
        if max_normal_gamma_hdr_delta > .01:
            failures.append(f'normal/GammaOnly pre-tonemap HDR changed: {max_normal_gamma_hdr_delta:.6f}')
        gamma_values = [state['sdr_viewport']['display_gamma'] for state in states]
        if max(gamma_values) - min(gamma_values) > .001:
            failures.append('display gamma changed between output modes/phases')
        gamma = gamma_values[0]
        for phase in range(3):
            for channel, letter in enumerate('RGB'):
                normal_channel = sdr[phase][:, :, channel]
                gamma_channel = sdr[phase + 3][:, :, channel]
                absolute = np.abs(normal_channel[fixed] - gamma_channel[fixed])
                rows_mode_delta.append({'phase': NAMES[phase], 'channel': letter,
                                        'fixed_pixels': fixed_count,
                                        'normal_minus_gamma_mae_8bit': float(absolute.mean()),
                                        'normal_minus_gamma_p95_8bit': float(np.percentile(absolute, 95)),
                                        'normal_minus_gamma_max_8bit': float(absolute.max())})
        for phase in (1, 2):
            for channel, letter in enumerate('RGB'):
                ratio = float(colors[phase][channel]) / float(colors[0][channel])
                # This is the historical strict source Gamma ratio; record and
                # gate it without changing the old verifier or its budget.
                day = sdr[0][:, :, channel]
                actual = sdr[phase][:, :, channel]
                expected = ratio * day
                selected = fixed & (day >= 64) & (day <= 245) & (expected <= 245) & (actual < 250)
                row = errors(actual, expected, selected, NAMES[phase], letter, 'normal_sdr', failures)
                if row:
                    row['source_gamma_ratio'] = ratio
                    rows_normal.append(row)

                hdr_day = hdr[3][:, :, channel]
                hdr_target = hdr[phase + 3][:, :, channel]
                expected_hdr = decode_srgb(encode_srgb(np.maximum(hdr_day, 0)) * ratio)
                encoded_delta = (encode_srgb(hdr_target) - encode_srgb(expected_hdr)) * 255
                selected_hdr = hdr_mask & (hdr_day > .005)
                row = errors(encoded_delta, np.zeros_like(encoded_delta), selected_hdr,
                             NAMES[phase], letter, 'gamma_only_pretonemap_arithmetic', failures)
                if row:
                    row['source_gamma_ratio'] = ratio
                    row['hdr_above_one_pixels'] = int(np.count_nonzero(hdr_target[selected_hdr] > 1))
                    rows_hdr.append(row)

                # Tonemapper=false shader uses pow(SceneColor * exposure,
                # 1/displayGamma). Day output anchors an unknown common
                # exposure; independently captured HDR supplies the ratio.
                # No SDR clipping assumption is applied to HDR itself.
                gamma_day = sdr[3][y:y + height, x:x + width, channel]
                gamma_actual = sdr[phase + 3][y:y + height, x:x + width, channel]
                hdr_ratio = np.divide(hdr_target, hdr_day,
                                      out=np.zeros_like(hdr_target), where=hdr_day > .005)
                expected_gamma = gamma_day * np.power(np.maximum(hdr_ratio, 0), 1 / gamma)
                selected_gamma = (hdr_mask & (hdr_day > .005) & (gamma_day >= 64) &
                                  (gamma_day <= 245) & (expected_gamma <= 245))
                row = errors(gamma_actual, expected_gamma, selected_gamma,
                             NAMES[phase], letter, 'gamma_only_sdr', failures)
                if row:
                    row['display_gamma'] = gamma
                    row['oracle'] = 'observed GammaOnly day anchor * (independent HDR target/day) ** (1/display_gamma)'
                    rows_gamma.append(row)

    result = {'schema': 'rok.mountain_f_sdr_output_verify.v1', 'passed': not failures,
              'failures': failures, 'manifest_sha256': sha(path),
              'configured_lut_dimension': configured_lut_dimension,
              'lut_dimension_provenance': ('legacy_runtime_cvar_only' if legacy_lut_metadata
                                           else 'manifest_and_each_viewport_cvar'),
              'normal_sdr_strict_budget_passed': len(rows_normal) == 6 and not any(
                  'normal_sdr' in failure for failure in failures),
              'gamma_only_hdr_oracle_passed': len(rows_gamma) == 6 and not any(
                  'gamma_only_sdr' in failure for failure in failures),
              'gamma_only_pretonemap_arithmetic_passed': len(rows_hdr) == 6 and not any(
                  'gamma_only_pretonemap_arithmetic' in failure for failure in failures),
              'fixed_geometry_pixels': fixed_count, 'max_vertex_drift_pixels': max_vertex_drift,
              'max_normal_gamma_hdr_delta': max_normal_gamma_hdr_delta,
              'normal_sdr_source_ratio_rows': rows_normal,
              'gamma_only_sdr_hdr_oracle_rows': rows_gamma,
              'gamma_only_pretonemap_source_arithmetic_rows': rows_hdr,
              'normal_gamma_pixel_difference_rows': rows_mode_delta,
              'historical_framing_manifest_sha256': sha(HISTORICAL_FRAMING),
              'numeric_budget_8bit': STRICT,
              'scope': 'Viewport and aligned HDR show only the representative F after checking all 18 source bindings. Normal SDR retains the strict Unity source-color ratio gate. GammaOnly independently checks the viewport output response from matched pre-tonemap HDR and actual display gamma; day output cancels unknown common exposure, so it is not an absolute texture or final Unity presentation oracle. Actual backbuffer may be 10-bit; the captured pre-PNG FColor and budget are 8-bit. Effective private LUT format was not captured.'}
    destination = Path(output).resolve() if output else path.parent / 'sdr_output_verify.json'
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
