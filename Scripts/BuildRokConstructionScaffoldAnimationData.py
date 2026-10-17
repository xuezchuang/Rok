"""Extract the four verified Build_shelf_5 authored clips without UE/Unity runtime.

The source curves are kept at original float precision. Runtime interpolation is
specified as unweighted cubic Hermite, not replaced with linear key segments.
"""
from __future__ import annotations

import json
import math
import re
from pathlib import Path

import PrepareRokConstructionScaffoldAssets as source

OUT = source.DATA / "animation_data.json"


def evaluate(keys: list[dict], t: float) -> float:
    if t <= keys[0]["time"]:
        return keys[0]["value"]
    if t >= keys[-1]["time"]:
        return keys[-1]["value"]
    for a, b in zip(keys, keys[1:]):
        if a["time"] <= t <= b["time"]:
            dt = b["time"] - a["time"]
            if dt <= 0:
                raise RuntimeError("Duplicate/reversed source animation key time")
            u = (t-a["time"])/dt
            h00 = 2*u**3-3*u**2+1
            h10 = u**3-2*u**2+u
            h01 = -2*u**3+3*u**2
            h11 = u**3-u**2
            return (h00*a["value"] + h10*dt*a["out_tangent"] +
                    h01*b["value"] + h11*dt*b["in_tangent"])
    raise RuntimeError("Source animation sample interval missing")


def build():
    native, contract = source.identity()
    components = {x["clip"]: x for x in native["source_animation_components"]}
    clips = []
    for clip in native["clips"]:
        asset = clip["asset"]
        path, meta = source.checked_source(asset, 7400000)
        text = path.read_text(encoding="utf-8-sig")
        if "--- !u!74 &7400000" not in text or "m_FloatCurves:" not in text:
            raise RuntimeError(f"Not an original Unity AnimationClip YAML: {path}")
        if clip["name"] not in components or len(clip["curves"]) not in (3, 6):
            raise RuntimeError(f"Unexpected scaffold animation binding set: {path}")
        expected = {"m_LocalPosition.x", "m_LocalPosition.y", "m_LocalPosition.z"}
        if len(clip["curves"]) == 6:
            expected |= {"m_LocalScale.x", "m_LocalScale.y", "m_LocalScale.z"}
        observed = {x["property"] for x in clip["curves"]}
        if observed != expected or any(x["path"] != "sprite1" or
                                   x["type"] != "UnityEngine.Transform"
                                   for x in clip["curves"]):
            raise RuntimeError(f"Source clip path/property differs: {path}")
        curves = []
        for curve in clip["curves"]:
            keys = curve["keys"]
            if (len(keys) != 4 or any(k["weighted_mode"] != "None" for k in keys)
                    or any(not all(math.isfinite(float(k[p])) for p in
                            ("time", "value", "in_tangent", "out_tangent", "in_weight", "out_weight"))
                           for k in keys)
                    or any(keys[i]["time"] >= keys[i+1]["time"] for i in range(3))):
                raise RuntimeError(f"Unsupported/invalid source curve: {clip['name']}/{curve['property']}")
            samples = []
            for a,b in zip(keys,keys[1:]):
                t=(a["time"]+b["time"])/2
                samples.append({"time":t,"value":evaluate(keys,t)})
            curves.append({**curve, "interpolation": "unweighted_cubic_Hermite",
                           "midpoint_samples": samples})
        clips.append({**clip,"curves":curves,"source_sha256":source.sha(path),
                      "source_meta_sha256":source.sha(meta),
                      "source_guid":asset["guid"],"source_file_id":asset["file_id"],
                      "component":components[clip["name"]]})
    result = {"schema":"rok.construction_scaffold.animation_data.v1",
              "native_sha256":source.NATIVE_SHA,"contract_sha256":source.CONTRACT_SHA,
              "scope":"Original four child AnimationClip float curves; mediator 2s timer is separate",
              "root_animation_source_enabled":False,
              "clips":clips,"clip_count":len(clips),
              "binding_count":sum(len(c["curves"]) for c in clips),
              "key_count":sum(len(x["keys"]) for c in clips for x in c["curves"]),
              "interpolation":"Unity unweighted cubic Hermite with original tangents; pre/post wrap preserved per binding",
              "runtime_limit":"No UE animation runner, particle shader or main-view Gamma acceptance"}
    if (result["clip_count"],result["binding_count"],result["key_count"]) != (4,15,60):
        raise RuntimeError("Scaffold animation count contract differs")
    source.write_once(OUT,(json.dumps(result,ensure_ascii=False,indent=2)+"\n").encode())
    return OUT


if __name__ == "__main__":
    path=build()
    print(json.dumps({"animation_data":str(path),"sha256":source.sha(path),"clips":4,"bindings":15,"keys":60}))
