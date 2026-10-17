import json
import re
from pathlib import Path
from rok_data_paths import DATA_ROOT, resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
OUTPUT_JSON = DATA_ROOT / "unity_ui_catalog.json"
OUTPUT_MD = OUTPUT_DIR / "unity_ui_catalog.md"


SOURCE_SLICES = [
    {
        "id": "main_interface",
        "name": "UI_IF_MainInterface",
        "prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIMainCity/UI_IF_MainInterface.prefab",
        "view": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/MainInterface/MainInterfaceView.cs",
        "mediator": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/MainInterface/MainInterfaceMediator.cs",
        "ue_target": "Source/Rok/RokMainHudWidget.cpp",
    },
    {
        "id": "building_menu",
        "name": "UI_Pop_BuildingMenu",
        "prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_map/UIMainCity/UI_Pop_BuildingMenu.prefab",
        "view": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingCityHud/BuildingMenuView.cs",
        "mediator": "ref/client-unity/Assets/Scripts/Hotfix/MVC/GlobalMediator/GlobalCityMenuHudMediator.cs",
        "ue_target": "Source/Rok/RokBuildingPanelWidget.cpp",
    },
    {
        "id": "build_city",
        "name": "UI_Win_BuildCity",
        "prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildCity/UI_Win_BuildCity.prefab",
        "view": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildCity/BuildCityView.cs",
        "mediator": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildCity/BuildCityMediator.cs",
        "ue_target": "Source/Rok/RokUnityFeatureScreenWidget.cpp",
    },
    {
        "id": "worker_queue",
        "name": "UI_Win_Worker",
        "prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIWorker/UI_Win_Worker.prefab",
        "view": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIWorker/UI_Win_WorkerView.cs",
        "mediator": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIWorker/UI_Win_WorkerMediator.cs",
        "ue_target": "Source/Rok/RokUnityFeatureScreenWidget.cpp",
    },
    {
        "id": "quest_task",
        "name": "UI_Win_Quest",
        "prefab": "ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_task/UIQuest/UI_Win_Quest.prefab",
        "view": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestView.cs",
        "mediator": "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/UIQuest/UI_Win_QuestMediator.cs",
        "ue_target": "Source/Rok/RokUnityFeatureScreenWidget.cpp",
    },
]


UI_TEXTURE_SOURCES = [
    "ref/client-unity/Assets/BundleAssets/UI/Bang/main_top.png",
    "ref/client-unity/Assets/BundleAssets/UI/Bang/main_bottom.png",
    "ref/client-unity/Assets/BundleAssets/UI/UIEffect/UE_ResFly_food/UE_ResFly_food.png",
    "ref/client-unity/Assets/BundleAssets/UI/UIEffect/UE_ResFly_wood/UE_ResFly_wood.png",
    "ref/client-unity/Assets/BundleAssets/UI/UIEffect/UE_ResFly_stone/UE_ResFly_stone.png",
    "ref/client-unity/Assets/BundleAssets/UI/UIEffect/UE_ResFly_gold/UE_ResFly_gold.png",
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_mainmenu.png",
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_map/ui_hud.png",
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_build/ui_build.png",
    "ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_task/task_icon.png",
]


UE_TEXTURE_TARGETS = [
    "Content/RokPrototype/UI/Processed/rok_main_top.uasset",
    "Content/RokPrototype/UI/Processed/rok_main_bottom.uasset",
    "Content/RokPrototype/UI/Processed/rok_food_icon.uasset",
    "Content/RokPrototype/UI/Processed/rok_wood_icon.uasset",
    "Content/RokPrototype/UI/Processed/rok_stone_icon.uasset",
    "Content/RokPrototype/UI/Processed/rok_gold_icon.uasset",
    "Content/RokPrototype/UI/Processed/rok_upgrade_icon.uasset",
]


def read_text(relative_path):
    path = resolve(relative_path)
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="ignore")


def exists(relative_path):
    return resolve(relative_path).exists()


def parse_main_modules():
    source = read_text("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/MainInterface/MainInterfaceMediator.cs")
    match = re.search(r"public\s+enum\s+EnumMainModule\s*\{(?P<body>.*?)\n\s*\}", source, re.S)
    if not match:
        return []

    modules = []
    for line in match.group("body").splitlines():
        item = re.match(r"\s*(\w+)\s*=", line)
        if not item:
            continue
        name = item.group(1)
        if name not in {"None", "All"}:
            modules.append(name)
    return modules


def prefab_stats(relative_path):
    source = read_text(relative_path)
    if not source:
        return {
            "exists": False,
            "named_object_count": 0,
            "sprite_reference_count": 0,
        }

    names = [
        line.split("m_Name:", 1)[1].strip()
        for line in source.splitlines()
        if "m_Name:" in line and line.split("m_Name:", 1)[1].strip()
    ]
    return {
        "exists": True,
        "named_object_count": len(names),
        "sprite_reference_count": source.count("m_Sprite:"),
        "sample_names": names[:24],
    }


def build_catalog():
    slices = []
    for item in SOURCE_SLICES:
        enriched = dict(item)
        enriched["source_exists"] = {
            "prefab": exists(item["prefab"]),
            "view": exists(item["view"]),
            "mediator": exists(item["mediator"]),
        }
        enriched["ue_target_exists"] = exists(item["ue_target"])
        enriched["prefab_stats"] = prefab_stats(item["prefab"])
        slices.append(enriched)

    catalog = {
        "schema": "rok.unity_ui_catalog.v1",
        "source_root": "ref/client-unity/Assets/BundleAssets/UI",
        "runtime_level": "/Game/Maps/L_RokUnityGameAppCity",
        "main_interface_modules": parse_main_modules(),
        "ui_slices": slices,
        "ui_texture_sources": [
            {
                "path": path,
                "exists": exists(path),
            }
            for path in UI_TEXTURE_SOURCES
        ],
        "ue_texture_targets": [
            {
                "path": path,
                "exists": exists(path),
            }
            for path in UE_TEXTURE_TARGETS
        ],
        "accepted_runtime_targets": [
            "URokMainHudWidget",
            "URokBuildingPanelWidget",
            "URokBuildingUpgradeSubsystem",
            "URokUnityGameplayCatalogSubsystem",
            "URokUnityRuntimeStateSubsystem",
        ],
    }
    return catalog


def write_markdown(catalog):
    lines = [
        "# Rok Unity UI Catalog",
        "",
        f"- Schema: `{catalog['schema']}`",
        f"- Runtime level: `{catalog['runtime_level']}`",
        f"- Main interface module count: {len(catalog['main_interface_modules'])}",
        f"- Main interface modules: {', '.join(catalog['main_interface_modules'])}",
        "",
        "## Runtime Slice",
        "",
        "| Slice | Unity prefab | UE target | Objects | Sprites |",
        "| --- | --- | --- | ---: | ---: |",
    ]

    for item in catalog["ui_slices"]:
        stats = item["prefab_stats"]
        lines.append(
            "| {name} | `{prefab}` | `{target}` | {objects} | {sprites} |".format(
                name=item["name"],
                prefab=item["prefab"],
                target=item["ue_target"],
                objects=stats.get("named_object_count", 0),
                sprites=stats.get("sprite_reference_count", 0),
            )
        )

    lines.extend([
        "",
        "## Texture Sources",
        "",
    ])
    for item in catalog["ui_texture_sources"]:
        status = "ok" if item["exists"] else "missing"
        lines.append(f"- {status}: `{item['path']}`")

    lines.extend([
        "",
        "## UE Texture Targets",
        "",
    ])
    for item in catalog["ue_texture_targets"]:
        status = "ok" if item["exists"] else "missing"
        lines.append(f"- {status}: `{item['path']}`")

    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    catalog = build_catalog()
    OUTPUT_JSON.write_text(json.dumps(catalog, indent=2, ensure_ascii=False), encoding="utf-8")
    write_markdown(catalog)
    print(f"Wrote {OUTPUT_JSON}")
    print(f"Wrote {OUTPUT_MD}")


if __name__ == "__main__":
    main()
