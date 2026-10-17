"""Independent local source contract checks; no engine invocation."""
import json,re
from BuildRokHospitalExplanation import build,ROOT,MEDIATOR,REQUIRED_SOURCES,runtime_text
x=build();raw=x['source_text'];assert x['language_id']==181001
assert raw==runtime_text()
assert len(x['sources'])==7 and {v['path'] for v in x['sources']}=={p.relative_to(ROOT).as_posix() for p in REQUIRED_SOURCES}
assert 'workbook_matches_runtime_bin' in x['text_source_comparison']
assert len(x['yellow_segments'])==13 and len(re.findall(r'(?m)^[1-7]\.',raw))==7
assert x['rich_text']==raw.replace('<color=yellow>','<yellow>').replace('</color>','</>')
label=x['nodes']['Rect/Right/pl_explain/sv_explain_view/v_explain_view/c_explain_view/lbl_explain'];assert label['font_px']==20 and label['alignment']==0 and label['line_spacing']==1.25
assert abs(label['rect'][2]-483)<.001
m=MEDIATOR.read_text(encoding='utf-8-sig');block=m[m.index('private void OnExplain()'):m.index('//治疗 加速')]
assert 'm_isShowedExplain' in block and 'LanguageUtils.getText(181001)' in block
assert 'preferredHeight- view.m_lbl_explain' in block and 'private void OnBack()' in block
assert 'Play(' not in block and 'Send' not in block
cpp=(ROOT/'Source/Rok/RokUnityFeatureScreenWidget.cpp').read_text(encoding='utf-8-sig')
assert 'SetHospitalExplanationVisible(true)' in cpp and 'HospitalExplanationBackButton' in cpp
helper=(ROOT/'Source/Rok/RokHospitalExplanationWidget.cpp').read_text(encoding='utf-8-sig')
assert 'FColor(255,255,0)' in helper and 'SetLineHeightPercentage(1.25f)' in helper
print('PASS: 7 source rules,13 yellow spans,Chinese source rectangle and local-only visibility contract')
