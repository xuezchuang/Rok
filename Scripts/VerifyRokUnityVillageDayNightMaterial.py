"""Independent source/math gate; never imports unreal or claims rendered parity."""
import ast
import hashlib
import json
import math
from pathlib import Path
from PIL import Image
from rok_data_paths import DATA_ROOT, resolve

ROOT = Path(__file__).resolve().parents[1]


def main():
    failures = []

    def check(value, label):
        if not value:
            failures.append(label)

    manifest_path = DATA_ROOT / "unity_village_day_night.json"
    data = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    for source in data["sources"] + [row["source"] for row in data["materials"].values()]:
        check(hashlib.sha256(resolve(source["path"]).read_bytes()).hexdigest() == source["sha256"], "source hash: " + source["path"])
    shader = (ROOT / "ref/client-unity/Assets/BundleAssets/Map/Shader/cm_city_mask_night.shader").read_text(encoding="utf-8-sig")
    compact = "".join(shader.split())
    for token in ("OUT.lightColor=_Color_3.rgb*l;", "abs(sin((_Time.y*0.5)))", "c_2=(c_2*_SpriteColor);",
                  "c_2.rgb=c_2.rgb+IN.lightColor*mask.b;", "c_2.w=(col.a*clamp(mask_1,0.0,1.0))*_CityTransparency;"):
        check(token in compact, "night shader drift: " + token)
    generator_path = ROOT / "Scripts/CreateRokUnityVillagePresentationMaterial.py"
    generator = generator_path.read_text(encoding="utf-8-sig")
    tree = ast.parse(generator)
    code = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                and any(isinstance(t, ast.Name) and t.id == "DAY_SHADER_CODE" for t in n.targets))
    check("gammaColor += NightLight.rgb * MaskSample.b;" in code, "night blue channel addition exists")
    check(code.index("gammaColor *= Lighting.rgb") < code.index("gammaColor += NightLight.rgb") < code.index("float3 linearColor"), "night addition after sprite lighting before Gamma conversion")
    check('night_light = vector("NightLight", (0, 0, 0, 0))' in generator, "day zero-light default")
    check('transparency, night_light)' in generator and '"Transparency", "NightLight")' in generator, "custom input actually connected")
    check(data["materials"]["night"]["color1"] == [1, 1, 1, 1] and data["materials"]["night"]["color2"] == [1, 1, 1, 1], "night uses source white team colors")
    for mask in data["masks"]:
        path = resolve(mask["mask"]["path"])
        check(hashlib.sha256(path.read_bytes()).hexdigest() == mask["mask"]["sha256"], "mask hash " + mask["transform_id"])
        with Image.open(path) as image:
            check(image.convert("RGB").getchannel("B").getextrema()[1] == 0, "no manufactured village lights " + mask["transform_id"])
    check(len(data["masks"]) == 13, "all thirteen source masks")
    samples = []
    for row in data["samples"]:
        source = data["presets"][row["from_preset"]]
        target = data["presets"][row["to_preset"]]
        t = min(1, max(0, row["elapsed"] / row["duration"]))
        intensity = source["intensity"] * (1 - t) + target["intensity"] * t
        rgb = [(source["ambient"][i] * (1 - t) + target["ambient"][i] * t)
               + (source["directional"][i] * (1 - t) + target["directional"][i] * t) * intensity * .75 for i in range(3)]
        scenario = next(s for s in data["scenarios"] if s["name"] == row["scenario"])
        resolved = scenario["requested_night"] if row["elapsed"] >= row["fixed_delay"] else scenario["initial_night"]
        check(all(math.isclose(a, b, abs_tol=2e-6) for a, b in zip(rgb, row["sprite_color"])), "SpriteColor " + row["name"])
        check(resolved == row["resolved_night"], "independent post-deadline material " + row["name"])
        samples.append({"name": row["name"], "sprite_color": rgb, "resolved_night": resolved})
    check(len(samples) == 24, "24 snapshot cases")
    report = {"passed": not failures, "failures": failures, "samples": samples,
              "manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
              "generator_sha256": hashlib.sha256(generator_path.read_bytes()).hexdigest(),
              "open_gates": ["UE compile and actual native-versus-UE day/night screenshots", "Gamma versus linear translucent edge blending",
                             "server phase authority; random coroutine, interruption, inactive and OnEnable scheduling"]}
    output = ROOT / "Saved/RokUnityMigration/village_day_night_material_verify.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "failures": failures, "samples": len(samples), "report": str(output)}))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
