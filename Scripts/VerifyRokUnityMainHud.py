import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_main_hud.json"
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_main_hud_verify.json"


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def content_asset_path(ue_target):
    if not ue_target.startswith("/Game/"):
        return None
    return PROJECT_ROOT / "Content" / (ue_target[len("/Game/"):] + ".uasset")


def main():
    failures = []
    checks = []
    if not MANIFEST_PATH.exists():
        failures.append("missing manifest: {0}".format(MANIFEST_PATH))
        manifest = {}
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    import_script = resolve(manifest.get("ue_import_script", ""))
    if not import_script.exists():
        failures.append("missing UE HUD atlas import script: {0}".format(import_script))

    sprites = manifest.get("sprites", [])
    if len(sprites) < 40:
        failures.append("expected at least 40 HUD sprite mappings, found {0}".format(len(sprites)))

    seen = set()
    for sprite in sprites:
        identity = (sprite.get("source_atlas"), sprite.get("sprite_name"))
        if identity in seen:
            failures.append("duplicate sprite mapping: {0}".format(identity))
        seen.add(identity)

        source_path = resolve(sprite.get("source_atlas", ""))
        if not source_path.exists():
            failures.append("missing source atlas: {0}".format(source_path))
            continue
        if sha256_file(source_path) != sprite.get("source_atlas_sha256"):
            failures.append("source atlas hash drift: {0}".format(source_path))

        meta_path = resolve(sprite.get("source_meta", ""))
        if not meta_path.exists():
            failures.append("missing source atlas metadata: {0}".format(meta_path))
        elif sha256_file(meta_path) != sprite.get("source_meta_sha256"):
            failures.append("source atlas metadata hash drift: {0}".format(meta_path))

        width, height = sprite.get("atlas_size", [0, 0])
        rect = sprite.get("rect", {})
        if (
            rect.get("x", -1) < 0
            or rect.get("y", -1) < 0
            or rect.get("width", 0) <= 0
            or rect.get("height", 0) <= 0
            or rect.get("x", 0) + rect.get("width", 0) > width
            or rect.get("y", 0) + rect.get("height", 0) > height
        ):
            failures.append("out-of-bounds sprite rect: {0}".format(identity))

        target_path = content_asset_path(sprite.get("ue_atlas_target", ""))
        if target_path is None or not target_path.exists():
            failures.append("missing UE atlas target for {0}: {1}".format(identity, target_path))

    layout = manifest.get("layout", {})
    if layout.get("reference_resolution") != [1440, 810]:
        failures.append("unexpected reference resolution")
    if layout.get("screen_match_mode") != "Expand":
        failures.append("unexpected CanvasScaler screen match mode")
    if layout.get("language_layout") != "non_arabic_mirrored":
        failures.append("non-Arabic layout mirror is not recorded")

    expected_anchors = {
        "resources": "top_right",
        "age": "top_right",
        "chat": "bottom_left",
        "search": "bottom_left",
        "city_world": "bottom_left",
        "features": "bottom_right",
        "mail": "bottom_right",
    }
    layout_groups = layout.get("groups", {})
    for group_name, expected_anchor in expected_anchors.items():
        actual_anchor = layout_groups.get(group_name, {}).get("anchor")
        if actual_anchor != expected_anchor:
            failures.append(
                "unexpected layout anchor for {0}: expected {1}, found {2}".format(
                    group_name,
                    expected_anchor,
                    actual_anchor,
                )
            )

    checks.append({"name": "sprite_mapping_count", "value": len(sprites)})
    checks.append({"name": "unique_sprite_mapping_count", "value": len(seen)})
    checks.append({"name": "layout_group_count", "value": len(layout_groups)})
    result = {
        "passed": not failures,
        "failure_count": len(failures),
        "failures": failures,
        "checks": checks,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(0 if result["passed"] else 1)


if __name__ == "__main__":
    main()
