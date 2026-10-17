import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_modal_ui.json"
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_modal_ui_verify.json"


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
    manifest = {}
    if not MANIFEST_PATH.exists():
        failures.append("missing modal UI manifest: {0}".format(MANIFEST_PATH))
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    for label, relative_path in manifest.get("source_prefabs", {}).items():
        source_path = resolve(relative_path)
        if not source_path.exists():
            failures.append("missing source prefab {0}: {1}".format(label, source_path))

    for label, relative_path in manifest.get("source_runtime", {}).items():
        source_path = resolve(relative_path)
        if not source_path.exists():
            failures.append("missing source runtime {0}: {1}".format(label, source_path))

    import_script = resolve(manifest.get("ue_import_script", ""))
    if not import_script.exists():
        failures.append("missing modal UI import script: {0}".format(import_script))

    sprites = manifest.get("sprites", [])
    if len(sprites) < 45:
        failures.append("expected at least 45 modal sprite mappings, found {0}".format(len(sprites)))

    seen = set()
    for sprite in sprites:
        identity = (sprite.get("source_atlas"), sprite.get("sprite_name"))
        if identity in seen:
            failures.append("duplicate modal sprite mapping: {0}".format(identity))
        seen.add(identity)

        source_path = resolve(sprite.get("source_atlas", ""))
        meta_path = resolve(sprite.get("source_meta", ""))
        if not source_path.exists():
            failures.append("missing modal source atlas: {0}".format(source_path))
            continue
        if sha256_file(source_path) != sprite.get("source_atlas_sha256"):
            failures.append("modal source atlas hash drift: {0}".format(source_path))
        if not meta_path.exists():
            failures.append("missing modal source atlas metadata: {0}".format(meta_path))
        elif sha256_file(meta_path) != sprite.get("source_meta_sha256"):
            failures.append("modal source atlas metadata hash drift: {0}".format(meta_path))

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
            failures.append("out-of-bounds modal sprite rect: {0}".format(identity))

        target_path = content_asset_path(sprite.get("ue_atlas_target", ""))
        if target_path is None or not target_path.exists():
            failures.append("missing UE modal atlas target for {0}: {1}".format(identity, target_path))

    for texture in manifest.get("independent_textures", []):
        source_path = resolve(texture.get("source", ""))
        if not source_path.exists():
            failures.append("missing independent modal texture: {0}".format(source_path))
        elif sha256_file(source_path) != texture.get("source_sha256"):
            failures.append("independent modal texture hash drift: {0}".format(source_path))
        target_path = content_asset_path(texture.get("ue_target", ""))
        if target_path is None or not target_path.exists():
            failures.append("missing UE independent modal texture: {0}".format(target_path))

    layouts = manifest.get("layouts", {})
    if manifest.get("reference_resolution") != [1440, 810]:
        failures.append("unexpected modal reference resolution")
    if manifest.get("screen_match_mode") != "Expand":
        failures.append("unexpected modal CanvasScaler mode")
    if layouts.get("player_profile_window") != [133.5, 85.5, 1173.0, 729.0]:
        failures.append("player profile window geometry drift")
    if layouts.get("type_2_window") != [160.0, 100.0, 1120.0, 700.0]:
        failures.append("type-2 window geometry drift")

    checks.append({"name": "sprite_mapping_count", "value": len(sprites)})
    checks.append({"name": "unique_sprite_mapping_count", "value": len(seen)})
    checks.append({"name": "source_prefab_count", "value": len(manifest.get("source_prefabs", {}))})
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
