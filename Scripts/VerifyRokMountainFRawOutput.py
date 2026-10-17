"""Verify same-frame packed R10G10B10A2 evidence without changing the SDR gate.

The UE 5.8 D3D12 ReadSurfaceData conversion is an exact 10->8-bit requantize.
This checks every RGB pixel against the existing pre-PNG FColor. The original
Mountain F SDR verifier still owns the fixed geometry mask and frozen budgets.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np

from VerifyRokMountainFSdrOutputDiagnostic import SIZE, STRICT, verify as verify_sdr


ROOT = Path(__file__).resolve().parents[1]
RUNS = (ROOT / 'Saved/RokUnityMigration/MountainFGammaAudit').resolve()
STAGES = ('normal', 'normal', 'normal', 'gamma_only', 'gamma_only', 'gamma_only')
PHASES = ('stable_day', 'stable_dusk', 'stable_night') * 2


def file_sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def requantize10_to8(values):
    # UE 5.8 Core/Public/Math/Color.h FColor::Requantize10to8.
    temp = values.astype(np.uint32) * 255 + 512
    return ((temp + (temp >> 10)) >> 10).astype(np.uint8)


def verified_run_file(run, reported_path, suffix, failures, index):
    path = Path(reported_path)
    if not reported_path or path.suffix != suffix or path.resolve() != (run / path.name).resolve():
        failures.append(f'capture {index}: unsafe or unexpected {suffix} path')
        return None
    path = run / path.name
    if not path.is_file():
        failures.append(f'capture {index}: missing {path.name}')
        return None
    return path


def verify(path, output=None):
    path = Path(path).resolve()
    if path.name != 'manifest.json' or path.parent.parent != RUNS:
        raise ValueError('Pass a MountainFGammaAudit/<run>/manifest.json')
    run = path.parent
    manifest = json.loads(path.read_text(encoding='utf-8-sig'))
    if manifest.get('gpu_packed_readback_diagnostic') is not True:
        raise ValueError('Manifest predates packed GPU readback; historical run left untouched')
    captures = manifest.get('captures') or []
    failures = []
    rows = []
    if len(captures) != 6 or manifest.get('sdr_output_diagnostic') is not True:
        failures.append('Expected the explicit six-frame Mountain F SDR output diagnostic')
    if manifest.get('passed') is not True or manifest.get('temporary_state_restored') is not True:
        failures.append('C++ capture or scoped runtime restoration failed')
    if manifest.get('sdr_configured_lut_dimension') != 64:
        failures.append('This raw backbuffer comparison requires the preserved LUT64 control')
    if len(captures) == 6:
        for index, capture in enumerate(captures):
            frame_failures = []
            meta = capture.get('gpu_packed_backbuffer') or {}
            fcolor = capture.get('pre_png_fcolor') or {}
            state = capture.get('at_request') or {}
            view = state.get('sdr_viewport') or {}
            cvars = view.get('cvars') or {}
            alpha = cvars.get('r.PostProcessing.PropagateAlpha') or {}
            if (capture.get('name') != PHASES[index] or capture.get('output_mode') != STAGES[index] or
                    alpha.get('value') not in ('1', 'True', 'true') or
                    (cvars.get('r.LUT.Size') or {}).get('value') != '64'):
                frame_failures.append(f'capture {index}: phase/mode or Alpha1/LUT64 controls differ')
            expected_png_name = f'{index:02d}_{STAGES[index]}_{PHASES[index]}.png'
            if Path(capture.get('path', '')).name != expected_png_name:
                frame_failures.append(f'capture {index}: PNG stage path differs')
            expected_raw_name = Path(expected_png_name).with_suffix('.a2b10g10r10').name
            expected_fcolor_name = Path(expected_png_name).with_suffix('.bgra8').name
            raw_name = Path(meta.get('path', '')).name
            fcolor_name = Path(fcolor.get('path', '')).name
            raw_path = verified_run_file(run, meta.get('path', ''), '.a2b10g10r10', frame_failures, index)
            fcolor_path = verified_run_file(run, fcolor.get('path', ''), '.bgra8', frame_failures, index)
            if raw_name != expected_raw_name or fcolor_name != expected_fcolor_name:
                frame_failures.append(f'capture {index}: raw/FColor stage filename differs')
            if (meta.get('source_api') !=
                    'OnViewportRendered -> render-thread FViewport::GetShaderResourceTexture -> FRHIGPUTextureReadback CopyTexture/fence/Lock' or
                    meta.get('pixel_format') != 'A2B10G10R10' or
                    meta.get('rhi_interface') != 'D3D12' or
                    meta.get('extent') != list(SIZE) or meta.get('sample_count') != 1 or
                    meta.get('source_rect') != [0, 0, *SIZE] or
                    meta.get('constrained_view_rect') != view.get('constrained_view_rect') or
                    view.get('actual_backbuffer_pixel_format') != meta.get('pixel_format') or
                    meta.get('bytes_per_pixel') != 4 or
                    not isinstance(meta.get('gpu_mask_native'), int) or meta['gpu_mask_native'] < 1 or
                    meta.get('file_row_stride_bytes') != SIZE[0] * 4 or
                    not isinstance(meta.get('mapped_row_pitch_pixels'), int) or
                    meta['mapped_row_pitch_pixels'] < SIZE[0] or
                    meta.get('mapped_row_pitch_bytes') != meta['mapped_row_pitch_pixels'] * 4 or
                    not isinstance(meta.get('mapped_buffer_height'), int) or
                    meta['mapped_buffer_height'] < SIZE[1] or
                    meta.get('bytes') != SIZE[0] * SIZE[1] * 4):
                frame_failures.append(f'capture {index}: GPU texture/stride/format metadata differs')
            if (meta.get('stage') != index or meta.get('capture_serial') != index + 1 or
                    capture.get('gpu_capture_serial') != meta.get('capture_serial') or
                    capture.get('gpu_viewport_game_frame') != meta.get('viewport_game_frame') or
                    not all(isinstance(meta.get(key), (int, float)) for key in
                            ('request_game_frame', 'viewport_game_frame', 'copy_render_frame',
                             'ready_render_frame', 'request_platform_seconds',
                             'viewport_platform_seconds', 'copy_platform_seconds', 'ready_platform_seconds')) or
                    meta.get('viewport_game_frame', -1) < meta.get('request_game_frame', 0) or
                    meta.get('ready_render_frame', -1) < meta.get('copy_render_frame', 0) or
                    meta.get('viewport_platform_seconds', -1) < meta.get('request_platform_seconds', 0) or
                    meta.get('copy_platform_seconds', -1) < meta.get('viewport_platform_seconds', 0) or
                    meta.get('ready_platform_seconds', -1) < meta.get('copy_platform_seconds', 0) or
                    capture.get('screenshot_processed_game_frame', -1) != meta.get('viewport_game_frame')):
                frame_failures.append(f'capture {index}: stage or capture timing correspondence differs')
            if raw_path and fcolor_path:
                if (raw_path.stat().st_size != SIZE[0] * SIZE[1] * 4 or
                        fcolor_path.stat().st_size != SIZE[0] * SIZE[1] * 4 or
                        file_sha(raw_path) != meta.get('sha256') or
                        file_sha(fcolor_path) != fcolor.get('sha256')):
                    frame_failures.append(f'capture {index}: packed/FColor size or SHA-256 differs')
                else:
                    packed = np.fromfile(raw_path, dtype='<u4').reshape(SIZE[1], SIZE[0])
                    bgra = np.fromfile(fcolor_path, dtype=np.uint8).reshape(SIZE[1], SIZE[0], 4)
                    decoded = (requantize10_to8(packed & 1023),
                               requantize10_to8((packed >> 10) & 1023),
                               requantize10_to8((packed >> 20) & 1023))
                    mismatches = [int(np.count_nonzero(decoded[channel] != bgra[:, :, 2 - channel]))
                                  for channel in range(3)]
                    if any(mismatches):
                        frame_failures.append(f'capture {index}: same-frame D3D12 packed->FColor RGB mismatch {mismatches}')
                    alpha = (packed >> 30).astype(np.uint8)
                    if not np.all(bgra[:, :, 3] == 255):
                        frame_failures.append(f'capture {index}: screenshot FColor alpha was not forced opaque')
                    rows.append({'index': index, 'mode': STAGES[index], 'phase': PHASES[index],
                                 'pixels': SIZE[0] * SIZE[1], 'rgb_mismatch_pixels': mismatches,
                                 'gpu_alpha_codes_present': sorted(int(x) for x in np.unique(alpha)),
                                 'packed_sha256': meta['sha256'], 'fcolor_sha256': fcolor['sha256']})
            failures.extend(frame_failures)

    # Only a complete new raw run may invoke the original verifier. Historical
    # manifests without packed bytes must never have their report rewritten.
    legacy = {'passed': False, 'failures': ['raw evidence incomplete; original SDR verifier not run']}
    legacy_ran = False
    if not failures and len(rows) == 6:
        # The original verifier retains the fixed mask, same-camera checks,
        # GammaOnly oracle, and frozen normal-SDR numeric budgets.
        legacy = verify_sdr(path, run / 'sdr_output_verify.json')
        legacy_ran = True
        if legacy.get('fixed_geometry_pixels') != 70370 or legacy.get('max_vertex_drift_pixels') != 0:
            failures.append('Original fixed 70370-pixel geometry mask or frozen camera moved')
        if legacy.get('numeric_budget_8bit') != STRICT:
            failures.append('Original numeric SDR budget changed')
    result = {'schema': 'rok.mountain_f_raw_output_verify.v1',
              'passed': not failures and legacy['passed'],
              'raw_readback_passed': not failures and len(rows) == 6,
              'normal_sdr_accepted': legacy.get('normal_sdr_strict_budget_passed') is True,
              'legacy_sdr_verify_passed': legacy['passed'],
              'failures': failures + [f'original SDR gate: {message}' for message in legacy['failures']],
              'manifest_sha256': file_sha(path),
              'original_sdr_verify': str(run / 'sdr_output_verify.json') if legacy_ran else None,
              'fixed_geometry_pixels': legacy.get('fixed_geometry_pixels'),
              'frozen_numeric_budget_8bit': legacy.get('numeric_budget_8bit'),
              'rows': rows,
              'scope': 'Raw D3D12 PF_A2B10G10R10 GPU copy versus post-ReadSurfaceData FColor; original six-frame fixed-mask SDR gate is unchanged. Passing raw correspondence alone is not Unity final SDR acceptance.'}
    destination = Path(output).resolve() if output else run / 'sdr_raw_output_verify.json'
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest')
    parser.add_argument('--output')
    args = parser.parse_args()
    answer = verify(args.manifest, args.output)
    print(json.dumps(answer, ensure_ascii=False, indent=2))
    sys.exit(0 if answer['passed'] else 1)
