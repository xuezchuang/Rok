"""Reproduce the three real training missing-data states as small WBPs.

The local geometry/text/styles are copied from the existing UE builders, not
from external screenshots. No new artwork or account state is generated.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC_ROOT = ROOT / "Saved/RokUi/Specs"
COMMON = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"


def slot(rect, z=1):
    x, y, w, h = rect
    return {"class": "/Script/UMG.CanvasPanelSlot", "props": {
        "LayoutData": f"(Offsets=(Left={x},Top={y},Right={w},Bottom={h}),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=0,Y=0)),Alignment=(X=0,Y=0))",
        "ZOrder": str(z)}}


def node(name, cls, rect, props=None, children=None, z=1):
    result = {"name": name, "class": "/Script/" + cls, "props": props or {}, "slot": slot(rect, z)}
    if children is not None:
        result["children"] = children
    return result


def text(name, content, rect, size, color=(1, 1, 1, 1), bold=False, center=False, shadow=True):
    result = node(name, "UMG.TextBlock", rect, {
        "Text": content, "Font": '(TypefaceFontName="%s",Size=%s)' % ("Bold" if bold else "Regular", size),
        "ColorAndOpacity": "(SpecifiedColor=(R=%s,G=%s,B=%s,A=%s))" % color,
        "Justification": "ETextJustify::Center" if center else "ETextJustify::Left",
        "ShadowOffset": "(X=1,Y=1)" if shadow else "(X=0,Y=0)",
        "ShadowColorAndOpacity": "(R=0,G=0,B=0,A=0.65)", "Visibility": "HitTestInvisible"})
    result["fonts"] = {"Font": "EngineDefault"}
    return result


def group(children, z):
    result = node("TrainArmyUnavailableState", "UMG.CanvasPanel", (0, 0, 0, 0),
                  {"Visibility": "SelfHitTestInvisible"}, children, z)
    result["slot"]["props"]["LayoutData"] = "(Offsets=(Left=0,Top=0,Right=0,Bottom=0),Anchors=(Minimum=(X=0,Y=0),Maximum=(X=1,Y=1)),Alignment=(X=0,Y=0))"
    return result


def build():
    # CapturePoint(X,Y) subtracts 45 from Y in the existing FeatureScreen.
    back_brush = (
        "(DrawAs=Image,ImageType=FullColor,ImageSize=(X=130,Y=48),"
        f"ResourceObject=\"/Script/Engine.Texture2D'{COMMON}'\","
        "UVRegion=(Min=(X=0.33056640625,Y=0.3349609375),Max=(X=0.42919921875,Y=0.4140625),bIsValid=True))")
    detail = group([
        text("TrainArmyDetailMissing", "兵种详情配置未载入", (650, 255, 540, 60), 22),
        node("TrainArmyDetailBackButtonBackground", "UMG.Image", (198, 690, 130, 48),
             {"Brush": back_brush, "Visibility": "HitTestInvisible"}),
        text("TrainArmyDetailBackButtonLabel", "返回", (198, 695.6, 130, 44), 23, bold=True, center=True),
        node("TrainArmyDetailBackButton", "Rok.RokUnityIndexedButton", (198, 690, 130, 48),
             {"Visibility": "Visible", "BackgroundColor": "(R=0,G=0,B=0,A=0)"}, z=3)], 10)
    catalog = group([
        text("TrainArmyMissingCatalog", "Unity 兵种目录未载入，不能生成训练预览。", (300, 425, 820, 50), 22,
             (.95, .65, .45, 1), True, True)], 7)
    catalog["children"][0]["props"]["Text"] = 'NSLOCTEXT("RokTrainArmy", "MissingCatalog", "Unity 兵种目录未载入，不能生成训练预览。")'
    model = group([
        node("TrainArmyUnavailablePortrait", "UMG.Image", (270, 250, 280, 280),
             {"Brush": "(DrawAs=Image,ImageType=FullColor,ImageSize=(X=280,Y=280))", "Visibility": "Collapsed"}),
        text("TrainParameterStaticPreview", "模型加载失败 · 静态诊断预览", (270, 540, 280, 30), 15,
             center=True, shadow=False)], 6)
    result = {}
    for key, state in (("train_army_detail_unavailable", detail), ("train_army_catalog_unavailable", catalog),
                       ("train_army_model_unavailable", model)):
        result[key] = {"version": 1, "label": key, "root": "TrainArmyUnavailableRoot", "nodes": [state],
            "notes": ["Text, bounds and z-order preserve the existing UE missing-data branch.",
                "The detail back action remains 1951; model portrait UV/resource remains runtime-selected.",
                "No source error is converted into fake data or a successful gameplay state."],
            "references": ["Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildTrainArmyDetail, BuildTrainArmy, BuildTrainArmySharedDisplay"]}
    return result


def walk(nodes):
    for value in nodes:
        yield value
        yield from walk(value.get("children", []))


def verify(specs):
    required = {
        "train_army_detail_unavailable": {"TrainArmyDetailMissing", "TrainArmyDetailBackButton"},
        "train_army_catalog_unavailable": {"TrainArmyMissingCatalog"},
        "train_army_model_unavailable": {"TrainArmyUnavailablePortrait", "TrainParameterStaticPreview"},
    }
    for key, spec in specs.items():
        nodes = list(walk(spec["nodes"]))
        names = [item["name"] for item in nodes]
        assert len(names) == len(set(names)), key
        assert required[key] <= set(names), key
        for item in nodes:
            if item["class"] == "/Script/UMG.TextBlock":
                assert item["fonts"] == {"Font": "EngineDefault"}
    assert specs["train_army_detail_unavailable"]["nodes"][0]["slot"]["props"]["ZOrder"] == "10"
    assert specs["train_army_catalog_unavailable"]["nodes"][0]["slot"]["props"]["ZOrder"] == "7"
    assert specs["train_army_model_unavailable"]["nodes"][0]["slot"]["props"]["ZOrder"] == "6"
    source = (ROOT / "Source/Rok/RokUnityFeatureScreenWidget.cpp").read_text(encoding="utf-8-sig")
    for state in ("DetailMissing", "CatalogMissing", "ModelMissing"):
        assert 'TryBuildTrainArmyUnavailableState(TEXT("' + state + '")' in source, state
    helper = (ROOT / "Source/Rok/RokTrainArmyUnavailableStates.cpp").read_text(encoding="utf-8-sig")
    atlas = '/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_troops_head.rok_fullscreen_troops_head'
    assert atlas in source and atlas in helper, "static preview must keep the existing troop atlas"
    assert 'TEXT("TrainArmyDetailBackButton"), 1951' in helper, "detail back action must remain unchanged"
    print("Three production training missing-state branches: binding, names, font roles and z-order passed.")


def main():
    specs = build()
    verify(specs)
    SPEC_ROOT.mkdir(parents=True, exist_ok=True)
    for key, spec in specs.items():
        target = SPEC_ROOT / (key + ".json")
        target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(target)


if __name__ == "__main__":
    main()
