"""Source/math checks only: does not verify UE shader compilation or pixels."""
import ast
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def day_gamma(main, mask, renderer, color1, color2, lighting, transparency):
    col = [a * b for a, b in zip(main, renderer)]
    red2 = mask[0] * 2
    team1 = min(1, max(0, red2 - 1))
    rgb = [col[i] * (1 - team1 - mask[1] + 2 * color1[i] * team1
                    + 2 * color2[i] * mask[1]) * lighting[i] for i in range(3)]
    return rgb + [col[3] * min(1, max(0, red2)) * transparency]


def main():
    failures = []
    def check(ok, label):
        if not ok:
            failures.append(label)
    shader_path = ROOT / "ref/client-unity/Assets/BundleAssets/Map/Building/map_building_with_mask-5.shader"
    generator = ROOT / "Scripts/CreateRokUnityStoneDepositMaterial.py"
    shader = "".join(shader_path.read_text(encoding="utf-8-sig").split())
    for token in (
        "col=tex2D(_MainTex,IN.texcoord)*IN.color;",
        "mask=tex2D(_Mask,IN.texcoord);",
        "tmpvar_5=clamp((mask_1-1.0),0.0,1.0);",
        "c_2.xyz=(((col.rgb*((1.0-tmpvar_5)-mask.g))+((2.0*col.rgb)*(_Color_1.rgb*tmpvar_5)))+((2.0*col.rgb)*(_Color_2.rgb*mask.g)));",
        "c_2=(c_2*_SpriteColor);",
        "c_2.w=(col.a*clamp(mask_1,0.0,1.0))*_CityTransparency;",
        "BlendSrcAlphaOneMinusSrcAlpha,SrcAlphaOneMinusSrcAlpha",
    ):
        check(token in shader, "Source shader drift: " + token)
    syntax = ast.parse(generator.read_text(encoding="utf-8"))
    code = next(ast.literal_eval(node.value) for node in syntax.body
                if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "DAY_SHADER_CODE" for t in node.targets))
    for token in ("MainSample * Renderer", "saturate(red2 - 1.0)",
                  "1.0 - team1 - MaskSample.g", "2.0 * Team1.rgb * team1",
                  "2.0 * Team2.rgb * MaskSample.g", "gammaColor *= Lighting.rgb",
                  "col.a * saturate(red2) * Transparency", "step(0.04045, gammaColor)"):
        check(token in code, "Generated shader drift: " + token)
    one = [1, 1, 1, 1]
    cases = [
        ("zero mask coverage", [0, 0, 1, 1], [0.3, 0.4, 0.5, 0]),
        ("half red is coverage only", [0.5, 0, 0, 1], [0.3, 0.4, 0.5, 0.8]),
        ("red team recolor", [1, 0, 0, 1], [0.06, 0.16, 0.3, 0.8]),
        ("green team recolor", [0.5, 1, 0, 1], [0.42, 0.64, 0.9, 0.8]),
        ("overlapping teams retain signed base term", [1, 1, 0, 1], [0.18, 0.4, 0.7, 0.8]),
    ]
    for label, mask, expected in cases:
        got = day_gamma([0.3, 0.4, 0.5, 0.8], mask, one, [0.1, 0.2, 0.3], [0.7, 0.8, 0.9], one, 1)
        check(all(math.isclose(a, b, abs_tol=1e-7) for a, b in zip(got, expected)), label)
    alpha = day_gamma(one, one, [1, 1, 1, 0.5], one, one, [1, 1, 1, 0], 0.25)[3]
    check(alpha == 0.125, "Opacity uses renderer alpha and transparency, not lighting alpha")
    report = {
        "failures": failures,
        "source_sha256": hashlib.sha256(shader_path.read_bytes()).hexdigest(),
        "generator_sha256": hashlib.sha256(generator.read_bytes()).hexdigest(),
        "analytical_cases": [c[0] for c in cases],
        "open_acceptance": ["UE material compile and real stone runtime rendering", "Unity Gamma blend versus UE linear translucent blend", "Continuous night runtime and asynchronous material timing remain separate gates"],
    }
    output = ROOT / "Saved/RokUnityMigration/stone_deposit_material_verify.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
