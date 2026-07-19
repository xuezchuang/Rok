import os
import re
import shutil
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "ref" / "client-unity" / "Assets" / "BundleAssets" / "UI"
OUTPUT_ROOT = PROJECT_ROOT / "Saved" / "RokUiProcessed"

BUILDING_MENU_ATLAS_SPRITES = (
    (
        Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png",
        "pl_mmu_1020",
        "rok_building_menu_title.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png",
        "pl_mmu_1021",
        "rok_building_menu_level_bg.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1038",
        "rok_building_menu_info.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1011",
        "rok_building_menu_upgrade.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1036",
        "rok_building_menu_feature.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1005",
        "rok_building_menu_remove.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "pl_hud_1000",
        "rok_building_menu_button_shadow.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "Arrow_down",
        "rok_building_select_arrow_down.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "Arrow_left",
        "rok_building_select_arrow_left.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "Arrow_right",
        "rok_building_select_arrow_right.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "Arrow_up",
        "rok_building_select_arrow_up.png",
    ),
)


def alpha_fit_to_canvas(image, canvas_size, max_fraction=0.92):
    image = image.convert("RGBA")
    alpha_bbox = image.getchannel("A").getbbox()
    if alpha_bbox:
        image = image.crop(alpha_bbox)

    max_width = int(canvas_size[0] * max_fraction)
    max_height = int(canvas_size[1] * max_fraction)
    scale = min(max_width / image.width, max_height / image.height, 1.0)
    resized_size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    image = image.resize(resized_size, Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
    offset = ((canvas_size[0] - image.width) // 2, (canvas_size[1] - image.height) // 2)
    canvas.alpha_composite(image, offset)
    return canvas


def open_ui(relative_path):
    source_path = SOURCE_ROOT / relative_path
    if not source_path.exists():
        raise FileNotFoundError(f"Missing UI source texture: {source_path}")
    return Image.open(source_path).convert("RGBA")


def save_rotated_bar(relative_path, output_name):
    image = open_ui(relative_path)
    rotated = image.transpose(Image.Transpose.ROTATE_90)
    rotated.save(OUTPUT_ROOT / output_name)


def save_resource_icon(relative_path, output_name, crop_top_frame):
    image = open_ui(relative_path)
    if crop_top_frame:
        image = image.crop((0, 0, 256, 256))
    alpha_fit_to_canvas(image, (64, 64)).save(OUTPUT_ROOT / output_name)


def save_upgrade_icon():
    source = SOURCE_ROOT / "UITextureStatic" / "ui_res_common" / "city_build_upgrad.png"
    target = OUTPUT_ROOT / "rok_upgrade_icon.png"
    if not source.exists():
        raise FileNotFoundError(f"Missing UI source texture: {source}")
    image = Image.open(source).convert("RGBA")
    if image.size == (100, 100):
        shutil.copyfile(source, target)
    else:
        alpha_fit_to_canvas(image, (100, 100), max_fraction=1.0).save(target)


def unity_sprite_rect(meta_text, sprite_name):
    pattern = re.compile(
        r"(?ms)^    - serializedVersion: 2\s+"
        r"name: " + re.escape(sprite_name) + r"\s+"
        r"rect:\s+serializedVersion: 2\s+"
        r"x: ([-+0-9.eE]+)\s+"
        r"y: ([-+0-9.eE]+)\s+"
        r"width: ([-+0-9.eE]+)\s+"
        r"height: ([-+0-9.eE]+)"
    )
    match = pattern.search(meta_text)
    if not match:
        raise RuntimeError("Missing Unity atlas sprite metadata: {0}".format(sprite_name))
    return tuple(int(round(float(value))) for value in match.groups())


def save_atlas_sprite(relative_atlas_path, sprite_name, output_name):
    atlas_path = SOURCE_ROOT / relative_atlas_path
    meta_path = Path(str(atlas_path) + ".meta")
    if not atlas_path.exists() or not meta_path.exists():
        raise FileNotFoundError("Missing Unity UI atlas source: {0}".format(atlas_path))
    atlas = Image.open(atlas_path).convert("RGBA")
    x, unity_y, width, height = unity_sprite_rect(
        meta_path.read_text(encoding="utf-8-sig"),
        sprite_name,
    )
    # Unity sprite metadata uses a bottom-left texture origin; Pillow crops
    # from the top-left.
    top = atlas.height - unity_y - height
    crop = atlas.crop((x, top, x + width, top + height))
    if crop.size != (width, height):
        raise RuntimeError("Unexpected crop size for Unity UI sprite {0}".format(sprite_name))
    crop.save(OUTPUT_ROOT / output_name)


def main():
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)

    save_rotated_bar(Path("Bang") / "main_top.png", "rok_main_top.png")
    save_rotated_bar(Path("Bang") / "main_bottom.png", "rok_main_bottom.png")
    save_resource_icon(Path("UIEffect") / "UE_ResFly_food" / "UE_ResFly_food.png", "rok_food_icon.png", True)
    save_resource_icon(Path("UIEffect") / "UE_ResFly_wood" / "UE_ResFly_wood.png", "rok_wood_icon.png", True)
    save_resource_icon(Path("UIEffect") / "UE_ResFly_stone" / "UE_ResFly_stone.png", "rok_stone_icon.png", True)
    save_resource_icon(Path("UIEffect") / "UE_ResFly_gold" / "UE_ResFly_gold.png", "rok_gold_icon.png", False)
    save_upgrade_icon()
    for atlas_path, sprite_name, output_name in BUILDING_MENU_ATLAS_SPRITES:
        save_atlas_sprite(atlas_path, sprite_name, output_name)

    outputs = sorted(path.name for path in OUTPUT_ROOT.glob("rok_*.png"))
    print("Rok UI processed textures:", ", ".join(outputs))


if __name__ == "__main__":
    main()
