"""Author detached commander divider bounds and biography preview rules.

Preserve the existing divider and biography Widget Blueprints. The template is
never mounted; C++ reads its named markers only in their successful WBP paths.
"""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
NAME = "commander_text_layout"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_CommanderTextLayout"


def build():
    extent_slot = canvas_slot(0, 0, 304, 38)
    extent_slot["props"]["bAutoSize"] = "False"
    return {
        "version": 1,
        "label": "Commander divider and biography text rules",
        "root": "CommanderTextLayoutRoot",
        "nodes": [
            {
                "name": "DividerExtent",
                "class": "/Script/UMG.Border",
                "props": {
                    "BrushColor": "(R=.20,G=.40,B=.55,A=.65)",
                    "ToolTipText": "Divider outer size; keep fixed Canvas anchors and Auto Size disabled.",
                },
                "slot": extent_slot,
            },
            {
                "name": "BiographyCharacterLimit",
                "class": "/Script/UMG.SpinBox",
                "props": {
                    "Value": "58",
                    "MinFractionalDigits": "0",
                    "MaxFractionalDigits": "0",
                    "Delta": "1",
                    "bOverride_MinValue": "True",
                    "MinValue": "0",
                    "ToolTipText": "Biography preview UTF-16 code-unit limit; 0 shows the full text. Invalid values retain 58.",
                },
                "slot": canvas_slot(0, 64, 180, 36),
            },
            {
                "name": "BiographyEllipsis",
                "class": "/Script/UMG.TextBlock",
                "fonts": {"Font": "EngineDefault"},
                "props": {
                    "Text": "…",
                    "ToolTipText": "Suffix appended only when biography text is shortened. Empty text adds no suffix.",
                },
                "slot": canvas_slot(0, 116, 180, 36),
            },
        ],
        "notes": [
            "Create-only detached template: preserve the existing commander divider and biography Widget Blueprints.",
            "DividerExtent Canvas slot width/height supplies the successful divider WBP's outer extent; its position and color are preview only.",
            "BiographyCharacterLimit Content/Value is an integer count using the existing FString Len/Left semantics; 0 disables truncation.",
            "Negative, fractional, non-finite and out-of-int32-range limits retain 58. Missing markers retain their individual native defaults.",
            "BiographyEllipsis Text is appended only to shortened text; empty text is a valid no-suffix choice.",
            "Defaults 304 x 38, 58 and the ellipsis preserve the existing UE presentation, not Unity geometry or truncation rules.",
            "The full native branches and -RokUiLegacy retain the original 304 x 38 and 58-plus-ellipsis behavior.",
            "Unity's Partline root is 304 x 60; its story logic assigns full localized hero l_desID. This template does not change that reference snapshot.",
        ],
        "references": [
            "Source/Rok/RokFeatureCommander.inl: BuildCommander divider and biography branches",
            "Scripts/RokUi/PrepareCommanderDivider.py",
            "Scripts/RokUi/PrepareCommanderDetails.py",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_Item_CaptainPartline.prefab",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Captain/CaptainMediator.cs",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_CaptainStroy_SubView.cs",
        ],
    }


if __name__ == "__main__":
    spec = build()
    target = ROOT / "Saved/RokUi/Specs" / f"{NAME}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {NAME} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={spec['root']}")
