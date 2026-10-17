"""Verify exact source identities and complete sampled geometry without an engine."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/RokUnityMigration/MailSpine'
def main():
    report=json.loads((OUT/'export_report.json').read_text())
    raw=(ROOT/report['output']).read_bytes();assert hashlib.sha256(raw).hexdigest()==report['sha256']
    clip=json.loads(raw);assert clip['skin']=='gift5' and clip['animation']=='UE_TreasureBox_stay' and clip['loop']
    assert clip['duration']==1.6 and clip['frame_count']==96 and clip['unique_poses']==16
    assert clip['attachment_types']==['MeshAttachment','RegionAttachment'] and clip['slot_blends']==['Normal']
    for src in clip['sources']:assert hashlib.sha256((ROOT/src['path']).read_bytes()).hexdigest()==src['sha256'],src
    prefab=(ROOT/'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_mail/UIMail/UI_Item_MailBtn.prefab').read_text(encoding='utf-8-sig')
    for prop,val in [('startingAnimation','UE_TreasureBox_stay'),('initialSkinName','gift5'),('m_LocalScale.x','0.06'),('m_LocalScale.y','0.06'),('m_LocalScale.z','0.06'),('m_AnchoredPosition.y','-5.1')]:
        assert re.search(r'propertyPath: '+re.escape(prop)+r'\s+value: '+re.escape(val)+r'\s',prefab)
    base=(ROOT/'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_AnimationBox.prefab').read_text(encoding='utf-8-sig')
    for contract in ['startingLoop: 1','timeScale: 1','unscaledTime: 0','pmaVertexColors: 1','useClipping: 1','initialFlipX: 0','initialFlipY: 0']:assert contract in base
    for i,frame in enumerate(clip['frames']):
        assert abs(frame['time']-i*1.6/96)<1e-6 and len(frame['draws'])==2
        for draw in frame['draws']:
            assert draw['blend']=='Normal' and len(draw['xy'])==len(draw['uv'])
            assert all(0<=u<=1 for u in draw['uv'])
            assert all(0<=t<len(draw['xy'])//2 for t in draw['triangles'])
        assert frame['draws'][0]['attachment']=='gift5_open' and frame['draws'][0]['rgba']==[1,1,1,0]
        assert frame['draws'][1]['attachment']=='gift5_stay' and frame['draws'][1]['rgba']==[1,1,1,1]
    result={'passed':True,'source_hashes_verified':len(clip['sources']),'frames':96,'duration':1.6,'unique_geometry_poses':16,'source_types':clip['attachment_types'],'blend':'Normal','color_contract':'Open draw alpha zero; stay draw white opaque; do not fabricate fractional-alpha animation.','remaining':['Native SkeletonGraphic mesh/UV/PMA comparison','UE compiler/tests and actual mail integration']}
    (OUT/'source_verify.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
