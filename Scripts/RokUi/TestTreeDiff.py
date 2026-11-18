"""Synthetic tree-diff regression checks; no Unreal process or asset writes.

Run: py -3.12 -B Scripts/RokUi/TestTreeDiff.py
"""

from copy import deepcopy
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from TreeDiff import differences, flatten, main


def widget(name, cls="CanvasPanel", children=None, **fields):
    return dict(name=name, **{"class": cls}, children=children or [], **fields)


def reusable(name, children):
    return widget(name, "WBP_Test_C", [widget("PageRoot", children=children, widget_tree_root=True)],
                  user_widget=True, reusable_widget=True)


def records(root, **options):
    result = flatten(root, "", {}, **options)
    result.pop("", None)
    return result


class TreeDiffTests(unittest.TestCase):
    def test_old_dumps_and_text_class_equivalence(self):
        before = widget("LiveRoot", children=[widget("Title", "TextBlock", text="Title", font_size=18)])
        after = deepcopy(before)
        after["name"] = "PageRoot"
        after["children"][0]["class"] = "RokUiTextBlock"
        self.assertEqual(differences(records(before), records(after)), [])

    def test_aliases_keep_raw_hierarchy_and_slot_data(self):
        before = widget("LiveRoot", children=[widget("GuildHeader", "TextBlock", text="Title")])
        after = widget("PageRoot", children=[widget("Header", "TextBlock", text="Title", logical_name="GuildHeader")])
        self.assertEqual(differences(records(before), records(after)), [])
        self.assertEqual(set(records(after, raw_names=True)), {"/Header"})
        nested = widget("Host", children=[reusable("List", [widget("Scroll", slot={"z": 3})])])
        raw = records(nested)
        self.assertIn("/List/PageRoot/Scroll", raw)
        self.assertEqual(raw["/List/PageRoot/Scroll"]["slot"], {"z": 3})

    def test_two_level_reusable_aliases_and_dynamic_row_identity(self):
        before = widget("Host", children=[widget("GuildScroll", children=[
            widget("GuildRowTitle_0", "TextBlock", text="First"),
            widget("GuildRowTitle_1", "TextBlock", text="Second"),
        ])])
        after = widget("Host", children=[reusable("List", [widget("Scroll", logical_name="GuildScroll", children=[
            reusable("Row_0", [widget("RowTitle", "RokUiTextBlock", text="First", logical_name="GuildRowTitle_0")]),
            reusable("Row_1", [widget("RowTitle", "RokUiTextBlock", text="Second", logical_name="GuildRowTitle_1")]),
        ])])])
        self.assertNotEqual(differences(records(before), records(after)), [])
        self.assertEqual(differences(records(before), records(after, logical=True)), [])
        after["children"][0]["children"][0]["children"][0]["children"][1]["children"][0]["children"][0]["text"] = "Changed"
        self.assertEqual(len(differences(records(before), records(after, logical=True))), 1)

    def test_ordinary_user_widgets_and_panels_are_not_unwrapped(self):
        root = widget("Host", children=[widget("PageRoot", children=[widget("Title", "TextBlock")])])
        self.assertEqual(set(records(root, logical=True)), {"/PageRoot", "/PageRoot/Title"})
        root["children"][0].update(user_widget=True, reusable_widget=False)
        self.assertEqual(set(records(root, logical=True)), {"/PageRoot", "/PageRoot/Title"})

    def test_non_canvas_reusable_root_keeps_its_content_and_name(self):
        root_text = widget("RootText", "TextBlock", text="Header", logical_name="LegacyHeader", widget_tree_root=True)
        root = widget("Host", children=[widget("Piece", "WBP_Test_C", [root_text],
                      user_widget=True, reusable_widget=True)])
        self.assertEqual(records(root, logical=True)["/LegacyHeader"]["text"], "Header")

    def test_layout_visibility_and_hierarchy_changes_still_fail(self):
        before = widget("Host", children=[widget("Panel", children=[widget("Title", "TextBlock", slot={"z": 0},
                              visibility="Visible", font_size=18, typeface="Regular")])])
        for key, value in (("slot", {"z": 1}), ("slot_class", "VerticalBoxSlot"), ("class", "Image"),
                           ("visibility", "Hidden"), ("font_size", 20), ("typeface", "Bold")):
            after = deepcopy(before)
            after["children"][0]["children"][0][key] = value
            self.assertEqual(len(differences(records(before), records(after, logical=True))), 1)
        after = deepcopy(before)
        after["children"].append(after["children"][0]["children"].pop())
        self.assertTrue(differences(records(before), records(after, logical=True)))

    def test_duplicate_aliases_do_not_silently_overwrite(self):
        root = widget("Host", children=[widget("A", logical_name="Same"), widget("B", logical_name="Same")])
        with self.assertRaisesRegex(ValueError, "duplicate widget path"):
            records(root)
        self.assertEqual(len(records(root, raw_names=True)), 2)

    def test_incomplete_reusable_boundary_is_rejected_in_logical_mode(self):
        root = widget("Host", children=[widget("Piece", user_widget=True, reusable_widget=True)])
        with self.assertRaisesRegex(ValueError, "no single exported tree root"):
            records(root, logical=True)

    def test_cli_compatibility_bom_and_exit_codes(self):
        with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            before = Path(directory) / "before.json"
            after = Path(directory) / "after.json"
            tree = widget("Host", children=[widget("Title", "TextBlock", text="Title")])
            before.write_text(json.dumps(tree), encoding="utf-8-sig")
            after.write_text(json.dumps(tree), encoding="utf-8")
            self.assertEqual(main([str(before), str(after)]), 0)
            tree["children"][0]["text"] = "Changed"
            after.write_text(json.dumps(tree), encoding="utf-8")
            self.assertEqual(main([str(before), str(after)]), 1)
            after.write_text("{broken", encoding="utf-8")
            self.assertEqual(main([str(before), str(after)]), 2)
            after.write_text("[]", encoding="utf-8")
            self.assertEqual(main([str(before), str(after)]), 2)


if __name__ == "__main__":
    unittest.main()
