"""Freeze the isolated five-family Unity diagnostic quad and all 42 RGB frames.

Only fixed diagnostic 2x2 textures are emitted here. Native source mips have a
different owner (CityGammaFamilyInputAudit) and are never admitted by this gate.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
RUN = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2/OutputRuns/20261008_234412_007_44320"
NATIVE = RUN / "native_state.json"
MATERIALS = ROOT / "Saved/UnityCityGammaRemainingFamiliesProbe/OutputRuns/20261008_230313_337_7496/native_families.json"
CONTRACT = ROOT / "Saved/UnityCityGammaFamilyStateProbeV2/state_contract.json"
EXPORTER = ROOT / "Scripts/ExportRokCityGammaFamilyStateProbe.cs"
OUT = ROOT / "Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v2"
NATIVE_SHA = "671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71"
FAMILIES = ["initial:5", "ground:ground_1", "ground:ground_2",
            "support:boundary_support_road", "support:boundary_support_wall_shadow"]
FRAMES = ["background_a", "background_b", "family_a", "family_b",
          "front_only", "front_occluder", "late_only", "late_behind"]
CONSTANTS = {"main": (205, 83, 41, 128), "splat": (255, 0, 0, 255),
             "mask_blue": (255, 0, 255, 255), "mask_no_blue": (255, 0, 0, 255),
             "white": (255, 255, 255, 255)}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha(path: Path) -> str:
    return sha(path.read_bytes())


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def immutable(path: Path, data: bytes) -> None:
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f"Existing fixture differs: {path}")
        return
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644)
    with os.fdopen(fd, "wb") as stream:
        stream.write(data)


def main() -> None:
    if file_sha(NATIVE) != NATIVE_SHA:
        raise ValueError("Pinned native state SHA differs")
    native = json.loads(NATIVE.read_text(encoding="utf-8"))
    materials = json.loads(MATERIALS.read_text(encoding="utf-8"))
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    if not native["complete"] or native["failures"] or not materials["complete"] or materials["failures"]:
        raise ValueError("Unity probe is incomplete")
    if [r["source_key"] for r in native["rows"]] != FAMILIES or [r["source_key"] for r in materials["rows"]] != FAMILIES:
        raise ValueError("Five-family order differs")
    if native["source_contract_sha256"] != file_sha(CONTRACT) or contract["exporter_sha256"] != file_sha(EXPORTER):
        raise ValueError("Unity source/exporter chain differs")
    OUT.mkdir(parents=True, exist_ok=True)
    textures = []
    for name, rgba in CONSTANTS.items():
        bgra = bytes((rgba[2], rgba[1], rgba[0], rgba[3])) * 4
        path = OUT / f"{name}.bgra8.bin"
        immutable(path, bgra)
        textures.append({"name": name, "file": relative(path), "sha256": sha(bgra),
                         "rgba8": list(rgba), "width": 2, "height": 2,
                         "format": "PF_B8G8R8A8", "srgb": False, "mips": 1,
                         "filter": "Point", "wrap": "Clamp"})
    rows = []
    cases = []
    for index, (row, material) in enumerate(zip(native["rows"], materials["rows"], strict=True)):
        if row["source_key"] != material["source_key"] or row["source_material_guid"] != material["source_material_guid"]:
            raise ValueError("Native/material family identity differs")
        properties = {p["name"]: p for p in material["declared_properties"]}
        row_frames = FRAMES + (["night_mask_b_zero"] if index == 0 else []) + (["road_explicit_white"] if index == 3 else [])
        if set(f["name"] for f in row["frames"]) != set(row_frames):
            raise ValueError("Native frame names differ")
        rows.append({"source_key": row["source_key"], "family_index": index,
                     "source_material_guid": row["source_material_guid"],
                     "renderer_file_id": row["renderer_file_id_decimal"],
                     "mesh_file_id": row["source_mesh_file_id_decimal"],
                     "source_queue": row["source_material_queue"],
                     "source_shader": row["shader_name"],
                     "source_material": material["source_material"],
                     "source_prefab": material["source_prefab"],
                     "source_renderer_path": row["source_renderer_path"],
                     "material_color": properties.get("_Color", {}).get("color", {"r": 1, "g": 1, "b": 1, "a": 1}),
                     "team1": properties.get("_Color_1", {}).get("color", {"r": 1, "g": 1, "b": 1, "a": 1}),
                     "team2": properties.get("_Color_2", {}).get("color", {"r": 1, "g": 1, "b": 1, "a": 1}),
                     "night_color3": properties.get("_Color_3", {}).get("color", {"r": 0, "g": 0, "b": 0, "a": 1}),
                     "tex_st": [{"scale": properties.get(f"_Tex{i}", {}).get("texture_scale", {"x": 1, "y": 1}),
                                  "offset": properties.get(f"_Tex{i}", {}).get("texture_offset", {"x": 0, "y": 0})}
                                 for i in range(4)],
                     "vertex_alpha8": 128 if index == 2 else 255,
                     "texture_slots": (["main", "mask_blue"] if index == 0 else
                                       ["main"] if index >= 3 else
                                       ["main", "main", "main", "main", "splat"]),
                     "source_texture_metadata": row["textures"]})
        by_name = {f["name"]: f for f in row["frames"]}
        for name in row_frames:
            frame = by_name[name]
            png = RUN / frame["png"]
            if file_sha(png) != frame["sha256"]:
                raise ValueError(f"Native PNG changed: {png}")
            with Image.open(png) as image:
                if image.size != (128, 128) or list(image.convert("RGBA").getpixel((64, 64))) != frame["center_rgba8"]:
                    raise ValueError(f"Native PNG extent/center differs: {png}")
            # _Time.y was not controlled in Unity. Only the B=0 night-mask
            # frame has an exact source expectation; retain the original RGB
            # image for inspection but do not claim exact time-dependent match.
            comparison = "night_time_pending" if index == 0 and name in ("family_a", "family_b") else "rgb_max_1_lsb"
            cases.append({"id": f"{index:02d}_{row['source_key'].replace(':', '_')}_{name}",
                          "family_index": index, "frame": name,
                          "source_png": relative(png), "source_png_sha256": frame["sha256"],
                          "source_center_rgba8": frame["center_rgba8"],
                          "comparison": comparison,
                          "background": "b" if name in ("background_b", "family_b") else "a",
                          "draw_family": name not in ("background_a", "background_b", "front_only", "late_only"),
                          "front": name in ("front_only", "front_occluder"),
                          "late": name in ("late_only", "late_behind"),
                          "mask_override": "mask_no_blue" if name == "night_mask_b_zero" else "white" if name == "road_explicit_white" else ""})
    if len(cases) != 42:
        raise ValueError("Expected exactly 42 source frames")
    manifest = {"schema": "rok.city_gamma_family_raster_fixture.v1",
                "scope": "isolated Unity diagnostic quad, never a source mesh or GameApp production packet",
                "native_state_path": relative(NATIVE), "native_state_sha256": NATIVE_SHA,
                "material_probe_path": relative(MATERIALS), "material_probe_sha256": file_sha(MATERIALS),
                "source_contract_path": relative(CONTRACT), "source_contract_sha256": file_sha(CONTRACT),
                "exporter_path": relative(EXPORTER), "exporter_sha256": file_sha(EXPORTER),
                "diagnostic": {"extent": [128, 128], "format": "PF_B8G8R8A8", "srgb": False,
                               "quad_xy": [-1, -1, 1, 1], "quad_indices": [0, 2, 1, 0, 3, 2],
                               "orthographic_size": 1.25, "camera_z": -5,
                               "front_z": -0.5, "late_z": 0.5,
                               "background_a": [0.07, 0.15, 0.71, 1],
                               "background_b": [0.74, 0.09, 0.05, 1],
                               "green": [0.03, 0.94, 0.07, 1],
                               "time_y": "unknown_unqualified_for_exact_night_B1"},
                "textures": textures, "rows": rows, "cases": cases}
    path = OUT / "manifest.json"
    immutable(path, (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(json.dumps({"manifest": str(path), "sha256": file_sha(path), "cases": len(cases)}))


if __name__ == "__main__":
    main()
