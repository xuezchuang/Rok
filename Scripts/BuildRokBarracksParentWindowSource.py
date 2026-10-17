"""Read-only source inventory for the barracks parent-window native experiment."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RokUnityMigration/BarracksParentWindowSource'
BASE=ROOT/'ref/client-unity/Assets'
def record(p):
    text=p.read_text(encoding='utf-8-sig');blocks=re.split(r'(?=--- !u!)',text)
    names={re.search(r'&(-?\d+)',b)[1]:re.search(r'  m_Name: (.*)',b)[1] for b in blocks if b.startswith('--- !u!1 &') and re.search(r'  m_Name: (.*)',b)}
    rects=[]
    for b in blocks:
        if not b.startswith('--- !u!224 &') or 'stripped' in b[:80]:continue
        game=re.search(r'm_GameObject: \{fileID: (-?\d+)',b)
        if not game:continue
        rects.append({'name':names.get(game[1]),'file_id':re.search(r'&(-?\d+)',b)[1],'serialized_rect':b})
    return {'path':p.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'rects':rects,'prefab_overrides':[b for b in blocks if b.startswith('--- !u!1001 &')],'animators':[b for b in blocks if b.startswith('--- !u!95 &')]}
def main():
    paths=[BASE/'Skyunion/RunTime/UIManager/UIManager.cs',BASE/'Skyunion/RunTime/UIManager/ViewBinder.cs',BASE/'Skyunion/RunTime/Interface/UIManager/UIDefine.cs',BASE/'Skyunion/RunTime/UIManager/Resources/UIRoot.prefab',BASE/'Scripts/Hotfix/MVC/UI.cs',BASE/'Scripts/Hotfix/MVC/GlobalMediator/GlobalCityMenuHudMediator.cs',BASE/'Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs',ROOT/'Source/Rok/RokUnityModalWidget.cpp']
    folder=BASE/'BundleAssets/UI/UIPrefabs'
    names=['UI_Win_BuildingUpdate','UI_Model_Window_Type2','UI_Item_BuildingUpgrade','UI_Model_ResourcesConsume','UI_Model_DoubleLineButton_Yellow','UI_Model_DoubleLineButton_Blue']
    for name in names:
        hits=list(folder.rglob(name+'.prefab'));assert len(hits)==1,(name,hits);paths+=hits
    anim=BASE/'BundleAssets/UI/UIAnimation/shared_uianimation'
    paths += [anim/(name+ext) for name,ext in [('UA_WinAnime_T1','.controller'),('UA_WinAnime_T1_On','.anim'),('UA_WinAnime_T1_Off','.anim')]]
    data={'scope':'Read-only serialized/source call-chain analysis; no engine executed and no new runtime geometry accepted','sources':[record(p) for p in paths],
    'call_chain':['GlobalCityMenuHudMediator.ShowUI(UI.s_buildingUpdate,null,m_curIndex)','UI.s_buildingUpdate WindowLayer/Stack/PopWin; s_PlayerRes is linkUI[], not ParentUI','UIManager.LoadUI -> AnchorUIGameObject -> AddToLayer; restores root anchoredPosition/sizeDelta/scale','UIRoot Container and WindowLayer stretch zero offsets, reference1440x810, matchHeight1','default Animator Open -> UA_WinAnime_T1_On; root scale1.05 ->1.1@7frames ->1@0.25s; no position curves'],
    'geometry_inference':{'root':[0,0,1440,810],'nested_type2':[160,55,1120,700],'body_top':94.2,'body_bottom':755,'header_top':63,'header_bottom':123,'ue_body_top':139.2,'ue_body_bottom':800,'ue_header_top':108,'difference_y':45,'reason':'UE frame uses bottom800 versus centered source nested window bottom755. Not explained by T1 root scale ending at1. This is static source-chain inference pending actual controller native run.'},
    'cost_button_requirements':['Instantiate food/wood resource rows only when next config amount>0, SetParent, localScale1, SetResourcesConsume tests authoritative balance for color','Resource container HorizontalLayoutGroup and nested button HorizontalLayoutGroup must execute; do not copy serialized placeholder count/text','Yellow300046 immediate/denar cost from proxy; Blue180359 and FormatTimeUpgrad effective time; preferredWidth resize to height27.7 then ForceRebuildLayoutImmediate','Prerequisite list and buttons visibility depend on build mode/authoritative conditions; use named fixture states in isolated probe, never accepted owned resource/queue values'],
    'minimal_native_target':{'canvas':'Original UIRoot Container/WindowLayer ancestor RectTransforms + original BuildingUpdate prefab/nested overrides;1440x810','manager_adapter':'Replay AnchorUIGameObject save/setParent/restore in exact order; linkUI not parent','animation':'Enable original root T1 controller; sample actual updates at0,7/60,.25,.5 and controllerstate/scale/anchoredRect; do not disable Animator for claimed final state','capture_nodes':['UI_Win_BuildingUpdate','UI_Model_Window_Type1','win','frame1','frame2','frame3','frame4','title','lbl_title','pl_upgrade','UI_Item_BuildingUpgrade','img_resBg','pl_btns'],'capture_outputs':'Original canvas rendered full window + GetWorldCorners/rect/scale hierarchy for all listed nodes, record source hashes, selected language Arab processing, applied fixture data and actual LayoutRebuilder result','gates':['Root/ancestor final RectTransform and opened scale measured','Nested header/frame y and title descendants measured with original overrides','Cost row count/layout after source-shaped data binding and preferredWidth button layout measured','FullMediator/server services not claimed; authoritative balances/denar discounts/prerequisite ownership remain unknown']}}
    OUT.mkdir(parents=True,exist_ok=True);(OUT/'source_review.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8');print(OUT/'source_review.json')
if __name__=='__main__':main()

