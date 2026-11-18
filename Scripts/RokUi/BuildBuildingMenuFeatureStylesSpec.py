"""Author the command icon resources used by the city building menu."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
ASSET = "/Game/RokUI/Building/WBP_RokBuildingMenuFeatureStyles"
SPEC = "building_menu_feature_styles"
ROOT_NAME = "BuildingMenuFeatureStylesRoot"
PLACEMENT_ATLAS = "/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.rok_placement_ui_hud"
MENU = "/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_"
HUD_SHEET = Path("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.tpsheet")

TEXTURES = {
    "FeatureDefault": "feature",
    "FeatureRemove": "remove",
    "FeatureSpeedUp": "speedup",
    "FeatureWorker": "worker",
    "FeatureInfantry": "train_infantry",
    "FeatureCavalry": "train_cavalry",
    "FeatureArcher": "train_archer",
    "FeatureSiege": "train_siege",
    "FeatureHospital": "hospital",
    "FeatureScout": "scout",
    "FeatureResearch": "research",
}
RESOURCE_SPRITES = {
    "FeatureFood": ("btn_hud_1021", 531, 755),
    "FeatureWood": ("btn_hud_1027", 504, 845),
    "FeatureStone": ("btn_hud_1029", 774, 935),
    "FeatureGold": ("btn_hud_1022", 441, 745),
}


def brush(texture, crop=None):
    fields = ["DrawAs=Image", "ImageType=FullColor", "ImageSize=(X=88,Y=88)",
              f'ResourceObject="/Script/Engine.Texture2D\'{texture}\'"']
    if crop is not None:
        x, y = crop
        top = 1024 - y - 88
        fields.append(
            f"UVRegion=(Min=(X={x / 1024:.12f},Y={top / 1024:.12f}),"
            f"Max=(X={(x + 88) / 1024:.12f},Y={(top + 88) / 1024:.12f}),bIsValid=True)"
        )
    return "(" + ",".join(fields) + ")"


def build():
    styles = {name: brush(f"{MENU}{suffix}.rok_building_menu_{suffix}") for name, suffix in TEXTURES.items()}
    styles.update({name: brush(PLACEMENT_ATLAS, (x, y)) for name, (_, x, y) in RESOURCE_SPRITES.items()})
    return {
        "version": 1,
        "label": "City building menu function icon resources",
        "root": ROOT_NAME,
        "nodes": [
            {
                "name": name,
                "class": "/Script/UMG.Image",
                "props": {"Brush": value, "Visibility": "HitTestInvisible"},
                "slot": canvas_slot(index % 5 * 104, index // 5 * 104, 88, 88),
            }
            for index, (name, value) in enumerate(styles.items())
        ],
        "notes": [
            "Each marker supplies ResourceObject and UVRegion. The live BuildingFeatureButtonImage owns its size, brush style and tint.",
            "Standalone button textures use the full resource. Resource-building buttons use their original ui_hud crops.",
            "Building types and upgrade state select markers. Missing markers and the native path retain the previous texture selection.",
        ],
        "references": [
            "Source/Rok/RokBuildingPanelWidget.cpp: ApplyFeatureButtonVisual / ApplyMenuLayout",
            "ref/config-tables/编译客户端配置/CSV/BuildingMenuData.csv",
            HUD_SHEET.as_posix(),
            "Scripts/PrepareRokUiTextures.py: BUILDING_MENU_ATLAS_SPRITES",
            "Scripts/BuildRokScoutMenuTexture.py",
            "Scripts/PrepareRokResearchTextures.py",
        ],
    }


def verify_source_crops():
    sheet = ROOT / HUD_SHEET
    if sheet.is_file():
        rows = [line.split(";") for line in sheet.read_text(encoding="utf-8-sig").splitlines()]
        names = {sprite for sprite, _, _ in RESOURCE_SPRITES.values()}
        rects = {row[0]: tuple(map(int, row[1:5])) for row in rows if row[0] in names}
        for sprite, x, y in RESOURCE_SPRITES.values():
            assert rects[sprite] == (x, y, 88, 88), sprite


if __name__ == "__main__":
    verify_source_crops()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={ROOT_NAME}")
