"""Verify saved page templates against their single-source layout specs in UE."""

import json
import os
from pathlib import Path

import unreal


def verify_children(tree, panel, specs):
    if not isinstance(panel, unreal.PanelWidget) or panel.get_children_count() != len(specs):
        raise RuntimeError(f"Unexpected child count for {panel.get_path_name()}")
    count = 0
    for index, spec in enumerate(specs):
        widget = panel.get_child_at(index)
        if widget.get_name() != spec["name"] or unreal.find_object(tree, spec["name"]) != widget:
            raise RuntimeError(f"Missing expected widget {spec['name']} under {panel.get_path_name()}")
        font_role = spec.get("fonts", {}).get("Font")
        widget_class = spec["class"].rsplit(".", 1)[-1]
        if widget_class == "TextBlock" and font_role in ("EngineDefault", "UnitySource"):
            widget_class = "RokUiTextBlock"
            expected_role = {"EngineDefault": unreal.RokUiFontRole.ENGINE_DEFAULT,
                             "UnitySource": unreal.RokUiFontRole.UNITY_SOURCE}[font_role]
            if widget.get_editor_property("font_role") != expected_role:
                raise RuntimeError(f"Wrong font role for {widget.get_path_name()}")
        if widget.get_class().get_name() != widget_class:
            raise RuntimeError(f"Expected {widget_class} at {widget.get_path_name()}")
        if spec.get("slot", {}).get("class") == "/Script/UMG.CanvasPanelSlot":
            if not unreal.WidgetLayoutLibrary.slot_as_canvas_slot(widget):
                raise RuntimeError(f"Missing canvas slot for {widget.get_path_name()}")
        count += 1
        if spec.get("children"):
            count += verify_children(tree, widget, spec["children"])
    return count


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    if project != Path(__file__).resolve().parents[2] or not (project / "Rok.uproject").is_file():
        raise RuntimeError(f"Unexpected Rok project: {project}")
    assets = json.loads(os.environ["ROK_VERIFY_WBP_SPECS"])
    if not assets:
        raise ValueError("Specify the new Widget Blueprint paths and spec names to verify")
    for asset, spec_name in assets.items():
        if not asset.startswith("/Game/RokUI/") or Path(spec_name).name != spec_name:
            raise ValueError(f"Invalid asset or spec name: {asset} {spec_name}")
        spec_path = project / "Saved" / "RokUi" / "Specs" / f"{spec_name}.json"
        spec = json.loads(spec_path.read_text(encoding="utf-8-sig"))
        blueprint = unreal.EditorAssetLibrary.load_asset(asset)
        if not isinstance(blueprint, unreal.WidgetBlueprint) or not blueprint.generated_class():
            raise RuntimeError(f"Expected a saved Widget Blueprint: {asset}")
        for outer in (blueprint, blueprint.generated_class()):
            tree = unreal.find_object(outer, "WidgetTree")
            root = unreal.find_object(tree, spec["root"]) if tree else None
            if not root:
                raise RuntimeError(f"Missing {spec['root']} in {outer.get_path_name()}")
            count = verify_children(tree, root, spec["nodes"])
        unreal.log(f"ROK_WBP_LAYOUT_VERIFIED {asset} trees=2 widgets={count}")


main()
