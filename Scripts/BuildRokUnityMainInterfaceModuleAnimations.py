"""Extract and verify the source curves used by world HUD zoom animations."""
from pathlib import Path
import csv
import hashlib
import json
import re
import sys

root = Path(__file__).resolve().parents[1]
animation_dir = root / "ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation"
records = []
for side in ("Left", "Right"):
    for state in ("On", "Off"):
        source = animation_dir / f"UA_MainIF_{side}_{state}.anim"
        text = source.read_text()
        # Unity duplicates curves under m_EditorCurves; extract runtime curves only.
        runtime = text.split("  m_FloatCurves:\n", 1)[1].split("  m_PPtrCurves:", 1)[0]
        curves = {}
        for block in runtime.split("  - curve:")[1:]:
            attribute = re.search(r"    attribute: (.+)", block)[1]
            keys = re.findall(r"time: (\S+)\s+value: (\S+)\s+inSlope: (\S+)\s+outSlope: (\S+)", block)
            curves[attribute] = [[float(value) for value in key] for key in keys]
        opening = state == "On"
        expected_x = ([-80, 20, 0] if opening else [0, 30, -300]) if side == "Left" else ([100, -20, 0] if opening else [0, -20, 100])
        assert curves["m_AnchoredPosition.x"] == [
            [0, expected_x[0], 0, 0], [0.15 if opening else 0.1, expected_x[1], 0, 0], [0.25, expected_x[2], 0, 0]]
        assert curves["m_Alpha"] == [[0, 0 if opening else 1, 0, 0], [0.25, 1 if opening else 0, 0, 0]]
        assert all(key[1] == int(opening) for key in curves["m_BlocksRaycasts"])
        records.append({"source": source.relative_to(root).as_posix(), "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                        "x": curves["m_AnchoredPosition.x"], "alpha": curves["m_Alpha"], "blocks_raycasts": opening})
table = root / "ref/config-tables/编译客户端配置/CSV/OpenLodUi.csv"
with table.open(encoding="utf-8-sig", newline="") as stream:
    rows = list(csv.DictReader(stream))[1:]
modules = [row for row in rows if row["uiName"] in ("CurAge", "Events", "HotTask", "Features", "PlayerResources", "Mail")]
assert len(modules) == 6
for row in modules:
    assert [row[column] for column in ("City", "Tactical", "TacticsToStrategy_1", "TacticsToStrategy_2", "Strategic", "NationWide", "Continental")] == ["1", "1", "0", "0", "0", "0", "0"]
    assert row["unArbAnimator"] == ("UA_MainIF_Left_" if row["uiName"] == "HotTask" else "UA_MainIF_Right_")
strategic = [row for row in rows if row["uiName"] in ("LodMenu", "Server")]
assert len(strategic) == 2
for row in strategic:
    assert [row[column] for column in ("City", "Tactical", "TacticsToStrategy_1", "TacticsToStrategy_2", "Strategic", "NationWide", "Continental")] == ["0", "0", "0", "0", "1", "1", "1"]
    assert row["unArbAnimator"] == ("UA_MainIF_Left_" if row["uiName"] == "LodMenu" else "UA_MainIF_Right_")
camera = root / "ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv"
with camera.open(encoding="utf-8-sig", newline="") as stream:
    camera_rows = {row["ID"]: row for row in csv.DictReader(stream)}
    assert camera_rows["dispatch"]["dxf"] == "2997"
    assert camera_rows["TacticsToStrategy2"]["dxf"] == "5400"
    assert camera_rows["limit_max"]["dxf"] == "72750"
city_source = root / "ref/client-unity/Assets/BundleAssets/Scene/Map_building/map_building_lod/city_lod3.prefab"
city_curve = city_source.read_text().split("  m_scale_explore_curve:", 1)[1].split("  m_PreInfinity:", 1)[0]
city_keys = [[float(value) for value in key] for key in re.findall(
    r"time: (\S+)\s+value: (\S+)\s+inSlope: (\S+)\s+outSlope: (\S+)", city_curve)]
assert [(key[0], key[1]) for key in city_keys] == [(3000, 0), (3000.001, 6), (5400, 6.4), (44939.875, 45), (72720, 55)]
assert city_keys[2][3] == city_keys[3][2] == 0.00097622967
assert city_keys[3][3] == city_keys[4][2] == 0.0003599696
result = {"schema": "rok.unity.main_interface_module_animations.v1", "curves": records, "modules": modules,
          "strategic_modules": strategic, "strategic_min_exclusive_dxf": 5400,
          "own_city_explore_curve": city_keys, "city_source": city_source.relative_to(root).as_posix(),
          "city_source_sha256": hashlib.sha256(city_source.read_bytes()).hexdigest(),
          "table_sha256": hashlib.sha256(table.read_bytes()).hexdigest(), "camera_sha256": hashlib.sha256(camera.read_bytes()).hexdigest(),
          "tactical_max_dxf": 2997, "ue": ["RokMainInterfaceModuleAnimation.h", "RokMainHudWidget.cpp", "RokWorldMapHudWidget.cpp"]}
output = root / "Saved/RokUnityMigration/unity_main_interface_module_animations.json"
if "--verify" in sys.argv:
    assert json.loads(output.read_text(encoding="utf-8")) == result, "Stale source animation evidence"
else:
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
print("PASS: four clips, eight module rows, tactical/strategic thresholds, own-city far curve, source hashes")
