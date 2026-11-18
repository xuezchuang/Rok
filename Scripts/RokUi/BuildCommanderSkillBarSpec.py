"""Author the host position, skill-cell bounds and repeat step."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
SPEC = "commander_skill_bar"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_CommanderSkillBar"


def build():
    host_slot = canvas_slot(978, 468, 404, 74)
    host_slot["props"]["ZOrder"] = "7"
    return {
        "version": 1,
        "label": "Commander skill bar layout",
        "root": "CommanderSkillBarRoot",
        "nodes": [{
            "name": "CommanderSkillHost",
            "class": "/Script/UMG.CanvasPanel",
            "props": {"Visibility": "SelfHitTestInvisible"},
            "slot": host_slot,
            "children": [
                {
                    "name": name,
                    "class": "/Script/UMG.Border",
                    "props": {"BrushColor": color, "Visibility": "HitTestInvisible"},
                    "slot": canvas_slot(x, 0, 74, 74),
                }
                for name, x, color in (
                    ("CommanderSkillCell", 0, "(R=.20,G=.40,B=.55,A=.65)"),
                    ("CommanderSkillNext", 82.5, "(R=.55,G=.40,B=.20,A=.65)"),
                )
            ],
        }],
        "notes": [
            "The host is positioned in the 1440x810 view. The former capture coordinates used a 45-unit vertical inset.",
            "Cell supplies the first item layout. Next minus Cell supplies the step between skills.",
            "The populated host hides both markers. The skill-item WBP retains its content and actions.",
        ],
        "references": ["Source/Rok/RokFeatureCommander.inl", "HeroSkill"],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
