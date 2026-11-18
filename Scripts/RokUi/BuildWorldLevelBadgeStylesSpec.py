"""Prepare the shared world-map city/entity level plate's Designer art."""

import hashlib
import json
import struct
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
KEY = "world_level_badge_styles"
ASSET = "/Game/RokUI/Hud/WBP_RokWorldLevelBadgeStyles"
ROOT_NAME = "WorldLevelBadgeStylesRoot"
IMAGE_NAME = "LevelBadgeBackground"
TEXTURE = "/Game/RokUI/Hud/Textures/T_RokWorldLevelBadge"
SOURCE = "Content/RokData/WorldEntityPresentation/pl_map_levelBg.png"
SOURCE_SHA256 = "ad43cd2a314d71dd1a4a8ac974054432be7bf668c487c415b8a52049f66ed698"


def build():
    data = (ROOT / SOURCE).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    assert struct.unpack(">II", data[16:24]) == (60, 33)
    assert hashlib.sha256(data).hexdigest() == SOURCE_SHA256
    resource = f"{TEXTURE}.{TEXTURE.rsplit('/', 1)[1]}"
    return {
        "version": 1,
        "label": "Shared world-map city and entity level background art",
        "asset": ASSET,
        "root": ROOT_NAME,
        "texture_imports": [{"source": SOURCE, "asset": TEXTURE, "size": [60, 33]}],
        "nodes": [{
            "name": IMAGE_NAME,
            "class": "/Script/UMG.Image",
            "props": {
                "Visibility": "HitTestInvisible",
                "Brush": (
                    "(DrawAs=Image,ImageType=FullColor,ImageSize=(X=60,Y=33),"
                    f'ResourceObject="/Script/Engine.Texture2D\'{resource}\'",'
                    "UVRegion=(bIsValid=False))"
                ),
            },
            "slot": canvas_slot(0, 0, 60, 33),
        }],
        "notes": [
            "Create only this detached style page and its exact persistent PNG import; leave both existing projected marker WBPs unchanged.",
            "HUD setup caches the background resource and optional UV once. Both level plates keep their own slot, brush style, size and tint.",
            "Designer may use a Texture2D or UI material, with an optional finite nonempty UV region. Missing or invalid art and the legacy/native path retain the original PNG fallback.",
        ],
        "references": [
            "Source/Rok/RokWorldMapHudWidget.cpp: CacheLevelBadgeArt / BuildSelfMarker / BuildEntityBadgeLayout",
            "Scripts/BuildRokUnityWorldEntityPresentation.py: crop_badge_sprites",
            "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_map.png.meta: pl_map_levelBg (269,168,60,33)",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIWorldObjectPop/UI_Tip_WorldObjectRssLevel.prefab",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIWorldObjectPop/UI_Tip_WorldObjectPlayerLevel.prefab",
        ],
    }


if __name__ == "__main__":
    target = ROOT / "Saved/RokUi/Specs" / f"{KEY}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(target)
