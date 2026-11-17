"""Build editable WBP specs for expedition source errors and empty chapter dialogue."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Saved" / "RokUi" / "Specs"
TEXT_CLASS = "/Script/Rok.RokUiTextBlock"
CANVAS_SLOT = "/Script/UMG.CanvasPanelSlot"


def text_node(name: str, text: str, rect: tuple[int, int, int, int], size: int,
              color: tuple[float, float, float, float]) -> dict:
    x, y, width, height = rect
    r, g, b, a = color
    return {
        "name": name,
        "class": TEXT_CLASS,
        "props": {
            "Text": text,
            "Font": (
                '(OutlineSettings=(OutlineColor=(R=0.000000,G=0.000000,B=0.000000,A=1.000000)),'
                f'TypefaceFontName="Regular",Size={size:.6f},MonospacedWidth=1.000000)'
            ),
            "ColorAndOpacity": f"(SpecifiedColor=(R={r:.6f},G={g:.6f},B={b:.6f},A={a:.6f}))",
            "ShadowColorAndOpacity": "(R=0.000000,G=0.000000,B=0.000000,A=0.650000)",
            "ShadowOffset": "(X=1.000000,Y=1.000000)",
            "Justification": "ETextJustify::Center",
            "AutoWrapText": "True",
            "Visibility": "HitTestInvisible",
            "FontRole": "EngineDefault",
        },
        "slot": {
            "class": CANVAS_SLOT,
            "props": {
                "LayoutData": (
                    f"(Offsets=(Left={x:.6f},Top={y:.6f},Right={width:.6f},Bottom={height:.6f}),"
                    "Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),"
                    "Alignment=(X=0.000000,Y=0.000000))"
                ),
                "ZOrder": "1",
            },
        },
    }


def write(name: str, label: str, root: str, node: dict, notes: list[str]) -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    payload = {"version": 1, "label": label, "root": root, "nodes": [node], "notes": notes}
    (OUTPUT / f"{name}.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def main() -> None:
    write(
        "expedition_source_error",
        "Editable expedition source error state",
        "ExpeditionSourceErrorRoot",
        text_node("ExpeditionSourceErrorText", "远征配置暂不可用", (240, 340, 960, 60), 20,
                  (1.0, 1.0, 1.0, 1.0)),
        ["C++ supplies the current source error; the centered text style and placement are editable."],
    )
    write(
        "chapter_dialog_empty",
        "Editable empty chapter dialogue state",
        "ChapterDialogEmptyRoot",
        text_node("ChapterDialogEmptyText", "对话组 0 在 TaskDialog.csv 中没有可显示的台词",
                  (212, 400, 900, 60), 16, (0.38, 0.31, 0.22, 1.0)),
        ["C++ supplies the selected dialogue group number; the legacy fallback remains available."],
    )


if __name__ == "__main__":
    main()
