"""Compare two widget-tree dumps (WBP conversion gate G3).

Usage: py -3.12 Scripts/RokUi/TreeDiff.py <before.json> <after.json>

Export examples (after building the editor tools):
  EditorCmd.ps1 -Name tree_before -Commands "Rok.UiWbp.DumpModalTree guild_purview before"
  EditorCmd.ps1 -Name tree_after -Wbp -Commands "Rok.UiWbp.DumpModalTree guild_purview after"

The dumps come from RokUiWbp::DumpTree (Saved/RokUi/Trees/*.json). Widgets are
matched by logical_name (resolved legacy name), falling back to name for old
dumps; the script reports missing/extra widgets and any
change in class, visibility, canvas layout, text, font size or typeface.
URokUiTextBlock is treated as equal to TextBlock (it only adds a font role).
Hierarchy is preserved by default. --logical removes explicitly marked Rok
reusable user-widget boundaries and their canvas roots. Other root widgets
remain in the comparison. This mode does not compose wrapper geometry;
use the full export/default diff to inspect wrapper layout and visibility.
--raw-names compares the original UObject names instead of resolved names.
Exit code 0 = same compared tree, 1 = differences, 2 = invalid input.
"""

import argparse
import json
import sys

EQUIVALENT_CLASSES = {"RokUiTextBlock": "TextBlock"}
COMPARED = ("class", "visibility", "slot", "slot_class", "text", "font_size", "typeface")


def flatten(node, prefix, out, is_root=True, *, logical=False, raw_names=False):
    if not isinstance(node, dict):
        raise ValueError("widget records must be JSON objects")
    if logical and node.get("user_widget") is True and node.get("reusable_widget") is True:
        roots = node.get("children", [])
        # Do not guess from names like PageRoot or from a generated class name.
        if len(roots) != 1 or not isinstance(roots[0], dict) or roots[0].get("widget_tree_root") is not True:
            raise ValueError(f"reusable widget {node['name']!r} has no single exported tree root")
        root = roots[0]
        children = root.get("children", []) if root.get("class") == "CanvasPanel" else [root]
        for child in children:
            flatten(child, prefix, out, False, logical=logical, raw_names=raw_names)
        return out

    name = node["name"] if raw_names else node.get("logical_name", node["name"])
    path = prefix if is_root else f"{prefix}/{name}"
    if path in out:
        raise ValueError(f"duplicate widget path: {path!r}")
    record = {key: node.get(key) for key in COMPARED}
    record["class"] = EQUIVALENT_CLASSES.get(record["class"], record["class"])
    out[path] = record
    for child in node.get("children", []):
        flatten(child, path, out, False, logical=logical, raw_names=raw_names)
    return out


def differences(before, after):
    problems = []
    for path in sorted(before.keys() - after.keys()):
        problems.append(f"missing: {path}")
    for path in sorted(after.keys() - before.keys()):
        problems.append(f"extra:   {path}")
    for path in sorted(before.keys() & after.keys()):
        for key in COMPARED:
            if before[path][key] != after[path][key]:
                problems.append(f"changed: {path} {key}: {before[path][key]!r} -> {after[path][key]!r}")
    return problems


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("--logical", action="store_true")
    parser.add_argument("--raw-names", action="store_true")
    args = parser.parse_args(argv)
    trees = []
    try:
        for filename in (args.before, args.after):
            with open(filename, encoding="utf-8-sig") as handle:
                tree = flatten(json.load(handle), "", {}, logical=args.logical, raw_names=args.raw_names)
            # Roots differ by name (live mode canvas vs generated page root).
            tree.pop("", None)
            trees.append(tree)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"invalid widget-tree dump: {error}", file=sys.stderr)
        return 2
    before, after = trees
    problems = differences(before, after)

    if args.logical:
        print("logical contents only: reusable wrapper geometry and visibility excluded")
    print(f"widgets: {len(before)} before, {len(after)} after, {len(problems)} differences")
    for line in problems[:200]:
        print(line)
    if len(problems) > 200:
        print(f"... {len(problems) - 200} more")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
