import json
import os
import re
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
UNITY_ASSETS_ROOT = PROJECT_ROOT / "ref" / "client-unity" / "Assets"
SCENE_ROOT = UNITY_ASSETS_ROOT / "BundleAssets" / "Map" / "Scenes"
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = OUTPUT_DIR / "unity_city_boundary.json"
OUTPUT_MD = OUTPUT_DIR / "unity_city_boundary.md"

BOUNDARY_NAME_PATTERNS = (
    "CityWall",
    "Citywall",
    "City_Wall",
    "CityGate",
    "city_wall",
)

BOUNDARY_ASSET_PATTERNS = (
    "CityWall",
    "CityGate",
    "Citywall",
    "city_wall",
)


def rel(path):
    return str(path.relative_to(PROJECT_ROOT)).replace("\\", "/")


def read_text(path):
    return path.read_text(encoding="utf-8", errors="ignore")


def build_guid_index():
    index = {}
    for meta_path in UNITY_ASSETS_ROOT.rglob("*.meta"):
        try:
            text = read_text(meta_path)
        except OSError:
            continue
        match = re.search(r"^guid:\s*([0-9a-fA-F]+)\s*$", text, re.M)
        if not match:
            continue
        asset_path = meta_path.with_suffix("")
        if asset_path.exists():
            index[match.group(1)] = rel(asset_path)
    return index


def parse_value_line(line):
    return line.split("value:", 1)[1].strip() if "value:" in line else ""


def parse_object_reference_guid(line):
    match = re.search(r"guid:\s*([0-9a-fA-F]+)", line)
    return match.group(1) if match else ""


def parse_prefab_instance_blocks(scene_path):
    text = read_text(scene_path)
    blocks = re.split(r"\n--- !u!1001 &", text)
    parsed = []
    for block in blocks:
        if "PrefabInstance:" not in block or "m_SourcePrefab:" not in block:
            continue
        full_block = "--- !u!1001 &" + block
        source_match = re.search(r"m_SourcePrefab:\s*\{fileID:\s*100100000,\s*guid:\s*([0-9a-fA-F]+),", full_block)
        source_guid = source_match.group(1) if source_match else ""
        modifications = []
        material_guids = []
        lines = full_block.splitlines()
        current_property = ""
        for index, line in enumerate(lines):
            stripped = line.strip()
            if stripped.startswith("propertyPath:"):
                current_property = stripped.split("propertyPath:", 1)[1].strip()
                continue
            if current_property and stripped.startswith("value:"):
                value = parse_value_line(stripped)
                object_guid = ""
                if index + 1 < len(lines):
                    object_guid = parse_object_reference_guid(lines[index + 1])
                    if object_guid:
                        material_guids.append(object_guid)
                modifications.append({
                    "property": current_property,
                    "value": value,
                    "object_guid": object_guid,
                })
                current_property = ""
        parsed.append({
            "scene": rel(scene_path),
            "source_prefab_guid": source_guid,
            "modifications": modifications,
            "material_override_guids": sorted(set(material_guids)),
        })
    return parsed


def modification_values(instance, property_name):
    return [
        item["value"]
        for item in instance["modifications"]
        if item["property"] == property_name
    ]


def last_value(instance, property_name, default=""):
    values = modification_values(instance, property_name)
    return values[-1] if values else default


def float_value(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def int_value(value, default=0):
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def is_boundary_instance(instance, guid_index):
    source_path = guid_index.get(instance["source_prefab_guid"], "")
    names = modification_values(instance, "m_Name")
    haystack = " ".join([source_path] + names)
    return any(pattern in haystack for pattern in BOUNDARY_NAME_PATTERNS)


def summarize_instance(instance, guid_index):
    names = modification_values(instance, "m_Name")
    active_values = [int_value(value, 0) for value in modification_values(instance, "m_IsActive")]
    material_guids = instance["material_override_guids"]
    return {
        "scene": instance["scene"],
        "source_prefab_guid": instance["source_prefab_guid"],
        "source_prefab_path": guid_index.get(instance["source_prefab_guid"], ""),
        "names": names,
        "primary_name": next((name for name in names if any(pattern in name for pattern in BOUNDARY_NAME_PATTERNS)), names[0] if names else ""),
        "active_values": active_values,
        "active": bool(active_values[-1]) if active_values else None,
        "local_position": {
            "x": float_value(last_value(instance, "m_LocalPosition.x")),
            "y": float_value(last_value(instance, "m_LocalPosition.y")),
            "z": float_value(last_value(instance, "m_LocalPosition.z")),
        },
        "local_scale": {
            "x": float_value(last_value(instance, "m_LocalScale.x", "1")),
            "y": float_value(last_value(instance, "m_LocalScale.y", "1")),
            "z": float_value(last_value(instance, "m_LocalScale.z", "1")),
        },
        "local_euler_hint": {
            "x": float_value(last_value(instance, "m_LocalEulerAnglesHint.x")),
            "y": float_value(last_value(instance, "m_LocalEulerAnglesHint.y")),
            "z": float_value(last_value(instance, "m_LocalEulerAnglesHint.z")),
        },
        "material_override_guids": material_guids,
        "material_override_paths": [guid_index.get(guid, "") for guid in material_guids],
    }


def discover_boundary_assets(guid_index):
    assets = []
    for path in UNITY_ASSETS_ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() == ".meta":
            continue
        name = path.name
        if any(pattern in name for pattern in BOUNDARY_ASSET_PATTERNS):
            assets.append({
                "path": rel(path),
                "extension": path.suffix.lower(),
                "size": path.stat().st_size,
            })
    return sorted(assets, key=lambda item: item["path"])


def material_main_texture_guid(material_path):
    path = PROJECT_ROOT / material_path
    if not path.exists() or path.suffix.lower() != ".mat":
        return ""
    text = read_text(path)
    match = re.search(r"- _MainTex:\s*\n\s*m_Texture:\s*\{fileID:\s*\d+,\s*guid:\s*([0-9a-fA-F]+),", text)
    return match.group(1) if match else ""


def build_material_records(boundary_assets, guid_index):
    records = []
    for asset in boundary_assets:
        if asset["extension"] != ".mat":
            continue
        main_guid = material_main_texture_guid(asset["path"])
        records.append({
            "path": asset["path"],
            "main_texture_guid": main_guid,
            "main_texture_path": guid_index.get(main_guid, ""),
        })
    return records


def build_manifest():
    guid_index = build_guid_index()
    instances = []
    for scene_path in sorted(SCENE_ROOT.glob("*.unity")):
        for instance in parse_prefab_instance_blocks(scene_path):
            if is_boundary_instance(instance, guid_index):
                instances.append(summarize_instance(instance, guid_index))

    boundary_assets = discover_boundary_assets(guid_index)
    materials = build_material_records(boundary_assets, guid_index)
    manifest = {
        "schema": "rok.unity_city_boundary.v1",
        "source_scene_root": rel(SCENE_ROOT),
        "boundary_instance_count": len(instances),
        "active_boundary_instance_count": sum(1 for item in instances if item["active"] is True),
        "inactive_boundary_instance_count": sum(1 for item in instances if item["active"] is False),
        "boundary_instances": instances,
        "boundary_assets": boundary_assets,
        "boundary_materials": materials,
        "ue_target_level": "/Game/Maps/L_RokUnityGameAppCity",
        "notes": [
            "This manifest is source evidence only. UE generation must preserve Unity active state unless a slice explicitly documents a runtime override.",
            "Do not accept hand-authored demo walls as migrated city-wall content when this manifest is available.",
        ],
    }
    return manifest


def write_markdown(manifest):
    lines = [
        "# Rok Unity City Boundary",
        "",
        f"- Schema: `{manifest['schema']}`",
        f"- Boundary instances: {manifest['boundary_instance_count']}",
        f"- Active instances: {manifest['active_boundary_instance_count']}",
        f"- Inactive instances: {manifest['inactive_boundary_instance_count']}",
        f"- Boundary assets: {len(manifest['boundary_assets'])}",
        f"- Boundary materials: {len(manifest['boundary_materials'])}",
        "",
        "## Instances",
        "",
        "| Scene | Name | Active | Source prefab | Position | Euler | Materials |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for item in manifest["boundary_instances"]:
        pos = item["local_position"]
        euler = item["local_euler_hint"]
        lines.append(
            "| `{scene}` | {name} | {active} | `{prefab}` | {x:.3f},{y:.3f},{z:.3f} | {ex:.1f},{ey:.1f},{ez:.1f} | {mat} |".format(
                scene=item["scene"],
                name=item["primary_name"],
                active=item["active"],
                prefab=item["source_prefab_path"],
                x=pos["x"],
                y=pos["y"],
                z=pos["z"],
                ex=euler["x"],
                ey=euler["y"],
                ez=euler["z"],
                mat=", ".join("`{0}`".format(path) for path in item["material_override_paths"] if path) or "-",
            )
        )
    lines.extend([
        "",
        "## Materials",
        "",
        "| Material | Main texture |",
        "| --- | --- |",
    ])
    for item in manifest["boundary_materials"]:
        lines.append(f"| `{item['path']}` | `{item['main_texture_path']}` |")
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    OUTPUT_JSON.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    write_markdown(manifest)
    print(
        "Rok Unity city boundary manifest generated: "
        f"instances={manifest['boundary_instance_count']} "
        f"active={manifest['active_boundary_instance_count']} "
        f"assets={len(manifest['boundary_assets'])}"
    )


if __name__ == "__main__":
    main()
