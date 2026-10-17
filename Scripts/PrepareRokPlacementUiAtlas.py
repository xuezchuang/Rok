"""Prepare a unique, hashed source package for the BuildCity action atlas.

Offline only: copies Unity source files to a fresh Saved evidence directory and writes
their metadata. It never writes Content or edits ref/Unity source files.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import os
import re
import shutil
import struct

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ATLAS_DIR = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map"
SOURCES = {
    "atlas_png": ATLAS_DIR / "ui_hud.png",
    "atlas_meta": ATLAS_DIR / "ui_hud.png.meta",
    "sprite_sheet": ATLAS_DIR / "ui_hud.tpsheet",
    "sprite_sheet_meta": ATLAS_DIR / "ui_hud.tpsheet.meta",
    "gray_material": ROOT / "ref/client-unity/Assets/BundleAssets/Default/UI_GrayWithMask.mat",
    "gray_material_meta": ROOT / "ref/client-unity/Assets/BundleAssets/Default/UI_GrayWithMask.mat.meta",
    "gray_shader": ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader",
    "gray_shader_meta": ROOT / "ref/client-unity/Assets/BundleAssets/UI/UIShader/ImageGrayShader.shader.meta",
}
SPECS = {
    "btn_hud_1008": (721, 575, 88, 88),
    "btn_hud_1004": (1, 435, 88, 88),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"Not a valid PNG IHDR: {path}")
    return struct.unpack(">II", header[16:24])


def parse_sheet(path: Path) -> tuple[tuple[int, int], dict[str, dict[str, int]]]:
    size = None
    sprites = {}
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        match = re.fullmatch(r":size=(\d+)x(\d+)", line)
        if match:
            size = (int(match.group(1)), int(match.group(2)))
            continue
        fields = line.split(";")
        if len(fields) >= 5 and fields[0] in SPECS:
            sprites[fields[0]] = dict(zip(("x", "y", "width", "height"), map(int, fields[1:5])))
    if size is None:
        raise RuntimeError("ui_hud.tpsheet is missing :size")
    return size, sprites


def parse_unity_rects(path: Path) -> dict[str, dict[str, int]]:
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    result = {}
    for name in SPECS:
        try:
            at = next(i for i, line in enumerate(lines) if line.strip() == f"name: {name}")
        except StopIteration as exc:
            raise RuntimeError(f"Unity SpriteMetaData missing {name}") from exc
        values = {}
        for line in lines[at + 1 : at + 14]:
            match = re.match(r"\s*(x|y|width|height):\s*(\d+)\s*$", line)
            if match:
                values[match.group(1)] = int(match.group(2))
        if set(values) != {"x", "y", "width", "height"}:
            raise RuntimeError(f"Unity SpriteMetaData rect incomplete for {name}")
        result[name] = values
    return result


def main() -> None:
    for path in SOURCES.values():
        if not path.is_file():
            raise FileNotFoundError(f"Required Unity reference source missing: {path}")
    size = png_size(SOURCES["atlas_png"])
    sheet_size, sheet_sprites = parse_sheet(SOURCES["sprite_sheet"])
    unity_sprites = parse_unity_rects(SOURCES["atlas_meta"])
    if size != (1024, 1024) or sheet_size != size:
        raise RuntimeError(f"Atlas dimensions differ: PNG={size} TPSheet={sheet_size}")
    for name, expected in SPECS.items():
        actual = tuple(sheet_sprites.get(name, {}).get(k) for k in ("x", "y", "width", "height"))
        if actual != expected:
            raise RuntimeError(f"TPSheet rect changed for {name}: {actual} != {expected}")
        if unity_sprites.get(name) != dict(zip(("x", "y", "width", "height"), expected)):
            raise RuntimeError(f"Unity .meta rect disagrees with TPSheet for {name}")

    image = Image.open(SOURCES["atlas_png"]).convert("RGBA")
    sprite_rows = []
    for name, (x, y_bottom, width, height) in SPECS.items():
        # TexturePacker's coordinates match Unity Sprite.rect bottom-left coordinates.
        # Convert to PNG's top-left pixel origin for byte-backed alpha inspection.
        top = size[1] - y_bottom - height
        alpha = image.crop((x, top, x + width, top + height)).getchannel("A")
        bbox = alpha.getbbox()
        sprite_rows.append({
            "name": name,
            "source_rect_bottom_left": {"x": x, "y": y_bottom, "width": width, "height": height},
            "png_pixel_rect_top_left": {"x": x, "y": top, "width": width, "height": height},
            "alpha_nonzero_bbox_in_rect": list(bbox) if bbox else None,
            "alpha_nonzero_pixel_count": sum(alpha.histogram()[1:]),
            "alpha_range": list(alpha.getextrema()),
        })

    utc = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f")
    run_dir = ROOT / "Saved/RokUnityMigration/PlacementUiAtlas" / f"{utc}_{os.getpid()}"
    run_dir.mkdir(parents=True, exist_ok=False)
    source_dir = run_dir / "Source"
    source_dir.mkdir()
    copied = []
    for role, source in SOURCES.items():
        target = source_dir / source.name
        shutil.copyfile(source, target)
        copied.append({
            "role": role,
            "source_path": source.relative_to(ROOT).as_posix(),
            "source_sha256": sha256(source),
            "copy_path": target.relative_to(ROOT).as_posix(),
            "copy_sha256": sha256(target),
            "bytes": target.stat().st_size,
        })

    sprites = []
    for row in sprite_rows:
        x, y, w, h = (row["source_rect_bottom_left"][k] for k in ("x", "y", "width", "height"))
        top = size[1] - y - h
        # This matches the existing UE MakeAtlasBrush/Slate UVRegion convention.
        uv_min = [x / size[0], top / size[1]]
        uv_max = [(x + w) / size[0], (top + h) / size[1]]
        sprites.append({**row, "slate_uv_min": uv_min, "slate_uv_max": uv_max})

    manifest = {
        "schema": "rok.placement_ui_atlas_source.v1",
        "scope": "Source-copy/metadata preparation only; no UE import or editor mutation.",
        "run_directory": run_dir.relative_to(ROOT).as_posix(),
        "atlas_size": list(size),
        "ue_target": "/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud",
        "source_coordinate_semantics": "TPSheet and Unity SpriteMetaData rects use bottom-left pixel origin; PNG byte pixels use top-left origin. Slate UV uses top=(H-(y+h)), then top/H through (top+h)/H.",
        "sprites": sprites,
        "sources": copied,
        "ue_texture_settings_contract": {
            "srgb": True,
            "mip_gen_settings": "TMGS_NO_MIPMAPS",
            "lod_group": "TEXTUREGROUP_UI",
            "never_stream": True,
            "compression_settings": "TC_EDITOR_ICON",
            "address_x": "TA_CLAMP",
            "address_y": "TA_CLAMP",
        },
    }
    manifest_path = run_dir / "atlas_manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"prepared": True, "manifest": str(manifest_path), "sprites": sprites}, ensure_ascii=False))


if __name__ == "__main__":
    main()
