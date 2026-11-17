"""Compare two widget-tree dumps (WBP conversion gate G3).

Usage: py -3.12 Scripts/RokUi/TreeDiff.py <before.json> <after.json>

The dumps come from RokUiWbp::DumpTree (Saved/RokUi/Trees/*.json). Widgets are
matched by their name path; the script reports missing/extra widgets and any
change in class, visibility, canvas layout, text, font size or typeface.
URokUiTextBlock is treated as equal to TextBlock (it only adds a font role).
Exit code 0 = same tree, 1 = differences.
"""

import json
import sys

EQUIVALENT_CLASSES = {"RokUiTextBlock": "TextBlock"}
COMPARED = ("class", "visibility", "slot", "slot_class", "text", "font_size", "typeface")


def flatten(node, prefix, out, is_root=True):
    path = prefix if is_root else f"{prefix}/{node['name']}"
    record = {key: node.get(key) for key in COMPARED}
    record["class"] = EQUIVALENT_CLASSES.get(record["class"], record["class"])
    out[path] = record
    for child in node.get("children", []):
        flatten(child, path, out, False)
    return out


def main() -> int:
    with open(sys.argv[1], encoding="utf-8") as handle:
        before = flatten(json.load(handle), "", {})
    with open(sys.argv[2], encoding="utf-8") as handle:
        after = flatten(json.load(handle), "", {})
    # Roots differ by name (live mode canvas vs generated page root); compare children only.
    before.pop("", None)
    after.pop("", None)

    problems = []
    for path in sorted(before.keys() - after.keys()):
        problems.append(f"missing: {path}")
    for path in sorted(after.keys() - before.keys()):
        problems.append(f"extra:   {path}")
    for path in sorted(before.keys() & after.keys()):
        for key in COMPARED:
            if before[path][key] != after[path][key]:
                problems.append(f"changed: {path} {key}: {before[path][key]!r} -> {after[path][key]!r}")

    print(f"widgets: {len(before)} before, {len(after)} after, {len(problems)} differences")
    for line in problems[:200]:
        print(line)
    if len(problems) > 200:
        print(f"... {len(problems) - 200} more")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
