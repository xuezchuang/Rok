"""Source-only F body/18 placements; no A or Grove input payload reused."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RokUnityMigration/MountainFGamma'
DECODE_SHADER='''Shader "Hidden/RokMountainFMipDecode" { Properties { _MainTex ("Source", 2D) = "white" {} _Mip ("Mip", Float) = 0 } SubShader { Pass {
ZTest Always Cull Off ZWrite Off
CGPROGRAM
#pragma vertex vert_img
#pragma fragment frag
#pragma target 3.0
#include "UnityCG.cginc"
sampler2D _MainTex; float _Mip;
float4 frag(v2f_img i):SV_Target { return tex2Dlod(_MainTex,float4(i.uv,0,_Mip)); }
ENDCG
} } }'''
def build():
    load=lambda p:json.loads((ROOT/p).read_text(encoding='utf-8-sig'))
    assets=load('Saved/RokUnityMigration/unity_world_map_assets.json');world=load('Saved/RokUnityMigration/unity_world_map.json')
    record=assets['object_sources']['build_mountain_F'];body=record['mountain_geometry_contract']['body_render_node']
    prefab=ROOT/record['source_prefab'];text=prefab.read_text(encoding='utf-8-sig')
    assert body['game_object']=='build_mountain_F' and body['mesh_file_id']=='3827334489394421038'
    assert body['material_guid']=='13645cd34a40fc8449f0396b7785e3a1'
    assert all(v in text for v in (body['mesh_file_id'],body['material_guid'],body['mesh_guid']))
    instances=[dict(tile=[t['x'],t['y']],source_instance=i) for t in world['prototype_window']['tiles'] for i in t['instances'] if i['prefab_id']=='build_mountain_F'];assert len(instances)==18
    mediator=ROOT/'ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GameEventGlobalMediator.cs';code=mediator.read_text(encoding='gb18030')
    section=code.split('private Color[][] lightSetting',1)[1].split('private float[] m_lightSettingValue',1)
    colors=[[float(x.strip().rstrip('f')) for x in row.split(',')] for row in re.findall(r'new Color\(([^)]+)\)',section[0])]
    intensity=[float(x.strip().rstrip('f')) for x in re.search(r'new\[\] \{([^}]+)',section[1])[1].split(',')];assert len(colors)==8 and len(intensity)==4
    samples=[dict(name=name,preset=i,spriteColor=[a+b*intensity[i]*.75 for a,b in zip(colors[2*i],colors[2*i+1])]) for name,i in [('stable_day',1),('stable_dusk',2),('stable_night',3)]]
    source_files=[prefab,ROOT/record['source_mesh']['path'],ROOT/body['material_path'],ROOT/'ref/client-unity/Assets/BundleAssets/land/Texture/build_mountain_F.png',ROOT/'ref/client-unity/Assets/BundleAssets/land/Texture/build_mountain_F.png.meta',mediator]
    result=dict(schema='rok.mountain_f_gamma.v1',body=body,instances=instances,samples=samples,source_inputs=[dict(path=p.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in source_files],excluded_edge=record['mountain_geometry_contract']['edge_render_node'],native_executed=False)
    OUT.mkdir(parents=True,exist_ok=True);(OUT/'source_data.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':print(json.dumps(build(),indent=2))
