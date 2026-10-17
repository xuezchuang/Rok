"""Source-only food composition checks, including the unresolved tie-order gate."""
import hashlib
import json
from pathlib import Path

from BuildRokUnityFoodDeposit import ROOT, OUT


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
    active = set(i["name"] for i in items) if previous is None else set(previous)
    candidates = [i for i in items if scale <= previous_scale or i["name"] in active]
    rect = {i["name"]: corners((i["position"][0], i["position"][2]),
                              [s*scale for s in i["size"]]) for i in items}
    outer = corners((0, 0), bounds)
    candidates = [i for i in candidates if i["priority"] >= 100 or contains(outer, rect[i["name"]])]
    survivors = []
    for i in candidates:
        if not any(overlaps(rect[p["name"]], rect[i["name"]]) for p in survivors):
            survivors.append(i)
    return [i["name"] for i in survivors]


def main():
    data = json.loads(OUT.read_text(encoding="utf-8"))
    assert len(data["items"]) == 9
    assert sorted(i["priority"] for i in data["items"]) == [60]*8+[103]
    for record in [data["source"], data["native_order_evidence"]] + data["algorithm_sources"] + [s[k] for i in data["items"] for s in i["sprites"] for k in ("texture", "mask", "material")]:
        assert hashlib.sha256((ROOT / record["path"]).read_bytes()).hexdigest() == record["sha256"]
    for record in data["algorithm_sources"]:
        copy = ROOT/"Saved/UnityFoodDepositProbe/Assets/Source"/Path(record["path"]).name
        assert hashlib.sha256(copy.read_bytes()).hexdigest() == record["sha256"]
    forward = data["items"]
    reverse = [forward[0]] + list(reversed(forward[1:]))
    scale = 1.129
    a = update(forward, scale, data["bounds_size"])
    b = update(reverse, scale, data["bounds_size"])
    assert set(a) != set(b), "Recheck source/order blocker: counterexample no longer reproduces"
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
              "counterexample": {"scale": scale, "first_order": [i["name"] for i in forward],
                                 "first_survivors": a, "second_order": [i["name"] for i in reverse],
                                 "second_survivors": b},
              "scope": "Source-extracted data and exact RotateSquar/GridCollideMgr predicate reproduction only; no UE runtime change or visual acceptance."}
    path = OUT.with_name("unity_food_deposit_verify.json")
    path.write_text(json.dumps(result, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
