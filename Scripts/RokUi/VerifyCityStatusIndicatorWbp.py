"""Read-only UE Python verification of the generated city status indicator WBP.

Run in the project's headless UE 5.8 editor after GeneratePage. This does not
compile C++, compile Blueprints, save assets, create game state or open a map.
"""
from pathlib import Path

import unreal


ASSET = "/Game/RokUI/Hud/WBP_RokCityStatusIndicator"
ROOT_NAME = "CityStatusIndicatorRoot"
EXPECTED = {
    "CityStatusPop": (unreal.CanvasPanel, ROOT_NAME),
    "CityStatusText": (unreal.RokUiTextBlock, "CityStatusPop"),
    "CityHospitalMark": (unreal.Image, "CityStatusPop"),
    "CityStatusAction": (unreal.RokUnityIndexedButton, "CityStatusPop"),
    "CityUpgradeFlag": (unreal.Image, ROOT_NAME),
    "CityStatusIdleLarge": (unreal.Image, ROOT_NAME),
    "CityStatusIdleSmall": (unreal.Image, ROOT_NAME),
}


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    if project != Path(__file__).resolve().parents[2] or not (project / "Rok.uproject").is_file():
        raise RuntimeError(f"Unexpected Rok project: {project}")
    blueprint = unreal.EditorAssetLibrary.load_asset(ASSET)
    if not isinstance(blueprint, unreal.WidgetBlueprint) or not blueprint.generated_class():
        raise RuntimeError(f"Expected generated Widget Blueprint: {ASSET}")
    for outer in (blueprint, blueprint.generated_class()):
        tree = unreal.find_object(outer, "WidgetTree")
        if not tree:
            raise RuntimeError(f"Missing template WidgetTree: {outer.get_path_name()}")
        for name, (kind, parent) in EXPECTED.items():
            widget = unreal.find_object(tree, name)
            if not isinstance(widget, kind):
                raise RuntimeError(f"Wrong/missing {kind.__name__}: {name}")
            slot = unreal.WidgetLayoutLibrary.slot_as_canvas_slot(widget)
            if not slot or widget.get_parent().get_name() != parent:
                raise RuntimeError(f"Wrong Canvas parent/slot: {name}")
            size = slot.get_size()
            if size.x <= 0 or size.y <= 0:
                raise RuntimeError(f"Non-positive Designer size: {name}")
        background = unreal.find_object(tree, "CityStatusBackground")
        if not isinstance(background, unreal.CanvasPanel) or background.get_children_count() != 9:
            raise RuntimeError("Missing nine-patch background")
        label = unreal.find_object(tree, "CityStatusText")
        if label.get_editor_property("font_role") != unreal.RokUiFontRole.ENGINE_DEFAULT:
            raise RuntimeError("CityStatusText must retain the existing HUD font role")
        for name in ("CityStatusPop", "CityHospitalMark", "CityUpgradeFlag", "CityStatusIdleLarge", "CityStatusIdleSmall"):
            if unreal.find_object(tree, name).get_editor_property("visibility") != unreal.SlateVisibility.COLLAPSED:
                raise RuntimeError(f"Runtime-owned state must start hidden: {name}")
    unreal.log("ROK_CITY_STATUS_WBP_VERIFIED trees=2 bindings=7 background_patches=9 read_only=true")


main()
