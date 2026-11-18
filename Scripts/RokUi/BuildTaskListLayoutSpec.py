"""Author task row extents without regenerating existing task pages or rows."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
SPEC = "task_list_layouts"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_TaskListLayouts"
ROWS = ("DailyServerRow", "MainServerRow", "SideServerRow", "MainLocalRow", "SideLocalRow")


def build():
    return {
        "version": 1,
        "label": "Task list row extents",
        "root": "TaskListLayoutsRoot",
        "nodes": [{
            "name": name,
            "class": "/Script/UMG.Border",
            "props": {"BrushColor": "(R=.20,G=.40,B=.55,A=.65)"},
            "slot": canvas_slot(0, index * 166, 1088, 150),
        } for index, name in enumerate(ROWS)],
        "notes": [
            "Detached layout only. Each marker's fixed Canvas slot supplies the corresponding row width and height.",
            "The ScrollBox or VerticalBox stacks these fixed extents, so row height also supplies the repeat stride.",
            "Marker positions arrange samples in Designer and are not consumed at runtime.",
            "Server main and side tasks, and local main and fixture side tasks, have independent extents.",
            "Daily catalog rows retain the size authored in their existing row WBP.",
            "Existing page/row style, task state, rewards and actions remain unchanged. Native rows or missing/invalid markers retain 1088x150.",
        ],
        "references": [
            "Source/Rok/RokFeatureTaskListLayout.cpp",
            "Source/Rok/RokUnityFeatureScreenWidget.cpp: AddServerDailyTaskRows, BuildTasks",
        ],
    }


if __name__ == "__main__":
    data = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
