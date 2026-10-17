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
V4 = ROOT / "Saved/UnityCityHallCameraMaterialProbeV4"
V1_RUN = V1 / "OutputRuns/20261009_052943_467_46956"
V1_NATIVE = V1_RUN / "hall_camera_material_native.json"
V1_NATIVE_SHA = "3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451"
V1_CONTRACT = V1 / "hall_camera_material_contract.json"
V1_CONTRACT_SHA = "4bac306793ef6a94cab283ab169a3d2cd912f27649f3d093df0efb6ab260df2f"
EXPORTER = ROOT / "Scripts/RenderRokUnityCityHallCameraMaterialNativeV4.cs"
VERIFIER = ROOT / "Scripts/VerifyRokUnityCityHallCameraMaterialProbeV4.py"
CONTRACT = V4 / "hall_camera_material_contract_v4.json"
REPORT = ROOT / "Saved/RokUnityMigration/BatchReview20261009HallSamplingDiagnostics/hall_sampling_native_prepare_v4.json"
DAY = "Assets/Source/BundleAssets/Map/Building/map_building_with_mask-5.shader"
NIGHT = "Assets/Source/BundleAssets/Map/Shader/cm_city_mask_night.shader"
SOURCE_SHA = {
    DAY: "d26a3a08eaccb769f0ea086316b36c80176095651059c7b881e3b5400d8acbfa",
    NIGHT: "292ec5dc10f4aa3fca7ecfc2bf5790bbd6910051ee126cffd16fa96ff0f98065",
}
PROBES = {
    DAY: "Assets/ProbeHallDiagnosticDayV4.shader",
    NIGHT: "Assets/ProbeHallDiagnosticNightV4.shader",
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
    source = source.replace(f'Shader "{names[0]}"', f'Shader "RokProbe/Hall{role}V4"', 1)
    need(source.count("float _CityTransparency;") == 1, "Source transparency declaration changed")
    source = source.replace("float _CityTransparency;", "float _CityTransparency;\n            float _RokProbeMode;", 1)
    signature = "fixed4 SpriteFrag(v2f IN) : SV_Target"
    need(source.count(signature) == 1, "Source fragment signature changed")
    source = source.replace(signature, "float4 SpriteFrag(v2f IN) : SV_Target", 1)
    marker = re.search(r'(float4 SpriteFrag\(v2f IN\) : SV_Target\s*\{)', source)
    need(marker is not None, "Fragment function opening absent")
    lines = [
        "                // Probe-only outputs have alpha=1 so the original SrcAlpha blend preserves float RGB.",
    ]
    for start, matrix in ((1, "unity_ObjectToWorld"), (9, "UNITY_MATRIX_VP")):
        for row in range(4):
            lines.append(f"                if (_RokProbeMode == {start+row}.0) return float4({matrix}[{row}].xyz, 1.0);")
    for start, matrix in ((5, "unity_ObjectToWorld"), (13, "UNITY_MATRIX_VP")):
        for row in range(4):
            lines.append(f"                if (_RokProbeMode == {start+row}.0) return float4({matrix}[{row}].w, 0.0, 0.0, 1.0);")
    lines += [
        "                if (_RokProbeMode == 20.0) return float4(IN.texcoord.xy, 0.0, 1.0);",
        "                if (_RokProbeMode == 21.0) return float4(tex2D(_MainTex, IN.texcoord).rgb, 1.0);",
        "                if (_RokProbeMode == 22.0) return float4(tex2D(_MainTex, IN.texcoord).a, 0.0, 0.0, 1.0);",
        "                if (_RokProbeMode == 23.0) return float4(tex2D(_Mask, IN.texcoord).rgb, 1.0);",
        "                if (_RokProbeMode == 24.0) return float4(tex2D(_Mask, IN.texcoord).a, 0.0, 0.0, 1.0);",
        "                if (_RokProbeMode == 27.0) return float4(_Time.y, 0.0, 0.0, 1.0);",
    ]
    source = source[:marker.end()] + "\n" + "\n".join(lines) + source[marker.end():]
    need(source.count("return c_2;") == 1, "Source output return changed")
    source = source.replace("return c_2;", "if (_RokProbeMode == 25.0) return float4(c_2.rgb, 1.0);\n"
                            "                if (_RokProbeMode == 26.0) return float4(c_2.a, 0.0, 0.0, 1.0);\n"
                            "                return c_2;", 1)
    need("Blend SrcAlpha OneMinusSrcAlpha, SrcAlpha OneMinusSrcAlpha" in source and
         "OUT.vertex = UnityObjectToClipPos(IN.vertex);" in source and
         "float4 SpriteFrag(v2f IN) : SV_Target" in source,
         "Source VS or blend path lost during instrumentation")
    return source


def prepare() -> dict:
    need(not V4.exists(), "V4 isolated project already exists; refusing overwrite")
    need(sha(V1_NATIVE) == V1_NATIVE_SHA and sha(V1_CONTRACT) == V1_CONTRACT_SHA,
         "Sealed V1 oracle/contract differs")
    need(EXPORTER.is_file() and VERIFIER.is_file(), "New V4 exporter/verifier absent")
    source_contract = json.loads(V1_CONTRACT.read_text(encoding="utf-8"))
    native = json.loads(V1_NATIVE.read_text(encoding="utf-8"))
    need(native["complete"] and native["case_count"] == 4 and
         [c["phase"] for c in native["cases"]] ==
         ["day_black", "day_white", "night_black", "night_white"], "Native1 phases differ")
    V4.mkdir(parents=True, exist_ok=False)
    for folder in ("Assets", "Packages", "ProjectSettings"):
        shutil.copytree(V1 / folder, V4 / folder,
                        ignore=shutil.ignore_patterns("RenderRokUnityCityHallCameraMaterialNative.cs",
                                                      "RenderRokUnityCityHallCameraMaterialNative.cs.meta")
                        if folder == "Assets" else None)
    shutil.copytree(V1 / "ProbeInputs", V4 / "ProbeInputs")
    need(not (V4 / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNative.cs").exists(),
         "V1 exporter accidentally copied")
    dest_exporter = V4 / "Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV4.cs"
    write_once(dest_exporter, EXPORTER.read_bytes())
    generated = {}
    for source_path, target_path in PROBES.items():
        original = V4 / source_path
        need(sha(original) == SOURCE_SHA[source_path], f"Copied source shader differs: {source_path}")
        role = "Day" if source_path == DAY else "Night"
        target = V4 / target_path
        write_once(target, instrument(original.read_text(encoding="utf-8-sig"), role).encode("utf-8"))
        generated[target_path] = sha(target)
    oldpins = [dict(pin) for pin in source_contract["input_files"]]
    oldpins = [pin for pin in oldpins if not pin["path"].endswith("RenderRokUnityCityHallCameraMaterialNative.cs")]
    oldpins.extend({"path": path, "sha256": digest} for path, digest in generated.items())
    oldpins.append({"path": "Assets/Editor/RenderRokUnityCityHallCameraMaterialNativeV4.cs",
                    "sha256": sha(dest_exporter)})
    need(len(oldpins) == 79 and len({pin["path"] for pin in oldpins}) == len(oldpins),
         "V4 source pin cardinality/identity differs")
    for item in oldpins:
        need(sha(V4 / item["path"]) == item["sha256"], f"V4 pinned copy differs: {item['path']}")
    contract = dict(source_contract)
    contract.update(schema="rok.hall_camera_material_contract.v4", probe_root=V4.as_posix(),
                    exporter_sha256=sha(EXPORTER), producer_sha256=sha(Path(__file__)),
                    verifier_sha256=sha(VERIFIER), input_files=sorted(oldpins, key=lambda x: x["path"]),
                    diagnostic_day_sha256=generated[PROBES[DAY]],
                    diagnostic_night_sha256=generated[PROBES[NIGHT]],
                    v1_native_manifest_path=V1_NATIVE.as_posix(), v1_native_manifest_sha256=V1_NATIVE_SHA,
                    v1_case_raw=[{"path": c["phase"], "sha256": c["raw_top_left_rgba8"]["sha256"]}
                                 for c in native["cases"]],
                    qualifier="source_camera_material_instrumented_diagnostic_only; original and clone mode0 must byteexact V1")
    write_once(CONTRACT, (json.dumps(contract, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    report = {"schema": "rok.hall_sampling_native_prepare.v4", "ready": True,
              "project": V4.relative_to(ROOT).as_posix(), "contract": CONTRACT.relative_to(ROOT).as_posix(),
              "contract_sha256": sha(CONTRACT), "exporter_sha256": sha(EXPORTER),
              "producer_sha256": sha(Path(__file__)), "verifier_sha256": sha(VERIFIER),
              "v1_oracle_sha256": V1_NATIVE_SHA, "source_shader_sha256": SOURCE_SHA,
              "diagnostic_shader_sha256": generated, "input_file_count": len(oldpins),
              "source_behaviors_executed": False, "original_four_frames_required": 4,
              "instrumented_mode0_four_frames_required": 4,
              "float_coverage_controls_required": 4,
              "float_witnesses_required_after_coverage": 48,
              "v3_failure": "44 float points were clear (0,0,0,1); V3 verifier false positive; V3 bytes preserved",
              "float_witness_scope": "instrumented renderer draws only; actual untouched shader uniform bytes remain unobserved",
              "unity_started": False}
    write_once(REPORT, (json.dumps(report, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    return report


if __name__ == "__main__":
    print(json.dumps(prepare(), indent=2, ensure_ascii=False))
