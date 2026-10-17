"""Check complete source FAQ and local tree contract, without running UE."""
import hashlib,json
from pathlib import Path
from BuildRokHelpCatalog import build,ROOT

def main():
    data=build()
    mediator=(ROOT/'ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Helper/UI_Win_HelperMediator.cs').read_text(encoding='utf-8-sig')
    for token in ['QueryRecords<GameSpriteDefine>()','isParent = true, isFold = true','m_currentTree[index].isFold = !m_currentTree[index].isFold','ShowDetails(m_currentTree[index].intData)','LanguageUtils.getText(define.text)','view.m_pl_detial.gameObject.SetActive(false)']:
        assert token in mediator,token
    assert [e['id'] for e in data['entries'][-8:]]==list(range(4032,4024,-1))
    assert all(e['group_id']==900004 and e['display_group_id']==900006 for e in data['entries'][-8:])
    assert sum(e['display_group_id']==900004 for e in data['entries'])==24
    assert next(e for e in data['entries'] if e['id']==4033)['display_group_id']==900001
    assert sum(e['display_group_id']==900006 for e in data['entries'])==15
    service=(ROOT/'ref/client-unity/Assets/Skyunion/RunTime/DataService/DataService.cs').read_text(encoding='utf-8-sig')
    assert 'listRecords = new List<T>(records);' in service and 'records[i] = ConvertToObject(rows[i]);' in service
    assert data['group_order']==[900005,900004,900003,900002,900007,900001,900006]
    for row in data['sources']:assert hashlib.sha256((ROOT/row['path']).read_bytes()).hexdigest()==row['sha256']
    assert data['layouts']['group']['']['rect'][3]==43
    assert data['layouts']['question']['']['rect'][3]==54
    assert data['layouts']['window']['pl_detial/sv_list/v/c/lbl_detial']['font_px']==20
    assert data['layouts']['frame']['win/frame2']['rect'][0]==564
    assert data['layouts']['group']['btn_btn/img_arrowUp']['flip_y']==False
    cpp=(ROOT/'Source/Rok/RokHelpWidget.cpp').read_text(encoding='utf-8-sig')
    assert all(x not in cpp for x in ['SaveConfig(', 'LaunchURL(', 'ProcessRequest(', 'SendRequest('])
    assert 'OnCloseRequested.Broadcast()' in cpp and 'VisibleQuestionIds.Contains(Action)' in cpp
    result={'passed':True,'entries':70,'groups':7,'runtime_verified':False,'visual_verified':False,'boundary':'Local FAQ only; SDK unavailable; host owns T1'}
    (ROOT/'Saved/RokUnityMigration/help_catalog_verify.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print('PASS: 70 source questions / 7 ordered groups, layout, text and local-only actions')

if __name__=='__main__':main()
