import json
from pathlib import Path

from PrepareRokUiTextures import (
    MAIN_HUD_ATLAS_SPRITES,
    MAIN_HUD_LAYOUT,
    PROJECT_ROOT,
    SOURCE_ROOT,
    sha256_file,
    unity_sprite_rect,
    unity_sprite_vector,
)


OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_main_hud.json"

ATLAS_TARGETS = {
    "UITextureStatic/ui_res_common/ui_mainmenu.png": {
        "size": [1024, 1024],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu",
    },
    "UITextureStatic/ui_res_common/ui_common.png": {
        "size": [2048, 1024],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common",
    },
    "UITextureStatic/ui_res_player/player_head.png": {
        "size": [512, 2048],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_player_head",
    },
    "UITextureStatic/ui_res_player/vip_icon.png": {
        "size": [512, 1024],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip",
    },
    "UITextureStatic/ui_res_map/ui_map.png": {
        "size": [1024, 512],
        "ue_target": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map",
    },
}


def normalized_uv(rect, atlas_size):
    x, unity_y, width, height = rect
    atlas_width, atlas_height = atlas_size
    top = atlas_height - unity_y - height
    return {
        "min": [x / atlas_width, top / atlas_height],
        "max": [(x + width) / atlas_width, (top + height) / atlas_height],
    }


def main():
    sprites = []
    for relative_atlas, sprite_name, optional_crop_name in MAIN_HUD_ATLAS_SPRITES:
        atlas_key = relative_atlas.as_posix()
        atlas_config = ATLAS_TARGETS.get(atlas_key)
        if not atlas_config:
            raise RuntimeError("Missing UE atlas target mapping: {0}".format(atlas_key))

        atlas_path = SOURCE_ROOT / relative_atlas
        meta_path = Path(str(atlas_path) + ".meta")
        if not atlas_path.exists() or not meta_path.exists():
            raise FileNotFoundError("Missing Unity HUD atlas source: {0}".format(atlas_path))

        meta_text = meta_path.read_text(encoding="utf-8-sig")
        rect = unity_sprite_rect(meta_text, sprite_name)
        sprites.append(
            {
                "source_atlas": atlas_path.relative_to(PROJECT_ROOT).as_posix(),
                "source_meta": meta_path.relative_to(PROJECT_ROOT).as_posix(),
                "source_atlas_sha256": sha256_file(atlas_path),
                "source_meta_sha256": sha256_file(meta_path),
                "sprite_name": sprite_name,
                "rect": {
                    "x": rect[0],
                    "y": rect[1],
                    "width": rect[2],
                    "height": rect[3],
                },
                "pivot": unity_sprite_vector(meta_text, sprite_name, "pivot", ("x", "y")),
                "border": unity_sprite_vector(meta_text, sprite_name, "border", ("x", "y", "z", "w")),
                "atlas_size": atlas_config["size"],
                "normalized_uv": normalized_uv(rect, atlas_config["size"]),
                "ue_atlas_target": atlas_config["ue_target"],
                "optional_crop_name": optional_crop_name,
            }
        )

    manifest = {
        "version": 1,
        "source_scene": "ref/client-unity/Assets/BundleAssets/Scenes/GameApp.unity",
        "source_prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIMainCity/UI_IF_MainInterface.prefab",
        "source_view": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/MainInterface/MainInterfaceView.cs",
        "source_mediator": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/MainInterface/MainInterfaceMediator.cs",
        "source_canvas": "ref/client-unity/Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab",
        "source_world_nameplate": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIWorldObjectPop/UI_Tip_WorldObjectPlayerLevel.prefab",
        "ue_import_script": "Scripts/ImportRokUnityMainHudAtlases.py",
        "ue_texture_settings": {
            "srgb": True,
            "mip_gen_settings": "TMGS_NoMipmaps",
            "lod_group": "TEXTUREGROUP_UI",
            "never_stream": True,
            "compression_settings": "TC_EditorIcon",
            "address_x": "TA_Clamp",
            "address_y": "TA_Clamp",
        },
        "layout": MAIN_HUD_LAYOUT,
        "atlases": ATLAS_TARGETS,
        "sprites": sprites,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Rok Unity main HUD manifest:", OUTPUT_PATH)
    print("Rok Unity main HUD sprites:", len(sprites))


if __name__ == "__main__":
    main()
