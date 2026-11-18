"""Static contracts for the mail/report specs; no engine or account access."""
from BuildBattleReportWbpSpecs import ROOT, build
import re


def walk(nodes, parent=None):
    for node in nodes:
        yield node, parent
        yield from walk(node.get("children", []), node)


def main():
    specs = build()
    for label, spec in specs.items():
        nodes = list(walk(spec["nodes"]))
        names = [node["name"] for node, _ in nodes]
        assert len(names) == len(set(names)), (label, "duplicate names")
        for node, parent in nodes:
            assert "ResourceObject" not in str(node), (label, node["name"], "unexpected artwork")
            if node["class"] == "/Script/UMG.TextBlock":
                assert node.get("fonts") == {"Font": "EngineDefault"}, (label, node["name"], "font role")
            slot = node.get("slot", {}).get("class", "")
            if slot.endswith("VerticalBoxSlot"):
                assert parent and parent["class"] == "/Script/UMG.VerticalBox", (label, node["name"], "vertical slot parent")
        print(f"{label}: {len(names)} widgets; unique names, slot ownership and font roles passed")
    contracts = {
        "battle_report_screen": {"BattleReportOverviewBody", "BattleReportDetailCanvas", "BattleReportDetailSize",
            "BattleReportPending", "BattleReportMissingSelf", "BattleReportRewardCards", "BattleReportPlayer",
            "BattleReportCurvePlot", "BattleReportCurveLineTemplate", "BattleReportCurvePointTemplate",
            "BattleReportCurveAxis_0", "BattleReportCurveAxis_3", "BattleReportOpponentRows"},
        "battle_report_row": {"BattleReportRowLayout", "BattleReportRowSelection", "BattleReportRowHitButton"},
        "battle_report_army_row": {"BattleReportArmyLayout", "BattleReportArmyCounts", "BattleReportArmyPosition"},
        "battle_report_reward_row": {"BattleReportRewardLayout", "BattleReportRewardIcon", "BattleReportRewardQuantity"},
        "server_mail_screen": {"ServerMailScroll", "ServerMailBody", "ServerMailRewardCards", "ServerMailClaim",
            "ServerMailClaimLabel", "ServerMailClaimSectionSize", "ServerMailEmpty", "ServerMailRewardUnavailable"},
        "server_mail_row": {"ServerMailRowLayout", "ServerMailRowHitButton", "ServerMailRowSelection"},
        "server_mail_reward_card": {"ServerMailRewardLayout", "ServerMailRewardIcon", "ServerMailRewardQuantity"},
    }
    for name, required in contracts.items():
        actual = {node["name"] for node, _ in walk(specs[name]["nodes"])}
        assert required <= actual, (name, sorted(required - actual))
    # Check the names actually used by the new C++ binding paths, not just a
    # second hand-maintained list. Legacy fallback names are intentionally out.
    paths = (("RokSystemMailScreen.cpp", "TryBuildSessionMailWbp", {
        "Page": "server_mail_screen", "Row": "server_mail_row",
        "RowPrototype": "server_mail_row", "RewardPrototype": "server_mail_reward_card"}),
        ("RokBattleReportScreen.cpp", "TryBuildBattleReportOverview", {
        "Page": "battle_report_screen", "Row": "battle_report_army_row",
        "ArmyPrototype": "battle_report_army_row", "RewardPrototype": "battle_report_reward_row"}))
    for filename, method, owners in paths:
        source = (ROOT / "Source" / "Rok" / filename).read_text(encoding="utf-8-sig")
        source = source[source.index("bool URokUnityFeatureScreenWidget::" + method):]
        assert "SetFont(" not in source and "SetBrushColor(" not in source, (filename, "style belongs in WBP")
        for owner, name in re.findall(r'RokMailWbp::(?:Find|SetText|Show|Size)\(\*?(\w+), TEXT\("([^"%]+)"\)', source):
            if owner in owners:
                actual = {node["name"] for node, _ in walk(specs[owners[owner]]["nodes"])}
                assert name in actual, (filename, owner, name, "binding missing from WBP")
    print("All runtime binding contracts passed. Engine generation/runtime/visual acceptance is separate.")


if __name__ == "__main__":
    main()
