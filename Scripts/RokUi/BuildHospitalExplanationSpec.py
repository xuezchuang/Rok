"""Create the small static WBP tree for the runtime-bound hospital explanation."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Content" / "RokData" / "hospital_explanation.json"
OUTPUT = ROOT / "Saved" / "RokUi" / "Specs" / "hospital_explanation.json"


def slot(slot_class: str, properties: dict) -> dict:
    return {"class": slot_class, "props": properties}


def widget(name: str, class_path: str, children=None, widget_slot=None, properties=None, fonts=None) -> dict:
    value = {"name": name, "class": class_path, "props": properties or {}}
    if children is not None:
        value["children"] = children
    if widget_slot is not None:
        value["slot"] = widget_slot
    if fonts:
        value["fonts"] = fonts
    return value


def build() -> dict:
    source = json.loads(SOURCE.read_text(encoding="utf-8"))
    if source.get("schema") != "rok.hospital.explanation.v1" or source.get("language_id") != 181001:
        raise SystemExit("hospital explanation source schema or language id changed")
    nodes = source.get("nodes", {})
    page_key = "Rect/Right/pl_explain"
    text_key = page_key + "/sv_explain_view/v_explain_view/c_explain_view/lbl_explain"
    page = nodes.get(page_key, {}).get("rect")
    label = nodes.get(text_key, {}).get("rect")
    if not page or not label or len(page) != 4 or len(label) != 4:
        raise SystemExit("hospital explanation source viewport/text rectangle missing")
    if any(float(value) <= 0 for value in (page[2], page[3], label[2], label[3])):
        raise SystemExit("hospital explanation source viewport/text size is not positive")
    left_inset = float(label[0]) - float(page[0])
    if left_inset < 0 or left_inset + float(label[2]) > float(page[2]) + 0.01:
        raise SystemExit("hospital explanation text exceeds its source viewport")

    scroll_layout = (
        "(Offsets=(Left=0.000000,Top=0.000000,"
        f"Right={float(page[2]):.6f},Bottom={float(page[3]):.6f}),"
        "Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),"
        "Alignment=(X=0.000000,Y=0.000000))"
    )
    scroll = widget(
        "HospitalExplanationScroll",
        "/Script/UMG.ScrollBox",
        properties={
            "ScrollBarVisibility": "Collapsed",
            "bAnimateWheelScrolling": "False",
            "WidgetStyle": "(TopShadowBrush=(DrawAs=NoDrawType),BottomShadowBrush=(DrawAs=NoDrawType))",
        },
        children=[
            widget(
                "HospitalExplanationInset",
                "/Script/UMG.Border",
                properties={
                    "BrushColor": "(R=0.000000,G=0.000000,B=0.000000,A=0.000000)",
                    "Padding": f"(Left={left_inset:.6f},Top=0.000000,Right=0.000000,Bottom=0.000000)",
                    "HorizontalAlignment": "HAlign_Left",
                },
                children=[
                    widget(
                        "HospitalExplanationTextWidth",
                        "/Script/UMG.SizeBox",
                        properties={"WidthOverride": f"{float(label[2]):.6f}"},
                        children=[
                            widget(
                                "HospitalExplanationText",
                                "/Script/UMG.RichTextBlock",
                                properties={
                                    "DefaultTextStyleOverride": "(Font=(TypefaceFontName=\"Regular\",Size=15.000000),ColorAndOpacity=(SpecifiedColor=(R=1.000000,G=1.000000,B=1.000000,A=1.000000)),ShadowOffset=(X=0.000000,Y=0.000000),ShadowColorAndOpacity=(R=0.000000,G=0.000000,B=0.000000,A=0.000000))",
                                    "bOverrideDefaultStyle": "True",
                                    "Justification": "Left",
                                    "WrapTextAt": f"{float(label[2]):.6f}",
                                    "AutoWrapText": "True",
                                    "LineHeightPercentage": "1.250000",
                                    "ShapedTextOptions": "(bOverride_TextFlowDirection=True,TextFlowDirection=LeftToRight)",
                                },
                                fonts={"DefaultTextStyleOverride.Font": "EngineDefault"},
                            )
                        ],
                    )
                ],
            )
        ],
        widget_slot=slot(
            "/Script/UMG.CanvasPanelSlot",
            {"LayoutData": scroll_layout},
        ),
    )
    return {
        "version": 1,
        "label": "Hospital explanation static viewport",
        "root": "HospitalExplanationSpecRoot",
        "nodes": [scroll],
        "notes": [
            "Viewport size, text width/inset, scrolling, font and text layout derive from hospital_explanation.json and are serialized into the WBP; native code supplies the runtime text and yellow rich-text style row."
        ],
    }


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(OUTPUT)


if __name__ == "__main__":
    main()
