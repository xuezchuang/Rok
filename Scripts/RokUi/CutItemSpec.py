"""Cuts one item (cell, tile, row) out of mode specs as its own widget spec.

Some C++ modes place each item's widgets flat in a shared canvas, at the item's
position plus an offset (e.g. PlayerHeadCell_1000, PlayerHeadChoiceImage_1000,
PlayerHeadChoiceButton_0 in PlayerHeadGrid). An item WBP needs them grouped,
relative to the item's origin. This writes the named widgets (with their
children) as the top level of a new spec, each canvas position minus its
group's origin. Rok.UiWbp.GeneratePage then builds the item WBP from it (with
-Prefix/-Suffix/-Index so the old flat names stay findable).

An item can combine several states: each --source/--origin group takes widgets
from one spec (e.g. the locked shade from a locked flag, renamed NAME=NEW to the
first item's id). Groups are written in argument order and, inside a group, in
source order, which is the draw order for equal z-orders.

Usage:
  py -3.12 Scripts/RokUi/CutItemSpec.py <out spec>
      --source <spec> --origin X,Y NAME[=NEW] [NAME[=NEW] ...]
      [--source <spec> --origin X,Y NAME[=NEW] ...]
Specs are names under Saved/RokUi/Specs (no .json) or paths.
"""
import copy
import json
import pathlib
import re
import sys

SPEC_DIR = pathlib.Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'
CANVAS_SLOT = '/Script/UMG.CanvasPanelSlot'


def spec_path(name):
    path = pathlib.Path(name)
    return path if path.suffix == '.json' else SPEC_DIR / f'{name}.json'


def parse_fields(text):
    """'Left=1.0,Top=2.0' -> {'Left': 1.0, 'Top': 2.0} (flat numeric fields only)."""
    return {key: float(value) for key, value in re.findall(r'(\w+)=(-?[\d.]+)', text)}


def shift_slot(node, origin):
    slot = node.get('slot')
    if not slot or slot.get('class') != CANVAS_SLOT:
        raise SystemExit(f"{node['name']}: not in a canvas (slot {slot and slot.get('class')})")
    props = slot.setdefault('props', {})
    layout = props.get('LayoutData', '()')
    anchors = re.search(r'Anchors=\(Minimum=\(([^)]*)\),Maximum=\(([^)]*)\)\)', layout)
    if anchors and (parse_fields(anchors.group(1)) != parse_fields(anchors.group(2))
                    or any(parse_fields(anchors.group(1)).values())):
        raise SystemExit(f"{node['name']}: only top-left anchors are supported ({layout})")
    offsets = re.search(r'Offsets=\(([^)]*)\)', layout)
    values = parse_fields(offsets.group(1)) if offsets else {}
    values['Left'] = values.get('Left', 0.0) - origin[0]
    values['Top'] = values.get('Top', 0.0) - origin[1]
    text = ','.join(f'{key}={values.get(key, 0.0):.6f}' for key in ('Left', 'Top', 'Right', 'Bottom'))
    new_offsets = f'Offsets=({text})'
    if offsets:
        layout = layout[:offsets.start()] + new_offsets + layout[offsets.end():]
    else:
        layout = '(' + new_offsets + ('' if layout == '()' else ',' + layout[1:-1]) + ')'
    props['LayoutData'] = layout


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get('children', []))


def parse_groups(tokens):
    groups = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if token == '--source':
            groups.append({'source': tokens[index + 1], 'origin': None, 'members': {}})
            index += 2
        elif token == '--origin':
            if not groups:
                raise SystemExit('--origin before --source')
            groups[-1]['origin'] = tuple(float(value) for value in tokens[index + 1].split(','))
            index += 2
        else:
            if not groups:
                raise SystemExit(f'{token}: widget before --source')
            name, _, new = token.partition('=')
            groups[-1]['members'][name] = new or name
            index += 1
    for group in groups:
        if group['origin'] is None or not group['members']:
            raise SystemExit(f"--source {group['source']}: needs --origin and widgets")
    return groups


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ('-h', '--help'):
        print(__doc__)
        return 2
    out_name = sys.argv[1]
    picked = []
    version = None
    labels = []
    for group in parse_groups(sys.argv[2:]):
        source = json.loads(spec_path(group['source']).read_text(encoding='utf-8'))
        version = source['version']
        labels.append(source['label'])
        wanted = dict(group['members'])
        for node in walk(source['nodes']):
            if node['name'] in wanted:
                item = copy.deepcopy(node)
                item['name'] = wanted.pop(node['name'])
                shift_slot(item, group['origin'])
                picked.append(item)
        if wanted:
            raise SystemExit(f"not in {group['source']}: {', '.join(wanted)}")
    names = [node['name'] for node in picked]
    duplicates = {name for name in names if names.count(name) > 1}
    if duplicates:
        raise SystemExit(f"duplicate widget names: {', '.join(sorted(duplicates))}")
    out = {'version': version, 'label': ' + '.join(labels) + ' item', 'root': 'ItemRoot',
           'nodes': picked, 'notes': []}
    target = spec_path(out_name)
    target.write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{target.name}: {len(picked)} widgets, {sum(1 for _ in walk(picked))} with children')
    return 0


if __name__ == '__main__':
    sys.exit(main())
