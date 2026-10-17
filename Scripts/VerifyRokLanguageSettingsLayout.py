"""Source checks for local language radio drafts; application/reload intentionally outside this slice."""
import csv,json,re
from pathlib import Path
from BuildRokTrainParameterLayout import documents,vector
from BuildRokUnityEconomyCatalog import read_language_lookup,localized_text
from PrepareRokUiTextures import unity_sprite_rect
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_setting/UILanguage'
def main():
    records=list(csv.DictReader((ROOT/'ref/config-tables/编译客户端配置/CSV/LanguageSet.csv').read_text(encoding='utf-8-sig').splitlines()))[1:]
    enabled=[r for r in records if r['enumSwitch']=='1']
    assert [int(r['ID']) for r in enabled]==[1,10,37,40]
    lang,_=read_language_lookup();labels=[localized_text(lang,int(r['l_languageID']),'cn') for r in enabled]
    assert labels==['العربية','English','Türkçe','简体中文']
    row=(BASE/'UI_LC_Language.prefab').read_text(encoding='utf-8-sig')
    for token in ['m_SizeDelta: {x: 796, y: 111}','m_CellSize: {x: 388, y: 114}','m_Spacing: {x: 20, y: 0}','m_StartCorner: 1','m_ChildAlignment: 5','_arab_Layout: 512']:
        assert token in row,token
    item=(BASE/'UI_Item_LanguageSelect.prefab').read_text(encoding='utf-8-sig')
    for token in ['m_FontSize: 26','m_SizeDelta: {x: 44, y: 44}','_arab_Layout: 10','_arab_Layout: 2','m_Alignment: 5']:
        assert token in item,token
    window=(BASE/'UI_Win_Language.prefab').read_text(encoding='utf-8-sig')
    for token in ['m_SizeDelta: {x: 1440, y: 810}','m_SizeDelta: {x: 883, y: 491.5}','m_AnchoredPosition: {x: 0, y: -25.9}','m_AnchoredPosition: {x: 0, y: -5.5}','m_SizeDelta: {x: 0, y: 370.4}','spacing: 0']:
        assert token in window,token
    # Window center405, rect offset -25.9 => top185.15; viewport inset5.5 =>190.65.
    assert abs((810/2+25.9-491.5/2+5.5)-190.65)<1e-6
    assert (111-114)/2==-1.5
    common=(ROOT/'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png.meta').read_text(encoding='utf-8-sig')
    for name,rect in [('pl_com_1000',(1952,906,74,67)),('ck_com_1000_1',(611,181,44,44)),('ck_com_1000_2',(697,231,44,44))]:
        assert tuple(unity_sprite_rect(common,name))==rect
    frame=(ROOT/'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_Window_TypeMid.prefab').read_text(encoding='utf-8-sig')
    for token in ['m_SizeDelta: {x: 898, y: 564}','m_FontSize: 30','m_SizeDelta: {x: 824.5, y: 40}']:
        assert token in frame,token
    frame_meta=(ROOT/'ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_frame.png.meta').read_text(encoding='utf-8-sig')
    assert tuple(unity_sprite_rect(frame_meta,'pl_fra_1006'))==(1,1483,898,564)
    button=(ROOT/'ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_noaddressable/UIButton/UI_Model_StandardButton_Blue.prefab').read_text(encoding='utf-8-sig')
    for token in ['m_FontSize: 32','r: 0.039215688','g: 0.105882354','b: 0.18039216']:
        assert token in button,token
    mediator=(ROOT/'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Setting/LanguageSetMediator.cs').read_text(encoding='utf-8-sig')
    change=mediator.split('private void OnValueChange',1)[1].split('private void OnItemEnter',1)[0]
    assert 'm_curLanguage = id;' in change and '//LanguageUtils.SetLanguage' in change
    assert 'm_curLanguage = (int)LanguageUtils.GetLanguage();' in mediator
    sure=mediator.split('private void OnSureClick',1)[1].split('private void OnCloseClick',1)[0]
    assert 'LanguageUtils.SaveCache();' in sure and 'CmdConstant.ReloadGame' in sure
    cpp=(ROOT/'Source/Rok/RokUnityFeatureScreenWidget.cpp').read_text(encoding='utf-8-sig')
    language=cpp.split('void URokUnityFeatureScreenWidget::BuildLanguageSettings()',1)[1].split('void URokUnityFeatureScreenWidget::UpdateLanguageDraftSelection()',1)[0]
    assert 'SetFlowDirectionPreference(EFlowDirectionPreference::LeftToRight)' in language
    assert 'BuildSplitWindowFrame' not in language and 'LanguageSettingsSourceFrame' in language
    assert 'FLinearColor::FromSRGBColor(FColor(10,27,46,255))' in language
    assert 'TextOptions.bOverride_TextFlowDirection=true' in language
    assert 'TextOptions.TextFlowDirection=ETextFlowDirection::LeftToRight' in language
    assert 'Label->SetShapedTextOptions(TextOptions)' in language
    assert 'KerningOnly' not in language
    action=cpp.split('if (ActionId >= 1460 && ActionId <= 1464)',1)[1].split('if (ActionId >= 1500',1)[0]
    assert 'UpdateLanguageDraftSelection' in action and 'CanCloseWithoutReload' in action
    assert all(x not in action for x in ['SetCurrentCulture','SaveConfig','ReloadGame','SetLanguage('])
    out={'source_verified':True,'source_order':[int(r['ID']) for r in enabled],'labels':labels,'viewport':[278.5,190.65,883,370.4],'row_height':111,'cell_height':114,'runtime_verified':False,'visual_verified':False,'boundary':'Local radio draft only; changed-language confirmation/reload remains unavailable.'}
    (ROOT/'Saved/RokUnityMigration/language_settings_layout_verify.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    print('PASS: LanguageSet enabled IDs, Chinese source grid, radio art/fonts, local-only draft boundary')
if __name__=='__main__':main()
