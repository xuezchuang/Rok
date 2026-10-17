#!/usr/bin/env python3
"""Strict Alliance Technology actual-Slate receipt verifier; self-test is offline only."""
from __future__ import annotations
import argparse, copy, hashlib, importlib.util, json, re
from pathlib import Path

SELF=Path(__file__).resolve()
ROOT=SELF.parents[3] if SELF.parents[1].name=='PendingAllianceTechnologyReadonlyRuntimeAuditV5' else SELF.parents[1]
STAGE=ROOT/'Saved/PendingAllianceTechnologyReadonlyRuntimeAuditV5'
SOURCE=ROOT/'Saved/PendingAllianceTechnologyReadonlyCatalogV1'
SCHEMA='rok.alliance_technology_readonly.runtime_audit.v2'
SCOPE='Anonymous HUD Alliance technology static reference: 460 source rows, 60 widget identities, six first/last Slate selections, three real wheels, configured level 1/2, close/reopen'
MANIFEST_SHA='82c8868cc3a83dec1fc0c7e5d126cc567a7d7b1d0781d892496901f94ba557ad'
PHASES=('00_city_hud','01_alliance_welcome','02_develop_first_default','03_configured_level_2',
 '04_configured_level_1','05_develop_last','06_territory_first_default','07_territory_last',
 '08_war_first_default','09_war_last','10_welcome_return','11_hud_return',
 '12_alliance_reopened','13_technology_reopened','14_final_hud')
CLICKS=(('open_alliance_from_hud','AllianceFeatureButton',0),
 ('open_technology_reference','AllianceTechnologyReferenceOpen',636),
 ('select_develop_first_101','AllianceTechnologyDefinitionHit_101',17101),
 ('configured_level_next_2','AllianceTechnologyConfiguredNext',685),
 ('configured_level_previous_1','AllianceTechnologyConfiguredPrevious',684),
 ('select_develop_last_122','AllianceTechnologyDefinitionHit_122',17122),
 ('select_territory_category','AllianceTechnologyCategory_2',682),
 ('select_territory_first_201','AllianceTechnologyDefinitionHit_201',17201),
 ('select_territory_last_216','AllianceTechnologyDefinitionHit_216',17216),
 ('select_war_category','AllianceTechnologyCategory_3',683),
 ('select_war_first_301','AllianceTechnologyDefinitionHit_301',17301),
 ('select_war_last_322','AllianceTechnologyDefinitionHit_322',17322),
 ('back_to_alliance_welcome','AllianceTechnologyReferenceBack',680),
 ('close_alliance_to_hud','FeatureFrameClose',1),
 ('reopen_alliance_from_hud','AllianceFeatureButton',0),
 ('reopen_technology_reference','AllianceTechnologyReferenceOpen',636),
 ('final_close_alliance_to_hud','FeatureFrameClose',1))
WHEELS=('scroll_develop_to_last','scroll_territory_to_last','scroll_war_to_last')
ORDER=(CLICKS[0][0],CLICKS[1][0],CLICKS[2][0],CLICKS[3][0],CLICKS[4][0],
 WHEELS[0],CLICKS[5][0],CLICKS[6][0],CLICKS[7][0],WHEELS[1],CLICKS[8][0],
 CLICKS[9][0],CLICKS[10][0],WHEELS[2],CLICKS[11][0],CLICKS[12][0],
 CLICKS[13][0],CLICKS[14][0],CLICKS[15][0],CLICKS[16][0])
INTERVALS=(0,1,2,2,3,4,4,5,6,6,6,7,8,8,8,9,10,11,12,13)
FILES=('Source/Rok/RokAllianceTechnologyReadonlyAuditSubsystem.h',
 'Source/Rok/RokAllianceTechnologyReadonlyAuditSubsystem.cpp',
 'Scripts/VerifyRokAllianceTechnologyReadonlyRuntime.py')
FALSE=('world_time_advanced_by_audit','source_pixel_parity_claim','unity_pixel_parity_accepted',
 'full_migration_accepted','alliance_membership_accepted','research_level_or_progress_accepted',
 'account_alliance_state_accepted')
CAMERA=('camera_x','camera_y','camera_z','camera_pitch','camera_yaw','camera_roll',
 'camera_scale_x','camera_scale_y','camera_scale_z')

def need(ok,msg):
 if not ok:raise ValueError(msg)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def checked(relative):
 need(relative and not Path(relative).is_absolute() and '..' not in Path(relative).parts,'path escape')
 p=(ROOT/relative).resolve(strict=True)
 need(ROOT.resolve() in p.parents and p.is_file(),'input outside project')
 return p
def point(p):return float(p['x']),float(p['y'])
def shown(w):return w['exists'] is True and w['visible'] is True and w['enabled'] is True

def office_helpers():
 p=checked('Saved/PendingAllianceOfficeReadonlyRuntimeAuditV3/Scripts/VerifyRokAllianceOfficeReadonlyRuntime.py')
 spec=importlib.util.spec_from_file_location('office_audit_verified_helpers',p)
 need(spec and spec.loader,'Office helper import unavailable')
 module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 return module

def source_rows():
 need(sha(SOURCE/'alliance_technology_readonly_catalog_v1.json')==MANIFEST_SHA,'candidate manifest SHA')
 spec=importlib.util.spec_from_file_location('tech_source_extract',SOURCE/'ExtractAllianceTechnologyReadonlyCatalogV1.py')
 need(spec and spec.loader,'source extractor unavailable')
 module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 fresh=module.build()
 need(module.encoded(fresh)==(SOURCE/'alliance_technology_readonly_catalog_v1.json').read_bytes(),
      'fresh source differs from candidate')
 published=checked('Saved/RokUnityMigration/AllianceTechnologyReadonlyCatalogV1.json')
 need(sha(published)==MANIFEST_SHA and published.read_bytes()==(SOURCE/'alliance_technology_readonly_catalog_v1.json').read_bytes(),
      'published runtime manifest differs')
 need(len(fresh['rows'])==460 and fresh['prerequisite_edges']==202
      and len({r['study_type'] for r in fresh['rows']})==60,'source count')
 return fresh['rows']

def hud_atlas_guard(cpp):
 return ('inline bool ResearchAtlasesReady()' in cpp
         and 'FTextureCompilingManager::Get().GetNumRemainingTextures() != 0' in cpp
         and 'RokAllianceTechnologyUiSettledProbe::ResearchAtlasesReady();' in cpp
         and 'Resource->TextureRHI->GetDesc().Extent != Expected.Extent' in cpp)

PNG8=b'\x89PNG\r\n\x1a\n'
NATIVE_PNG_LITERAL=r'"\x89PNG\r\n\x1a\n"'

def check_png_signature(raw):
 need(len(raw)>=8 and raw[:8]==PNG8,'canonical PNG signature')

def native_png_guard(cpp):
 return cpp.count(NATIVE_PNG_LITERAL)==1 and r'"\x89PNG\r\n\x1a\r\n"' not in cpp

def expected_inputs():
 report=json.loads((STAGE/'release_report.json').read_text(encoding='utf-8'))
 need(report['schema']=='rok.alliance_technology_readonly.audit_stage.v5'
      and set(report['files'])==set(FILES),'release schema/files')
 cpp=(STAGE/FILES[1]).read_text(encoding='utf-8')
 pins=dict(re.findall(r'\{TEXT\("([^\"]+)"\), TEXT\("([0-9a-f]{64})"\)\}',cpp))
 need(len(pins)>=50 and pins==report['source_and_product_inputs_sha256'],'native pin set')
 need('ProcessMouseButtonDownEvent' in cpp and 'ProcessMouseWheelOrGestureEvent' in cpp
      and 'RequestScreenshot' in cpp and 'AllianceTechnologyDefinitionHit_322' in cpp
      and 'if (!bSettled)' in cpp and hud_atlas_guard(cpp) and native_png_guard(cpp),'native gate')
 for path,item in report['files'].items():need(sha(STAGE/path)==item['sha256'],'stage file drift '+path)
 return {**pins,**{p:report['files'][p]['sha256'] for p in FILES}}

def check_claim(d):
 need(d['schema']==SCHEMA and d['complete'] is True and d['passed'] is True
      and d['failures']==[] and d['attempts']==[] and d['scope']==SCOPE
      and d['baseline_phase']==PHASES[0] and d['completion_events']==0
      and d['source_row_count']==460 and d['study_type_count']==60
      and d['prerequisite_edges']==202 and d['scroll_events']==3,'claim/scope/count')
 need(all(d[k] is False for k in FALSE),'unsupported claim')
 need(d['engine_major']==5 and d['engine_minor']==8 and Path(d['project']).resolve()==ROOT.resolve()
      and Path(d['engine_dir']).resolve()==Path('D:/ue/UE_5.8/Engine').resolve()
      and d['world']=='/Game/Maps/L_RokUnityGameAppCity' and type(d['pid']) is int and d['pid']>0,
      'engine/project/map/PID')

def check_atlases(atlases):
 need(len(atlases)==2,'two research atlases')
 for a,name,width in zip(atlases,('research_icon1','research_icon2'),(512,1024)):
  need(a['path']==f'/Game/RokPrototype/UI/ResearchAtlases/rok_research_{name}.rok_research_{name}'
       and a['srgb'] is True and a['ui_group'] is True and a['no_mip_generation'] is True
       and a['alpha_channel'] is True and a['texture_mips']==a['rhi_mips']==1
       and a['rhi_valid'] is True and a['texture_width']==a['rhi_width']==width
       and a['texture_height']==a['rhi_height']==2048,'atlas RHI/alpha/mip semantics '+name)

def check_settled(p,children):
 need(p['ready'] is True and p['structure_valid'] is True and p['direct_children']>5
      and p['unsettled_children']==p['unsettled_not_visible_children']==0
      and p['remaining_texture_compilations']==0 and p['texture_count']==6
      and p['texture_rhi_ready'] is True and p['portrait_gate_applicable'] is False
      and p['portrait_present'] is False,'direct child/texture settled')
 need(len(children)==p['direct_children'] and all(c['exists'] is True and c['opacity']>=.995
      and all(abs(x)<=.25 for x in point(c['render_translation'])) for c in children)
      and sum(c['visible'] is False for c in children)==p['not_visible_children'],
      'all direct children including hidden')

def check_rows(s,source,category):
 actual=s['source_rows'];need(len(actual)==460 and s['prerequisite_edges']==202,'runtime 460/202')
 for row,got in zip(source,actual):
  expected={'id':row['id'],'category_type':row['category_type'],'study_type':row['study_type'],
   'level_key':row['study_level_key'],'title_locale_id':row['title_locale_id'],
   'description_locale_id':row['description_locale_id'],'title':row['title_cn'],
   'description':row['description_cn'],'icon_atlas':row['icon']['atlas'],
   'icon_sprite':row['icon']['sprite'],
   'icon_rect':dict(zip(('x','y','w','h'),row['icon']['rect_xywh'])),
   'atlas_size':{'x':row['icon']['atlas_size'][0],'y':row['icon']['atlas_size'][1]},
   'prerequisites':[{'slot':p['slot'],'study_type':p['study_type'],
     'required_level':p['required_level'],'source_row_id':p['source_row_id']}
     for p in row['prerequisites']]}
  need(got==expected,'runtime row mismatch '+str(row['id']))
 definitions=[r for r in source if r['category_type']==category and r['study_level_key']==1]
 observed=s['definition_widgets']
 need(len(observed)==len(definitions)==(16 if category==2 else 22),'definition count')
 for row,item in zip(definitions,observed):
  t=row['study_type'];need(item['study_type']==t and item['action_id']==17000+t,'button action identity')
  for key in ('Definition','Name','Id','Icon','DefinitionHit'):
   need(item[key]['exists'] is True and item[key]['name']==f'AllianceTechnology{key}_{t}',
        'definition widget identity '+str(t))
  need(item['Name']['text']==row['title_cn']
       and item['Id']['text']==f'studyType {t} · 配置等级 1','definition text '+str(t))
  icon=item['Icon'];atlas=row['icon'];width,height=atlas['atlas_size'];x,y,w,h=atlas['rect_xywh']
  uv_min=(x/width,(height-y-h)/height);uv_max=((x+w)/width,(height-y)/height)
  need(icon['brush_resource']==f"/Game/RokPrototype/UI/ResearchAtlases/rok_research_{atlas['atlas']}.rok_research_{atlas['atlas']}"
       and all(abs(a-b)<=1e-5 for a,b in zip(point(icon['uv_min']),uv_min))
       and all(abs(a-b)<=1e-5 for a,b in zip(point(icon['uv_max']),uv_max))
       and point(icon['brush_size'])==(52.0,52.0),'source atlas UV/size '+str(t))

def check_state(s,phase,business,primary,selection,camera,pid,source):
 need(s['window_foreground'] is True and s['window_owner_pid']==s['foreground_owner_pid']==pid
      and s['client_width']==1440 and s['client_height']==810
      and s['own_window_identity']==s['foreground_window_identity']
      and re.fullmatch(r'0x[0-9a-fA-F]+',s['own_window_identity']),'foreground own window')
 need(s['business']==business and s['primary_press_count']==primary and s['selected_actor']==selection
      and s['modal_open'] is False and all(abs(s[k]-v)<=.001 for k,v in zip(CAMERA,camera)),
      'business/HTTP/camera')
 check_atlases(s['research_atlases'])
 w=s['widgets'];phase_index=PHASES.index(phase)
 if phase_index in (0,11,14):
  need(s['screen_open'] is False and shown(w['AllianceFeatureButton']),'HUD phase');return
 need(s['screen_open'] is True and shown(w['FeatureFrameClose']) and shown(w['FeatureFrameTitle'])
      and shown(w['FeatureModeCanvas']),'Alliance frame')
 check_settled(s['alliance_ui_settled'],s['all_direct_children_including_hidden'])
 need(s['active_mode']==6 and s['alliance_preview_state']==s['selected_alliance_page']==0
      and s['pending_action_id']==-1,'anonymous Alliance state')
 if phase_index in (1,10,12):
  need(s['technology_reference_open'] is False and w['FeatureFrameTitle']['text']=='联盟'
       and shown(w['AllianceTechnologyReferenceOpen'])
       and w['AllianceTechnologyReferenceBack']['exists'] is False,'welcome entry');return
 need(s['technology_reference_open'] is True and w['FeatureFrameTitle']['text']=='联盟科技配置参考'
      and shown(w['AllianceTechnologyReferenceBack'])
      and w['AllianceTechnologyReferenceBoundary']['text']=='仅显示 AllianceStudy 静态配置；当前研究等级、进度、捐献与解锁由服务器决定'
      and shown(w['AllianceTechnologyDetailIcon'])
      and w['AllianceTechnologyIconUnavailable']['exists'] is False
      and s['source_row_count']==460 and s['study_type_count']==60,'reference boundary/icon')
 category=2 if phase_index in (6,7) else 3 if phase_index in (8,9) else 1
 study={5:122,6:201,7:216,8:301,9:322}.get(phase_index,101)
 level=2 if phase_index==3 else 1
 need((s['selected_category'],s['selected_study_type'],s['selected_config_level'])==(category,study,level),
      'category/study/config selection')
 check_rows(s,source,category)
 selected=next(r for r in source if r['study_type']==study and r['study_level_key']==level)
 prereq='配置前置 ID：'+('、'.join(str(p['source_row_id']) for p in selected['prerequisites']) or '无')
 need(w['AllianceTechnologyDetailTitle']['text']==selected['title_cn']
      and w['AllianceTechnologyDetailDescription']['text']==selected['description_cn']
      and w['AllianceTechnologyDetailIdentity']['text']==f"源配置行 ID {selected['id']} · studyType {study} · 配置等级 {level}"
      and w['AllianceTechnologyDetailPrerequisites']['text']==prereq,'selected detail source truth')
 icon=w['AllianceTechnologyDetailIcon'];atlas=selected['icon'];width,height=atlas['atlas_size'];x,y,cw,ch=atlas['rect_xywh']
 need(icon['brush_resource']==f"/Game/RokPrototype/UI/ResearchAtlases/rok_research_{atlas['atlas']}.rok_research_{atlas['atlas']}"
      and all(abs(a-b)<=1e-5 for a,b in zip(point(icon['uv_min']),(x/width,(height-y-ch)/height)))
      and all(abs(a-b)<=1e-5 for a,b in zip(point(icon['uv_max']),((x+cw)/width,(height-y)/height)))
      and point(icon['brush_size'])==(108.0,108.0),'selected detail atlas UV/size')

def check_wheel(a,label,pid,business,primary):
 need(a['label']==label and a['method']=='ProcessMouseWheelOrGestureEvent'
      and a['target']=='AllianceTechnologyDefinitionScroll'
      and a['target_widget_class']=='/Script/UMG.ScrollBox'
      and a['exact_scroll_in_path'] is True and a['own_game_window'] is True
      and a['foreground'] is True and a['wheel_handled'] is True
      and a['own_window_pid']==pid and a['wheel_delta']==-2048
      and a['scroll_end_before']>a['scroll_before']
      and a['scroll_after']>a['scroll_before'] and a['failure']==''
      and a['business_before']==a['business_after']==business
      and a['primary_before']==a['primary_after']==primary,'actual Slate wheel')
 need(any('SScrollBox' in t for t in a['hit_path_types']),'wheel hit path')
 size=point(a['scroll_geometry_size'])
 need(size[0]>0 and size[1]>0 and a['layout_stable_frames']>=2
      and a['layout_first_frame']<a['layout_last_frame']==a['frame'],
      'prewheel real-frame stable positive-range geometry')

def validate(d,run):
 source=source_rows();check_claim(d);inputs=expected_inputs()
 need(d['inputs_sha256']==inputs,'runtime exact input set')
 for relative,digest in inputs.items():need(sha(checked(relative))==digest,'input drift '+relative)
 friend=checked('Source/Rok/RokUnityFeatureScreenWidget.h').read_text(encoding='utf-8-sig')
 need(friend.count('friend class URokAllianceTechnologyReadonlyAuditSubsystem;')==2
      and friend.count('friend class URokAllianceOfficeReadonlyAuditSubsystem;')==2
      and all('friend class '+x+';' in friend for x in (
       'URokCommanderSkillReadonlyDetailAuditSubsystem','URokDailyTaskReadonlyCatalogAuditSubsystem',
       'URokVipDailyPointHelpTipAuditSubsystem','URokActivityCalendarReadonlyAuditSubsystem')),
      'all existing audit friends retained')
 office=office_helpers();business=d['baseline_business'];office.check_business(business)
 caps,acts=d['captures'],d['actions']
 need(len(caps)==15 and tuple(c['phase'] for c in caps)==PHASES
      and len(acts)==20 and tuple(a['label'] for a in acts)==ORDER,'15 PNG/20 action route')
 first=caps[0]['requested_state'];primary,selection=first['primary_press_count'],first['selected_actor']
 camera=tuple(first[k] for k in CAMERA);previous=-1
 for i,cap in enumerate(caps):
  frames=(cap['requested_frame'],cap['processed_frame'],cap['next_frame'])
  need(previous<frames[0]<=frames[1]<frames[2],'capture ordering');previous=frames[2]
  p=Path(cap['png'])
  need(p.is_absolute() and p.resolve(strict=True)==(run/(PHASES[i]+'.png')).resolve(strict=True)
       and p.parent.resolve()==run.resolve(),'original PNG origin')
  need(cap['png_width']==1440 and cap['png_height']==810,'PNG extent metadata')
  check_png_signature(p.read_bytes())
  office.check_png(p,cap['png_sha256'],cap['png_bytes'])
  for key,frame in zip(('requested_state','processed_state','next_frame_state'),frames):
   need(cap[key]['frame']==frame,'sample frame')
   check_state(cap[key],PHASES[i],business,primary,selection,camera,d['pid'],source)
 need(len({c['png_sha256'] for c in caps})>=10,'UI PNG diversity')
 for a,label,interval in zip(acts,ORDER,INTERVALS):
  need(caps[interval]['next_frame']<a['frame']<caps[interval+1]['requested_frame'],
       'action/capture chronology')
  if label in WHEELS:check_wheel(a,label,d['pid'],business,primary)
  else:
   spec=next(x for x in CLICKS if x[0]==label)
   cls='/Script/Rok.RokUiAnimatedButton' if spec[2]==0 else '/Script/Rok.RokUnityIndexedButton'
   office.check_click(a,(*spec,cls),d['pid'],business,primary)
 check_state(d['final_state'],PHASES[-1],business,primary,selection,camera,d['pid'],source)
 need(d['final_state']['frame']>=caps[-1]['next_frame'],'final state chronology')

def reject(fn,label):
 try:fn()
 except (ValueError,KeyError,TypeError,OSError):return
 raise RuntimeError('negative accepted: '+label)

def self_test():
 expected_inputs()
 cpp=(STAGE/FILES[1]).read_text(encoding='utf-8')
 need(hud_atlas_guard(cpp) and native_png_guard(cpp),'positive HUD atlas and PNG guards')
 check_png_signature(PNG8+b'IHDR')
 reject(lambda:check_png_signature(PNG8[:-1]+b'\r'+b'IHDR'),'bad PNG signature CR');n=1
 reject(lambda:native_png_guard(cpp.replace(NATIVE_PNG_LITERAL,r'"\x89PNG\r\n\x1a\r\n"')) or need(False,'old native PNG literal accepted'),'bad native PNG literal');n+=1
 for old in ('RokAllianceTechnologyUiSettledProbe::ResearchAtlasesReady();',
             'FTextureCompilingManager::Get().GetNumRemainingTextures() != 0',
             'Resource->TextureRHI->GetDesc().Extent != Expected.Extent'):
  reject(lambda old=old:hud_atlas_guard(cpp.replace(old,'removed')) or need(False,'removed guard accepted'),old)
 d={'schema':SCHEMA,'complete':True,'passed':True,'failures':[],'attempts':[],'scope':SCOPE,
    'baseline_phase':PHASES[0],'completion_events':0,'source_row_count':460,
    'study_type_count':60,'prerequisite_edges':202,'scroll_events':3,'engine_major':5,
    'engine_minor':8,'project':str(ROOT),'engine_dir':'D:/ue/UE_5.8/Engine',
    'world':'/Game/Maps/L_RokUnityGameAppCity','pid':1234,**{k:False for k in FALSE}}
 check_claim(d)
 for k,bad in (('complete',False),('passed',False),('scope','account research level accepted'),
               ('source_row_count',459),('study_type_count',59),('prerequisite_edges',201),
               ('scroll_events',2),('engine_minor',7),('pid',0),('completion_events',1),
               ('alliance_membership_accepted',True),('research_level_or_progress_accepted',True),
               ('full_migration_accepted',True),('unity_pixel_parity_accepted',True)):
  x=copy.deepcopy(d);x[k]=bad;reject(lambda x=x:check_claim(x),'claim '+k);n+=1
 def atlas(name,width):return {'path':f'/Game/RokPrototype/UI/ResearchAtlases/rok_research_{name}.rok_research_{name}',
  'srgb':True,'ui_group':True,'no_mip_generation':True,'alpha_channel':True,
  'texture_mips':1,'rhi_mips':1,'rhi_valid':True,'texture_width':width,'rhi_width':width,
  'texture_height':2048,'rhi_height':2048}
 a=[atlas('research_icon1',512),atlas('research_icon2',1024)];check_atlases(a)
 for k,bad in (('srgb',False),('ui_group',False),('alpha_channel',False),('rhi_width',32),
               ('rhi_mips',2),('texture_height',1024),('no_mip_generation',False)):
  x=copy.deepcopy(a);x[0][k]=bad;reject(lambda x=x:check_atlases(x),'atlas '+k);n+=1
 p={'ready':True,'structure_valid':True,'direct_children':6,'unsettled_children':0,
    'not_visible_children':1,'unsettled_not_visible_children':0,'remaining_texture_compilations':0,
    'texture_count':6,'texture_rhi_ready':True,'portrait_gate_applicable':False,'portrait_present':False}
 children=[{'exists':True,'visible':i!=0,'opacity':1.0,
            'render_translation':{'x':0,'y':0}} for i in range(6)]
 check_settled(p,children)
 for k,bad in (('unsettled_children',1),('texture_count',4),('texture_rhi_ready',False),
               ('not_visible_children',0),('remaining_texture_compilations',1)):
  x=copy.deepcopy(p);x[k]=bad;reject(lambda x=x:check_settled(x,children),'settled '+k);n+=1
 x=copy.deepcopy(children);x[0]['opacity']=.5;reject(lambda:check_settled(p,x),'hidden child');n+=1
 x=copy.deepcopy(children);x[0]['render_translation']['x']=2
 reject(lambda:check_settled(p,x),'hidden child translation');n+=1
 rows=json.loads((SOURCE/'alliance_technology_readonly_catalog_v1.json').read_text(encoding='utf-8'))['rows']
 def widget(name,text=''):
  return {'exists':True,'name':name,'text':text,'brush_resource':'','uv_min':{'x':0,'y':0},
          'uv_max':{'x':0,'y':0},'brush_size':{'x':0,'y':0}}
 observed=[]
 for row in rows:
  icon=row['icon'];observed.append({'id':row['id'],'category_type':row['category_type'],
   'study_type':row['study_type'],'level_key':row['study_level_key'],
   'title_locale_id':row['title_locale_id'],'description_locale_id':row['description_locale_id'],
   'title':row['title_cn'],'description':row['description_cn'],'icon_atlas':icon['atlas'],
   'icon_sprite':icon['sprite'],'icon_rect':dict(zip(('x','y','w','h'),icon['rect_xywh'])),
   'atlas_size':{'x':icon['atlas_size'][0],'y':icon['atlas_size'][1]},
   'prerequisites':[{'slot':p['slot'],'study_type':p['study_type'],
                    'required_level':p['required_level'],'source_row_id':p['source_row_id']}
                   for p in row['prerequisites']]})
 definitions=[]
 for row in rows:
  if row['category_type']!=1 or row['study_level_key']!=1:continue
  t=row['study_type'];icon=row['icon'];x,y,w,h=icon['rect_xywh'];aw,ah=icon['atlas_size']
  item={'study_type':t,'action_id':17000+t}
  for key in ('Definition','Name','Id','Icon','DefinitionHit'):
   item[key]=widget(f'AllianceTechnology{key}_{t}')
  item['Name']['text']=row['title_cn'];item['Id']['text']=f'studyType {t} · 配置等级 1'
  item['Icon'].update({'brush_resource':f"/Game/RokPrototype/UI/ResearchAtlases/rok_research_{icon['atlas']}.rok_research_{icon['atlas']}",
    'uv_min':{'x':x/aw,'y':(ah-y-h)/ah},'uv_max':{'x':(x+w)/aw,'y':(ah-y)/ah},
    'brush_size':{'x':52,'y':52}})
  definitions.append(item)
 state={'source_rows':observed,'prerequisite_edges':202,'definition_widgets':definitions}
 check_rows(state,rows,1)
 prerequisite_index=next(i for i,r in enumerate(rows) if r['prerequisites'])
 for group,index,key,bad in (('source_rows',459,'title','forged'),('source_rows',prerequisite_index,'prerequisites',[]),
                              ('definition_widgets',0,'action_id',0),('definition_widgets',0,'Name',widget('bad'))):
  x=copy.deepcopy(state);x[group][index][key]=bad
  reject(lambda x=x:check_rows(x,rows,1),'source/widget '+key);n+=1
 x=copy.deepcopy(state);x['definition_widgets'][0]['Icon']['uv_min']['x']+=.01
 reject(lambda:check_rows(x,rows,1),'icon UV');n+=1
 for bad in ('../escape','/absolute/escape',''):
  reject(lambda bad=bad:checked(bad),'path escape');n+=1
 wheel={'label':WHEELS[0],'method':'ProcessMouseWheelOrGestureEvent',
        'target':'AllianceTechnologyDefinitionScroll','target_widget_class':'/Script/UMG.ScrollBox',
        'exact_scroll_in_path':True,'own_game_window':True,'foreground':True,
        'wheel_handled':True,'own_window_pid':1234,'wheel_delta':-2048,
        'scroll_before':0,'scroll_end_before':100,'scroll_after':80,'failure':'',
        'business_before':{},'business_after':{},'primary_before':0,'primary_after':0,
        'hit_path_types':['SWindow','SScrollBox'],
        'scroll_geometry_size':{'x':400,'y':457},'layout_stable_frames':2,
        'layout_first_frame':10,'layout_last_frame':11,'frame':11}
 check_wheel(wheel,WHEELS[0],1234,{},0)
 x=copy.deepcopy(wheel);x['scroll_end_before']=0
 reject(lambda:check_wheel(x,WHEELS[0],1234,{},0),'prewheel zero range')
 x=copy.deepcopy(wheel);x['layout_stable_frames']=1
 reject(lambda:check_wheel(x,WHEELS[0],1234,{},0),'prewheel one frame')
 return {'synthetic_negative_cases':n,'prewheel_negative_cases':2,
         'actual_runtime_verified':False}

def main():
 p=argparse.ArgumentParser();p.add_argument('--self-test',action='store_true');p.add_argument('--manifest',type=Path);a=p.parse_args()
 need(a.self_test != bool(a.manifest),'choose one mode')
 if a.self_test:print(json.dumps(self_test(),sort_keys=True));return
 path=a.manifest.resolve(strict=True);validate(json.loads(path.read_text(encoding='utf-8')),path.parent)
 print(json.dumps({'actual_runtime_verified':True,'original_png':15,'actual_actions':20,'source_rows':460}))
if __name__=='__main__':main()
