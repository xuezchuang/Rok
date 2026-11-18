"""Prepare the editable brush states for expedition stages and teams."""

import json
from pathlib import Path

from BuildExpeditionWbpSpecs import image


ROOT = Path(__file__).resolve().parents[2]
SPEC = "expedition_state_brushes"
ASSET = "/Game/RokUI/Feature/WBP_RokExpeditionStateBrushes"
STATES = (
    ("StageFlagType1", "img_bat_flag1.png", (61, 87)),
    ("StageFlagType2", "img_bat_flag2.png", (61, 87)),
    ("StageFlagLocked", "img_bat_flag4.png", (61, 87)),
    ("StageBaseType1", "img_bat_flagfra.png", (49, 40)),
    ("StageBaseType2", "img_bat_flagfra2.png", (49, 40)),
    ("StageSelectedGlow", "selected_glow.png", (144, 144)),
    ("StarEmpty", "icon_com_star1.png", (26, 26)),
    ("StarFull", "icon_com_star2.png", (26, 26)),
    ("CapacityAvailable", "img_bat_queue1.png", (16, 40)),
    ("CapacityLocked", "img_bat_queue2.png", (16, 40)),
    ("SquadAvailable", "img_bat_trop3.png", (78, 88)),
    ("SquadLocked", "img_bat_trop4.png", (78, 88)),
)


def build():
    return {
        "version": 1,
        "label": "Expedition stage and team brush states",
        "root": "ExpeditionStateBrushesRoot",
        "nodes": [
            image(name, (index % 5 * 166, index // 5 * 166, *size), asset=texture)
            for index, (name, texture, size) in enumerate(STATES)
        ],
        "notes": [
            "The existing stage row, map and task pages still own geometry and image-level color.",
            "Preview dimensions show the brush variants. Runtime copies the brush while retaining the target image size.",
            "C++ selects the brush from unlock, stage type, selection, stars and capacity state.",
            "Missing markers or textures retain the original per-image fallback.",
        ],
        "references": [
            "Source/Rok/RokUnityExpeditionScreen.cpp",
            "Scripts/RokUi/ImportExpeditionWbpTextures.py",
            "Content/RokData/Expedition",
        ],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
