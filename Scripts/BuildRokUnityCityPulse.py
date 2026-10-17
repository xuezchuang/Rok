"""Extract operation_2009's two source particle layers without hand-tuned curves."""
import hashlib
import json
import re
from pathlib import Path
from BuildRokUnityWorldMapManifest import parse_unity_yaml_objects

ROOT = Path(__file__).resolve().parents[1]
PREFAB = ROOT / "ref/client-unity/Assets/BundleAssets/Effect/Prefabs/operation_2009.prefab"
TEXTURE = ROOT / "ref/client-unity/Assets/BundleAssets/Effect/Textures/shared_effecttextures/circle006.png"
MATERIAL = ROOT / "ref/client-unity/Assets/BundleAssets/Effect/Materials/shared_effectmaterials/circle006.mat"
OUTPUT = ROOT / "Saved/RokUnityMigration/unity_city_pulse.json"


def section(text, key):
    match = re.search(rf"(?m)^( +){re.escape(key)}:[ \t]*$", text)
    if not match:
        raise ValueError(f"Missing section {key}")
    indent = len(match[1])
    tail = text[match.end():]
    # Unity serializes sequence items at the same indentation as their key.
    end = re.search(rf"(?m)^(?: {{0,{indent - 1}}}\S| {{{indent}}}(?!- )\S)", tail)
    return tail[:end.start()] if end else tail


def number(text, key):
    match = re.search(rf"(?m)^\s*{re.escape(key)}: ([-+\d.eE]+)\s*$", text)
    if not match:
        raise ValueError(f"Missing number {key}")
    return float(match[1])


def vector(text, key):
    match = re.search(rf"{re.escape(key)}: \{{([^}}]+)\}}", text)
    if not match:
        raise ValueError(f"Missing vector {key}")
    return {key.strip(): float(value) for key, value in
            (field.split(":") for field in match[1].split(","))}


def constant(text, key):
    curve = section(text, key)
    if number(curve, "minMaxState") != 0:
        raise ValueError(f"{key} is no longer constant")
    return number(curve, "scalar")


def record(path):
    return {"path": path.relative_to(ROOT).as_posix(),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    objects = parse_unity_yaml_objects(PREFAB)
    layers = []
    for obj in objects:
        if obj["class_id"] != 198:
            continue
        body = obj["body"]
        supported = {"InitialModule", "EmissionModule", "SizeModule", "ColorModule"}
        for module in re.findall(r"(?m)^  (\w+Module):", body):
            if module not in supported and number(section(body, module), "enabled"):
                raise ValueError(f"Unsupported active particle module: {module}")
        game_id = re.search(r"m_GameObject: \{fileID: (\d+)\}", body)[1]
        renderer = next(o["body"] for o in objects if o["class_id"] == 199
                        and f"m_GameObject: {{fileID: {game_id}}}" in o["body"])
        transform = next(o["body"] for o in objects if o["class_id"] == 4
                         and f"m_GameObject: {{fileID: {game_id}}}" in o["body"])
        initial = section(body, "InitialModule")
        emission = section(body, "EmissionModule")
        size_module = section(body, "SizeModule")
        gradient = section(section(section(body, "ColorModule"), "gradient"), "maxGradient")
        if number(gradient, "m_Mode") != 0:
            raise ValueError("Only linear color gradients are supported")
        for i in range(int(number(gradient, "m_NumColorKeys"))):
            color = vector(gradient, f"key{i}")
            if any(color[channel] != 1 for channel in ("r", "g", "b")):
                raise ValueError("Non-white lifetime color needs an RGB evaluator")
        alpha_keys = [{"time": number(gradient, f"atime{i}") / 65535,
                       "value": vector(gradient, f"key{i}")["a"]}
                      for i in range(int(number(gradient, "m_NumAlphaKeys")))]
        size_keys = []
        if number(size_module, "enabled"):
            curve = section(section(size_module, "curve"), "maxCurve")
            for key in re.split(r"- serializedVersion: 3", curve)[1:]:
                size_keys.append({name: number(key, name)
                                  for name in ("time", "value", "inSlope", "outSlope", "weightedMode")})
                if size_keys[-1]["weightedMode"]:
                    raise ValueError("Weighted size curves require evaluator support")
        bursts = []
        if number(emission, "m_BurstCount"):
            if number(emission, "m_BurstCount") != 1:
                raise ValueError("Expected exactly one burst")
            burst_text = section(emission, "m_Bursts")
            bursts.append({"time": number(burst_text, "time"),
                           "count": constant(burst_text, "countCurve"),
                           "cycles": number(burst_text, "cycleCount"),
                           "probability": number(burst_text, "probability")})
        layers.append({"file_id": obj["file_id"], "duration": number(body, "lengthInSec"),
                       "looping": bool(number(body, "looping")), "prewarm": bool(number(body, "prewarm")),
                       "lifetime": constant(initial, "startLifetime"), "size": constant(initial, "startSize"),
                       "speed": constant(initial, "startSpeed"),
                       "color": vector(section(initial, "startColor"), "maxColor"),
                       "rate": constant(emission, "rateOverTime"), "bursts": bursts,
                       "alpha_keys": alpha_keys, "size_keys": size_keys,
                       "local_rotation": vector(transform, "m_LocalRotation"),
                       "local_position": vector(transform, "m_LocalPosition"),
                       "local_scale": vector(transform, "m_LocalScale"),
                       "render_mode": number(renderer, "m_RenderMode"),
                       "render_alignment": number(renderer, "m_RenderAlignment")})
    if len(layers) != 2:
        raise ValueError(f"Expected two source layers, got {len(layers)}")
    result = {"schema": "rok.unity.city_pulse.v1", "prefab": record(PREFAB),
              "material": record(MATERIAL), "texture": record(TEXTURE), "layers": layers,
              "source_owner": "GlobalViewLevelMediator.CityDepend / SetCityObj_Effect (own city only)"}
    result["shader_identity"] = {
        "name": "Mobile/Particles/Additive",
        "evidence": "Unity 2019.4.28f1c1 AssetDatabase loaded circle006.mat; Saved/UnityBuiltinShaderProbeMaterial.log",
        "builtin_file_id": 10720,
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
