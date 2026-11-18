"""Build original Rok mail/report layouts; data bindings follow the reference protocol.

No screenshot coordinates or new third-party artwork are used. Every stable layout,
text style, reward card and chart style is editable in a generated Widget Blueprint.
"""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC_ROOT = ROOT / "Saved" / "RokUi" / "Specs"


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
    result = node(name, "/Script/UMG.TextBlock", rect, {
        "Text": value,
        "Font": f"(OutlineSettings=(OutlineColor=(R=0.000000,G=0.000000,B=0.000000,A=1.000000)),TypefaceFontName=\"{'Bold' if bold else 'Regular'}\",Size={size}.000000,MonospacedWidth=1.000000)",
        "ColorAndOpacity": f"(SpecifiedColor=(R={color[0]},G={color[1]},B={color[2]},A={color[3]}))",
        "ShadowColorAndOpacity": "(R=0.000000,G=0.000000,B=0.000000,A=0.650000)",
        "ShadowOffset": "(X=1.000000,Y=1.000000)",
        "Justification": "ETextJustify::Left",
        "AutoWrapText": "False",
        "Visibility": visibility,
    }, z=z)
    result["fonts"] = {"Font": "EngineDefault"}
    return result


PAPER = (0.88, 0.87, 0.82, 1.0)
MUTED = (0.65, 0.68, 0.69, 1.0)
ACCENT = (0.85, 0.72, 0.44, 1.0)


def vertical(child, bottom=12):
    child["slot"] = {"class": "/Script/UMG.VerticalBoxSlot", "props": {
        "Padding": f"(Left=14,Top=0,Right=14,Bottom={bottom})",
        "HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Top",
        "Size": "(Value=1,SizeRule=Automatic)"}}
    return child


def sized(name, height, children):
    return node(name + "Size", "/Script/UMG.SizeBox", props={
        "WidthOverride": "790", "bOverride_WidthOverride": "True",
        "HeightOverride": str(height), "bOverride_HeightOverride": "True"},
        children=[node(name, "/Script/UMG.CanvasPanel", children=children)])


def border(name, rect, color=(0.075, 0.090, 0.096, 1), visibility="HitTestInvisible"):
    return node(name, "/Script/UMG.Border", rect, {
        "BrushColor": f"(R={color[0]},G={color[1]},B={color[2]},A={color[3]})",
        "Padding": "(Left=0,Top=0,Right=0,Bottom=0)", "Visibility": visibility})


def curve_primitive(name, width, height, color):
    return node(name, "/Script/UMG.Image", (0, 0, width, height), {
        "Brush": "(DrawAs=Box,TintColor=(SpecifiedColor=(R=%s,G=%s,B=%s,A=%s)))" % color,
        "Visibility": "Collapsed"})


def marker(name, width, height):
    return node(name, "/Script/UMG.CanvasPanel", (0, 0, width, height), {"Visibility": "Collapsed"})


def report_screen():
    header = sized("BattleReportHeader", 126, [
        border("BattleReportHeaderPlate", (0, 0, 790, 126)),
        text("BattleReportTitle", "战斗报告", (18, 14, 754, 36), 27, ACCENT, True),
        text("BattleReportOpponent", "", (18, 56, 754, 28), 20, PAPER),
        text("BattleReportTime", "", (18, 94, 754, 24), 16, MUTED)])
    rewards = node("BattleReportRewardsSection", "/Script/UMG.VerticalBox", children=[
        vertical(text("BattleReportRewardHeading", "战斗奖励", None, 19, ACCENT), 8),
        vertical(text("BattleReportEmptyRewards", "无奖励记录", None, 18, MUTED), 0),
        vertical(node("BattleReportRewardCards", "/Script/UMG.WrapBox", props={
            "InnerSlotPadding": "(X=10,Y=10)", "WrapSize": "760", "bExplicitWrapSize": "True"}), 0)])
    player = sized("BattleReportPlayerSection", 102, [
        border("BattleReportPlayerPlate", (0, 0, 790, 102), (0.12, 0.15, 0.16, 1)),
        text("BattleReportPlayer", "", (16, 12, 460, 31), 23, PAPER, True),
        text("BattleReportHero", "", (16, 56, 460, 28), 18, MUTED),
        text("BattleReportArmyTotal", "", (500, 16, 274, 27), 18, PAPER),
        text("BattleReportArmyRemaining", "", (500, 56, 274, 27), 18, PAPER)])
    chart = sized("BattleReportCurveSection", 267, [
        text("BattleReportCurveHeading", "部队数量变化", (0, 0, 790, 30), 19, ACCENT),
        border("BattleReportCurveBacking", (66, 44, 704, 161), (0.045, 0.060, 0.068, 1)),
        node("BattleReportCurvePlot", "/Script/UMG.CanvasPanel", (76, 50, 680, 148), {"Visibility": "HitTestInvisible"}),
        *[text(f"BattleReportCurveAxis_{tick}", "0", (0, 39 + tick * 50, 64, 25), 15, MUTED) for tick in range(4)],
        text("BattleReportNoCurve", "没有可用的兵力变化记录", (120, 105, 600, 28), 18, MUTED, visibility="Collapsed"),
        text("BattleReportStartTime", "", (67, 216, 280, 26), 16, MUTED),
        text("BattleReportEndTime", "", (568, 216, 200, 26), 16, MUTED),
        curve_primitive("BattleReportCurveLineTemplate", 1, 3, (0.24, 0.68, 0.79, 1)),
        curve_primitive("BattleReportCurvePointTemplate", 8, 8, (0.81, 0.91, 0.89, 1))])
    metadata = node("BattleReportMetadata", "/Script/UMG.VerticalBox", children=[
        vertical(text("BattleReportMissingSelf", "战报中没有当前角色的部队信息。", None, 18, MUTED, visibility="Collapsed"), 6),
        vertical(text("BattleReportCasualties", "", None, 18, MUTED), 6),
        vertical(text("BattleReportExperience", "", None, 18, MUTED), 6),
        vertical(text("BattleReportDuration", "", None, 17, MUTED), 6)])
    body = node("BattleReportOverviewBody", "/Script/UMG.VerticalBox", (0, 0, 818, 0), children=[
        vertical(header), vertical(text("BattleReportPending", "正在获取战斗详情…", None, 21, MUTED, visibility="Collapsed")),
        vertical(rewards, 18), vertical(player), vertical(chart), vertical(metadata),
        vertical(text("BattleReportOpponentHeading", "交战对象", None, 19, ACCENT), 8),
        vertical(node("BattleReportOpponentRows", "/Script/UMG.VerticalBox"), 0)])
    body["slot"]["props"]["bAutoSize"] = "True"
    return {"version": 1, "label": "Rok editable battle report overview", "root": "BattleReportRoot", "nodes": [
        border("BattleReportPanel", (0, 0, 830, 590), (0.04, 0.05, 0.055, 0.97)),
        node("BattleReportDetailScroll", "/Script/UMG.ScrollBox", (4, 6, 822, 578), {
            "AllowOverscroll": "False", "ScrollBarVisibility": "ESlateVisibility::Visible"}, children=[
            node("BattleReportDetailSize", "/Script/UMG.SizeBox", props={"WidthOverride": "818", "bOverride_WidthOverride": "True"}, children=[
                node("BattleReportDetailCanvas", "/Script/UMG.CanvasPanel", children=[body])])]),
        text("BattleReportEmpty", "暂无战斗报告", (32, 60, 760, 45), 23, MUTED, visibility="Collapsed")],
        "notes": ["Original Rok layout and plain editable shapes; C++ binds protocol values only.",
                  "Vertical flow grows for long reward/opponent lists. Curve style is copied from hidden templates; geometry follows report events."]}


def mail_screen():
    body = text("ServerMailBody", "邮件正文", None, 20, PAPER)
    body["props"].update({"AutoWrapText": "True", "WrapTextAt": "756"})
    content = node("ServerMailContent", "/Script/UMG.VerticalBox", children=[
        vertical(sized("ServerMailHeader", 124, [
            border("ServerMailHeaderPlate", (0, 0, 790, 124)),
            text("ServerMailTitle", "邮件", (18, 12, 752, 36), 26, ACCENT, True),
            text("ServerMailSubtitle", "", (18, 54, 752, 29), 20, PAPER),
            text("ServerMailTime", "", (18, 92, 752, 24), 16, MUTED)])),
        vertical(body, 20), vertical(text("ServerMailSender", "", None, 17, MUTED)),
        vertical(text("ServerMailRewardHeading", "邮件附件", None, 19, ACCENT), 8),
        vertical(node("ServerMailRewardCards", "/Script/UMG.WrapBox", props={
            "InnerSlotPadding": "(X=10,Y=10)", "WrapSize": "756", "bExplicitWrapSize": "True"})),
        vertical(text("ServerMailRewardUnavailable", "部分附件详情暂不可用。", None, 17, MUTED, visibility="Collapsed")),
        vertical(sized("ServerMailClaimSection", 66, [
            node("ServerMailClaim", "/Script/Rok.RokUnityIndexedButton", (0, 8, 220, 46), {
                "BackgroundColor": "(R=0.24,G=0.32,B=0.34,A=1)", "Visibility": "Visible"}, children=[
                    text("ServerMailClaimLabel", "领取附件", None, 20, PAPER)])]), 4)])
    return {"version": 1, "label": "Rok server mail detail", "root": "ServerMailRoot", "nodes": [
        border("ServerMailPanel", (0, 0, 830, 590), (0.04, 0.05, 0.055, 0.97)),
        node("ServerMailScroll", "/Script/UMG.ScrollBox", (6, 8, 818, 574), {
            "AllowOverscroll": "False", "ScrollBarVisibility": "ESlateVisibility::Visible"}, children=[content]),
        text("ServerMailEmpty", "当前暂无邮件。", (32, 62, 760, 44), 23, MUTED, visibility="Collapsed")],
        "notes": ["Server inbox only; no sample player, mail body or reward is installed as runtime data."]}


def list_row(prefix):
    return {"version": 1, "label": "Rok mail list entry", "root": prefix + "Root", "nodes": [
        marker(prefix + "Layout", 470, 108),
        border(prefix + "Plate", (6, 5, 458, 98)),
        border(prefix + "Selection", (6, 5, 4, 98), (0.78, 0.65, 0.36, 1), "Collapsed"),
        text(prefix + "Title", "", (22, 14, 405, 32), 22, PAPER, True),
        text(prefix + "Subject", "", (22, 52, 405, 42), 17, MUTED),
        text(prefix + "Unread", "●", (433, 15, 22, 23), 16, ACCENT, visibility="Collapsed"),
        node(prefix + "HitButton", "/Script/Rok.RokUnityIndexedButton", (0, 0, 470, 108), {
            "Visibility": "Visible", "BackgroundColor": "(R=0,G=0,B=0,A=0)"}, z=10)]}


def reward_card(prefix):
    icon = node(prefix + "Icon", "/Script/UMG.Image", (12, 12, 46, 46), {"Visibility": "Collapsed"})
    return {"version": 1, "label": "Rok data-bound reward card", "root": prefix + "Root", "nodes": [
        marker(prefix + "Layout", 242, 76), border(prefix + "Plate", (0, 0, 242, 76)), icon,
        text(prefix + "Name", "", (70, 10, 162, 29), 17, PAPER),
        text(prefix + "Quantity", "", (70, 42, 162, 24), 16, ACCENT)]}


def army_card():
    return {"version": 1, "label": "Rok battle participant", "root": "BattleReportArmyRoot", "nodes": [
        marker("BattleReportArmyLayout", 790, 158), border("BattleReportArmyPlate", (0, 0, 790, 150)),
        text("BattleReportArmyName", "", (16, 10, 750, 30), 21, PAPER, True),
        text("BattleReportArmyCounts", "", (16, 47, 750, 27), 18, PAPER),
        text("BattleReportArmyCasualties", "", (16, 82, 750, 25), 17, MUTED),
        text("BattleReportArmyPosition", "", (16, 118, 750, 25), 17, MUTED)]}


def build() -> dict[str, dict]:
    return {"battle_report_screen": report_screen(), "battle_report_row": list_row("BattleReportRow"),
            "battle_report_army_row": army_card(), "battle_report_reward_row": reward_card("BattleReportReward"),
            "battle_report_round_row": {"version": 1, "label": "Reserved report event row", "root": "BattleReportRoundRoot", "nodes": [
                text("BattleReportRoundSummary", "", (12, 4, 760, 28), 17, MUTED)]},
            "server_mail_screen": mail_screen(), "server_mail_row": list_row("ServerMailRow"),
            "server_mail_reward_card": reward_card("ServerMailReward")}


def main() -> None:
    SPEC_ROOT.mkdir(parents=True, exist_ok=True)
    references = [
        "ref/server/data/work/common/protocol/Common.sproto: EmailInfo, RewardInfo, BattleReportEx",
        "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/EmailProxy.cs: template arguments and type mapping",
        "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Mail/MailMediator.cs: OnSystemEmail",
        "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_MailFightReport_SubView.cs: report values and event ordering",
    ]
    for name, spec in build().items():
        spec["references"] = references
        target = SPEC_ROOT / f"{name}.json"
        target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(target)


if __name__ == "__main__":
    main()
