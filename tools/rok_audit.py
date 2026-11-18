#!/usr/bin/env python3
"""Read-only project audit CLI for the ROK Unity-to-UE migration."""

from __future__ import annotations

import argparse
import datetime as _dt
import fnmatch
import json
import os
import re
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


SKIP_DIRS = {
    ".git",
    ".vs",
    "__pycache__",
    "Binaries",
    "DerivedDataCache",
    "Intermediate",
    "Library",
    "Logs",
    "Temp",
    "obj",
}

UE_PROJECT_EXTS = {".uproject", ".ini", ".umap", ".uasset", ".csv", ".json", ".py", ".md"}
UNITY_TEXTURE_EXTS = {".png", ".jpg", ".jpeg", ".tga", ".psd", ".exr"}
UNITY_CONFIG_EXTS = {".csv", ".json", ".bytes", ".xml"}

REQUIRED_DIRS = [
    "Config",
    "Content",
    "Content/Maps",
    "Content/RokPrototype",
    "Content/RokPrototype/Blueprints",
    "Content/RokPrototype/Materials",
    "Content/RokPrototype/Meshes",
    "Content/RokPrototype/Textures",
    "Content/RokPrototype/UI",
    "Plugins/McpAutomationBridge",
    "Scripts",
    "Source/Rok",
    "ref/client-unity",
    "ref/server",
    "ref/resources",
    "ref/config-tables",
]

IMPORTANT_FILES = [
    ("project", "Rok.uproject", "error"),
    ("config", "Config/DefaultEngine.ini", "error"),
    ("config", "Config/DefaultGame.ini", "warning"),
    ("config", "Config/DefaultInput.ini", "warning"),
    ("config", "Config/DefaultEditor.ini", "warning"),
    ("map", "Content/Maps/L_RokPrototype_Lit.umap", "error"),
    ("map", "Content/Maps/L_RokPrototype.umap", "warning"),
    ("map", "Content/Maps/L_RokCityRuntime.umap", "error"),
    ("map", "Content/Maps/L_RokCitySingleBuilding.umap", "error"),
    ("script", "Scripts/CreateRokPrototypeScene.py", "warning"),
    ("script", "Scripts/CreateRokRuntimeCityScene.py", "warning"),
    ("script", "Scripts/CreateRokSingleBuildingScene.py", "warning"),
    ("script", "Scripts/ExtractRokUnityCityPlacement.py", "warning"),
    ("script", "Scripts/VerifyRokPrototypeScene.py", "warning"),
    ("script", "Scripts/VerifyRokRuntimeCityScene.py", "warning"),
    ("script", "Scripts/VerifyRokSingleBuildingScene.py", "warning"),
    ("generated evidence", "Saved/RokUnityPlacement/unity_city_placement.json", "warning"),
    ("generated evidence", "Saved/RokUnityPlacement/unity_city_placement.csv", "warning"),
    ("generated evidence", "Saved/RokUnityPlacement/unity_init_building_grid.csv", "warning"),
]

EXPECTED_BLUEPRINTS = [
    "BP_RokRuntimeCityCameraPawn",
    "BP_RokRuntimeCityGameMode",
    "BP_RokSingleBuildingCameraPawn",
    "BP_RokSingleBuildingGameMode",
    "BP_RokStrategyCameraPawn",
    "BP_RokPlayerController",
]

EXPECTED_WIDGETS = [
    "WBP_RokMainHud",
    "WBP_RokBuildingPanel",
    "WBP_RokCityRuntimeHud",
]

EXPECTED_DATA_CONFIGS = [
    "DT_Buildings",
    "DT_Troops",
    "DT_Heroes",
    "DT_Resources",
    "DT_Technologies",
    "BuildingLevelData",
    "BuildingModelData",
]


@dataclass(frozen=True)
class Finding:
    level: str
    message: str
    detail: str = ""


@dataclass
class Inventory:
    root: Path
    files: list[Path]

    def by_ext(self, ext: str) -> list[Path]:
        wanted = ext.lower()
        return [p for p in self.files if p.suffix.lower() == wanted]

    def matching(self, pattern: str) -> list[Path]:
        pattern_lower = pattern.lower()
        return [p for p in self.files if fnmatch.fnmatch(p.as_posix().lower(), pattern_lower)]


def rel(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def scan_files(root: Path, exts: set[str] | None = None) -> list[Path]:
    files: list[Path] = []
    for current, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        base = Path(current)
        for name in filenames:
            path = base / name
            if exts is None or path.suffix.lower() in exts:
                files.append(path)
    files.sort(key=lambda p: p.as_posix().lower())
    return files


def normalize_asset_name(path: Path) -> str:
    name = path.stem.lower()
    name = re.sub(r"^(bp|wbp|dt|da|t|tx|m|mi|l)_", "", name)
    name = re.sub(r"[^a-z0-9]+", "", name)
    return name


def find_assets_by_stem(files: Iterable[Path], expected_stem: str) -> list[Path]:
    lower = expected_stem.lower()
    return [p for p in files if p.stem.lower() == lower]


def exists_any_named(files: Iterable[Path], names: Sequence[str]) -> dict[str, list[Path]]:
    return {name: find_assets_by_stem(files, name) for name in names}


def count_exts(files: Iterable[Path]) -> Counter[str]:
    return Counter(p.suffix.lower() or "<none>" for p in files)


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return path.read_text(encoding="utf-8-sig", errors="replace")


def audit_required_dirs(ue_root: Path) -> tuple[list[tuple[str, bool]], list[Finding]]:
    rows: list[tuple[str, bool]] = []
    findings: list[Finding] = []
    for item in REQUIRED_DIRS:
        exists = (ue_root / item).is_dir()
        rows.append((item, exists))
        if not exists:
            level = "warning" if item.startswith("ref/") else "error"
            findings.append(Finding(level, f"Missing required folder: {item}"))
    return rows, findings


def audit_important_files(ue_root: Path) -> tuple[list[tuple[str, str, bool]], list[Finding]]:
    rows: list[tuple[str, str, bool]] = []
    findings: list[Finding] = []
    for group, item, level in IMPORTANT_FILES:
        exists = (ue_root / item).is_file()
        rows.append((group, item, exists))
        if not exists:
            findings.append(Finding(level, f"Missing important {group} file: {item}"))
    return rows, findings


def audit_uproject(ue_root: Path) -> tuple[list[str], list[Finding]]:
    notes: list[str] = []
    findings: list[Finding] = []
    path = ue_root / "Rok.uproject"
    if not path.exists():
        return notes, findings

    try:
        data = json.loads(read_text(path))
    except json.JSONDecodeError as exc:
        findings.append(Finding("error", "Rok.uproject is not valid JSON", str(exc)))
        return notes, findings

    engine = str(data.get("EngineAssociation", ""))
    notes.append(f"EngineAssociation: `{engine or 'not set'}`")
    if engine and engine != "5.2":
        findings.append(Finding("warning", "Unexpected EngineAssociation", f"Expected 5.2, found {engine}"))

    plugins = {p.get("Name"): bool(p.get("Enabled")) for p in data.get("Plugins", []) if isinstance(p, dict)}
    for plugin in ["McpAutomationBridge", "PythonScriptPlugin", "EditorScriptingUtilities", "EnhancedInput"]:
        enabled = plugins.get(plugin)
        notes.append(f"Plugin `{plugin}`: `{enabled}`")
        if enabled is not True:
            findings.append(Finding("warning", f"Expected plugin is not enabled: {plugin}"))

    modules = [m.get("Name") for m in data.get("Modules", []) if isinstance(m, dict)]
    notes.append(f"Modules: `{', '.join(str(m) for m in modules) or 'none'}`")
    if "Rok" not in modules:
        findings.append(Finding("error", "Runtime module `Rok` is missing from Rok.uproject"))

    return notes, findings


def audit_maps_config(ue_root: Path) -> tuple[list[str], list[Finding]]:
    notes: list[str] = []
    findings: list[Finding] = []
    path = ue_root / "Config/DefaultEngine.ini"
    if not path.exists():
        return notes, findings

    text = read_text(path)
    for key in ["EditorStartupMap", "GameDefaultMap", "GlobalDefaultGameMode"]:
        match = re.search(rf"^{re.escape(key)}=(.+)$", text, flags=re.MULTILINE)
        value = match.group(1).strip() if match else ""
        notes.append(f"{key}: `{value or 'not set'}`")
        if not value:
            findings.append(Finding("warning", f"{key} is not set in Config/DefaultEngine.ini"))

    default_map = re.search(r"^GameDefaultMap=(.+)$", text, flags=re.MULTILINE)
    if default_map:
        asset = default_map.group(1).strip().split(".", 1)[0]
        if asset.startswith("/Game/"):
            rel_asset = "Content/" + asset.removeprefix("/Game/") + ".umap"
            if not (ue_root / rel_asset).exists():
                findings.append(Finding("error", "GameDefaultMap points to a missing map", rel_asset))

    return notes, findings


def audit_naming(ue_root: Path, inventory: Inventory) -> tuple[list[str], list[Finding]]:
    findings: list[Finding] = []
    notes: list[str] = []

    rules = [
        ("Maps", ".umap", r"^L_", "map assets should use L_ prefix", []),
        ("Blueprints", ".uasset", r"^BP_", "Blueprint assets should use BP_ prefix", []),
        ("Widgets", ".uasset", r"^WBP_", "Widget assets should use WBP_ prefix", []),
        ("Materials", ".uasset", r"^(M|MI)_", "Material assets should use M_ or MI_ prefix", []),
        (
            "Textures",
            ".uasset",
            r"^(T|TX)_",
            "authored texture assets should use T_ or TX_ prefix",
            ["ReferenceSprites", "CitySprites", "RuntimeCity", "UI/Reference", "UI/Processed"],
        ),
    ]

    for folder, ext, prefix_re, desc, excluded_parts in rules:
        folder_files = [
            p
            for p in inventory.files
            if f"/{folder}/" in "/" + rel(p, ue_root)
            and p.suffix.lower() == ext
            and not any(excluded in rel(p, ue_root) for excluded in excluded_parts)
        ]
        bad = [p for p in folder_files if not re.match(prefix_re, p.stem)]
        notes.append(f"{folder}: {len(folder_files)} checked, {len(bad)} naming warnings")
        for path in bad[:12]:
            findings.append(Finding("warning", f"Naming convention mismatch: {rel(path, ue_root)}", desc))
        if len(bad) > 12:
            findings.append(Finding("warning", f"{len(bad) - 12} additional {folder} naming mismatches omitted"))

    return notes, findings


def audit_expected_assets(ue_root: Path, inventory: Inventory) -> tuple[list[str], list[Finding]]:
    findings: list[Finding] = []
    notes: list[str] = []
    uassets = inventory.by_ext(".uasset")

    bp_hits = exists_any_named(uassets, EXPECTED_BLUEPRINTS)
    for name, hits in bp_hits.items():
        notes.append(f"Blueprint `{name}`: {'found' if hits else 'missing'}")
        if not hits:
            findings.append(Finding("warning", f"Expected Blueprint asset is missing: {name}"))

    widget_hits = exists_any_named(uassets, EXPECTED_WIDGETS)
    for name, hits in widget_hits.items():
        notes.append(f"Widget `{name}`: {'found' if hits else 'missing'}")
        if not hits:
            findings.append(Finding("warning", f"Expected UI Widget asset is missing: {name}"))

    config_like = [p for p in inventory.files if p.suffix.lower() in {".uasset", ".csv", ".json"}]
    for name in EXPECTED_DATA_CONFIGS:
        matches = [p for p in config_like if name.lower() in p.stem.lower()]
        notes.append(f"Data/config `{name}`: {'found' if matches else 'missing'}")
        if not matches:
            findings.append(Finding("warning", f"Expected DataTable/config asset is missing: {name}"))

    if not (ue_root / "Content/RokPrototype/Data").exists() and not (ue_root / "Content/Data").exists():
        findings.append(Finding("warning", "No UE Content Data folder found for DataTables/DataAssets"))

    return notes, findings


def audit_unity_comparison(ue_root: Path, ue_inv: Inventory, unity_root: Path | None) -> tuple[list[str], list[Finding]]:
    notes: list[str] = []
    findings: list[Finding] = []
    if unity_root is None:
        notes.append("Unity path was not provided; comparison skipped.")
        return notes, findings
    if not unity_root.exists():
        findings.append(Finding("error", "Unity path does not exist", str(unity_root)))
        notes.append("Unity comparison could not run.")
        return notes, findings

    unity_files = scan_files(unity_root, {".prefab", ".unity"} | UNITY_TEXTURE_EXTS | UNITY_CONFIG_EXTS)
    unity_prefabs = [p for p in unity_files if p.suffix.lower() == ".prefab"]
    unity_ui_prefabs = [p for p in unity_prefabs if "ui" in p.as_posix().lower()]
    unity_scenes = [p for p in unity_files if p.suffix.lower() == ".unity"]
    unity_textures = [p for p in unity_files if p.suffix.lower() in UNITY_TEXTURE_EXTS]
    unity_configs = [p for p in unity_files if p.suffix.lower() in UNITY_CONFIG_EXTS]

    ue_blueprints = [p for p in ue_inv.by_ext(".uasset") if p.stem.startswith("BP_")]
    ue_widgets = [p for p in ue_inv.by_ext(".uasset") if p.stem.startswith("WBP_") or "/UI/" in "/" + rel(p, ue_root)]
    ue_maps = ue_inv.by_ext(".umap")
    ue_textures = [p for p in ue_inv.by_ext(".uasset") if "/Textures/" in "/" + rel(p, ue_root) or p.stem.startswith(("T_", "TX_"))]
    ue_configs = [p for p in ue_inv.files if p.suffix.lower() in {".csv", ".json"} or p.stem.startswith("DT_")]

    comparisons = [
        ("Unity Prefabs vs UE Blueprints", unity_prefabs, ue_blueprints),
        ("Unity UI prefabs vs UE Widgets", unity_ui_prefabs, ue_widgets),
        ("Unity scenes vs UE maps", unity_scenes, ue_maps),
        ("Unity textures vs UE textures", unity_textures, ue_textures),
        ("Unity config files vs UE DataTables / JSON configs", unity_configs, ue_configs),
    ]

    for label, source, target in comparisons:
        source_names = {normalize_asset_name(p) for p in source}
        target_names = {normalize_asset_name(p) for p in target}
        matched = source_names & target_names
        notes.append(f"{label}: Unity {len(source)}, UE {len(target)}, normalized name matches {len(matched)}")
        if source and not target:
            findings.append(Finding("warning", f"{label}: source assets found but no UE targets found"))
        elif source and len(matched) == 0:
            findings.append(Finding("warning", f"{label}: no normalized name overlap found"))

    key_scene = unity_root / "Assets/BundleAssets/Map/Scenes/Map_TTS.unity"
    key_map = ue_root / "Content/Maps/L_RokCitySingleBuilding.umap"
    if key_scene.exists() and not key_map.exists():
        findings.append(Finding("error", "Unity Map_TTS scene exists but UE single-building map is missing"))

    sprite = unity_root.parent / "resources/Sprite/Castle_6_5.png"
    sprite_mask = unity_root.parent / "resources/Sprite/Castle_6_5_mask.png"
    if sprite.exists():
        notes.append(f"Reference sprite Castle_6_5.png: found at `{rel(sprite, ue_root)}`")
    if sprite.exists() and not sprite_mask.exists():
        findings.append(Finding("warning", "Castle_6_5.png exists but Castle_6_5_mask.png was not found"))

    return notes, findings


def markdown_table(headers: Sequence[str], rows: Sequence[Sequence[object]]) -> str:
    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join("---" for _ in headers) + " |",
    ]
    for row in rows:
        lines.append("| " + " | ".join(str(cell).replace("\n", " ") for cell in row) + " |")
    return "\n".join(lines)


def bullet_list(items: Sequence[str], empty: str) -> str:
    if not items:
        return f"- {empty}"
    return "\n".join(f"- {item}" for item in items)


def build_report(ue_root: Path, unity_root: Path | None, out_path: Path) -> tuple[str, list[Finding]]:
    ue_inv = Inventory(ue_root, scan_files(ue_root, UE_PROJECT_EXTS))
    all_findings: list[Finding] = []

    dir_rows, findings = audit_required_dirs(ue_root)
    all_findings.extend(findings)
    file_rows, findings = audit_important_files(ue_root)
    all_findings.extend(findings)
    uproject_notes, findings = audit_uproject(ue_root)
    all_findings.extend(findings)
    map_config_notes, findings = audit_maps_config(ue_root)
    all_findings.extend(findings)
    naming_notes, findings = audit_naming(ue_root, ue_inv)
    all_findings.extend(findings)
    asset_notes, findings = audit_expected_assets(ue_root, ue_inv)
    all_findings.extend(findings)
    unity_notes, findings = audit_unity_comparison(ue_root, ue_inv, unity_root)
    all_findings.extend(findings)

    errors = [f for f in all_findings if f.level == "error"]
    warnings = [f for f in all_findings if f.level == "warning"]
    ext_counts = count_exts(ue_inv.files)

    next_actions: list[str] = []
    if any("DataTable" in f.message or "Data/config" in f.message for f in warnings):
        next_actions.append("Decide the first UE DataTable/DataAsset slice for building, troop, hero, and resource config ingestion.")
    if any("Widget" in f.message for f in warnings):
        next_actions.append("Create or intentionally defer WBP assets for the C++ HUD/building panel classes so UI migration state is explicit.")
    if any("L_RokPrototype.umap" in f.message for f in warnings):
        next_actions.append("Either restore `/Game/Maps/L_RokPrototype` as the baseline map or update project guidance if the baseline was retired.")
    if unity_root is not None:
        next_actions.append("Use the Unity-vs-UE comparison counts to pick one narrow asset family for the next migration pass.")
    if not next_actions:
        next_actions.append("No blocking audit issue found; run the existing UE scene verification scripts before changing content.")

    lines: list[str] = []
    lines.append("# ROK Unity-to-UE Project Audit")
    lines.append("")
    lines.append(f"- Generated: {_dt.datetime.now().isoformat(timespec='seconds')}")
    lines.append(f"- UE root: `{ue_root}`")
    lines.append(f"- Unity root: `{unity_root if unity_root else 'not provided'}`")
    lines.append(f"- Output: `{out_path}`")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"- Errors: {len(errors)}")
    lines.append(f"- Warnings: {len(warnings)}")
    lines.append(f"- Scanned UE audit files: {len(ue_inv.files)}")
    lines.append(f"- UE file types: " + ", ".join(f"`{k}` {v}" for k, v in sorted(ext_counts.items())))
    lines.append("")
    lines.append("## Errors")
    lines.append("")
    lines.append(bullet_list([f"{f.message}" + (f" - {f.detail}" if f.detail else "") for f in errors], "None"))
    lines.append("")
    lines.append("## Warnings")
    lines.append("")
    lines.append(bullet_list([f"{f.message}" + (f" - {f.detail}" if f.detail else "") for f in warnings], "None"))
    lines.append("")
    lines.append("## UE Project Checks")
    lines.append("")
    lines.append("### Required Folders")
    lines.append("")
    lines.append(markdown_table(["Path", "Exists"], [(p, "yes" if ok else "no") for p, ok in dir_rows]))
    lines.append("")
    lines.append("### Important Files")
    lines.append("")
    lines.append(markdown_table(["Group", "Path", "Exists"], [(g, p, "yes" if ok else "no") for g, p, ok in file_rows]))
    lines.append("")
    lines.append("### Project Settings")
    lines.append("")
    lines.append(bullet_list(uproject_notes + map_config_notes, "No project settings inspected."))
    lines.append("")
    lines.append("### Naming Conventions")
    lines.append("")
    lines.append(bullet_list(naming_notes, "No naming checks ran."))
    lines.append("")
    lines.append("### Expected Assets")
    lines.append("")
    lines.append(bullet_list(asset_notes, "No expected asset checks ran."))
    lines.append("")
    lines.append("## Unity vs UE Comparison")
    lines.append("")
    lines.append(bullet_list(unity_notes, "Unity comparison skipped."))
    lines.append("")
    lines.append("## Suggested Next Actions")
    lines.append("")
    lines.append(bullet_list(next_actions, "None"))
    lines.append("")

    return "\n".join(lines), all_findings


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate a read-only Markdown audit for the ROK UE migration project.")
    parser.add_argument("--ue", required=True, type=Path, help="Unreal project root path.")
    parser.add_argument("--unity", type=Path, help="Optional Unity project root path.")
    parser.add_argument("--out", required=True, type=Path, help="Output Markdown report path.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    ue_root = args.ue.resolve()
    unity_root = args.unity.resolve() if args.unity else None
    out_path = args.out.resolve()

    if not ue_root.exists():
        raise SystemExit(f"UE path does not exist: {ue_root}")
    if not (ue_root / "Rok.uproject").exists():
        raise SystemExit(f"UE path does not look like the Rok project root: {ue_root}")

    report, findings = build_report(ue_root, unity_root, out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(report, encoding="utf-8")

    errors = sum(1 for f in findings if f.level == "error")
    warnings = sum(1 for f in findings if f.level == "warning")
    print(f"Wrote audit report: {out_path}")
    print(f"Errors: {errors}; warnings: {warnings}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
