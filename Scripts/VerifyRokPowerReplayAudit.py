"""Read-only checks of real-tick UI replay trace; never a pixel-match assertion."""
import argparse
import hashlib
import json
from pathlib import Path

def evolution_errors(a, b):
    """Observe consecutive snapshots without notification; no emitter advancement."""
    errors = []
    tolerance = 2e-6  # Float clock subtraction/accumulation, not an early death allowance.
    if a['notifications'] != b['notifications']:
        return ['unaccounted notification in state interval']
    for key in ('frame', 'elapsed', 'wall_elapsed', 'world_seconds'):
        if b[key] < a[key]:
            errors.append(f'{key} moved backwards')
    delta = b['mesh_time'] - a['mesh_time']
    if delta < -tolerance:
        errors.append('mesh clock reset without notification')
    delta = max(0, delta)
    if not a['playing']:
        if b['playing'] or b['particles'] or delta > tolerance:
            errors.append('idle effect mutated without notification')
        return errors
    if not b['playing']:
        if not delta > a['remaining'] or b['remaining'] != 0 or b['particles']:
            errors.append('root did not cross strict lifetime boundary')
        return errors
    if abs(b['remaining'] - max(0, a['remaining'] - delta)) > tolerance:
        errors.append('root lifetime changed without notification')
    expected = []
    for p in a['particles']:
        if p['pending']:
            expected.append({'age': 0, 'pending': delta == 0})
        elif p['age'] + delta < 1:
            expected.append({'age': p['age'] + delta, 'pending': False})
    # No lost old cohort, inserted cohort or reordered cohort is permitted here.
    if len(expected) != len(b['particles']):
        errors.append('cohort disappeared before age >= 1 or unexpected cohort appeared')
    for p, q in zip(expected, b['particles']):
        if p['pending'] != q['pending'] or abs(p['age'] - q['age']) > tolerance:
            errors.append('cohort age/birth continuity changed')
    return errors

def verify(path, native_path=None):
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    failures = []
    def check(ok, message):
        if not ok:
            failures.append(message)
    trace = data.get('trace', [])
    before = [x for x in trace if x['kind'] == 'before_notification']
    after = [x for x in trace if x['kind'] == 'after_notification']
    ticks = [x for x in trace if x['kind'] == 'tick']
    check(data.get('runtime_complete') is True, 'runtime incomplete')
    check(len(before) == len(after) == 3, 'expected exactly three UI notifications')
    for i, (a, b) in enumerate(zip(trace, trace[1:])):
        if a['kind'] == 'before_notification' and b['kind'] == 'after_notification':
            check(b['notifications'] == a['notifications'] + 1, f'trace {i} notification count')
            continue
        for error in evolution_errors(a, b):
            check(False, f'trace {i}: {error}')
    for i, (a, b) in enumerate(zip(before, after)):
        check(a['elapsed'] >= [0, .203, .707][i], f'event {i} before threshold')
        check(a['frame'] == b['frame'], f'event {i} not synchronous observation')
        check(abs(b['remaining'] - a['remaining'] - 1) < 1e-5, f'event {i} lifetime extension')
        check(b['mesh_time'] == 0 and b['playing'], f'event {i} mesh restart')
        check(b['particles'][:-1] == a['particles'], f'event {i} cleared/changed old particles')
        check(b['particles'][-1:] == [{'age': 0, 'pending': True}], f'event {i} newborn must be pending')
        born = next((t for t in ticks if t['frame'] > b['frame'] and t['notifications'] == i + 1
                     and len(t['particles']) == len(b['particles']) and not t['particles'][-1]['pending']), None)
        check(born is not None and born['particles'][-1]['age'] == 0, f'event {i} real next-tick birth age zero not observed')
    check(any(len(t['particles']) >= 3 for t in ticks), 'three overlapping particle cohorts not observed')
    check(any(t['notifications'] == 3 and t['playing'] and not t['particles'] for t in ticks), 'root outliving particles not observed')
    check(bool(ticks) and not ticks[-1]['playing'] and ticks[-1]['remaining'] == 0, 'root did not finish')
    endings = [(a, b) for a, b in zip(ticks, ticks[1:])
               if a['notifications'] == b['notifications'] == 3 and a['playing'] and not b['playing']]
    check(len(endings) == 1, 'expected one final root lifetime crossing')
    for a, b in endings:
        check(b['mesh_time'] - a['mesh_time'] > a['remaining'], 'root destroyed before strict lifetime crossing')
    check(all(p['pending'] or 0 <= p['age'] < 1 for t in ticks for p in t['particles']), 'expired particle retained')
    captures = data.get('captures', [])
    check(len(captures) == 1, 'expected one independent screenshot')
    inputs = []
    for c in captures:
        processed = c.get('processed_state')
        check(c['elapsed'] >= c['request_threshold'], 'screenshot requested before threshold')
        if c['request_threshold'] < 3:
            check(c['playing'], 'requested live-effect capture arrived after root expiry')
        check(c in trace, 'screenshot request absent from chronological trace')
        if processed:
            check(processed in trace, 'screenshot processed state absent from chronological trace')
            check(processed['notifications'] == c['notifications'], 'notification crossed screenshot request/processing')
            check(processed['playing'] == c['playing'], 'root lifetime boundary crossed screenshot request/processing')
            for key in ('frame', 'elapsed', 'wall_elapsed', 'world_seconds'):
                check(processed[key] >= c[key], f'screenshot {key} moved backwards')
            # Validate all observed intermediate steps, rather than treating multiple real ticks as one birth tick.
            if c in trace and processed in trace:
                first, last = trace.index(c), trace.index(processed)
                check(last > first, 'screenshot processed before request')
                for a, b in zip(trace[first:last], trace[first + 1:last + 1]):
                    for error in evolution_errors(a, b):
                        check(False, f'screenshot interval: {error}')
        image = Path(c['path'])
        check(image.is_file() and image.stat().st_size > 0 and 'processed_state' in c, 'screenshot missing/unprocessed')
        if image.is_file():
            from PIL import Image
            with Image.open(image) as im:
                check(im.width > 0 and im.height > 0, 'invalid screenshot dimensions')
                inputs.append({'path': str(image), 'size': im.size, 'sha256': hashlib.sha256(image.read_bytes()).hexdigest()})
    native_comparison = []
    if native_path:
        native = json.loads(native_path.read_text(encoding='utf-8-sig'))
        check(not native.get('failures') and bool(native.get('captures')), 'native probe incomplete')
        for c in captures:
            if not native.get('captures'):
                continue
            n = min(native['captures'], key=lambda n: abs(n['state']['elapsed'] - c['elapsed']))
            run_frames = [f for f in native['frames'] if f['run'] == n['state']['run']]
            native_comparison.append({'native_sha256': hashlib.sha256(native_path.read_bytes()).hexdigest(),
                'ue_request_elapsed': c['elapsed'], 'ue_processed_elapsed': c.get('processed_state', {}).get('elapsed'),
                'native_capture_elapsed': n['state']['elapsed'], 'native_requested_target': n['target'],
                'native_replay_operations': [f for f in run_frames if f['operation'] != 'normal LateUpdate'],
                'ue_particles_at_request': c['particles'], 'native_remaining_lifetimes': n['state']['remaining'],
                'native_images': n['images'], 'comparison_kind': 'Nearest recorded actual time, descriptive only; different background/projection and independently scheduled events prohibit pixel-equality interpretation.'})
    return {'passed': not failures, 'failures': failures, 'manifest_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'actual_notifications': before, 'images': inputs,
            'native_comparison': native_comparison,
            'limits': 'UI injection only. No Unity pixel parity, physical input, server/business updates or exact-zero runtime boundary claim. Strict equality covered by PowerGain lifecycle unit test; runtime naturally brackets expiry. Frame schedule and 120 Hz source tape remain distinct.'}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--native', type=Path, default=Path(__file__).resolve().parents[1] / 'Saved/RokUnityMigration/PowerGain/ReplayVisual/native_replay_visual.json')
    args = parser.parse_args()
    result = verify(args.manifest, args.native)
    output = args.manifest.with_name('verify.json')
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))
    raise SystemExit(0 if result['passed'] else 1)
