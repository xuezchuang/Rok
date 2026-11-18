"""Author the chat contact-fold endpoint without changing the visual page."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_ChatContactsLayout"
SPEC = "chat_contacts_layout"


def build():
    return {
        "version": 1,
        "label": "Chat contact fold endpoint",
        "root": "ChatContactsLayoutRoot",
        "nodes": [{
            "name": "ContactsCollapsedContent",
            "class": "/Script/UMG.Border",
            "props": {
                "BrushColor": "(R=.20,G=.40,B=.55,A=.65)",
                "RenderTransform": "(Translation=(X=-330,Y=0),Scale=(X=1,Y=1))",
            },
            "slot": {
                "class": "/Script/UMG.CanvasPanelSlot",
                "props": {
                    "LayoutData": (
                        "(Offsets=(Left=350,Top=0,Right=330,Bottom=80),"
                        "Anchors=(Minimum=(X=0,Y=0),Maximum=(X=0,Y=0)),"
                        "Alignment=(X=0,Y=0))"
                    ),
                },
            },
        }],
        "notes": [
            "Only ContactsCollapsedContent RenderTransform.Translation.X supplies the folded content endpoint.",
            "Marker slot geometry, color and other transforms are Designer preview only; the template never enters the live tree.",
            "The opened endpoint remains zero. C++ retains 0.2 seconds, InCubic easing and interrupted-toggle continuity.",
            "Missing layout, missing marker or non-finite endpoint retains the native -330 fallback.",
            "The visual contact background and emoji origin do not define the contact-fold travel.",
        ],
        "references": [
            "Source/Rok/RokChatAnimation.h: FRokChatContactAnimation",
            "Source/Rok/RokUnityFeatureScreenWidget.cpp: UpdateWindowAnimationForMode",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Chat/UI_Win_ChatMediator.cs: OnHideContactList / OnPopContactList",
        ],
    }


if __name__ == "__main__":
    data = build()
    destination = ROOT / "Saved/RokUi/Specs" / f"{SPEC}.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Rok.UiWbp.GeneratePage {ASSET} {SPEC} -ParentClass=/Script/Rok.RokUiPageWidget -RootName={data['root']}")
