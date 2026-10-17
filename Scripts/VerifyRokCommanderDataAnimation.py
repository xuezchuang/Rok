"""Check the commander data-panel source chain and exact unweighted curve keys.

This is source verification only. Run Rok.Ui.Animation.Commander.DataPanel in
UE5.8 and capture the real HUD commander entry for runtime/visual acceptance.
"""
import json
import hashlib
from pathlib import Path

from BuildRokUnityUiAnimationManifest import Builder

ROOT = Path(__file__).resolve().parents[1]
UI = "ref/client-unity/Assets/BundleAssets/UI/"
SCRIPT = "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/"


def main():
    builder = Builder()
    clip_path = UI + "UIAnimation/UA_IFCaptain_Right_On.anim"
    clip = builder.clip(clip_path)
    controller = builder.read(UI + "UIAnimation/UA_IFCaptain_Left.controller")
    non_arab = controller.split("m_Name: OpenNoArb", 1)[1].split("--- !u!", 1)[0]
    assert clip["source_guid"] in non_arab
    builder.evidence(SCRIPT + "Common/Logic/UI_Item_CaptainData_SubView.cs",
                     ['public void Open()', 'LanguageUtils.IsArabic()', '"OpenNoArb"'])
    # This legacy mediator is not UTF-8. Verify its ASCII call without changing
    # or lossy-decoding reference source bytes.
    mediator_path = SCRIPT + "Captain/CaptainMediator.cs"
    mediator = (ROOT / mediator_path).read_bytes()
    assert b"view.m_UI_Item_CaptainData.Open();" in mediator
    builder.sources[mediator_path] = {"path": mediator_path,
        "sha256": hashlib.sha256(mediator).hexdigest(), "bytes": len(mediator)}
    times = [0, 0.083333336, 0.31666666, 0.33333334]
    values = {"m_AnchoredPosition.x": [200, 200, 0, 0], "m_Alpha": [0, 0, 1.1, 1]}
    assert len(clip["tracks"]) == 2
    for track in clip["tracks"]:
        assert track["path"] == ""
        assert [k["time"] for k in track["keys"]] == times
        assert [k["value"] for k in track["keys"]] == values[track["attribute"]]
        assert all(k["weighted_mode"] == 0 for k in track["keys"])
        assert track["keys"][1]["out_slope"] == 0
        assert track["keys"][2]["in_slope"] == 0
    assert not clip["loop"]
    report = {
        "source_verified": True,
        "clip": clip,
        "sources": builder.sources,
        "ue_target": "Source/Rok/RokUnityFeatureScreenWidget.cpp",
        "runtime_entry": "Main HUD commander button -> OpenCommander",
        "automation_test": "Rok.Ui.Animation.Commander.DataPanel",
        "authority": "Presentation only; hero ownership and actions unchanged",
        "remaining_gates": ["UE5.8 build and automation", "HUD runtime visual comparison with Unity"],
    }
    output = ROOT / "Saved/RokUnityMigration/commander_data_animation_source_verify.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"source_verified": True, "tracks": len(clip["tracks"]), "output": str(output)}))


if __name__ == "__main__":
    main()
