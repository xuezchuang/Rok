import hashlib
import json
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_commander_ui.json"
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_commander_ui_verify.json"


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def content_asset_path(target):
    if not target.startswith("/Game/"):
        return None
    return PROJECT_ROOT / "Content" / (target[len("/Game/"):] + ".uasset")


def main():
    failures = []
    manifest = {}
    if not MANIFEST_PATH.exists():
        failures.append("missing commander manifest")
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    for key in ("source_prefab", "source_head_prefab", "source_mediator", "source_hero_table", "ue_widget", "ue_hud", "ue_import_script"):
        path = resolve(manifest.get(key, ""))
        if not path.exists():
            failures.append("missing {0}: {1}".format(key, path))

    hero = manifest.get("selected_hero", {})
    if hero.get("id") != 2007 or hero.get("name") != "拜巴尔一世" or hero.get("appellation") != "天选之豹":
        failures.append("selected commander identity drift")
    if hero.get("skills") != [200701, 200702, 200703, 200704, 200705]:
        failures.append("selected commander skill list drift")

    portrait = manifest.get("portrait", {})
    for key in ("source_skeleton", "source_texture", "generated_png"):
        path = resolve(portrait.get(key, ""))
        if not path.exists():
            failures.append("missing portrait {0}: {1}".format(key, path))
    portrait_png = resolve(portrait.get("generated_png", ""))
    if portrait_png.exists() and sha256_file(portrait_png) != portrait.get("generated_png_sha256"):
        failures.append("commander portrait hash drift")
    portrait_target = content_asset_path(portrait.get("ue_target", ""))
    if portrait_target is None or not portrait_target.exists():
        failures.append("missing imported commander portrait: {0}".format(portrait_target))

    required = {
        "hero_background_10",
        "hero_head_2007",
        "hero_head_4000",
        "hero_head_1006",
        "img_com_heroframe4",
        "img_com_heroframe5",
        "img_hero_rare4",
        "hero_skill_200701",
        "hero_skill_200702",
        "hero_skill_200703",
        "hero_skill_200704",
        "hero_skill_200705",
    }
    sprites = manifest.get("sprites", [])
    mapped = {row.get("sprite_name") for row in sprites}
    missing = sorted(required - mapped)
    if missing:
        failures.append("missing required commander sprites: {0}".format(", ".join(missing)))
    for row in sprites:
        source = resolve(row.get("source_atlas", ""))
        sheet = resolve(row.get("source_tpsheet", ""))
        if not source.exists() or sha256_file(source) != row.get("source_atlas_sha256"):
            failures.append("commander atlas drift: {0}".format(source))
        if not sheet.exists() or sha256_file(sheet) != row.get("source_tpsheet_sha256"):
            failures.append("commander tpsheet drift: {0}".format(sheet))
        x, y, width, height = row.get("rect", [0, 0, 0, 0])
        atlas_width, atlas_height = row.get("atlas_size", [0, 0])
        if x < 0 or y < 0 or width <= 0 or height <= 0 or x + width > atlas_width or y + height > atlas_height:
            failures.append("out-of-bounds commander sprite: {0}".format(row.get("sprite_name")))
        target = content_asset_path(row.get("ue_atlas_target", ""))
        if target is None or not target.exists():
            failures.append("missing imported commander atlas: {0}".format(target))

    result = {
        "passed": not failures,
        "failure_count": len(failures),
        "failures": failures,
        "checks": {
            "sprite_mapping_count": len(sprites),
            "selected_hero_id": hero.get("id"),
            "portrait_animation": portrait.get("animation"),
        },
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(0 if result["passed"] else 1)


if __name__ == "__main__":
    main()
