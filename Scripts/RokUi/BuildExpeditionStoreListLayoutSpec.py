"""Author the expedition store's single-column searchable list geometry."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
SPEC = "expedition_store_list_layout"
ASSET = "/Game/RokUI/Modal/Pages/WBP_RokModal_ExpeditionStoreListLayout"


def build():
    return {
        "version": 1,
        "label": "Expedition store list layout",
        "root": "ExpeditionStoreListLayoutRoot",
        "nodes": [
            {
                "name": name,
                "class": "/Script/UMG.Border",
                "props": {"BrushColor": color},
                "slot": canvas_slot(*bounds),
            }
            for name, bounds, color in (
                ("RowExtent", (740, 0, 700, 76), "(R=.20,G=.40,B=.55,A=.65)"),
                ("FirstRow", (0, 0, 700, 76), "(R=.20,G=.55,B=.30,A=.65)"),
                ("NextRow", (0, 84, 700, 76), "(R=.55,G=.40,B=.20,A=.65)"),
                ("ScrollPadding", (740, 100, 12, 8), "(R=.55,G=.20,B=.40,A=.85)"),
            )
        ],
        "notes": [
            "RowExtent size sets each outer row size; the row WBP still owns its inner layout and style.",
            "FirstRow position sets the first row origin inside the existing search list's scroll content.",
            "The Y distance from FirstRow to NextRow sets the vertical step. NextRow X and marker sizes are samples only.",
            "ScrollPadding size adds right and bottom space after the rows. Its position is a Designer sample only.",
            "Only the expedition store's normal single-column DefinitionListSearch WBP route reads these markers.",
            "Defaults preserve 700x76 rows, origin 0,0, an 84-pixel step, and right/bottom padding 12,8.",
            "All four markers must be valid; missing or invalid geometry retains the original layout.",
        ],
        "references": ["Source/Rok/RokUnityModalWidget.cpp: BuildDefinitionList and PopulateDefinitionRows"],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
