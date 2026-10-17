"""Check actual native temporal evidence; refuses absent/lost scenarios."""
import json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RokUnityMigration/WorldDayNightContinuous'
def main():
    data=json.loads((OUT/'native_timeline.json').read_text(encoding='utf-8'))
    failures=[]
    def check(value,label):
        if not value: failures.append(label)
    frames=data['frames'];events=data['events'];groups={}
    for f in frames:
        groups.setdefault(f['scenario'],[]).append(f)
        check(len(f['layers'])==14 and len({x['id'] for x in f['layers']})==14,'14 native identities')
        check(sum(x['migratedGridLayer'] for x in f['layers'])==13,'13 migrated source layers')
        extra=[x for x in f['layers'] if not x['migratedGridLayer']]
        check(len(extra)==1 and extra[0]['id']=='6942459152414132847' and not extra[0]['active'],'source extra remains inactive')
        check(all(math.isfinite(v) for v in f['rgb']),'finite RGB')
    for name in ['cycle_1796','cycle_2396','cycle_56','cycle_1798','lifecycle']:
        check(name in groups and len(groups[name])>10,'missing continuous '+name)
    for name in ['cycle_1796','cycle_2396','cycle_56','cycle_1798']:
        fs=groups.get(name,[])
        check(len({tuple(round(v,4) for v in f['rgb']) for f in fs})>20,'missing intermediate colors '+name)
    check(any(e['kind']=='reverse_completion_done' and 'city_building_night' in e['value'] for e in events),'reverse callback does not apply source arrival order')
    check(any(e['kind']=='gameobject_off' for e in events) and any(e['kind']=='component_disabled_pending_day' for e in events),'missing interruption scenarios')
    life=[e for e in events if e['scenario']=='lifecycle']
    marks={e['kind']:e for e in life if e['kind'] not in ['asset_request','asset_complete']}
    off=marks['gameobject_off'];on=marks['gameobject_on'];disabled=marks['component_disabled_pending_day'];enabled=marks['component_enabled']
    owner=off['owner']
    check(not any(e['kind']=='asset_request' and e['owner']==owner and e['value']=='map_building' and off['frame']<=e['frame']<on['frame'] for e in life),'GameObject disable must stop waiting day request')
    check(any(e['kind']=='asset_request' and e['owner']==owner and e['value']=='city_building_night' and on['frame']<e['frame']<disabled['frame'] for e in life),'OnEnable night must request again')
    check(any(e['kind']=='asset_complete' and e['owner']==owner and e['value']=='map_building' and disabled['frame']<e['frame']<enabled['frame'] for e in life),'component disable must not stop pending coroutine')
    orders={}
    for name in ['cycle_1796','cycle_2396','cycle_56']:
        ns=[e for e in events if e['scenario']==name and e['kind']=='phase_notify']
        f=ns[1]['frame'];now=next(x for x in groups[name] if x['frame']==f);prior=next(x for x in groups[name] if x['frame']==f-1)
        orders[name]='poll_before_light' if max(abs(a-b) for a,b in zip(now['rgb'],prior['rgb']))>1e-5 else 'light_before_poll'
    report={'passed':not failures,'failures':failures,'frames':len(frames),'events':len(events),'scenarios':{k:len(v) for k,v in groups.items()},'observed_update_order':orders,'limits':['Callback adapter is explicit completion-order evidence, not Addressables load latency.','Native temporal state must still be replay-compared against UE; source initial global color is retained across manager reconstruction.']}
    (OUT/'verify.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report));return bool(failures)
if __name__=='__main__':raise SystemExit(main())
