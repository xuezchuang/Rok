import hashlib
import os
import re
import shutil
from pathlib import Path

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
        "btn_hud_1010",
        "rok_building_menu_speedup.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1026",
        "rok_building_menu_worker.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1016",
        "rok_building_menu_train_infantry.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1033",
        "rok_building_menu_train_cavalry.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1015",
        "rok_building_menu_train_archer.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1032",
        "rok_building_menu_train_siege.png",
    ),
    (
        Path("UITextureStatic") / "ui_res_map" / "ui_hud.png",
        "btn_hud_1025",
        "rok_building_menu_hospital.png",
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


# Exact Unity sprites used by UI_IF_MainInterface and the visible world-city
# nameplate in the 16:10 reference capture. These are deterministic atlas
# extractions; no artwork is synthesized by this pipeline.
MAIN_HUD_ATLAS_SPRITES = (
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1014", "rok_hud_player_bg.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1015", "rok_hud_player_arrow.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "icon_mmu_1001", "rok_hud_power_icon.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "pb_mmu_1000", "rok_hud_power_fill.png"),
    (Path("UITextureStatic") / "ui_res_player" / "vip_icon.png", "vip_icon_100", "rok_hud_vip_icon.png"),
    (Path("UITextureStatic") / "ui_res_player" / "player_head.png", "img_player_frame", "rok_hud_player_frame.png"),
    (Path("UITextureStatic") / "ui_res_player" / "player_head.png", "player_head_1000", "rok_hud_player_head_1000.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "icon_mmu_1003_3", "rok_hud_position_search.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "icon_mmu_1007", "rok_hud_favorite.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "pl_mmu_1001", "rok_hud_panel_bg.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_common.png", "img_com_cur1", "rok_hud_food.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_common.png", "img_com_cur2", "rok_hud_wood.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_common.png", "img_com_cur3", "rok_hud_stone.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_common.png", "img_com_cur4", "rok_hud_gold.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_common.png", "img_com_cur5", "rok_hud_gem.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1010", "rok_hud_add.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1012", "rok_hud_task_entry.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "icon_mmu_1006", "rok_hud_task_arrow.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_common.png", "pl_com_1001", "rok_hud_task_row.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "img_mmu_box", "rok_hud_task_reward.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_common.png", "img_com_redpoint", "rok_hud_redpoint.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_common.png", "img_com_redpot", "rok_hud_redpot.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "pl_mmu_1000", "rok_hud_chat_bar.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "icon_mmu_1000", "rok_hud_chat.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "pl_mmu_1023_1", "rok_hud_chat_line_1.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "pl_mmu_1023_2", "rok_hud_chat_line_2.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1006", "rok_hud_search.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1001", "rok_hud_build.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1017_2", "rok_hud_city.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1017_1", "rok_hud_world.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "pl_mmu_1019", "rok_hud_feature_bg.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "pl_mmu_1017", "rok_hud_feature_label.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1007", "rok_hud_campaign.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1002", "rok_hud_alliance.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1009", "rok_hud_items.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1004", "rok_hud_commander.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1000", "rok_hud_more.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "btn_mmu_1005", "rok_hud_mail.png"),
    (Path("UITextureStatic") / "ui_res_common" / "ui_mainmenu.png", "icon_mmu_activity", "rok_hud_activity.png"),
    (Path("UITextureStatic") / "ui_res_map" / "ui_map.png", "pl_map_buildingNameBg", "rok_hud_city_name_bg.png"),
    (Path("UITextureStatic") / "ui_res_map" / "ui_map.png", "pl_map_levelBg", "rok_hud_city_level_bg.png"),
)


MAIN_HUD_LAYOUT = {
    "reference_resolution": [1440, 810],
    "screen_match_mode": "Expand",
    "reference_capture_expanded_resolution": [1440, 900],
    "language_layout": "non_arabic_mirrored",
    "groups": {
        "player_power": {"position": [0, 0], "size": [302, 125], "anchor": "top_left"},
        "position": {"position": [298, 5], "size": [280, 50], "anchor": "top_left"},
        "resources": {"position": [980, 10], "size": [440, 42], "anchor": "top_right"},
        "age": {"position": [1045, 62], "size": [375, 58], "anchor": "top_right"},
        "hot_tasks": {"position": [0, 136], "size": [232, 381], "anchor": "top_left"},
        "chat": {"position": [0, 809], "size": [768, 91], "anchor": "bottom_left"},
        "search": {"position": [18, 650], "size": [92, 92], "anchor": "bottom_left"},
        "city_world": {"position": [0, 752], "size": [123, 148], "anchor": "bottom_left"},
        "features": {"position": [870, 793], "size": [570, 107], "anchor": "bottom_right"},
        "mail": {"position": [1324, 680], "size": [100, 100], "anchor": "bottom_right"},
    },
}


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


def unity_sprite_vector(meta_text, sprite_name, property_name, component_names):
    sprite_start = re.search(
        r"(?m)^    - serializedVersion: 2\s*\r?\n      name: " + re.escape(sprite_name) + r"\s*$",
        meta_text,
    )
    if not sprite_start:
        raise RuntimeError("Missing Unity atlas sprite metadata: {0}".format(sprite_name))
    next_sprite = re.search(r"(?m)^    - serializedVersion: 2\s*$", meta_text[sprite_start.end():])
    sprite_end = sprite_start.end() + next_sprite.start() if next_sprite else len(meta_text)
    sprite_block = meta_text[sprite_start.start():sprite_end]
    vector_match = re.search(
        re.escape(property_name) + r": \{" +
        r", ".join(re.escape(name) + r": ([-+0-9.eE]+)" for name in component_names) +
        r"\}",
        sprite_block,
    )
    if not vector_match:
        raise RuntimeError(
            "Missing Unity atlas sprite {0} metadata for {1}".format(property_name, sprite_name)
        )
    return [float(value) for value in vector_match.groups()]


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


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
    output_path = OUTPUT_ROOT / output_name
    crop.save(output_path)


def main():
    global Image
    from PIL import Image as PillowImage

    Image = PillowImage
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

    for atlas_path, sprite_name, output_name in MAIN_HUD_ATLAS_SPRITES:
        save_atlas_sprite(atlas_path, sprite_name, output_name)

    outputs = sorted(path.name for path in OUTPUT_ROOT.glob("rok_*.png"))
    print("Rok UI processed textures:", ", ".join(outputs))


if __name__ == "__main__":
    main()
