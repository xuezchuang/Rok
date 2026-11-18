"""Update the resource labels in the two existing Rok HUD Widget Blueprints."""

import os
from math import isclose
from pathlib import Path

import unreal


ASSETS = ("WBP_RokCityHud", "WBP_RokTacticalHud")
RESOURCES = ("Food", "Wood", "Stone", "Gold", "Gem")
# Points, colour, shadow offset, shadow alpha, slot position and slot size.
LEGACY = (20.0, (0.96, 0.95, 0.90, 1.0), (1.0, 1.0), 0.7, (43, 7), (88, 30))
STYLE = (16.5, (244 / 255, 244 / 255, 244 / 255, 1.0), (0, 2), 180 / 255, (37.54, 7), (92.32, 30))


def matches(widget, slot, role, style):
    size, colour, shadow, shadow_alpha, position, extent = style
    font = widget.get_editor_property("font")
    actual_colour = widget.get_editor_property("color_and_opacity").get_editor_property("specified_color")
    actual_shadow = widget.get_editor_property("shadow_color_and_opacity")
    offset = widget.get_editor_property("shadow_offset")
    pos = slot.get_position()
    box = slot.get_size()
    expected = (size, *colour, 0, 0, 0, shadow_alpha, *shadow, *position, *extent)
    actual = (font.get_editor_property("size"), actual_colour.r, actual_colour.g, actual_colour.b,
              actual_colour.a, actual_shadow.r, actual_shadow.g, actual_shadow.b, actual_shadow.a,
              offset.x, offset.y, pos.x, pos.y, box.x, box.y)
    return (widget.get_editor_property("font_role") == role
            and widget.get_editor_property("justification") == unreal.TextJustify.CENTER
            and all(isclose(a, b, abs_tol=0.0001) for a, b in zip(actual, expected)))


def get_labels(outer):
    tree = unreal.find_object(outer, "WidgetTree")
    if not tree:
        raise RuntimeError(f"Missing WidgetTree in {outer.get_path_name()}")
    for resource in RESOURCES:
        name = resource + "ResourceValue"
        widget = unreal.find_object(tree, name)
        if not isinstance(widget, unreal.RokUiTextBlock):
            raise RuntimeError(f"Expected a Rok text widget at {tree.get_path_name()}.{name}")
        slot = unreal.WidgetLayoutLibrary.slot_as_canvas_slot(widget)
        if not slot or widget.get_parent().get_name() != resource + "Resource":
            raise RuntimeError(f"Expected a resource-cell canvas slot for {widget.get_path_name()}")
        yield widget, slot


def verify(blueprint, style, role):
    for outer in (blueprint, blueprint.generated_class()):
        for widget, slot in get_labels(outer):
            if not matches(widget, slot, role, style):
                raise RuntimeError(f"Unexpected resource text style: {widget.get_path_name()}")


def apply_style(widget, slot, role):
    size, colour, shadow, shadow_alpha, position, extent = STYLE
    font = widget.get_editor_property("font")
    font.set_editor_property("size", size)
    widget.set_editor_property("font", font)
    widget.set_editor_property("font_role", role)
    widget.set_editor_property("color_and_opacity", unreal.SlateColor(specified_color=unreal.LinearColor(*colour)))
    widget.set_editor_property("shadow_offset", unreal.Vector2D(*shadow))
    widget.set_editor_property("shadow_color_and_opacity", unreal.LinearColor(0, 0, 0, shadow_alpha))
    slot.set_position(unreal.Vector2D(*position))
    slot.set_size(unreal.Vector2D(*extent))


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    if project != Path(__file__).resolve().parents[2] or not (project / "Rok.uproject").is_file():
        raise RuntimeError(f"Unexpected Rok project: {project}")
    mode = os.environ["ROK_HUD_RESOURCE_TEXT_MODE"]
    if mode not in ("Inspect", "Apply", "Verify"):
        raise ValueError(f"Unknown HUD resource text mode: {mode}")

    intended_role = unreal.RokUiFontRole.UNITY_SOURCE
    legacy_role = unreal.RokUiFontRole.ENGINE_DEFAULT
    pending = []
    for asset in ASSETS:
        path = f"/Game/RokUI/Hud/{asset}"
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(blueprint, unreal.WidgetBlueprint) or not blueprint.generated_class():
            raise RuntimeError(f"Expected a compiled Rok Widget Blueprint: {path}")
        needs_update = any(not matches(widget, slot, intended_role, STYLE) for widget, slot in get_labels(blueprint))
        if needs_update and mode != "Verify":
            verify(blueprint, LEGACY, legacy_role)
            pending.append((path, blueprint))
        else:
            verify(blueprint, STYLE, intended_role)

    if mode == "Apply":
        for path, blueprint in pending:
            for widget, slot in get_labels(blueprint):
                apply_style(widget, slot, intended_role)
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            verify(blueprint, STYLE, intended_role)
        for path, _ in pending:
            if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
                raise RuntimeError(f"Could not save HUD asset {path}")
    unreal.log(f"ROK_HUD_RESOURCE_TEXT mode={mode} assets={len(ASSETS)} pending={len(pending)}")


main()
