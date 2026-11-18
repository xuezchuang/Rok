"""Prepare the hospital's two bed illustrations and civilization marks."""

import json
from pathlib import Path

from BuildTrainResourceCostLayoutSpec import canvas_slot


ROOT = Path(__file__).resolve().parents[2]
KEY = "hospital_art_styles"
ASSET = "/Game/RokUI/Feature/WBP_RokFeature_HospitalArtStyles"
ROOT_NAME = "HospitalArtStylesRoot"
COMMON = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common"
BEDS = (
    ("HospitalBedEmptyStyle", "bed_empty.png", "T_RokHospitalBedEmpty", (512, 298)),
    ("HospitalBedFullStyle", "bed_full.png", "T_RokHospitalBedFull", (431, 437)),
)


def image(name, resource, size, position, uv=""):
    resource_ref = f"{resource}.{resource.rsplit('/', 1)[1]}"
    return {
        "name": name,
        "class": "/Script/UMG.Image",
        "props": {
            "Visibility": "HitTestInvisible",
            "Brush": (
                f"(DrawAs=Image,ImageType=FullColor,ImageSize=(X={size[0]},Y={size[1]}),"
                f'ResourceObject="/Script/Engine.Texture2D\'{resource_ref}\'"{uv})'
            ),
        },
        "slot": canvas_slot(*position, *size),
    }


def build():
    imports = []
    nodes = []
    for index, (name, filename, texture_name, size) in enumerate(BEDS):
        asset = f"/Game/RokUI/Feature/Textures/{texture_name}"
        imports.append({"source": f"Content/RokData/Hospital/{filename}", "asset": asset, "size": list(size)})
        nodes.append(image(name, asset, size, (index * 536, 0)))

    for index, left in enumerate((1074, 1029)):
        uv = (f",UVRegion=(Min=(X={left/2048},Y={701/1024}),"
              f"Max=(X={(left+43)/2048},Y={744/1024}),bIsValid=True)")
        nodes.append(image(f"HospitalCivilizationMark{index}", COMMON, (43, 43), (index * 67, 462), uv))

    return {
        "version": 1,
        "label": "Hospital bed and civilization art",
        "root": ROOT_NAME,
        "asset": ASSET,
        "texture_imports": imports,
        "nodes": nodes,
        "notes": [
            "This detached page supplies the bed brush resources and UVs, and both civilization mark brushes.",
            "The Hospital page owns the original slots, bed mirroring, and bed tint. C++ selects the appropriate art using the hospital and account states.",
        ],
        "references": [
            "Source/Rok/RokUnityFeatureScreenWidget.cpp: BuildHospital / BuildHospitalPage",
            "Scripts/BuildRokHospitalArt.py",
            "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIHospital/UI_Win_Hospital.prefab",
            "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Hospital/HospitalMediator.cs: LoadFinish",
        ],
    }


if __name__ == "__main__":
    target = ROOT / "Saved/RokUi/Specs" / f"{KEY}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(target)
