"""Source evidence for resource badges and the separate strategic resource view."""
from pathlib import Path
import hashlib
import json
import re
import sys

root = Path(__file__).resolve().parents[1]
paths = {
    "ui_root": "ref/client-unity/Assets/Skyunion/RunTime/UIManager/Resources/UIRoot.prefab",
    "badge": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIWorldObjectPop/UI_Tip_WorldObjectRssLevel.prefab",
    "projection": "ref/client-unity/Assets/Scripts/Client/MapManager/MapElementUI.cs",
    "lod_owner": "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GlobalViewLevelMediator.cs",
    "strategic_owner": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_LodMenu_SubView.cs",
}
texts = {name: (root / path).read_text(encoding="utf-8-sig", errors="replace") for name, path in paths.items()}
curve = texts["ui_root"].split("  RssTitleCurve:", 1)[1].split("  m_PreInfinity:", 1)[0]
keys = [[float(value) for value in key] for key in re.findall(
    r"time: (\S+)\s+value: (\S+)\s+inSlope: (\S+)\s+outSlope: (\S+)", curve)]
assert keys == [[0, -0.7, 0, 0], [3000, -0.8, 0, 0]]
assert "m_AnchoredPosition: {x: 0, y: -30}" in texts["badge"]
assert "m_SizeDelta: {x: 60, y: 33}" in texts["badge"]
rss_hud = texts["lod_owner"].split("var RssHud_1 =", 1)[1].split("var RssHud_2", 1)[0]
assert "ElementUIType.RssTitle" in rss_hud and "maxLodLevel = MapViewLevel.TacticsToStrategy_1" in rss_hud
rss_map = texts["strategic_owner"].split("public void OnRssMiniMap(bool show)", 1)[1]
assert 'Instantiate("resource_minimap"' in rss_map
assert "new Vector3(3600, 0, 3600)" in rss_map
assert "lod.IsExplore = true" in rss_map
result = {
    "schema": "rok.unity.resource_presentation.v1",
    "sources": {name: {"path": path, "sha256": hashlib.sha256((root / path).read_bytes()).hexdigest()} for name, path in paths.items()},
    "rss_title_keys": keys, "badge_size": [60, 33], "badge_center_ui_down": 30,
    "badge_scale": "UIRoot.AllUIScaleCurve plus MapElementUI viewport-depth correction",
    "badge_max_dxf": 4500,
    "ue_badge_target": "Source/Rok/RokWorldMapHudWidget.cpp",
    "strategic_resource_layer": {
        "status": "runtime_layer_implemented_visual_parity_pending", "resource_minimap_position": [3600, 0, 3600],
        "ue_target": "Source/Rok/RokStrategicResourceLayer.cpp",
        "source_manifest": "Saved/RokUnityMigration/unity_strategic_resources.json",
        "point_source": "MapFixPoint via m_guildRssRegins; not normal nearby ResourceGathering entities",
        "point_scaling": "LevelDetailScale.IsExplore=true; use m_scale_explore_curve",
    },
}
output = root / "Saved/RokUnityMigration/unity_resource_presentation.json"
if "--verify" in sys.argv:
    assert json.loads(output.read_text(encoding="utf-8")) == result, "Stale evidence"
else:
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("PASS: resource badge curve/layout/source hashes; distinct strategic runtime layer recorded")
