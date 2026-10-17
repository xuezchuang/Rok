"""Prepare an isolated Unity2019 Hall shader-constant and pixel-path witness.

V1/V2 projects, runs, source references and the UE Hall inputs remain untouched.
Only Assets/Packages/ProjectSettings are copied, never Unity cache folders.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
V1 = ROOT / "Saved/UnityCityHallCameraMaterialProbeV1"
V7 = ROOT / "Saved/UnityCityHallCameraMaterialProbeV7"
V1_RUN = V1 / "OutputRuns/20261009_052943_467_46956"
V1_NATIVE = V1_RUN / "hall_camera_material_native.json"
V1_NATIVE_SHA = "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451"
V1_CONTRACT = V1 / "hall_camera_material_contract.json"
V1_CONTRACT_SHA = "4bac306793ef6a94cab283ab169a3d2cd912f27649f3d093df0efb6ab260df2f"
EXPORTER = ROOT / "Scripts/RenderRokUnityCityHallCameraMaterialNativeV7.cs"
VERIFIER = ROOT / "Scripts/VerifyRokUnityCityHallCameraMaterialProbeV7.py"
CONTRACT = V7 / "hall_camera_material_contract_v7.json"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009HallSamplingDiagnostics/hall_sampling_native_prepare_v7.json"
DAY = "Assets/Source/BundleAssets/Map/Building/map_building_with_mask-5.shader"
NIGHT = "Assets/Source/BundleAssets/Map/Shader/cm_city_mask_night.shader"
SOURCE_SHA = {
    DAY: "d26a3a08eaccb769f0ea086316b36c80176095651059c7b881e3b5400d8acbfa",
    NIGHT: "292ec5dc10f4aa3fca7ecfc2bf5790bbd6910051ee126cffd16fa96ff0f98065",
}
PROBES = {
    DAY: "Assets/ProbeHallDiagnosticDayV7.shader",
    NIGHT: "Assets/ProbeHallDiagnosticNightV7.shader",
}


def need(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_once(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644), "wb") as stream:
        stream.write(data)


def instrument(source: str, role: str) -> str:
    source = source.replace("\r\n", "\n")
    names = re.findall(r'^Shader\s+"([^"]+)"', source, re.MULTILINE)
    need(len(names) == 1, "Exactly one source Shader name required")
    source = source.replace(f'Shader "{names[0]}"', f'Shader "RokProbe/Hall{role}V7"', 1)
    need(source.count("Blend SrcAlpha OneMinusSrcAlpha, SrcAlpha OneMinusSrcAlpha") == 1,
         "Source blend state changed")
    source = source.replace("Blend SrcAlpha OneMinusSrcAlpha, SrcAlpha OneMinusSrcAlpha",
                            "Blend [_RokSrcBlend] [_RokDstBlend], [_RokSrcBlend] [_RokDstBlend]", 1)
    source = re.sub(r'(Properties\s*\{)',
                    r'\1\n        [HideInInspector] _RokSrcBlend("Probe source blend", Float) = 5\n'
                    r'        [HideInInspector] _RokDstBlend("Probe source blend", Float) = 10',
                    source, count=1)
    need(source.count("#pragma target 2.0") == 1, "Source shader model changed")
    source = source.replace("#pragma target 2.0", "#pragma target 4.0", 1)
    need(source.count("float _CityTransparency;") == 1, "Source transparency declaration changed")
    source = source.replace("float _CityTransparency;", "float _CityTransparency;\n            float _RokProbeMode;", 1)
    signature = "fixed4 SpriteFrag(v2f IN) : SV_Target"
    need(source.count(signature) == 1, "Source fragment signature changed")
    source = source.replace(signature, "float4 SpriteFrag(v2f IN) : SV_Target", 1)
    helper = """float4 RokProbePackFloat(float value)
            {
                uint bits = asuint(value);
                return float4((bits & 255u), ((bits >> 8) & 255u),
                              ((bits >> 16) & 255u), ((bits >> 24) & 255u)) / 255.0;
            }
            """
    source = source.replace("float4 SpriteFrag(v2f IN) : SV_Target", helper + "float4 SpriteFrag(v2f IN) : SV_Target", 1)
    marker = re.search(r'(float4 SpriteFrag\(v2f IN\) : SV_Target\s*\{)', source)
    need(marker is not None, "Fragment function opening absent")
    lines = ["                // Diagnostic modes pack one IEEE754 float into RGBA8 with Blend One Zero."]
    for start, matrix in ((1, "unity_ObjectToWorld"), (17, "UNITY_MATRIX_VP")):
        for row in range(4):
            for col in range(4):
                mode = start + row*4 + col
                lines.append(f"                if (_RokProbeMode == {mode}.0) return RokProbePackFloat({matrix}[{row}][{col}]);")
    lines += [
        "                if (_RokProbeMode == 33.0) return RokProbePackFloat(IN.texcoord.x);",
        "                if (_RokProbeMode == 34.0) return RokProbePackFloat(IN.texcoord.y);",
        "                if (_RokProbeMode == 35.0) return RokProbePackFloat(tex2D(_MainTex, IN.texcoord).r);",
        "                if (_RokProbeMode == 36.0) return RokProbePackFloat(tex2D(_MainTex, IN.texcoord).g);",
        "                if (_RokProbeMode == 37.0) return RokProbePackFloat(tex2D(_MainTex, IN.texcoord).b);",
        "                if (_RokProbeMode == 38.0) return RokProbePackFloat(tex2D(_MainTex, IN.texcoord).a);",
        "                if (_RokProbeMode == 39.0) return RokProbePackFloat(tex2D(_Mask, IN.texcoord).r);",
        "                if (_RokProbeMode == 40.0) return RokProbePackFloat(tex2D(_Mask, IN.texcoord).g);",
        "                if (_RokProbeMode == 41.0) return RokProbePackFloat(tex2D(_Mask, IN.texcoord).b);",
        "                if (_RokProbeMode == 42.0) return RokProbePackFloat(tex2D(_Mask, IN.texcoord).a);",
        "                if (_RokProbeMode == 47.0) return RokProbePackFloat(_Time.y);",
        "                if (_RokProbeMode == 48.0) return RokProbePackFloat(0.75);",
    ]
    source = source[:marker.end()] + "\n" + "\n".join(lines) + source[marker.end():]
    need(source.count("return c_2;") == 1, "Source output return changed")
    source = source.replace("return c_2;", "if (_RokProbeMode == 43.0) return RokProbePackFloat(c_2.r);\n"
                            "                if (_RokProbeMode == 44.0) return RokProbePackFloat(c_2.g);\n"
                            "                if (_RokProbeMode == 45.0) return RokProbePackFloat(c_2.b);\n"
                            "                if (_RokProbeMode == 46.0) return RokProbePackFloat(c_2.a);\n"
                            "                return c_2;", 1)
    need("Blend [_RokSrcBlend] [_RokDstBlend], [_RokSrcBlend] [_RokDstBlend]" in source and
         "OUT.vertex = UnityObjectToClipPos(IN.vertex);" in source and
         "float4 SpriteFrag(v2f IN) : SV_Target" in source,
         "Source VS or blend path lost during instrumentation")
    return source


def prepare() -> dict:
    need(not V7.exists(), "V7 isolated project already exists; refusing overwrite")
    need(sha(V1_NATIVE) == V1_NATIVE_SHA and sha(V1_CONTRACT) == V1_CONTRACT_SHA,
         "Sealed V1 oracle/contract differs")
    need(EXPORTER.is_file() and VERIFIER.is_file(), "New V7 exporter/verifier absent")
    source_contract = json.loads(V1_CONTRACT.read_text(encoding="utf-8"))
    native = json.loads(V1_NATIVE.read_text(encoding="utf-8"))
    need(native["complete"] and native["case_count"] == 4 and
         [c["phase"] for c in native["cases"]] ==
         ["day_black", "day_white", "night_black", "night_white"], "Native1 phases differ")
    V7.mkdir(parents=True, exist_ok=False)
    for folder in ("Assets", "Packages", "ProjectSettings"):
        shutil.copytree(V1 / folder, V7 / folder,
                        ignore=shutil.ignore_patterns("RenderRokUnityCityHallCameraMaterialNative.cs",
                                                      "RenderRokUnityCityHallCameraMaterialNative.cs.meta")
                        if folder == "Assets" else None)
    shutil.copytree(V1 / "ProbeInputs", V7 / "ProbeInputs")
    need(not (V7 / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNative.cs").exists(),
         "V1 exporter accidentally copied")
    dest_exporter = V7 / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV7.cs"
    write_once(dest_exporter, EXPORTER.read_bytes())
    generated = {}
    for source_path, target_path in PROBES.items():
        original = V7 / source_path
        need(sha(original) == SOURCE_SHA[source_path], f"Copied source shader differs: {source_path}")
        role = "Day" if source_path == DAY else "Night"
        target = V7 / target_path
        write_once(target, instrument(original.read_text(encoding="utf-8-sig"), role).encode("utf-8"))
        generated[target_path] = sha(target)
    oldpins = [dict(pin) for pin in source_contract["input_files"]]
    oldpins = [pin for pin in oldpins if not pin["path"].endswith("RenderRokUnityCityHallCameraMaterialNative.cs")]
    oldpins.extend({"path": path, "sha256": digest} for path, digest in generated.items())
    oldpins.append({"path": "Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV7.cs",
                    "sha256": sha(dest_exporter)})
    need(len(oldpins) == 79 and len({pin["path"] for pin in oldpins}) == len(oldpins),
         "V7 source pin cardinality/identity differs")
    for item in oldpins:
        need(sha(V7 / item["path"]) == item["sha256"], f"V7 pinned copy differs: {item['path']}")
    contract = dict(source_contract)
    contract.update(schema="rok.hall_camera_material_contract.v7", probe_root=V7.as_posix(),
                    exporter_sha256=sha(EXPORTER), producer_sha256=sha(Path(__file__)),
                    verifier_sha256=sha(VERIFIER), input_files=sorted(oldpins, key=lambda x: x["path"]),
                    diagnostic_day_sha256=generated[PROBES[DAY]],
                    diagnostic_night_sha256=generated[PROBES[NIGHT]],
                    v1_native_manifest_path=V1_NATIVE.as_posix(), v1_native_manifest_sha256=V1_NATIVE_SHA,
                    v1_case_raw=[{"path": c["phase"], "sha256": c["raw_top_left_rgba8"]["sha256"]}
                                 for c in native["cases"]],
                    qualifier="source_camera_material_instrumented_diagnostic_only; original and clone mode0 must byteexact V1")
    write_once(CONTRACT, (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    report = {"schema": "rok.hall_sampling_native_prepare.v7", "ready": True,
              "project": V7.relative_to(ROOT).as_posix(), "contract": CONTRACT.relative_to(ROOT).as_posix(),
              "contract_sha256": sha(CONTRACT), "exporter_sha256": sha(EXPORTER),
              "producer_sha256": sha(Path(__file__)), "verifier_sha256": sha(VERIFIER),
              "v1_oracle_sha256": V1_NATIVE_SHA, "source_shader_sha256": SOURCE_SHA,
              "diagnostic_shader_sha256": generated, "input_file_count": len(oldpins),
              "source_behaviors_executed": False, "original_four_frames_required": 4,
              "instrumented_mode0_four_frames_required": 4,
              "packed_rgba8_sentinels_required": 4,
              "packed_float_witnesses_required_after_sentinel": 92,
              "v3_v5_failure": "V5 full Float RT covered both source-positive points, while 1x1 ARGB32 ReadPixels returned clear. V7 reads each whole ARGB32 target and indexes the bottom-left buffer at the requested top-left pixel.",
              "v6_failure": "V6 96 packed raw samples include one IEEE754 negative zero; JsonUtility displayed it as positive 0.0. V7 records decoded_bits_u32 so raw sign and every other bit remain authoritative.",
              "first_sentinel_full_frame_required": True,
              "packed_witness_scope": "Blend One Zero instrumented clone after four mode0 byteexact frames; untouched source shader uniforms remain unobserved",
              "unity_started": False}
    write_once(REPORT, (json.dumps(report, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    return report


if __name__ == "__main__":
    print(json.dumps(prepare(), indent=2, ensure_ascii=False))
