"""Build source-derived editable battle-report screen and row specs."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC_ROOT = ROOT / "Saved" / "RokUi" / "Specs"
ATLAS = "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_mail.rok_fullscreen_mail"


def f(value: float) -> str:
    return f"{float(value):.6f}"


def canvas_slot(x: float, y: float, width: float, height: float, z: int = 1) -> dict:
    return {
        "class": "/Script/UMG.CanvasPanelSlot",
        "props": {
            "LayoutData": (
                f"(Offsets=(Left={f(x)},Top={f(y)},Right={f(width)},Bottom={f(height)}),"
                "Anchors=(Minimum=(X=0.000000,Y=0.000000),Maximum=(X=0.000000,Y=0.000000)),"
                "Alignment=(X=0.000000,Y=0.000000))"
            ),
            "ZOrder": str(z),
        },
    }


def node(name: str, class_path: str, rect=None, props=None, children=None, z=1) -> dict:
    result = {"name": name, "class": class_path, "props": props or {}}
    if rect is not None:
        result["slot"] = canvas_slot(*rect, z)
    if children is not None:
        result["children"] = children
    return result


def text(name: str, value: str, rect, size: int, color, bold=False, z=2, visibility="HitTestInvisible") -> dict:
    return node(name, "/Script/Rok.RokUiTextBlock", rect, {
        "Text": value,
        "Font": f"(OutlineSettings=(OutlineColor=(R=0.000000,G=0.000000,B=0.000000,A=1.000000)),TypefaceFontName=\"{'Bold' if bold else 'Regular'}\",Size={size}.000000,MonospacedWidth=1.000000)",
        "ColorAndOpacity": f"(SpecifiedColor=(R={color[0]},G={color[1]},B={color[2]},A={color[3]}))",
        "ShadowColorAndOpacity": "(R=0.000000,G=0.000000,B=0.000000,A=0.650000)",
        "ShadowOffset": "(X=1.000000,Y=1.000000)",
        "Justification": "ETextJustify::Left",
        "AutoWrapText": "False",
        "Visibility": visibility,
        "FontRole": "EngineDefault",
    }, z=z)


def image(name: str, rect, uv, z=2) -> dict:
    u, v, w, h = uv
    min_u, min_v = u / 2048.0, 1.0 - (v + h) / 1024.0
    max_u, max_v = (u + w) / 2048.0, 1.0 - v / 1024.0
    brush = (
        "(TintColor=(SpecifiedColor=(R=1.000000,G=1.000000,B=1.000000,A=1.000000)),"
        "DrawAs=Image,ImageType=FullColor,ImageSize=(X=77.000000,Y=75.000000),Margin=(),"
        f"ResourceObject=\"/Script/Engine.Texture2D'{ATLAS}'\","
        f"UVRegion=(Min=(X={f(min_u)},Y={f(min_v)}),Max=(X={f(max_u)},Y={f(max_v)}),bIsValid=True))"
    )
    return node(name, "/Script/UMG.Image", rect, {
        "Brush": brush,
        "Visibility": "HitTestInvisible",
    }, z=z)


def build() -> dict[str, dict]:
    # Positions are the existing 1440x810 mail-window composition; the right
    # report pane is mounted at (547,128), so page children use local coordinates.
    screen_nodes = [
        node("BattleReportPanel", "/Script/UMG.Border", (0, 0, 830, 590), {
            "BrushColor": "(R=0.047059,G=0.054902,B=0.058824,A=0.960000)",
            "Padding": "(Left=0,Top=0,Right=0,Bottom=0)",
        }, z=1),
        node("BattleReportDetailScroll", "/Script/UMG.ScrollBox", (5, 4, 820, 582), {
            "ScrollBarVisibility": "ESlateVisibility::Collapsed",
            "AllowOverscroll": "False",
            "bAnimateWheelScrolling": "False",
        }, children=[node("BattleReportDetailSize", "/Script/UMG.SizeBox", props={
            "HeightOverride": "245.000000",
            "bOverride_HeightOverride": "True",
        }, children=[node("BattleReportDetailCanvas", "/Script/UMG.CanvasPanel", props={
        }, children=[
            image("BattleReportPoster", (2, 1, 803, 118), (1, 650, 822, 773), z=1),
            text("BattleReportTitle", "战斗报告", (22, 12, 520, 38), 29, (0.91, 0.79, 0.52, 1.0), True, z=3),
            text("BattleReportOpponent", "交战记录", (23, 60, 600, 28), 22, (1.0, 1.0, 1.0, 1.0), z=3),
            text("BattleReportTime", "", (577, 87, 224, 26), 15, (0.74, 0.75, 0.70, 1.0), z=3),
            text("BattleReportDuration", "", (23, 131, 750, 30), 18, (0.74, 0.75, 0.70, 1.0), z=3),
            text("BattleReportPending", "正在获取战斗详情…", (24, 154, 750, 44), 23,
                 (0.74, 0.75, 0.70, 1.0), z=4, visibility="Collapsed"),
            text("BattleReportArmyHeading", "部队战况", (23, 170, 750, 34), 22,
                 (0.91, 0.79, 0.52, 1.0), True, z=3, visibility="Collapsed"),
            text("BattleReportArmyCol0", "部队", (24, 212, 100, 27), 17, (0.74, 0.75, 0.70, 1.0), visibility="Collapsed"),
            text("BattleReportArmyCol1", "参战", (281, 212, 100, 27), 17, (0.74, 0.75, 0.70, 1.0), visibility="Collapsed"),
            text("BattleReportArmyCol2", "剩余", (389, 212, 100, 27), 17, (0.74, 0.75, 0.70, 1.0), visibility="Collapsed"),
            text("BattleReportArmyCol3", "轻伤", (497, 212, 100, 27), 17, (0.74, 0.75, 0.70, 1.0), visibility="Collapsed"),
            text("BattleReportArmyCol4", "重伤", (605, 212, 100, 27), 17, (0.74, 0.75, 0.70, 1.0), visibility="Collapsed"),
            text("BattleReportArmyCol5", "阵亡", (713, 212, 100, 27), 17, (0.74, 0.75, 0.70, 1.0), visibility="Collapsed"),
            text("BattleReportRewardHeading", "战斗奖励", (23, 348, 750, 32), 22,
                 (0.91, 0.79, 0.52, 1.0), True, z=3, visibility="Collapsed"),
            text("BattleReportEmptyRewards", "无", (24, 384, 750, 25), 19,
                 (0.74, 0.75, 0.70, 1.0), visibility="Collapsed"),
            text("BattleReportExperience", "", (24, 384, 750, 29), 19,
                 (1.0, 1.0, 1.0, 1.0), visibility="Collapsed"),
            text("BattleReportRoundsHeading", "战斗回合", (23, 430, 750, 34), 22,
                 (0.91, 0.79, 0.52, 1.0), True, z=3, visibility="Collapsed"),
            text("BattleReportNoRounds", "本场战报没有回合记录。", (24, 469, 750, 29), 18,
                 (0.74, 0.75, 0.70, 1.0), visibility="Collapsed"),
        ])])], z=3),
        text("BattleReportEmpty", "暂无战斗报告", (57, 87, 690, 44), 24,
             (0.74, 0.75, 0.70, 1.0), z=4),
    ]
    page = {"version": 1, "label": "Editable battle-report detail pane", "root": "BattleReportRoot",
            "nodes": screen_nodes, "notes": [
                "The right-side panel, scroll viewport, empty state and content host are editable in Designer.",
                "C++ supplies report-specific values and grows the content SizeBox for dynamic rows.",
            ]}

    # A separate clickable native parent keeps indexed input behavior in C++;
    # artwork, typography and row spacing remain editor-authored.
    row_nodes = [
        node("BattleReportRowPlate", "/Script/UMG.Border", (12, 7, 460, 90), {
            "BrushColor": "(R=0.047059,G=0.054902,B=0.058824,A=0.960000)",
            "Padding": "(Left=0,Top=0,Right=0,Bottom=0)",
        }, z=1),
        image("BattleReportRowIcon", (24, 19, 68, 66), (1930, 650, 77, 75), z=2),
        text("BattleReportRowTitle", "战斗报告", (107, 17, 340, 32), 23,
             (1.0, 1.0, 1.0, 1.0), True, z=3),
        text("BattleReportRowSubject", "交战记录", (107, 57, 300, 24), 17,
             (0.74, 0.75, 0.70, 1.0), z=3),
        text("BattleReportRowUnread", "●", (433, 23, 28, 22), 18,
             (0.92, 0.19, 0.12, 1.0), z=4),
        node("BattleReportRowHitButton", "/Script/Rok.RokUnityIndexedButton", (0, 0, 470, 100), {
            "Visibility": "Visible",
            "BackgroundColor": "(R=0.000000,G=0.000000,B=0.000000,A=0.000000)",
        }, z=10),
    ]
    row = {"version": 1, "label": "Editable battle-report list row", "root": "BattleReportRowRoot",
           "nodes": row_nodes, "notes": [
               "The indexed-button parent retains click behavior; text, icon, plate and placement are editable.",
               "C++ supplies title, opponent, unread state and selection styling.",
           ]}

    army_nodes = [
        node("BattleReportArmyPlate", "/Script/UMG.Border", (0, 0, 784, 41), {
            "BrushColor": "(R=0.078000,G=0.088000,B=0.085000,A=1.000000)",
            "Padding": "(Left=0,Top=0,Right=0,Bottom=0)",
        }, z=1),
        text("BattleReportArmyName", "我方", (12, 7, 245, 27), 19, (1.0, 1.0, 1.0, 1.0), z=2),
        text("BattleReportArmyBegin", "—", (269, 7, 102, 27), 19, (1.0, 1.0, 1.0, 1.0), z=2),
        text("BattleReportArmyEnd", "—", (377, 7, 102, 27), 19, (1.0, 1.0, 1.0, 1.0), z=2),
        text("BattleReportArmyMinor", "—", (485, 7, 102, 27), 19, (1.0, 1.0, 1.0, 1.0), z=2),
        text("BattleReportArmySevere", "—", (593, 7, 102, 27), 19, (1.0, 1.0, 1.0, 1.0), z=2),
        text("BattleReportArmyDead", "—", (701, 7, 102, 27), 19, (1.0, 1.0, 1.0, 1.0), z=2),
    ]
    army = {"version": 1, "label": "Editable battle-report army row", "root": "BattleReportArmyRowRoot",
            "nodes": army_nodes, "notes": [
                "The reusable row owns casualty background, six columns, font and alignment.",
                "C++ supplies friendly/enemy labels and counts for each battle.",
            ]}

    reward_nodes = [
        image("BattleReportRewardIcon", (0, 0, 40, 40), (1930, 650, 77, 75), z=1),
        text("BattleReportRewardText", "奖励 ×0", (44, 8, 210, 30), 17, (1.0, 1.0, 1.0, 1.0), z=2),
    ]
    reward = {"version": 1, "label": "Editable battle-report reward row", "root": "BattleReportRewardRowRoot",
              "nodes": reward_nodes, "notes": [
                  "C++ selects the reward icon and supplies localized label and quantity.",
                  "The parent decides the three-column grid position based on the dynamic reward count.",
              ]}

    round_nodes = [
        node("BattleReportRoundPlate", "/Script/UMG.Border", (0, 0, 784, 33), {
            "BrushColor": "(R=0.078000,G=0.088000,B=0.085000,A=1.000000)",
            "Padding": "(Left=0,Top=0,Right=0,Bottom=0)",
        }, z=1),
        text("BattleReportRoundSummary", "第 1 回合     我方剩余 0     敌方剩余 0", (12, 4, 760, 28), 17,
             (1.0, 1.0, 1.0, 1.0), z=2),
    ]
    round_row = {"version": 1, "label": "Editable battle-report round row", "root": "BattleReportRoundRowRoot",
                 "nodes": round_nodes, "notes": [
                     "C++ supplies round number and remaining army counts; the WBP owns row typography and backing plate.",
                 ]}
    return {
        "battle_report_screen": page,
        "battle_report_row": row,
        "battle_report_army_row": army,
        "battle_report_reward_row": reward,
        "battle_report_round_row": round_row,
    }


def main() -> None:
    SPEC_ROOT.mkdir(parents=True, exist_ok=True)
    for name, spec in build().items():
        target = SPEC_ROOT / f"{name}.json"
        target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(target)


if __name__ == "__main__":
    main()
