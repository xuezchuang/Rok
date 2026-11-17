"""Build editable expedition page and row specs for Rok.UiWbp.GeneratePage."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC_ROOT = ROOT / "Saved" / "RokUi" / "Specs"
FEATURE_BOUNDARY = {
    "name": "FeatureBoundaryFeedback",
    "class": "/Script/Rok.RokUiTextBlock",
    "props": {
        "Text": "",
        "Font": '(TypefaceFontName="Regular",Size=16.000000)',
        "ColorAndOpacity": "(SpecifiedColor=(R=1.000000,G=0.870000,B=0.450000,A=1.000000))",
        "Justification": "Center",
        "Visibility": "Collapsed",
    },
    "slot": {"class": "/Script/UMG.CanvasPanelSlot", "props": {
        "LayoutData": "(Offsets=(Left=390,Top=765,Right=660,Bottom=36))", "ZOrder": "100"
    }},
}
COMMON_ATLAS = "/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"
COMMON_ATLAS_SIZE = (2048, 1024)


def slot(x, y, w, h, z=1):
    return {"class": "/Script/UMG.CanvasPanelSlot", "props": {
        "LayoutData": f"(Offsets=(Left={x},Top={y},Right={w},Bottom={h}))", "ZOrder": str(z)
    }}


def node(name, cls, rect=None, props=None, children=None, z=1):
    result = {"name": name, "class": cls, "props": props or {}}
    if rect is not None:
        result["slot"] = slot(*rect, z)
    if children is not None:
        result["children"] = children
    return result


def canvas(name, rect=None, children=(), z=1, props=None):
    canvas_props = {"Visibility": "SelfHitTestInvisible"}
    if props:
        canvas_props.update(props)
    return node(name, "/Script/UMG.CanvasPanel", rect,
                canvas_props, list(children), z)


def image(name, rect, z=1, visibility="HitTestInvisible", asset=None, uv=None, atlas_size=None,
          mirror_x=False, opacity=None, color=None):
    props = {"Visibility": visibility}
    if asset:
        if asset.startswith("/Game/"):
            texture_path = asset
        else:
            texture_name = Path(asset).stem
            texture_path = f"/Game/RokPrototype/UI/ExpeditionSource/Art/{texture_name}.{texture_name}"
        brush = (
            "(TintColor=(SpecifiedColor=(R=1.000000,G=1.000000,B=1.000000,A=1.000000)),"
            "DrawAs=Image,ImageType=FullColor,"
            f"ImageSize=(X={rect[2]:.6f},Y={rect[3]:.6f}),Margin=(),"
            f"ResourceObject=\"/Script/Engine.Texture2D'{texture_path}'\""
        )
        if uv and atlas_size:
            u0, v0, u1, v1 = uv
            width, height = atlas_size
            # Unity sprite rectangles use a bottom-left origin; Slate UVs use
            # a top-left origin. Keep this conversion aligned with
            # URokUnityFeatureScreenWidget::MakeAtlasBrush.
            top_v0, top_v1 = height - v1, height - v0
            brush += (
                f",UVRegion=(Min=(X={u0 / width:.9f},Y={top_v0 / height:.9f}),"
                f"Max=(X={u1 / width:.9f},Y={top_v1 / height:.9f}),bIsValid=True)"
            )
        props["Brush"] = brush + ")"
    if mirror_x:
        props["RenderTransform"] = (
            "(Translation=(X=0.000000,Y=0.000000),Scale=(X=-1.000000,Y=1.000000),"
            "Shear=(X=0.000000,Y=0.000000))"
        )
    if opacity is not None:
        props["RenderOpacity"] = f"{opacity:.6f}"
    if color is not None:
        props["ColorAndOpacity"] = (
            f"(R={color[0]:.6f},G={color[1]:.6f},B={color[2]:.6f},A={color[3]:.6f})"
        )
    return node(name, "/Script/UMG.Image", rect, props, z=z)


def atlas_brush(asset, uv, atlas_size, image_size):
    u0, v0, u1, v1 = uv
    width, height = atlas_size
    top_v0, top_v1 = height - v1, height - v0
    if asset.startswith("/Game/"):
        texture_path = asset
    else:
        texture_name = Path(asset).stem
        texture_path = f"/Game/RokPrototype/UI/ExpeditionSource/Art/{texture_name}.{texture_name}"
    return (
        "(TintColor=(SpecifiedColor=(R=1.000000,G=1.000000,B=1.000000,A=1.000000)),"
        f"DrawAs=Image,ImageType=FullColor,ImageSize=(X={image_size[0]:.6f},Y={image_size[1]:.6f}),Margin=(),"
        f"ResourceObject=\"/Script/Engine.Texture2D'{texture_path}'\","
        f"UVRegion=(Min=(X={u0 / width:.9f},Y={top_v0 / height:.9f}),"
        f"Max=(X={u1 / width:.9f},Y={top_v1 / height:.9f}),bIsValid=True))"
    )


def atlas_border_nodes(prefix, position, size, source_rect, atlas_size, margins, z=1, mirror_x=False):
    """Expand the source UI's MakeAtlasBorder into Designer-owned image patches."""
    source_x, source_y, source_w, source_h = source_rect
    source_left = round(margins[0] * source_w)
    source_top = round(margins[1] * source_h)
    source_right = round(margins[2] * source_w)
    source_bottom = round(margins[3] * source_h)
    dest_left, dest_top, dest_right, dest_bottom = map(float, (source_left, source_top, source_right, source_bottom))
    # BuildSplitWindowFrame mirrors the right half as a whole CanvasPanel.
    # Mirroring reverses both the source edges and the destination corner widths.
    if mirror_x:
        dest_left, dest_right = dest_right, dest_left
    if dest_left + dest_right > size[0] and dest_left + dest_right > 0:
        scale = size[0] / (dest_left + dest_right)
        dest_left, dest_right = dest_left * scale, dest_right * scale
    if dest_top + dest_bottom > size[1] and dest_top + dest_bottom > 0:
        scale = size[1] / (dest_top + dest_bottom)
        dest_top, dest_bottom = dest_top * scale, dest_bottom * scale
    sx = [source_x, source_x + source_left, source_x + source_w - source_right, source_x + source_w]
    sy = [source_y, source_y + source_bottom, source_y + source_h - source_top, source_y + source_h]
    dx = [position[0], position[0] + dest_left, position[0] + size[0] - dest_right, position[0] + size[0]]
    dy = [position[1], position[1] + dest_top, position[1] + size[1] - dest_bottom, position[1] + size[1]]
    out = []
    for row, (source_y0, source_y1, dest_y0, dest_y1) in enumerate(((sy[2], sy[3], dy[0], dy[1]),
                                                                        (sy[1], sy[2], dy[1], dy[2]),
                                                                        (sy[0], sy[1], dy[2], dy[3]))):
        for col in range(3):
            src_col = 2 - col if mirror_x else col
            source_x0, source_x1 = sx[src_col], sx[src_col + 1]
            dest_x0, dest_x1 = dx[col], dx[col + 1]
            if source_x1 <= source_x0 or source_y1 <= source_y0 or dest_x1 <= dest_x0 or dest_y1 <= dest_y0:
                continue
            patch_name = f"{prefix}_{row}_{col}"
            patch = image(patch_name, (dest_x0, dest_y0, dest_x1 - dest_x0, dest_y1 - dest_y0), z,
                          asset="/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame",
                          uv=(source_x0, source_y0, source_x1, source_y1), atlas_size=atlas_size,
                          mirror_x=mirror_x)
            out.append(patch)
    return out


def text(name, rect, value="", size=18, color=(0.94, 0.88, 0.69, 1), bold=False, z=2,
         align="Center", visibility="HitTestInvisible"):
    face = "Bold" if bold else "Regular"
    return node(name, "/Script/Rok.RokUiTextBlock", rect, {
        "Text": value,
        "Font": f'(TypefaceFontName="{face}",Size={size}.000000)',
        "ColorAndOpacity": (
            f"(SpecifiedColor=(R={color[0]:.6f},G={color[1]:.6f},"
            f"B={color[2]:.6f},A={color[3]:.6f}))"
        ),
        "Justification": f"ETextJustify::{align}",
        "ShadowOffset": "(X=1.000000,Y=1.000000)",
        "ShadowColorAndOpacity": "(R=0.000000,G=0.000000,B=0.000000,A=0.650000)",
        "Visibility": visibility,
    }, z=z)


def button(name, rect, label="", size=18, z=5, label_color=(1, 0.93, 0.77, 1)):
    children = []
    if label:
        label_node = text(name + "Label", (0, 0, rect[2], rect[3]), label, size,
                          label_color, True, 2)
        label_node["slot"] = {"class": "/Script/UMG.ButtonSlot", "props": {
            "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
            "HorizontalAlignment": "HAlign_Fill",
            "VerticalAlignment": "VAlign_Fill",
        }}
        children.append(label_node)
    return node(name, "/Script/Rok.RokUnityIndexedButton", rect, {
        "Visibility": "Visible",
        "IsFocusable": "False",
        "BackgroundColor": "(R=0.000000,G=0.000000,B=0.000000,A=0.000000)",
    }, children, z)


def atlas_state_button(name, rect, label, size, z, label_color, gold_uv, disabled_uv):
    """Keep the atlas art and label inside the button's disabled foreground path."""
    width, height = rect[2], rect[3]
    label_y = max(1.0, (height - size * 1.6) * 0.5)
    content = canvas(name + "Content", children=[
        image(name + "Gold", (0, 0, width, height), 1, asset=COMMON_ATLAS,
              uv=gold_uv, atlas_size=COMMON_ATLAS_SIZE),
        image(name + "Disabled", (0, 0, width, height), 1, "Collapsed", asset=COMMON_ATLAS,
              uv=disabled_uv, atlas_size=COMMON_ATLAS_SIZE),
        text(name + "Label", (0, label_y, width, height - 4), label, size,
             label_color, True, 2),
    ])
    content["slot"] = {"class": "/Script/UMG.ButtonSlot", "props": {
        "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
        "HorizontalAlignment": "HAlign_Fill",
        "VerticalAlignment": "VAlign_Fill",
    }}
    return node(name, "/Script/Rok.RokUnityIndexedButton", rect, {
        "Visibility": "Visible",
        "IsFocusable": "False",
        "BackgroundColor": "(R=0.000000,G=0.000000,B=0.000000,A=0.000000)",
    }, [content], z)


def row_canvas(name, width, height, children):
    return canvas(name, children=children)


def save(name, nodes, label):
    data = {"version": 1, "label": label, "root": "PageRoot", "nodes": nodes}
    SPEC_ROOT.mkdir(parents=True, exist_ok=True)
    (SPEC_ROOT / f"{name}.json").write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def make_specs():
    # Map stage and reward rows are template assets; data-derived map coordinates and
    # ordering remain dynamic, while each template's geometry is Designer-authored.
    stage_row = [
        image("ExpeditionStageBase_1", (20.5, 74, 49, 40), 1),
        image("ExpeditionStageSelectedGlow_1", (-27, -48, 144, 144), 2, "Collapsed",
              color=(1.0, 0.65, 0.15, 0.8)),
        image("ExpeditionStageFlag_1", (14.5, -9, 61, 87), 3),
        canvas("ExpeditionStageTitle_1", (15, 20, 60, 32), [
            text("ExpeditionStageNormalTitle_1", (0, 0, 60, 32), "1", 21,
                 (1, 1, 1, 1), True, 1),
            text("ExpeditionStageSelectedTitle_1", (0, 0, 60, 32), "1", 24,
                 (1, 0.86, 0.45, 1), True, 2, visibility="Collapsed"),
        ], 4),
        *(image(f"ExpeditionStageStar_{i}_1", (6 + i * 26, -2 if i != 1 else -7, 26, 26), 4, "Collapsed") for i in range(3)),
        button("ExpeditionStageButton_1", (0, 0, 90, 102), z=8),
    ]
    reward_row = [
        image("ExpeditionRewardFrame", (0, 0, 76, 72), 1, "Collapsed"),
        image("ExpeditionReward_0Icon", (0, 0, 48, 48), 3, "Collapsed"),
        text("ExpeditionReward_0Name", (0, 0, 76, 48), "", 11, (1, 0.95, 0.80, 1), False, 3, visibility="Collapsed"),
        text("ExpeditionReward_0Count", (-3, 48, 54, 22), "0", 15, (1, 1, 1, 1), True, 4),
    ]
    enemy_row = [
        image("ExpeditionEnemyHeroFrame_0", (0, 0, 72, 81), 1),
        image("ExpeditionEnemyHeroIcon_0", (3, 4, 66, 73), 2),
    ]
    star_row = [
        image("ExpeditionStarConditionStar_0", (0, 0, 43, 43), 1, asset="icon_com_star2.png"),
        text("ExpeditionStarConditionText_0", (56, 7, 426, 34), "", 20,
             (1, 1, 1, 1), True, align="Left"),
    ]
    hero_choice_row = [
        image("ExpeditionHeroChoiceButtonBacking", (100, 13, 720, 65), 1,
              asset=COMMON_ATLAS, uv=(1098, 571, 1300, 652), atlas_size=COMMON_ATLAS_SIZE),
        image("ExpeditionHeroChoiceFrame_0", (8, 6, 74, 84), 2),
        image("ExpeditionHeroChoiceIcon_0", (11, 8, 68, 78), 3),
        text("ExpeditionHeroChoiceName_0", (100, 24, 600, 46), "", 22, align="Left"),
        button("ExpeditionHeroChoiceButton_0", (0, 0, 1000, 96), z=8),
    ]
    soldier_row = [
        node("ExpeditionSoldierShade_10101", "/Script/UMG.Border", (0, 1, 575, 98), {
            "BrushColor": "(R=0.035000,G=0.045000,B=0.040000,A=0.750000)",
            "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
        }, z=1),
        image("ExpeditionSoldierPortrait_10101", (13, 12, 70, 70), 2),
        text("ExpeditionSoldierName_10101", (92, 16, 388, 28), "", 18, (1, 1, 1, 1), True, 4, "Left"),
        node("ExpeditionSoldierFill_10101", "/Script/UMG.ProgressBar", (93, 62, 384, 16), {
            # The native Expedition path has no CommonTexture and keeps the
            # engine default progress-bar brushes plus this flat green tint.
            "Percent": "0.000000", "FillColorAndOpacity": "(R=0.100000,G=0.640000,B=0.280000,A=1.000000)",
            "Visibility": "HitTestInvisible",
        }, z=3),
        node("ExpeditionSoldierSlider_10101", "/Script/UMG.Slider", (92, 50, 385, 37), {
            "MinValue": "0.000000", "MaxValue": "1.000000", "StepSize": "1.000000",
            "SliderBarColor": "(R=0.000000,G=0.000000,B=0.000000,A=0.000000)",
            "SliderHandleColor": "(R=1.000000,G=1.000000,B=1.000000,A=1.000000)",
            "IsFocusable": "False",
        }, z=4),
        text("ExpeditionSoldierQuantity_10101", (486, 54, 77, 28), "0", 17, (1, 1, 1, 1), True, 5, "Right"),
    ]
    save("expedition_stage_row", stage_row, "Expedition stage map marker template")
    save("expedition_map_reward_row", reward_row, "Expedition map reward template")
    save("expedition_enemy_hero_row", enemy_row, "Expedition enemy commander template")
    save("expedition_star_condition_row", star_row, "Expedition objective star template")
    save("expedition_hero_choice_row", hero_choice_row, "Expedition commander choice template")
    save("expedition_soldier_row", soldier_row, "Expedition soldier slider template")

    first_reward_stamp = text(
        "ExpeditionFirstRewardStamp", (339, 640, 52, 30), "首胜", 15, (1, 1, 1, 1), True, 9)
    first_reward_stamp["props"]["RenderTransform"] = (
        "(Translation=(X=0.000000,Y=0.000000),Scale=(X=1.000000,Y=1.000000),"
        "Shear=(X=0.000000,Y=0.000000),Angle=-33.000000)"
    )
    first_reward_stamp["props"]["RenderTransformPivot"] = "(X=0.500000,Y=0.500000)"

    map_children = [
        image("ExpeditionMapBackdrop", (0, -45, 1440, 900), 0, asset="hero_background_14.png"),
        image("ExpeditionParchmentLeft", (0, 10, 720, 794), 1, asset="img_bg_battle_frame1.png"),
        image("ExpeditionParchmentRight", (720, 10, 720, 794), 1, asset="img_bg_battle_frame1.png"),
        node("ExpeditionMapScroll", "/Script/UMG.ScrollBox", (74, 109, 1292, 625), {
            "ScrollBarVisibility": "ESlateVisibility::Collapsed",
            "ConsumeMouseWheel": "Always",
            "Clipping": "ClipToBoundsAlways",
            "AllowOverscroll": "False", "bAnimateWheelScrolling": "False",
        }, [node("ExpeditionMapExtent", "/Script/UMG.SizeBox", props={
            "WidthOverride": "1292.000000", "bOverride_WidthOverride": "True",
            "HeightOverride": "8192.000000", "bOverride_HeightOverride": "True",
        }, children=[canvas("ExpeditionMapContent", children=[
            *(image(f"ExpeditionMapTile_{i}", (0, i * 1024, 1292, 1024), 0) for i in range(8))
        ])])], 2),
        text("ExpeditionTitle", (520, 55, 400, 52), "远征", 34, (0.39, 0.21, 0.06, 1), True, 6),
        text("ExpeditionMedalLabel", (1112, 12, 105, 34), "", 18, z=6),
        image("ExpeditionMedalIcon", (1072, 0, 54, 54), 5, asset="img_com_cur8.png"),
        text("ExpeditionMedals", (1112, 12, 105, 34), "0", 19, (1, 1, 1, 1), True, 6),
        image("ExpeditionGemIcon", (1232, 0, 53, 53), 5, asset=COMMON_ATLAS,
              uv=(783, 435, 863, 515), atlas_size=COMMON_ATLAS_SIZE),
        text("ExpeditionGems", (1278, 12, 90, 34), "0", 19, (1, 1, 1, 1), True, 6),
        image("ExpeditionMapBackArt", (74, 46, 54, 54), 7, asset="btn_bat_back.png"),
        button("ExpeditionMapBackButton", (74, 46, 54, 54), "", 30, 8),
        image("ExpeditionRankArt", (1372, 84, 49, 49), 7, asset="btn_bat_rank.png"),
        button("ExpeditionRankButton", (1372, 84, 49, 49), "", 17, 8),
        image("ExpeditionRulesArt", (1372, 154, 49, 49), 7, asset="btn_bat_rule.png"),
        button("ExpeditionRulesButton", (1372, 154, 49, 49), "", 20, 8),
        image("ExpeditionShopFrame", (101, 114, 99, 100), 7, asset="btn_bat_frame.png"),
        image("ExpeditionShopArt", (113, 129, 77, 71), 8, asset="btn_bat_shop.png"),
        button("ExpeditionShopButton", (101, 114, 99, 100), "", 17, 9),
        image("ExpeditionChestArt", (101, 213, 86, 82), 7, asset="img_bat_box.png"),
        button("ExpeditionChestButton", (101, 213, 86, 82), "", 17, 8),
        node("ExpeditionMapInfoPanel", "/Script/UMG.Border", (165, 617, 1150, 132), {
            "BrushColor": "(R=0.035000,G=0.045000,B=0.040000,A=0.750000)",
            "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
        }, z=5),
        text("ExpeditionSelectedLevel", (181, 630, 180, 56), "1/80", 36, (1, 1, 1, 1), True, 7),
        *(image(f"ExpeditionSelectionStar_{i}", (202 + 52 * i, 678, 48, 48), 7) for i in range(3)),
        text("ExpeditionRewardTitle", (364, 624, 200, 28), "首次完成奖励", 19, z=7),
        image("ExpeditionFirstRewardSeal", (338, 628, 54, 54), 8, asset="img_bat_first.png"),
        first_reward_stamp,
        canvas("ExpeditionRewardContent", (0, 0, 1440, 810), z=6),
        *(image(f"ExpeditionCapacitySlot_{i}", (906 + 20 * i, 684, 16, 40), 7, asset="img_bat_queue2.png") for i in range(5)),
        atlas_state_button("ExpeditionChallenge", (1082, 645, 202, 75), "挑战", 25, 12,
                           (0.39, 0.21, 0.06, 1), (677, 600, 879, 681), (473, 528, 675, 609)),
        FEATURE_BOUNDARY,
    ]
    save("expedition_map", map_children, "Editable expedition map page")

    task_children = [
        node("ExpeditionTaskBackdropShade", "/Script/UMG.Border", (0, -180, 2240, 1170), {
            "BrushColor": "(R=0.002000,G=0.026000,B=0.065000,A=0.710000)",
            "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
        }, z=1),
        canvas("ExpeditionSideRoot", (1294, -24, 146, 875), z=6, children=[
            node("ExpeditionSideShade", "/Script/UMG.Border", (0, 0, 146, 875), {
                "BrushColor": "(R=0.035000,G=0.045000,B=0.040000,A=0.750000)",
                "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
            }, z=1),
            text("ExpeditionTroopCount", (12, 8, 122, 32), "0/1", 19, (1, 1, 1, 1), True, 3),
            *(image(f"ExpeditionSquadSlot_{i}", (16, 86 + 133 * i, 114, 127), 2) for i in range(5)),
            *(image(f"ExpeditionSquadHeroFrame_{i}", (31, 103 + 133 * i, 84, 95), 3, "Collapsed") for i in range(5)),
            *(image(f"ExpeditionSquadHeroIcon_{i}", (34, 106 + 133 * i, 78, 88), 4, "Collapsed") for i in range(5)),
            *(button(f"ExpeditionSquadButton_{i}", (16, 86 + 133 * i, 114, 127), z=6) for i in range(5)),
        ]),
        image("ExpeditionBackButtonArt", (20, -35, 57, 57), 11, asset=COMMON_ATLAS,
              uv=(604, 611, 673, 680), atlas_size=COMMON_ATLAS_SIZE),
        button("ExpeditionBackButton", (20, -35, 57, 57), "", 30, 12),
        image("ExpeditionPreviewButtonArt", (1312, 17, 111, 42), 11, asset=COMMON_ATLAS,
              uv=(1098, 571, 1300, 652), atlas_size=COMMON_ATLAS_SIZE),
        button("ExpeditionPreviewButton", (1312, 17, 111, 42), "预览", 17, 12),
        canvas("ExpeditionCenterRoot", (0, 0, 1440, 810), z=7, children=[
        image("ExpeditionTaskPanelImage", (382, 65, 676, 725), 1,
              asset="/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.rok_expedition_task_panel"),
            canvas("ExpeditionCenterFlagMes", (0, 0, 1440, 810), z=2, children=[
                node("ExpeditionEnemiesShade", "/Script/UMG.Border", (450, 421, 540, 124), {
                    "BrushColor": "(R=0.120000,G=0.012000,B=0.010000,A=0.660000)",
                    "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
                }, z=1),
                text("ExpeditionSetupHint", (495, 204, 450, 34), "请在右侧配置您的统帅", 17, (1, .67, .22, 1), True, 3),
                text("ExpeditionEnemyTitle", (470, 427, 500, 25), "敌方统帅", 18, (1, 1, 1, 1), True, 5),
                canvas("ExpeditionEnemyHeroes", (0, 0, 1440, 810), z=6),
            ]),
            canvas("ExpeditionCenterImg", (286, 84, 868, 111), z=4, children=[
                image("ExpeditionCenterFrameLeft", (0, -8, 434, 128), 1,
                      asset="/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.rok_expedition_result_atlas",
                      uv=(1, 131, 455, 269), atlas_size=(512, 512)),
                image("ExpeditionCenterFrameRight", (434, -8, 434, 128), 1,
                      asset="/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.rok_expedition_result_atlas",
                      uv=(1, 131, 455, 269), atlas_size=(512, 512), mirror_x=True),
        text("ExpeditionDetailTitle", (207, 25, 454, 52), "任务目标", 36, (0.96, 0.94, 0.88, 1), True, 4),
            ]),
            canvas("ExpeditionStarConditionList", (0, 0, 1440, 810), z=7),
        ]),
        atlas_state_button("ExpeditionChallenge", (620, 598, 200, 70), "开始", 29, 12,
                           (0.39, 0.21, 0.06, 1), (677, 600, 879, 681), (473, 528, 675, 609)),
        canvas("ExpeditionPreviewRoot", (1004, 82, 420, 510), [
                image("ExpeditionPreviewPanel", (0, 0, 420, 510), 1, asset="img_bg_battle_frame1.png"),
            text("ExpeditionPreviewTitle", (30, 20, 360, 40), "敌方统帅", 24, (1, .91, .7, 1), True, 2),
            canvas("ExpeditionPreviewEnemies", (0, 0, 420, 510), z=3),
        ], 20),
        FEATURE_BOUNDARY,
    ]
    save("expedition_task", task_children, "Editable expedition objective page")

    composer_children = [
        node("ExpeditionArmyBackdrop", "/Script/UMG.Border", (0, 0, 1440, 810), {
            "BrushColor": "(R=0.000000,G=0.000000,B=0.000000,A=0.760000)",
            "Padding": "(Left=0,Top=0,Right=0,Bottom=0)",
        }, z=20),
        canvas("ExpeditionArmyRoot", (0, 0, 1440, 810), z=21, children=[
            node("ExpeditionArmyFrameBacking", "/Script/UMG.Border", (169, 84, 1097, 678), {
                "BrushColor": "(R=0.150000,G=0.150000,B=0.150000,A=1.000000)",
                "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
            }, z=1),
            *atlas_border_nodes("ExpeditionArmyBodyLeft", (142, 89), (578, 689),
                                (1, 822, 524, 659), (1024, 2048),
                                (493 / 524, 278 / 659, 15 / 524, 291 / 659), 1),
            *atlas_border_nodes("ExpeditionArmyBodyRight", (720, 89), (578, 689),
                                (1, 822, 524, 659), (1024, 2048),
                                (493 / 524, 278 / 659, 15 / 524, 291 / 659), 1, mirror_x=True),
            *atlas_border_nodes("ExpeditionArmyHeaderLeft", (142, 37), (578, 60),
                                (1, 329, 560, 60), (1024, 2048),
                                (539 / 560, 0, 8 / 560, 0), 5),
            *atlas_border_nodes("ExpeditionArmyHeaderRight", (720, 37), (578, 60),
                                (1, 329, 560, 60), (1024, 2048),
                                (539 / 560, 0, 8 / 560, 0), 5, mirror_x=True),
            text("ExpeditionArmyTitle", (142, 45, 1156, 42), "创建部队", 30, (0.96, 0.94, 0.88, 1), True, 7),
            image("ExpeditionArmyCloseArt", (1220, 41, 69, 69), 7, asset=COMMON_ATLAS,
                  uv=(883, 589, 952, 658), atlas_size=COMMON_ATLAS_SIZE),
            button("ExpeditionArmyCloseButton", (1220, 41, 69, 69), "", 30, 8),
            node("ExpeditionArmyPortraitShade", "/Script/UMG.Border", (178, 91, 438, 661), {
                "BrushColor": "(R=0.090000,G=0.090000,B=0.090000,A=0.680000)",
                "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
            }, z=2),
            image("ExpeditionArmyPortraitScene", (194, 144, 404, 415), 3, asset="hero_background_14.png", opacity=0.35),
            canvas("ExpeditionArmyContentRoot", (0, 0, 1440, 810), z=6, children=[
                canvas("ExpeditionArmyPortraitHost", (151, 112, 495, 546), z=4),
                text("ExpeditionArmyCommanderName", (205, 142, 320, 40), "", 26, (1, 1, 1, 1), True, 7, "Left"),
                text("ExpeditionArmyHeroPower", (644, 112, 310, 29), "统帅战力：—", 18, (1, 1, 1, 1), True, 7, "Left"),
                text("ExpeditionArmyTotalPower", (973, 112, 239, 29), "总战力：—", 18, (1, 1, 1, 1), True, 7, "Right"),
                node("ExpeditionArmySkillsShade", "/Script/UMG.Border", (189, 650, 412, 82), {
                    "BrushColor": "(R=0.035000,G=0.045000,B=0.040000,A=0.750000)",
                    "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
                }, z=4),
                canvas("ExpeditionArmySkills", (189, 650, 412, 82), z=5, children=[
                    *(image(f"ExpeditionArmySkill_{i}", (8 + 68 * i, 12, 58, 58), 1) for i in range(5))
                ]),
                node("ExpeditionArmyTroops", "/Script/UMG.ScrollBox", (642, 144, 580, 407), {
                    "ScrollBarVisibility": "ESlateVisibility::Collapsed"
                }, [], 5),
                text("ExpeditionNoAvailableSoldiers", (651, 248, 550, 38), "暂无可编入远征的部队", 19,
                     (1, 1, 1, 1), True, 7),
                node("ExpeditionArmySummaryShade", "/Script/UMG.Border", (642, 565, 580, 56), {
                    "BrushColor": "(R=0.035000,G=0.045000,B=0.040000,A=0.750000)",
                    "Padding": "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)",
                }, z=4),
                text("ExpeditionArmyLoad", (664, 582, 260, 34), "负载：0", 17, (1, 1, 1, 1), True, 7, "Left"),
                text("ExpeditionArmyCount", (909, 582, 290, 34), "部队数量：0/0", 17, (1, 1, 1, 1), True, 7, "Right"),
                image("ExpeditionArmySwitchHeroArt", (198, 588, 149, 51), 7, asset=COMMON_ATLAS,
                      uv=(1098, 571, 1300, 652), atlas_size=COMMON_ATLAS_SIZE),
                button("ExpeditionArmySwitchHero", (198, 588, 149, 51), "更换统帅", 18, 8,
                       label_color=(1, 1, 1, 1)),
                image("ExpeditionArmyClearArt", (644, 660, 267, 75), 7, asset=COMMON_ATLAS,
                      uv=(1098, 571, 1300, 652), atlas_size=COMMON_ATLAS_SIZE),
                button("ExpeditionArmyClear", (644, 660, 267, 75), "清空", 30, 8,
                       label_color=(0.02, 0.11, 0.19, 1)),
                image("ExpeditionArmyReadyArt", (940, 660, 267, 75), 7, asset=COMMON_ATLAS,
                      uv=(677, 600, 879, 681), atlas_size=COMMON_ATLAS_SIZE),
                image("ExpeditionArmyReadyDisabledArt", (940, 660, 267, 75), 7, "Collapsed", asset=COMMON_ATLAS,
                      uv=(473, 528, 675, 609), atlas_size=COMMON_ATLAS_SIZE),
                button("ExpeditionArmyReady", (940, 660, 267, 75), "准备", 30, 8,
                       label_color=(0.39, 0.21, 0.06, 1)),
            ]),
            canvas("ExpeditionHeroChoiceRoot", (0, 0, 1440, 810), z=6, children=[
                node("ExpeditionHeroChoiceScroll", "/Script/UMG.ScrollBox", (205, 130, 1028, 584), {
                    "ScrollBarVisibility": "ESlateVisibility::Collapsed"
                }, [] , 3),
            ]),
        ]),
        FEATURE_BOUNDARY,
    ]
    save("expedition_army_composer", composer_children, "Editable expedition army composer overlay")


if __name__ == "__main__":
    make_specs()
