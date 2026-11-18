"""Read-only headless UE verification after generating WBP_RokWorldMarchEntry.

Checks both saved/editor and generated templates. Does not compile, save, open
maps or create live game/account state. It cannot establish runtime input parity.
"""
from pathlib import Path

import unreal


ENTRY = "/Game/RokUI/Hud/WBP_RokWorldMarchEntry"
ART = "/Game/RokUI/Hud/WBP_RokWorldMarchRow"
LAYERS = ("WorldMarchRarityFrame", "WorldMarchPortrait", "WorldMarchSelectionFrame", "WorldMarchStatusIcon")


def trees(blueprint):
    for outer in (blueprint, blueprint.generated_class()):
        tree = unreal.find_object(outer, "WidgetTree")
        if not tree:
            raise RuntimeError(f"Missing saved WidgetTree on {outer.get_path_name()}")
        yield tree


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    if project != Path(__file__).resolve().parents[2] or not (project / "Rok.uproject").is_file():
        raise RuntimeError(f"Unexpected project: {project}")
    entry = unreal.EditorAssetLibrary.load_asset(ENTRY)
    art_blueprint = unreal.EditorAssetLibrary.load_asset(ART)
    for blueprint in (entry, art_blueprint):
        if not isinstance(blueprint, unreal.WidgetBlueprint) or not blueprint.generated_class():
            raise RuntimeError("Missing generated entry/artwork WBP")
    for tree in trees(entry):
        action = unreal.find_object(tree, "WorldMarchEntryAction")
        art = unreal.find_object(tree, "WorldMarchEntryArtwork")
        if not isinstance(action, unreal.RokUnityIndexedButton):
            raise RuntimeError("Missing indexed march entry action")
        if not art or art.get_class() != art_blueprint.generated_class() or action.get_content() != art:
            raise RuntimeError("Entry must compose the existing march artwork WBP as its button content")
        slot = unreal.WidgetLayoutLibrary.slot_as_canvas_slot(action)
        if not slot:
            raise RuntimeError("March entry action requires an editable Canvas slot")
        anchors = slot.get_anchors()
        if (anchors.minimum.x, anchors.minimum.y, anchors.maximum.x, anchors.maximum.y) != (0, 0, 1, 1):
            raise RuntimeError("Initial march action must cover the existing dynamic row")
        if art.get_editor_property("visibility") != unreal.SlateVisibility.HIT_TEST_INVISIBLE:
            raise RuntimeError("Artwork must allow its parent action to receive input")
    for tree in trees(art_blueprint):
        for name in LAYERS:
            image = unreal.find_object(tree, name)
            if not isinstance(image, unreal.Image) or not unreal.WidgetLayoutLibrary.slot_as_canvas_slot(image):
                raise RuntimeError(f"Missing existing dynamic art layer: {name}")
    unreal.log("ROK_WORLD_MARCH_ENTRY_VERIFIED entry_trees=2 artwork_trees=2 action=1 layers=4 read_only=true")


main()
