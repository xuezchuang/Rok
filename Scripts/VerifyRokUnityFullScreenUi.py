import hashlib
import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_full_screen_ui.json"
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "RokUnityMigration" / "unity_full_screen_ui_verify.json"


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def content_asset_path(ue_target):
    if not ue_target.startswith("/Game/"):
        return None
    return PROJECT_ROOT / "Content" / (ue_target[len("/Game/"):] + ".uasset")


def main():
    failures = []
    checks = []
    manifest = {}
    if not MANIFEST_PATH.exists():
        failures.append("missing full-screen UI manifest: {0}".format(MANIFEST_PATH))
    else:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    for label, relative_path in manifest.get("source_prefabs", {}).items():
        source_path = PROJECT_ROOT / relative_path
        if not source_path.exists():
            failures.append("missing source prefab {0}: {1}".format(label, source_path))

    for label, relative_path in manifest.get("source_runtime", {}).items():
        source_path = PROJECT_ROOT / relative_path
        if not source_path.exists():
            failures.append("missing source runtime {0}: {1}".format(label, source_path))

    for field_name in ("ue_import_script", "ue_widget"):
        path = PROJECT_ROOT / manifest.get(field_name, "")
        if not path.exists():
            failures.append("missing {0}: {1}".format(field_name, path))

    sprites = manifest.get("sprites", [])
    if len(sprites) < 168:
        failures.append("expected at least 168 full-screen sprite mappings, found {0}".format(len(sprites)))

    seen = set()
    required_sprite_names = {
        "btn_com_1000_1",
        "btn_com_mid2_3",
        "btn_com_max3",
        "btn_com_min3",
        "btn_com_min4",
        "btn_com_1006_2",
        "btn_com_1006_3",
        "btn_com_1007_1",
        "btn_com_bagMax",
        "img_com_itemTopBg",
        "img_com_page",
        "icon_com_vipcur",
        "pb_com_1000_1",
        "pb_com_1000_4",
        "pl_fra_1000",
        "pl_fra_1002",
        "pl_fra_1003",
        "activity_img_2000",
        "activity_icon_1000",
        "activity_icon_1003",
        "activity_icon_1006",
        "icon_mail_1004",
        "img_mail_title_1",
        "task_icon_1003",
        "task_icon_1005",
        "task_icon_1009",
        "task_icon_1012",
        "item_1_1005",
        "item_1_1009",
        "item_1_1001",
        "item_1_1002",
        "item_1_1003",
        "item_1_1004",
        "item_2_1000",
        "item_2_1001",
        "item_2_1002",
        "item_2_1003",
        "item_3_1003",
        "item_3_1004",
        "item_3_1005",
        "item_3_1007",
        "item_equip0_100",
        "item_equip0_101",
        "item_equip0_102",
        "item_equip0_103",
        "item_5_1018",
        "item_5_1031",
        "item_5_2006",
        "img_item_bg1",
        "img_item_bg2",
        "img_item_bg3",
        "img_item_bg4",
        "img_item_bg5",
        "pl_ali_join",
        "icon_ali_star",
        "img_ali_aliFlag",
        "icon_ali_1000",
        "icon_ali_1007",
        "icon_ali_1008",
        "icon_ali_1013",
        "img_ali_r5",
        "alliance_flag_2_101",
        "alliance_flag_3_100",
        "paging_401",
        "paging_401_1",
        "paging_402",
        "paging_402_1",
        "paging_503",
        "paging_503_1",
        "pl_player_1003",
        "pl_player_1004",
        "vip_icon_100",
        "vip_icon_101",
        "vip_icon_117",
        "btn_chat_close",
        "btn_chat_delete",
        "btn_chat_look",
        "btn_chat_send",
        "btn_chat_upset",
        "btn_chat_upsetun",
        "icon_chat_1000",
        "icon_chat_1001",
        "icon_chat_1002",
        "icon_chat_1002_1",
        "icon_chat_1003",
        "icon_chat_1004",
        "icon_chat_1004_1",
        "icon_chat_1005",
        "icon_chat_1006",
        "icon_chat_1007",
        "icon_chat_1008",
        "icon_chat_1009",
        "icon_chat_1010",
        "icon_chat_ali",
        "icon_chat_share_1",
        "icon_chat_share_2",
        "icon_chat_share_3",
        "icon_chat_world",
        "img_chat_1000",
        "img_chat_1001",
        "img_chat_choose",
        "pl_chat_1000",
        "pl_chat_1001",
        "pl_chat_1002",
        "pl_chat_1003",
        "pl_chat_1004",
        "btn_emoji_1000",
        "btn_emoji_1001",
        "img_emoji_1000",
        "img_emoji_pop1",
        "img_emoji_pop2",
        "img_emoji_pop3",
        "img_emoji_popfre1",
        "img_emoji_popfre2",
        "img_emoji_popfre3",
        "pl_emoji_fra",
        "pl_emoji_pop",
    }
    for sprite in sprites:
        identity = (sprite.get("source_atlas"), sprite.get("sprite_name"))
        if identity in seen:
            failures.append("duplicate full-screen sprite mapping: {0}".format(identity))
        seen.add(identity)

        source_path = PROJECT_ROOT / sprite.get("source_atlas", "")
        meta_path = PROJECT_ROOT / sprite.get("source_meta", "")
        if not source_path.exists():
            failures.append("missing full-screen source atlas: {0}".format(source_path))
            continue
        if sha256_file(source_path) != sprite.get("source_atlas_sha256"):
            failures.append("full-screen source atlas hash drift: {0}".format(source_path))
        if not meta_path.exists():
            failures.append("missing full-screen source metadata: {0}".format(meta_path))
        elif sha256_file(meta_path) != sprite.get("source_meta_sha256"):
            failures.append("full-screen source metadata hash drift: {0}".format(meta_path))

        width, height = sprite.get("atlas_size", [0, 0])
        rect = sprite.get("rect", {})
        if (
            rect.get("x", -1) < 0
            or rect.get("y", -1) < 0
            or rect.get("width", 0) <= 0
            or rect.get("height", 0) <= 0
            or rect.get("x", 0) + rect.get("width", 0) > width
            or rect.get("y", 0) + rect.get("height", 0) > height
        ):
            failures.append("out-of-bounds full-screen sprite rect: {0}".format(identity))

        if width > 0 and height > 0:
            expected_uv = {
                "min": [rect.get("x", 0) / width, (height - rect.get("y", 0) - rect.get("height", 0)) / height],
                "max": [(rect.get("x", 0) + rect.get("width", 0)) / width, (height - rect.get("y", 0)) / height],
            }
            if sprite.get("normalized_uv") != expected_uv:
                failures.append("normalized UV drift: {0}".format(identity))

        target_path = content_asset_path(sprite.get("ue_atlas_target", ""))
        if target_path is None or not target_path.exists():
            failures.append("missing UE full-screen atlas target for {0}: {1}".format(identity, target_path))

    mapped_sprite_names = {sprite.get("sprite_name") for sprite in sprites}
    missing_required_sprites = sorted(required_sprite_names - mapped_sprite_names)
    if missing_required_sprites:
        failures.append("missing required full-screen sprite mappings: {0}".format(", ".join(missing_required_sprites)))

    rewards = manifest.get("reference_rewards", {})
    if rewards.get("mail_100030") != [{"item_id": 502090001, "count": 3, "sprite": "item_5_1031"}]:
        failures.append("mail 100030 reward mapping drift")
    if manifest.get("runtime_bindings", {}).get("tasks") != "URokUnityRuntimeStateSubsystem.GetActiveTask":
        failures.append("task runtime binding must use GetActiveTask")
    if manifest.get("runtime_bindings", {}).get("task_rewards") != "TaskMain.reward -> ItemPackageShow.group (compiled CSV)":
        failures.append("task reward binding must use the compiled reward group")
    if rewards.get("task_200900_runtime") != [
        {"currency": "denar", "count": 50, "sprite": "img_com_cur5"},
        {"currency": "food", "count": 2000, "sprite": "img_com_cur1"},
        {"currency": "wood", "count": 2000, "sprite": "img_com_cur2"},
    ]:
        failures.append("task 200900 runtime reward mapping drift")

    for region in manifest.get("spine_regions", []):
        source_path = PROJECT_ROOT / region.get("source", "")
        if not source_path.exists():
            failures.append("missing full-screen Spine source: {0}".format(source_path))
        elif sha256_file(source_path) != region.get("source_sha256"):
            failures.append("full-screen Spine source hash drift: {0}".format(source_path))
        target_path = content_asset_path(region.get("ue_target", ""))
        if target_path is None or not target_path.exists():
            failures.append("missing UE full-screen Spine texture: {0}".format(target_path))
        atlas_width, atlas_height = region.get("atlas_size", [0, 0])
        packed_x, packed_y, packed_width, packed_height = region.get("top_left_rect", [0, 0, 0, 0])
        if (
            packed_x < 0
            or packed_y < 0
            or packed_width <= 0
            or packed_height <= 0
            or packed_x + packed_width > atlas_width
            or packed_y + packed_height > atlas_height
        ):
            failures.append("out-of-bounds full-screen Spine region: {0}".format(region.get("region")))
        if region.get("region") == "gift5_stay" and (
            not region.get("rotated")
            or region.get("top_left_rect") != [610, 3, 161, 187]
            or region.get("display_size") != [187, 161]
        ):
            failures.append("gift5_stay rotated packed geometry drift")
        if region.get("region") == "gift1_stay" and (
            region.get("rotated")
            or region.get("top_left_rect") != [1090, 67, 171, 136]
            or region.get("display_size") != [171, 136]
        ):
            failures.append("gift1_stay packed geometry drift")
        if region.get("region") == "gift3_stay" and (
            not region.get("rotated")
            or region.get("top_left_rect") != [940, 12, 148, 182]
            or region.get("display_size") != [182, 148]
        ):
            failures.append("gift3_stay rotated packed geometry drift")
        if region.get("region") == "gift6_stay" and (
            not region.get("rotated")
            or region.get("top_left_rect") != [972, 205, 146, 173]
            or region.get("display_size") != [173, 146]
        ):
            failures.append("gift6_stay rotated packed geometry drift")

    layouts = manifest.get("layouts", {})
    if manifest.get("reference_resolution") != [1440, 810]:
        failures.append("unexpected full-screen reference resolution")
    if manifest.get("screen_match_mode") != "Expand":
        failures.append("unexpected full-screen CanvasScaler mode")
    if layouts.get("activity_outer") != [168.0, 84.0, 1104.0, 732.0]:
        failures.append("activity outer geometry drift")
    if layouts.get("mail_outer") != [36.5, 60.0, 1367.0, 780.0]:
        failures.append("mail outer geometry drift")
    if layouts.get("task_outer") != [146.0, 73.0, 1148.0, 754.0]:
        failures.append("task outer geometry drift")
    if layouts.get("items_reference_outer") != [170.0, 73.0, 1100.0, 754.0]:
        failures.append("items reference outer geometry drift")
    if layouts.get("items_reference_grid") != [184.0, 199.0, 700.0, 618.0]:
        failures.append("items reference grid geometry drift")
    if layouts.get("items_reference_detail") != [891.0, 211.0, 350.0, 590.0]:
        failures.append("items reference detail geometry drift")
    if layouts.get("alliance_welcome_outer") != [170.0, 73.0, 1100.0, 754.0]:
        failures.append("alliance welcome outer geometry drift")
    if layouts.get("alliance_join_outer") != [146.0, 73.0, 1148.0, 754.0]:
        failures.append("alliance join outer geometry drift")
    if layouts.get("alliance_main_outer") != [146.0, 73.0, 1148.0, 754.0]:
        failures.append("alliance main outer geometry drift")
    if layouts.get("vip_outer") != [160.0, 100.0, 1120.0, 700.0]:
        failures.append("VIP outer geometry drift")
    if layouts.get("vip_content") != [208.0, 160.0, 1024.0, 612.0]:
        failures.append("VIP content geometry drift")
    if layouts.get("vip_page") != [254.0, 320.0, 932.0, 400.0]:
        failures.append("VIP page geometry drift")
    expected_chat_layouts = {
        "chat_design_background": [-5.2, 0.0, 1028.8, 817.2],
        "chat_design_contacts": [0.83, 0.0, 330.0, 794.2],
        "chat_design_conversation": [326.3, 0.0, 694.7, 810.0],
        "chat_design_messages": [342.65, 82.6, 652.0, 611.4],
        "chat_design_composer": [326.65, 711.27, 694.0, 98.73],
        "chat_design_input": [339.95, 736.64, 520.0, 48.0],
        "chat_design_emoji_button": [868.65, 730.64, 60.0, 60.0],
        "chat_design_send_button": [927.65, 730.64, 60.0, 60.0],
    }
    for layout_name, expected_rect in expected_chat_layouts.items():
        if layouts.get(layout_name) != expected_rect:
            failures.append("{0} geometry drift".format(layout_name))

    expected_chat_atlases = {
        "UITextureStatic/ui_res_chat/ui_chat.png": {
            "size": [1024, 128],
            "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_chat",
        },
        "UITextureStatic/ui_res_chat/ui_emoji.png": {
            "size": [256, 1024],
            "ue_target": "/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_emoji",
        },
    }
    for atlas_name, expected_config in expected_chat_atlases.items():
        if manifest.get("atlases", {}).get(atlas_name) != expected_config:
            failures.append("{0} atlas configuration drift".format(atlas_name))

    preview_only_state = manifest.get("preview_only_state", {})
    if not preview_only_state.get("item_owned_counts_and_use_commands"):
        failures.append("items must remain explicitly marked preview-only until inventory state is connected")
    if not preview_only_state.get("alliance_membership_join_create_and_commands"):
        failures.append("alliance membership must remain explicitly marked preview-only until server state is connected")
    if not preview_only_state.get("vip_progress_rewards_and_purchases"):
        failures.append("VIP progress, rewards, and purchases must remain preview-only until server state is connected")
    if not preview_only_state.get("chat_contacts_messages_and_send_commands"):
        failures.append("chat contacts, messages, and send commands must remain preview-only until server state is connected")
    runtime_bindings = manifest.get("runtime_bindings", {})
    if runtime_bindings.get("items") is not None:
        failures.append("items runtime binding must stay null until authoritative inventory state exists")
    if runtime_bindings.get("alliance") is not None:
        failures.append("alliance runtime binding must stay null until authoritative alliance state exists")
    if runtime_bindings.get("vip") is not None:
        failures.append("VIP runtime binding must stay null until authoritative VIP state exists")
    if runtime_bindings.get("chat") is not None:
        failures.append("chat runtime binding must stay null until authoritative chat state exists")

    reference_vip_state = manifest.get("reference_vip_state", {})
    if reference_vip_state.get("state_kind") != "unity_first_login_test_fixture":
        failures.append("VIP reference state must be an explicit first-login test fixture")
    if reference_vip_state.get("account_state_authority") != "server":
        failures.append("VIP account state authority must remain server")
    if reference_vip_state.get("normal_offline_behavior") != "unsynchronized_disabled":
        failures.append("VIP offline behavior must remain honest and disabled")
    if reference_vip_state.get("current_level") != 0:
        failures.append("VIP reference current level drift")
    if reference_vip_state.get("current_points") != 0 or reference_vip_state.get("next_level_points") != 200:
        failures.append("VIP reference progress drift")
    if reference_vip_state.get("today_points") != 40 or reference_vip_state.get("continuous_login_days") != 1:
        failures.append("VIP reference login reward state drift")
    if reference_vip_state.get("daily_points_claimed") is not False:
        failures.append("VIP first-login daily points must remain unclaimed")
    if reference_vip_state.get("free_chest_claimed") is not False:
        failures.append("VIP first-login free chest must remain unclaimed")
    if reference_vip_state.get("special_chest_purchased") is not False:
        failures.append("VIP first-login special chest must remain unpurchased")
    if reference_vip_state.get("special_chest_price_usd") != 0.99:
        failures.append("VIP0 special chest reference price drift")
    for config_key in (
        "vip_config",
        "vip_day_point_config",
        "vip_attribute_config",
        "unity_mediator",
        "unity_player_proxy",
        "server_role_logic",
        "server_role_definition",
    ):
        config_path = PROJECT_ROOT / reference_vip_state.get(config_key, "")
        if not config_path.exists():
            failures.append("missing VIP source config {0}: {1}".format(config_key, config_path))

    checks.extend(
        [
            {"name": "sprite_mapping_count", "value": len(sprites)},
            {"name": "unique_sprite_mapping_count", "value": len(seen)},
            {"name": "source_prefab_count", "value": len(manifest.get("source_prefabs", {}))},
            {"name": "reference_reward_group_count", "value": len(manifest.get("reference_rewards", {}))},
        ]
    )
    result = {
        "passed": not failures,
        "failure_count": len(failures),
        "failures": failures,
        "checks": checks,
    }
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(0 if result["passed"] else 1)


if __name__ == "__main__":
    main()
