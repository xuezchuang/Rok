"""Lossless center transport for the one accepted Grove prefab; no engine calls."""
import json
import hashlib
import struct
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Saved/RokUnityMigration/GroveCenterScale'

def main():
    data = json.loads((OUT / 'manifest.json').read_text())
    p = np.asarray(data['positions'], dtype=np.float32)
    c = np.asarray(data['centers_unity'], dtype=np.float32)
    baked = c + (p-c)*np.float32(data['bake_scale'])
    color = np.asarray(data['source_color_rgba'], dtype=np.float32)
    blob = bytearray()
    views, accessors = [], []
    def add(values, kind, component=5126):
        values = np.asarray(values, dtype='<f4' if component == 5126 else '<u4')
        while len(blob) % 4:
            blob.append(0)
        offset = len(blob)
        raw = values.tobytes()
        blob.extend(raw)
        views.append(dict(buffer=0, byteOffset=offset, byteLength=len(raw)))
        item = dict(bufferView=len(views)-1, componentType=component, count=len(values), type=kind)
        if kind == 'VEC3':
            item.update(min=values.min(axis=0).tolist(), max=values.max(axis=0).tolist())
        accessors.append(item)
        assert np.array_equal(np.frombuffer(blob[offset:offset+len(raw)], dtype=values.dtype).reshape(values.shape), values)
        return len(accessors)-1
    attrs = dict(POSITION=add(baked, 'VEC3'), TEXCOORD_0=add(data['uv0'], 'VEC2'),
                 TEXCOORD_1=add(color[:, :2], 'VEC2'), TEXCOORD_2=add(color[:, 2:], 'VEC2'))
    # Unity-to-glTF needs one winding reversal. UE's glTF Y/Z conversion does
    # not reverse indices: GLTFMeshFactory only reverses for a negative-determinant
    # MeshGlobalTransform. Our node is identity, so its non-mirrored path retains
    # the accessor order (GLTFMeshFactory.cpp:524,742; GLTFMesh.cpp:528).
    expected_triangles = np.asarray(data['triangles'], dtype='<u4')[:, [0,2,1]]
    indices = add(expected_triangles.reshape(-1), 'SCALAR', 5125)
    gltf = dict(asset=dict(version='2.0', generator='Rok source Grove center transport'),
                buffers=[dict(uri='grove.bin', byteLength=len(blob))], bufferViews=views,
                accessors=accessors, meshes=[dict(name='SM_RokGroveCenterScale', primitives=[dict(attributes=attrs, indices=indices, mode=4)])],
                nodes=[dict(mesh=0)], scenes=[dict(nodes=[0])], scene=0)
    (OUT/'grove.bin').write_bytes(blob)
    (OUT/'grove.gltf').write_text(json.dumps(gltf, indent=2))
    # Read saved transport back, including its accessor/view byte offsets.
    decoded_gltf = json.loads((OUT/'grove.gltf').read_text())
    decoded_blob = (OUT/'grove.bin').read_bytes()
    accessor = decoded_gltf['accessors'][decoded_gltf['meshes'][0]['primitives'][0]['indices']]
    view = decoded_gltf['bufferViews'][accessor['bufferView']]
    assert accessor['componentType']==5125 and accessor['type']=='SCALAR' and accessor['count']==927
    decoded_triangles = np.frombuffer(decoded_blob, dtype='<u4', count=accessor['count'],
        offset=view.get('byteOffset',0)+accessor.get('byteOffset',0)).reshape(-1,3)
    assert np.array_equal(decoded_triangles, expected_triangles), 'Saved GLTF winding differs from exact source reversal'
    report = dict(passed=True, vertices=len(p), triangles=len(data['triangles']),
                  input_sha256=hashlib.sha256((OUT/'manifest.json').read_bytes()).hexdigest(),
                  gltf_sha256=hashlib.sha256((OUT/'grove.gltf').read_bytes()).hexdigest(),
                  bin_sha256=hashlib.sha256(blob).hexdigest(),
                  transport='glTF float32 UV0 source; UV1 COLOR.rg; UV2 COLOR.ba; no COLOR_0 quantization',
                  ue_expected_position='baked Unity(x,z,y)*100', ue_expected_winding='source(a,c,b)',
                  indices_decode_exact_source_reversal=True,
                  winding_conversion='Generator reverses source once; identity-node UE GLTF import preserves accessor order',
                  texture_v_flip_in_material=True, engine_import_verified=False)
    (OUT/'gltf_verify.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report))

if __name__ == '__main__':
    main()
