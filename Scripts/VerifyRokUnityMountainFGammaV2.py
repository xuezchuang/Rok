"""Verify V2 Unity GPU RGBA readback against independently decoded imported BC1 blocks."""
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Saved/RokUnityMigration/MountainFGammaV2'
OLD = ROOT / 'Saved/RokUnityMigration/MountainFGamma'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def decode_bc1(data, width, height, model='nvidia'):
    """Decode BC1 by NVIDIA sampling or ideal 2/3 palette for comparison.

    The exact interpolation equations are documented in Guerrilla Games'
    GPC 2025 "How to Decimate Your Textures", bonus slide 36, citing
    Fabian Giesen, GPU BCn decoding. The original Unity GPU here is NVIDIA.
    """
    assert model in ('nvidia', 'ideal')
    count_x, count_y = max(1, (width + 3) // 4), max(1, (height + 3) // 4)
    assert len(data) == count_x * count_y * 8, 'Wrong imported BC1 size'
    result = np.empty((height, width, 4), dtype=np.uint8)
    for block_y in range(count_y):
        for block_x in range(count_x):
            offset = (block_y * count_x + block_x) * 8
            c0 = int.from_bytes(data[offset:offset + 2], 'little')
            c1 = int.from_bytes(data[offset + 2:offset + 4], 'little')
            packed = int.from_bytes(data[offset + 4:offset + 8], 'little')

            def expand(value):
                red, green, blue = (value >> 11) & 31, (value >> 5) & 63, value & 31
                if model == 'ideal':
                    return (red * 255 // 31, green * 255 // 63, blue * 255 // 31, 255)
                return (red << 3 | red >> 2, green << 2 | green >> 4, blue << 3 | blue >> 2, 255)

            a, b = expand(c0), expand(c1)
            if c0 > c1:
                if model == 'ideal':
                    palette = [a, b,
                               tuple((2 * a[i] + b[i]) // 3 for i in range(4)),
                               tuple((a[i] + 2 * b[i]) // 3 for i in range(4))]
                else:
                    red0, red1 = (c0 >> 11) & 31, (c1 >> 11) & 31
                    blue0, blue1 = c0 & 31, c1 & 31
                    green_diff = b[1] - a[1]
                    scaled_green_diff = 80 * green_diff + (green_diff >> 2)
                    palette = [a, b,
                               (((2 * red0 + red1) * 22) >> 3,
                                a[1] + ((128 + scaled_green_diff) >> 8),
                                ((2 * blue0 + blue1) * 22) >> 3, 255),
                               (((red0 + 2 * red1) * 22) >> 3,
                                b[1] + ((128 - scaled_green_diff) >> 8),
                                ((blue0 + 2 * blue1) * 22) >> 3, 255)]
            else:
                palette = [a, b,
                           tuple((a[i] + b[i] + (1 if model == 'nvidia' else 0)) // 2 for i in range(3)) + (255,),
                           (0, 0, 0, 0)]
            for y in range(4):
                row = block_y * 4 + y
                if row >= height:
                    continue
                for x in range(4):
                    col = block_x * 4 + x
                    if col < width:
                        result[row, col] = palette[(packed >> (2 * (y * 4 + x))) & 3]
    return result


def main():
    prepared = json.loads((OUT / 'prepare.json').read_text(encoding='utf-8'))
    assert prepared['schema'] == 'rok.mountain_f_gamma_v2.prepare.v1'
    assert prepared['compiler_exit_code'] == 0 and prepared['source_files_match_old_manifest']
    assert prepared['source_manifest_sha256'] == sha(OLD / 'manifest.json')
    assert prepared['editor_script_sha256'] == sha(ROOT / 'Scripts/RenderRokUnityMountainFGammaV2.cs')
    assert prepared['probe_shader_sha256'] == sha(ROOT / 'Saved/UnityMountainFGammaV2Probe/Assets/Editor/RokMountainFMipDecodeV2.shader')
    report = {'schema': 'rok.mountain_f_gamma_v2.verify.v1', 'prepared': True,
              'native_checked': False, 'passed': False, 'product_modified': False}
    native_path = OUT / 'native_render.json'
    if not native_path.exists():
        report['pending'] = 'Run the isolated Unity V2 batchmode exporter before accepting texture mips.'
        (OUT / 'verify.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(report, indent=2))
        return
    native = json.loads(native_path.read_text(encoding='utf-8'))
    assert native['unityVersion'] == '2019.4.28f1c1' and native['colorSpace'] == 'Gamma'
    assert 'NVIDIA' in native['device'].upper() and 'RTX' in native['device'].upper(), 'This exact BC1 oracle is validated only for this NVIDIA RTX GPU'
    assert native['sourceShader'] == 'custom/landmap/z_cm_slg_mountain'
    assert native['meshGuid'] == 'aee73fed0485b1244aea2938ba78f8ee'
    assert native['meshLocalId'] == '3827334489394421038'
    assert native['materialGuid'] == '13645cd34a40fc8449f0396b7785e3a1'
    assert native['textureGuid'] == 'b9d81a5ad9e692c499abcf99c1cabd63'
    assert native['texturePath'] == 'Assets/BundleAssets/land/Texture/build_mountain_F.png'
    assert native['materialMainTextureIsSource'] and native['importerReadableForRawBlockAudit']
    assert native['textureFormat'] == 'DXT1' and native['graphicsFormat'].startswith('RGBA_DXT1')
    assert (native['width'], native['height'], native['mipCount']) == (1024, 1024, 11)
    assert (native['vertices'], native['triangles']) == (1002, 1870)
    assert len(native['mips']) == 11
    rows = []
    for level, mip in enumerate(native['mips']):
        width, height = max(1, 1024 >> level), max(1, 1024 >> level)
        assert (mip['level'], mip['width'], mip['height']) == (level, width, height)
        bc1_path, png_path = OUT / f'native_mip{level}.bc1', OUT / f'native_mip{level}.png'
        assert Path(mip['bc1Path']).resolve() == bc1_path.resolve()
        assert Path(mip['rgbaPath']).resolve() == png_path.resolve()
        raw = bc1_path.read_bytes()
        expected = decode_bc1(raw, width, height)
        ideal = decode_bc1(raw, width, height, model='ideal')
        actual = np.asarray(Image.open(png_path).convert('RGBA'))
        assert actual.shape == expected.shape
        assert mip['compressedBytes'] == len(raw) and mip['rgbaBytes'] == width * height * 4
        alpha = actual[:, :, 3]
        zero, opaque = int(np.count_nonzero(alpha == 0)), int(np.count_nonzero(alpha == 255))
        partial = width * height - zero - opaque
        assert (zero, opaque, partial) == (mip['alphaZero'], mip['alphaOpaque'], mip['alphaPartial'])
        assert partial == 0, f'GPU mip {level} has fractional alpha'
        variants = [('as_is', expected), ('flip_y', expected[::-1, :, :])]
        orientation, reference = min(variants, key=lambda item: np.abs(actual.astype(np.int16) - item[1].astype(np.int16)).mean())
        ideal_reference = ideal[::-1, :, :] if orientation == 'flip_y' else ideal
        assert np.array_equal(alpha, reference[:, :, 3]), f'Mip {level} alpha differs from imported BC1 blocks'
        rgb_diff = np.abs(actual[:, :, :3].astype(np.int16) - reference[:, :, :3].astype(np.int16))
        ideal_rgb_diff = np.abs(actual[:, :, :3].astype(np.int16) - ideal_reference[:, :, :3].astype(np.int16))
        max_rgb_error = int(rgb_diff.max())
        assert max_rgb_error <= 1, f'Mip {level} GPU color differs from imported BC1 blocks: {max_rgb_error}'
        if level == 0:
            assert int(actual[:, :, :3].max()) - int(actual[:, :, :3].min()) >= 16, 'Flat/unbound GPU mip 0'
            source_image = Image.open(ROOT / 'ref/client-unity/Assets/BundleAssets/land/Texture/build_mountain_F.png').convert('RGB')
            assert source_image.size == (2048, 2048)
            source_small = np.asarray(source_image.resize((1024, 1024), Image.Resampling.BILINEAR))
            source_error = min(float(np.abs(actual[:, :, :3].astype(np.int16) - variant.astype(np.int16)).mean())
                               for variant in (source_small, source_small[::-1, :, :]))
            assert source_error < 40, f'GPU mip 0 does not resemble bound source PNG: {source_error:.2f}'
        rows.append({'level': level, 'size': [width, height], 'alpha_zero': zero, 'alpha_opaque': opaque,
                     'orientation': orientation, 'max_bc1_rgb_error': max_rgb_error,
                     'mean_bc1_rgb_error': float(rgb_diff.mean()),
                     'ideal_bc1_max_rgb_error': int(ideal_rgb_diff.max()),
                     'ideal_bc1_mean_rgb_error': float(ideal_rgb_diff.mean()),
                     'bc1_sha256': sha(bc1_path), 'png_sha256': sha(png_path)})
    report.update(native_checked=True, passed=True, mips=rows,
                  bc1_decoder='NVIDIA_RGB565_endpoint_expansion_and_palette_interpolation',
                  bc1_decoder_source='https://static.graphicsprogrammingconference.com/public/2025/slides/how-to-decimate-your-textures/Malan-how-to-decimate-your-textures-bcn-compression-tricks-in-horizon-forbidden-west.pdf#page=37',
                  bc1_decoder_source_page='Bonus slide 36 (PDF page 37); equations for NVIDIA GPU sampling',
                  mip0_source_png_mae=source_error,
                  scope='Imported DXT1 BC1 block chain cross-checked with GPU RGBA readback; body texture only, no UE product acceptance.')
    (OUT / 'verify.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed': True, 'mips': len(rows), 'mip0_source_mae': source_error}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / 'verify.json').write_text(json.dumps({'schema': 'rok.mountain_f_gamma_v2.verify.v1',
            'prepared': (OUT / 'prepare.json').exists(), 'native_checked': False,
            'passed': False, 'failure': str(error), 'product_modified': False}, indent=2) + '\n', encoding='utf-8')
        raise
