"""Validate the source graphics card and its Chinese runtime layout; no UE launch."""
import json
import re
from pathlib import Path
from BuildRokTrainParameterLayout import documents, vector
from PrepareRokUiTextures import unity_sprite_rect
ROOT = Path(__file__).resolve().parents[1]
SOURCE = 'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_player/UIPlayerData/UI_Item_GraphicsSetting.prefab'

def main():
    docs = documents((ROOT / SOURCE).read_text(encoding='utf-8-sig'))
    objects = {k:v for k,v in docs.items() if v.startswith('GameObject:')}
    rects = {k:v for k,v in docs.items() if v.startswith('RectTransform:')}
    by_name = {}
    cache = {}
    def parts(rect):
        game = objects[re.search(r'm_GameObject: \{fileID: (-?\d+)', rect)[1]]
        return game, [docs[key] for key in re.findall(r'component: \{fileID: (-?\d+)', game)]
    def resolve(key):
        if key in cache: return cache[key]
        rect = rects[key]
        parent = re.search(r'm_Father: \{fileID: (-?\d+)', rect)[1]
        if parent == '0': return [0, 0], [1000, 367]
        origin, parent_size = resolve(parent)
        lo, hi = vector(rect, 'm_AnchorMin'), vector(rect, 'm_AnchorMax')
        pivot, pos, delta = vector(rect, 'm_Pivot'), vector(rect, 'm_AnchoredPosition'), vector(rect, 'm_SizeDelta')
        _, components = parts(rect)
        flags = [int(v) for c in components for v in re.findall(r'_arab_Layout: (\d+)', c)]
        if any(f & 2 and not f & 1 for f in flags):
            lo[0], hi[0] = 1-hi[0], 1-lo[0]
            pivot[0], pos[0] = 1-pivot[0], -pos[0]
        size = [parent_size[i]*(hi[i]-lo[i])+delta[i] for i in range(2)]
        origin = [origin[i]+parent_size[i]*(lo[i]+(hi[i]-lo[i])*pivot[i])+pos[i]-pivot[i]*size[i] for i in range(2)]
        cache[key] = origin, size
        return cache[key]
    for key, rect in rects.items():
        game, components = parts(rect)
        name = re.search(r'm_Name: (.*)', game)[1]
        origin, size = resolve(key)
        by_name.setdefault(name, []).append({'position':[origin[0],367-origin[1]-size[1]],'size':size,'components':components,'active':'m_IsActive: 1' in game})
    for name, xy, wh in [('img_picture',[58.5,83.5],[883,53]),('img_framerate',[58.5,265.5],[883,53]),
                         ('lbl_dec1',[16,-7.55],[505.8,55.1]),('lbl_dec2',[16,30.45],[524.05,55.1]),
                         ('lbl_dec3',[16,198.45],[505.8,55.1]),('sd_picture',[88,99.3],[824,21.4]),('sd_framerate',[88,281.3],[824,21.4])]:
        row = by_name[name][0]
        assert all(abs(a-b)<.001 for a,b in zip(row['position'],xy)), name
        assert all(abs(a-b)<.001 for a,b in zip(row['size'],wh)), name
    assert not by_name['lbl_dec4'][0]['active']
    for name in ['sd_picture','sd_framerate']:
        block = ''.join(by_name[name][0]['components'])
        for token in ['m_Direction: 1','m_MinValue: 1','m_MaxValue: 3','m_WholeNumbers: 1','_arab_Layout: 32']:
            assert token in block, token
    arab = (ROOT/'ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs').read_text(encoding='utf-8-sig')
    assert 'isArabUI == Skyunion.LanguageUtils.IsArabic()' in arab
    assert 'compment.direction = Slider.Direction.LeftToRight' in arab
    quality = (ROOT/'ref/client-unity/Assets/Scripts/Hotfix/Common/Utils/QualitySetting.cs').read_text(encoding='utf-8-sig')
    assert 'systemMemorySize > 1000 * 3' in quality and 'systemMemorySize > 1000 * 2' in quality
    logic = (ROOT/'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_GraphicsSetting_SubView.cs').read_text(encoding='utf-8-sig')
    assert 'LanguageUtils.getTextFormat(300227' in logic and 'PlayOneShot(RS.SoundUiCommonSlider)' in logic
    meta = (ROOT/'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png.meta').read_text(encoding='utf-8-sig')
    for name, rect in [('pl_com_1005',(1,611,570,2)),('pb_com_1000_1',(401,112,20,23)),('pb_com_1000_4',(384,59,12,19)),('pb_com_1000_5',(679,474,20,40))]:
        assert tuple(unity_sprite_rect(meta,name)) == rect, name
    cpp=(ROOT/'Source/Rok/RokUnityFeatureScreenWidget.cpp').read_text(encoding='utf-8-sig')
    scope=cpp.split('void URokUnityFeatureScreenWidget::BuildGraphicsSettingsCard',1)[1].split('void URokUnityFeatureScreenWidget::BuildGeneralSettings',1)[0]
    for token in ['GeneralSettingsGraphicsCard','GeneralSettingsGraphicsRecommendation','Sound_Ui_CommonScroll','SetStepSize(1)','SetMinValue(1)','SetMaxValue(3)','FPlatformMemory::GetConstants().TotalPhysical']:
        assert token in scope, token
    assert 'TrainArmy' not in scope
    evidence={name:[{k:v for k,v in r.items() if k!='components'} for r in rows] for name,rows in by_name.items()}
    out=ROOT/'Saved/RokUnityMigration/graphics_settings_layout_verify.json'
    out.write_text(json.dumps({'source':SOURCE,'source_verified':True,'rects':evidence,'runtime_verified':False,'visual_verified':False},ensure_ascii=False,indent=2),encoding='utf-8')
    print('PASS: graphics card geometry, Chinese slider direction, source sprites, RAM thresholds and local callbacks')
if __name__ == '__main__': main()
