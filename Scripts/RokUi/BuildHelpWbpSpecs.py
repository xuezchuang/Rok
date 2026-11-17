"""Build editable Help screen and repeated-row specs from the source catalog."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / "Content" / "RokData" / "help_catalog.json"
SPEC_ROOT = ROOT / "Saved" / "RokUi" / "Specs"
ATLAS_ASSETS = {
    "common": "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common",
    "mail": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_mail.rok_fullscreen_mail",
    "frame": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame",
    "setting": "/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_setting.rok_modal_atlas_setting",
}


def f(value: float) -> str:
    return f"{float(value):.6f}"


def canvas_slot(x: float, y: float, width: float, height: float, z: int = 1) -> dict:
    layout = (
        f"(Offsets=(Left={f(x)},Top={f(y)},Right={f(width)},Bottom={f(height)}),"
        "Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),"
        "Alignment=(X=0.000000,Y=0.000000))"
    )
    return {"class": "/Script/UMG.CanvasPanelSlot", "props": {"LayoutData": layout, "ZOrder": str(z)}}


def widget(name: str, class_path: str, rect=None, z=1, props=None, children=None, slot=None) -> dict:
    result = {"name": name, "class": class_path, "props": props or {}}
    if rect is not None:
        result["slot"] = slot or canvas_slot(*rect, z)
    elif slot is not None:
        result["slot"] = slot
    if children is not None:
        result["children"] = children
    return result


def linear_rgba(rgba):
    def channel(v):
        v = round(float(v) * 255.0) / 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4

    return [channel(v) for v in rgba[:3]] + [round(float(rgba[3]) * 255.0) / 255.0]


def image_node(name: str, node: dict, offset=(0.0, 0.0), z=1) -> dict:
    atlas = node["atlas"]
    atlas_path = ATLAS_ASSETS[atlas]
    u, v, width, height = node["uv"]
    atlas_width, atlas_height = node["atlas_size"]
    min_u = u / atlas_width
    min_v = 1.0 - (v + height) / atlas_height
    max_u = (u + width) / atlas_width
    max_v = 1.0 - v / atlas_height
    brush = (
        "(TintColor=(SpecifiedColor=(R=1.000000,G=1.000000,B=1.000000,A=1.000000)),"
        "DrawAs=Image,ImageType=FullColor,"
        f"ImageSize=(X={f(width)},Y={f(height)}),Margin=(),"
        f"ResourceObject=\"/Script/Engine.Texture2D'{atlas_path}'\","
        "OutlineSettings=(CornerRadii=(X=0.000000,Y=0.000000,Z=0.000000,W=0.000000),"
        "Color=(SpecifiedColor=(R=0.000000,G=0.000000,B=0.000000,A=0.000000)),RoundingType=HalfHeightRadius),"
        f"UVRegion=(Min=(X={f(min_u)},Y={f(min_v)}),Max=(X={f(max_u)},Y={f(max_v)}),bIsValid=True))"
    )
    rect = node["rect"]
    scale = ("-1.000000" if node.get("flip_x") else "1.000000", "-1.000000" if node.get("flip_y") else "1.000000")
    tint = linear_rgba(node.get("rgba", [1.0, 1.0, 1.0, 1.0]))
    props = {
        "Brush": brush,
        "ColorAndOpacity": f"(R={f(tint[0])},G={f(tint[1])},B={f(tint[2])},A={f(tint[3])})",
        "RenderTransformPivot": "(X=0.500000,Y=0.500000)",
        "RenderTransform": f"(Scale=(X={scale[0]},Y={scale[1]}))",
        "Visibility": "HitTestInvisible",
    }
    return widget(name, "/Script/UMG.Image", (rect[0] + offset[0], rect[1] + offset[1], rect[2], rect[3]), z, props)


def glyph_node(name: str, node: dict, offset=(0.0, 0.0), z=1, slot=None) -> dict:
    rect = node["rect"]
    rgba = linear_rgba(node.get("rgba", [1.0, 1.0, 1.0, 1.0]))
    props = {
        "SourceFontSize": str(int(node["font_px"])),
        "bSourceBold": "True" if node["bold"] else "False",
        "SourceAlignment": str(int(node["alignment"])),
        "SourceGlyphSize": f"(X={f(rect[2])},Y={f(rect[3])})",
        "SourceGlyphColor": f"(R={f(rgba[0])},G={f(rgba[1])},B={f(rgba[2])},A={f(rgba[3])})",
        "Visibility": "HitTestInvisible",
    }
    return widget(
        name,
        "/Script/Rok.RokHelpSourceTextWidget",
        (rect[0] + offset[0], rect[1] + offset[1], rect[2], rect[3]),
        z,
        props,
        slot=slot,
    )


def button_node(name: str, node: dict, offset=(0.0, 0.0), z=10) -> dict:
    rect = node["rect"]
    return widget(
        name,
        "/Script/Rok.RokUnityIndexedButton",
        (rect[0] + offset[0], rect[1] + offset[1], rect[2], rect[3]),
        z,
        {"Visibility": "Visible"},
    )


def border_node(name: str, rect, child: dict, padding="(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)") -> dict:
    props = {
        "BrushColor": "(R=0.000000,G=0.000000,B=0.000000,A=0.000000)",
        "Padding": padding,
        "HorizontalAlignment": "HAlign_Fill",
        "VerticalAlignment": "VAlign_Fill",
    }
    return widget(name, "/Script/UMG.Border", rect, props=props, children=[child])


def source_node(section: dict, path: str) -> dict:
    try:
        return section[path]
    except KeyError as error:
        raise SystemExit(f"missing Help source layout node: {path}") from error


def build_specs() -> dict[str, dict]:
    source = json.loads(CATALOG.read_text(encoding="utf-8"))
    if source.get("schema") != "rok.help.v1" or len(source.get("entries", [])) != 70:
        raise SystemExit("Help catalog schema or question count changed")
    layouts = source["layouts"]
    frame, window, group, question = (layouts[key] for key in ("frame", "window", "group", "question"))
    frame_offset = (146.0, 28.0)
    window_offset = (138.0, 11.5)

    main_nodes = []
    for index in range(1, 5):
        path = f"win/frame{index}"
        main_nodes.append(image_node(f"HelpFrame_{path.replace('/', '_')}", source_node(frame, path), frame_offset, 1 if index <= 2 else 2))
    main_nodes.append(glyph_node("HelpFrame_title_lbl_title", source_node(frame, "title/lbl_title"), frame_offset, 4))
    main_nodes.append(image_node("HelpCloseArt", source_node(frame, "title/btn_close"), frame_offset, 5))
    main_nodes.append(image_node("HelpBackArt", source_node(frame, "title/btn_back"), frame_offset, 5))
    main_nodes.append(button_node("HelpCloseButton", source_node(frame, "title/btn_close"), frame_offset, 10))
    main_nodes.append(button_node("HelpBackButton", source_node(frame, "title/btn_back"), frame_offset, 10))

    list_children = []
    for path, node in sorted(window.items()):
        if path.startswith("pl_list/") and path != "pl_list/sv_helperlist":
            name = "HelpList_" + path.replace("/", "_")
            if node.get("atlas"):
                list_children.append(image_node(name, node, window_offset, 1))
            elif node.get("font_px"):
                list_children.append(glyph_node(name, node, window_offset, 2))
    list_scroll = source_node(window, "pl_list/sv_helperlist")
    list_children.append(widget(
        "HelpListScroll", "/Script/UMG.ScrollBox",
        (list_scroll["rect"][0] + window_offset[0], list_scroll["rect"][1] + window_offset[1], list_scroll["rect"][2], list_scroll["rect"][3]),
        props={"ScrollBarVisibility": "Collapsed", "bAnimateWheelScrolling": "False"},
    ))
    list_children.append(button_node("HelpCustomerServiceButton", source_node(window, "pl_list/btn_head"), window_offset, 10))
    main_nodes.append(widget("HelpListPage", "/Script/UMG.CanvasPanel", (0, 0, 1440, 810), 3,
                             {"Visibility": "SelfHitTestInvisible"}, list_children))

    divider = source_node(window, "pl_detial/line")
    detail_children = [image_node("HelpDetailDivider", divider, window_offset, 1)]
    question_rect = source_node(window, "pl_detial/lbl_question")["rect"]
    question_node = glyph_node(
        "HelpQuestionText", {**source_node(window, "pl_detial/lbl_question"), "rect": [0, 0, question_rect[2], question_rect[3]]},
        slot={"class": "/Script/UMG.BorderSlot", "props": {
            "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
            "HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill",
        }},
    )
    detail_children.append(border_node(
        "HelpQuestionTextRegion",
        (question_rect[0] + window_offset[0], question_rect[1] + window_offset[1], question_rect[2], question_rect[3]),
        question_node,
    ))
    answer_scroll = source_node(window, "pl_detial/sv_list")
    answer_text = source_node(window, "pl_detial/sv_list/v/c/lbl_detial")
    answer_glyph = glyph_node(
        "HelpAnswerText", {**answer_text, "rect": [0, 0, answer_text["rect"][2], answer_text["rect"][3]]},
        slot={"class": "/Script/UMG.SizeBoxSlot", "props": {
            "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
            "HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill",
        }},
    )
    answer_size = widget("HelpAnswerWidth", "/Script/UMG.SizeBox", props={"WidthOverride": f(answer_text["rect"][2])}, children=[answer_glyph])
    answer_inset = widget("HelpAnswerInset", "/Script/UMG.Border", props={
        "BrushColor": "(R=0.000000,G=0.000000,B=0.000000,A=0.000000)",
        "Padding": "(Left=10.000000,Top=0.000000,Right=10.000000,Bottom=0.000000)",
        "HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill",
    }, children=[answer_size])
    detail_children.append(widget(
        "HelpAnswerScroll", "/Script/UMG.ScrollBox",
        (answer_scroll["rect"][0] + window_offset[0], answer_scroll["rect"][1] + window_offset[1], answer_scroll["rect"][2], answer_scroll["rect"][3]),
        props={"ScrollBarVisibility": "Collapsed"}, children=[answer_inset],
    ))
    main_nodes.append(widget("HelpDetailPage", "/Script/UMG.CanvasPanel", (0, 0, 1440, 810), 3,
                             {"Visibility": "Collapsed"}, detail_children))

    main_spec = {"version": 1, "label": "Editable source Help screen", "root": "HelpRoot", "nodes": main_nodes,
                 "notes": ["C++ supplies catalog content and routes input; static screen, frame, viewport and detail layout live in WBP."]}

    group_nodes = [
        image_node("HelpGroupPlate", source_node(group, "btn_btn")),
        glyph_node("HelpGroupText", source_node(group, "btn_btn/lbl_title"), z=2),
        image_node("HelpGroupArrowDown", source_node(group, "btn_btn/img_arrowDown"), z=3),
        image_node("HelpGroupArrowUp", source_node(group, "btn_btn/img_arrowUp"), z=3),
    ]
    question_nodes = [
        glyph_node("HelpQuestionLabel", source_node(question, "btn_btn/lbl_title"), z=2),
        image_node("HelpQuestionLine", source_node(question, "line"), z=1),
    ]
    group_spec = {"version": 1, "label": "Editable Help group row", "root": "HelpGroupRowRoot", "nodes": group_nodes,
                  "notes": ["Row artwork and source-glyph position are editable; C++ supplies text, arrow state and expansion action hitbox."]}
    question_spec = {"version": 1, "label": "Editable Help question row", "root": "HelpQuestionRowRoot", "nodes": question_nodes,
                     "notes": ["Question glyph and separator layout are editable; C++ supplies text and indexed action hitbox."]}
    return {"help_screen": main_spec, "help_group_row": group_spec, "help_question_row": question_spec}


def main() -> None:
    SPEC_ROOT.mkdir(parents=True, exist_ok=True)
    for name, spec in build_specs().items():
        path = SPEC_ROOT / f"{name}.json"
        path.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(path)


if __name__ == "__main__":
    main()
