"""Source-only stone composition checks, including the unresolved tie-order gate."""
import hashlib
import json
from pathlib import Path

from BuildRokUnityStoneDeposit import ROOT, OUT


def corners(position, size):
    # Exact RotateSquar.UpdateRect constants and corner ordering.
    x, z = position
    w, h = size[0] / 2, size[1] / 2
    k = .70721
    world = [(x+h*k-w*k, z+h*k+w*k), (x+h*k+w*k, z+h*k-w*k),
             (x-h*k+w*k, z-h*k-w*k), (x-h*k-w*k, z-h*k+w*k)]
    return [(a*k+b*k, -a*k+b*k) for a, b in world]


def contains(a, b):
    return all(a[2][0] <= x <= a[1][0] and a[2][1] <= y <= a[0][1] for x, y in b)


def overlaps(a, b):
    # Source checks corners in either direction, not a generic SAT test.
    return any(a[2][0] <= x <= a[1][0] and a[2][1] <= y <= a[0][1] for x, y in b) or any(
        b[2][0] <= x <= b[1][0] and b[2][1] <= y <= b[0][1] for x, y in a)


def update(items, scale, bounds, previous=None, previous_scale=-1):
    active = set(i["source_transform_id"] for i in items) if previous is None else set(previous)
    candidates = [i for i in items if scale <= previous_scale or i["source_transform_id"] in active]
    rect = {i["source_transform_id"]: corners((i["position"][0], i["position"][2]),
                              [s*scale for s in i["size"]]) for i in items}
    outer = corners((0, 0), bounds)
    candidates = [i for i in candidates if i["priority"] >= 100 or contains(outer, rect[i["source_transform_id"]])]
    survivors = []
    for i in candidates:
        if not any(overlaps(rect[p["source_transform_id"]], rect[i["source_transform_id"]]) for p in survivors):
            survivors.append(i)
    return [i["source_transform_id"] for i in survivors]


def main():
    data = json.loads(OUT.read_text(encoding="utf-8"))
    assert len(data["items"]) == 18
    assert len({i["source_transform_id"] for i in data["items"]}) == 18
    assert {i["name"] for i in data["items"]} == {"stone_1"}
    assert sorted(i["priority"] for i in data["items"]) == [60]*17+[103]
    for record in [data["source"], data["native_order_evidence"]] + data["algorithm_sources"] + [s[k] for i in data["items"] for s in i["sprites"] for k in ("texture", "mask", "material")]:
        assert hashlib.sha256((ROOT / record["path"]).read_bytes()).hexdigest() == record["sha256"]
    for record in data["algorithm_sources"]:
        copy = ROOT/"Saved/UnityStoneDepositProbe/Assets/Source"/Path(record["path"]).name
        assert hashlib.sha256(copy.read_bytes()).hexdigest() == record["sha256"]
    forward = data["items"]
    reverse = [forward[0]] + list(reversed(forward[1:]))
    scale = 1.129
    a = update(forward, scale, data["bounds_size"])
    b = update(reverse, scale, data["bounds_size"])
    order_changes_output = set(a) != set(b)
    native = json.loads((ROOT/data["native_order_evidence"]["path"]).read_text(encoding="utf-8"))
    samples_checked = 0
    for run in native["runs"]:
        previous, previous_scale = None, -1
        for sample in run["samples"]:
            got = update(forward, sample["scale"], data["bounds_size"], previous, previous_scale)
            assert got == sample["active"], (sample, got)
            previous, previous_scale = got, sample["scale"]
            samples_checked += 1
    result = {"extraction_checks_passed": True, "runtime_integration_allowed": True,
              "native_samples_matched": samples_checked,
              "order_limit": "Captured native registration order only, not a cross-context guarantee.",
              "order_comparison_changes_output": order_changes_output, "order_comparison": {"scale": scale, "first_order": [i["source_transform_id"] for i in forward],
                                 "first_survivors": a, "second_order": [i["source_transform_id"] for i in reverse],
                                 "second_survivors": b},
              "scope": "Source-extracted data and exact RotateSquar/GridCollideMgr predicate reproduction only; no UE runtime change or visual acceptance."}
    path = OUT.with_name("unity_stone_deposit_verify.json")
    path.write_text(json.dumps(result, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
