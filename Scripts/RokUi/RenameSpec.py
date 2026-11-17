"""Copies a widget spec with some widgets renamed.

Rok.UiWbp.GeneratePage merges a mode's state specs by widget name, keeping the
first layout of each name. When one widget has a different layout per state
(a tab that is larger while selected), rename it per state first, e.g.
PlayerHeadTabBackground_0=PlayerHeadTabBackgroundSelected_0 in the state where
tab 0 is selected and ...=PlayerHeadTabBackgroundDark_0 where it is not; the
page then has both looks and C++ shows the one for the state.

Usage:
  py -3.12 Scripts/RokUi/RenameSpec.py <source spec> <out spec> OLD=NEW [OLD=NEW ...]
Specs are names under Saved/RokUi/Specs (no .json) or paths.
"""
import json
import pathlib
import sys

SPEC_DIR = pathlib.Path(__file__).resolve().parents[2] / 'Saved' / 'RokUi' / 'Specs'


def spec_path(name):
    path = pathlib.Path(name)
    return path if path.suffix == '.json' else SPEC_DIR / f'{name}.json'


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get('children', []))


def main():
    if len(sys.argv) < 4 or sys.argv[1] in ('-h', '--help'):
        print(__doc__)
        return 2
    source = json.loads(spec_path(sys.argv[1]).read_text(encoding='utf-8'))
    renames = dict(pair.split('=', 1) for pair in sys.argv[3:])
    for node in walk(source['nodes']):
        if node['name'] in renames:
            node['name'] = renames.pop(node['name'])
    if renames:
        raise SystemExit(f"not in {sys.argv[1]}: {', '.join(renames)}")
    source['label'] = f"{source['label']} (renamed)"
    spec_path(sys.argv[2]).write_text(json.dumps(source, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{sys.argv[2]}: {len(sys.argv) - 3} renamed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
