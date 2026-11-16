"""Extract the authored Expedition map and UI sprites from the Unity prefabs' atlases."""

from pathlib import Path
import shutil

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic"
OUTPUT = ROOT / "Content/RokData/Expedition"

SPRITES = {
    "ui_res_battle/ui_battle": {
        "btn_bat_back", "btn_bat_rank", "btn_bat_rule", "btn_bat_shop",
        "btn_bat_frame", "img_bat_box", "img_bat_first",
        "img_bat_flag1", "img_bat_flag2", "img_bat_flag4",
        "img_bat_flagfra", "img_bat_flagfra2", "img_bat_queue1", "img_bat_queue2",
        "img_bat_trop3", "img_bat_trop4",
    },
    "ui_res_common/ui_common": {"icon_com_star1", "icon_com_star2", "img_com_cur8"},
    "ui_res_hero/hero_background": {"hero_background_14"},
}


def build():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    battle = SOURCE / "ui_res_battle"
    for name in ("img_bg_battle_frame1.png", *(f"img_bg_battle_map{i}.jpg" for i in range(1, 4))):
        shutil.copyfile(battle / name, OUTPUT / name)
    # UIEffect/UI_10047.prefab uses this authored glow at the selected flag.
    effect = SOURCE.parent.parent / "Effect/UIsprite/shared_uisprite/glow005.png"
    shutil.copyfile(effect, OUTPUT / "selected_glow.png")
    for path, requested in SPRITES.items():
        atlas = SOURCE / path
        image = Image.open(atlas.with_suffix(".png")).convert("RGBA")
        remaining = set(requested)
        for line in atlas.with_suffix(".tpsheet").read_text(encoding="utf-8-sig").splitlines():
            name, *fields = line.split(";")
            if name not in remaining:
                continue
            x, y, width, height = map(int, fields[:4])
            top = image.height - y - height
            image.crop((x, top, x + width, top + height)).save(OUTPUT / f"{name}.png")
            remaining.remove(name)
        if remaining:
            raise RuntimeError(f"{atlas}: missing sprites {sorted(remaining)}")


if __name__ == "__main__":
    build()
