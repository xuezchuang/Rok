"""Extract verified Unity UI motion and actual runtime-prefab controller bindings.

Read-only with respect to Unity/UE source and assets. Only the generated JSON is
written. This deliberately does not infer animation families from prefab names.
"""

import argparse
import hashlib
import json
import math
import re
from collections import Counter
from pathlib import Path
from rok_data_paths import resolve


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RokUnityMigration"
COVERAGE_PATH = OUTPUT_DIR / "unity_ui_runtime_coverage.json"
OUTPUT_PATH = OUTPUT_DIR / "unity_ui_animation_manifest.json"
ASSETS = "ref/client-unity/Assets/"
UI = ASSETS + "BundleAssets/UI/"
ANIMATIONS = UI + "UIAnimation/"
SHARED = ANIMATIONS + "shared_uianimation/"
MAIN_PREFAB = UI + "UIPrefabs/ui_prefabs_map/UIMainCity/UI_IF_MainInterface.prefab"
TASK_PREFAB = UI + "UIPrefabs/ui_prefabs_map/UIMainCity/UI_Item_HotTask.prefab"
REDPOINT_PREFAB = UI + "UIPrefabs/ui_prefabs_common/UICommon/UI_Common_Redpoint.prefab"
SUPPLEMENTAL_PREFABS = [
    UI + "UIEffect/UI_10010.prefab",
	UI + "UIPrefabs/ui_prefabs_login/UILogin/CreateCharFlagItem.prefab",
    TASK_PREFAB,
    UI + "UIPrefabs/ui_prefabs_common/UICommon/UI_Common_AlertMin.prefab",
    UI + "UIPrefabs/ui_prefabs_map/UIWorldObjectPop/UI_Pop_WorldArmyCmd.prefab",
    UI + "UIPrefabs/ui_prefabs_player/UIPlayerData/UI_Win_ChangeName.prefab",
]
BUTTON_SOURCE = ASSETS + "Scripts/Client/Compoment/UIComponent/BtnAnimation.cs"
MEDIATOR_SOURCE = ASSETS + "Scripts/Hotfix/MVC/View_Mediator/MainInterface/MainInterfaceMediator.cs"
VIEW_SOURCE = ASSETS + "Scripts/Hotfix/MVC/View_Mediator/MainInterface/MainInterfaceView.cs"
MANAGER_SOURCE = ASSETS + "Skyunion/RunTime/UIManager/UIManager.cs"
BINDER_SOURCE = ASSETS + "Skyunion/RunTime/UIManager/ViewBinder.cs"
CHAT_MEDIATOR_SOURCE = ASSETS + "Scripts/Hotfix/MVC/View_Mediator/Chat/UI_Win_ChatMediator.cs"
ARAB_LAYOUT_SOURCE = ASSETS + "Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs"

# Only these already-traced clips are extracted. Other controllers are reported
# as custom, not silently assigned a similar-looking common animation.
CLIP_PATHS = [
    ANIMATIONS + "UI_10010.anim",
    ANIMATIONS + "UA_InterfaceChat.anim",
    ANIMATIONS + "UA_ItemAnime_BCBShow.anim",
	ANIMATIONS + "UA_BuildingCity_On.anim",
	ANIMATIONS + "UA_BuildingCity_Off.anim",
	SHARED + "UI_10011.anim",
    ANIMATIONS + "UA_CreateCharView_CiviChange.anim",
    ANIMATIONS + "UA_CreateCharView_Welcome.anim",
    ANIMATIONS + "UA_CreateCharView_CharShow.anim",
    ANIMATIONS + "UA_CreateCharView_CharOut.anim",
    ANIMATIONS + "UA_IFSearchRes_On.anim",
    ANIMATIONS + "UA_IFSearchRes_Off.anim",
	SHARED + "UA_BuildingMenu_On.anim",
	SHARED + "UA_BuildingMenu_Off.anim",
    SHARED + "CommonBtnDownAni.anim",
    SHARED + "CommonBtnUpAni.anim",
    SHARED + "UA_PopAnime_On.anim",
    SHARED + "UA_PopAnime_Off.anim",
    ANIMATIONS + "UA_PopAnime_Off_hasCallback.anim",
    SHARED + "UA_WinAnime_T1_On.anim",
    SHARED + "UA_WinAnime_T1_Off.anim",
    SHARED + "UA_WinAnime_IF_On.anim",
    ANIMATIONS + "UA_MainIF_HotTask_On.anim",
    ANIMATIONS + "UA_MainIF_HotTask_Off.anim",
    ANIMATIONS + "UA_MainIF_Rotation_ROn.anim",
    ANIMATIONS + "UA_MainIF_Rotation_ROff.anim",
    ANIMATIONS + "UA_TaskBox.anim",
    ANIMATIONS + "MissionItemRemove.anim",
    ANIMATIONS + "UA_Chat_On.anim",
    ANIMATIONS + "UA_Chat_Off.anim",
    ANIMATIONS + "UA_IFCaptain_Left_On.anim",
]
CONTROLLER_PATHS = {
    ANIMATIONS + "UI_10010.controller": "power_gain_effect",
    ANIMATIONS + "UA_InterfaceChat.controller": "hud_alliance_chat_unread",
    ANIMATIONS + "UA_ItemAnime_BCB.controller": "build_city_card",
	ANIMATIONS + "UA_BuildingCity.controller": "build_city_drawer",
	SHARED + "UI_10011.controller": "civilization_selection",
    ANIMATIONS + "UA_CreateCharView.controller": "civilization",
    ANIMATIONS + "UA_CreateCharView_CharChoose.controller": "civilization_character",
    ANIMATIONS + "UA_IFSearchRes.controller": "resource_search",
	ANIMATIONS + "UA_BuildingMenu.controller": "building_menu",
    SHARED + "UA_WinAnime_T1.controller": "T1",
    SHARED + "UA_WinAnime_IF.controller": "IF",
    SHARED + "UA_PopAnime.controller": "Pop",
    ANIMATIONS + "UA_PopAnime_hasCallback.controller": "Pop",
    ANIMATIONS + "UI_Item_HotTask.controller": "task_sidebar",
    ANIMATIONS + "UI_Item_Features.controller": "more_menu",
    ANIMATIONS + "UA_Chat.controller": "chat_drawer",
}
TRACK_SECTIONS = {
    "m_RotationCurves": ("rotation", "m_LocalRotation"),
    "m_EulerCurves": ("euler", "localEulerAnglesRaw"),
    "m_PositionCurves": ("position", "m_LocalPosition"),
    "m_ScaleCurves": ("scale", "m_LocalScale"),
    "m_FloatCurves": ("float", None),
}


class SourceError(RuntimeError):
    pass


def scalar(value):
    value = value.strip()
    if value.startswith("{") and value.endswith("}"):
        return {key.strip(): scalar(item) for key, item in
                (part.split(":", 1) for part in value[1:-1].split(","))}
    if value in {".inf", "Infinity", "inf"}:
        return "Infinity"
    if value in {"-.inf", "-Infinity", "-inf"}:
        return "-Infinity"
    try:
        number = float(value)
        if not math.isfinite(number):
            raise SourceError(f"Non-finite source value: {value}")
        return number
    except ValueError:
        return value.strip('"')


def field(text, name, default=None):
    match = re.search(rf"(?m)^\s*{re.escape(name)}:[ \t]*(.*)$", text)
    return scalar(match.group(1)) if match else default


def section(text, name):
    match = re.search(rf"(?m)^  {re.escape(name)}:[^\n]*(?:\n|$)", text)
    if not match:
        return "", 0
    end = re.search(r"(?m)^  [A-Za-z_][^:\n]*:", text[match.end():])
    stop = match.end() + end.start() if end else len(text)
    return text[match.end():stop], match.end()


def unity_blocks(text):
    matches = list(re.finditer(r"(?m)^--- !u!(\d+) &(-?\d+)[^\n]*\n", text))
    return [{"class_id": int(match[1]), "file_id": match[2],
             "line": text.count("\n", 0, match.start()) + 1,
             "text": text[match.end():matches[i + 1].start() if i + 1 < len(matches) else len(text)]}
            for i, match in enumerate(matches)]


def reference(text, name):
    value = field(text, name, {})
    return value if isinstance(value, dict) else {}


def file_id(value):
    # Unity IDs exceed IEEE double precision: always read IDs from raw text.
    match = re.search(r"fileID:\s*(-?\d+)", value)
    return match[1] if match else "0"


def referenced_id(text, name):
    match = re.search(rf"(?m)^\s*{re.escape(name)}:[ \t]*(.*)$", text)
    return file_id(match[1]) if match else "0"


class Builder:
    def __init__(self):
        self.sources = {}
        self.guid_paths = {}

    def read(self, source_path):
        path = resolve(source_path)
        if not path.is_file():
            raise SourceError(f"Missing source: {source_path}")
        data = path.read_bytes()
        text = data.decode("utf-8-sig").replace("\r\n", "\n")
        self.sources[source_path] = {
            "path": source_path, "sha256": hashlib.sha256(data).hexdigest(),
            "bytes": len(data), "lines": len(text.splitlines()),
        }
        return text

    def guid(self, source_path):
        meta = self.read(source_path + ".meta")
        guid = field(meta, "guid")
        if not isinstance(guid, str) or not re.fullmatch(r"[0-9a-f]{32}", guid):
            raise SourceError(f"Missing/invalid GUID: {source_path}.meta")
        self.guid_paths[guid] = source_path
        return guid

    def index_motion_sources(self):
        for extension in ("*.controller.meta", "*.anim.meta"):
            for meta in sorted((PROJECT_ROOT / (ASSETS + "BundleAssets")).rglob(extension)):
                text = meta.read_text(encoding="utf-8-sig")
                guid = field(text, "guid")
                if isinstance(guid, str):
                    self.guid_paths[guid] = meta.relative_to(PROJECT_ROOT).as_posix()[:-5]

    def resolve(self, guid):
        path = self.guid_paths.get(guid)
        if not path:
            raise SourceError(f"Unresolved motion source GUID: {guid}")
        self.read(path)
        self.guid(path)
        return path

    def evidence(self, source_path, tokens):
        text = self.read(source_path)
        result = []
        for token in tokens:
            start = text.find(token)
            if start < 0:
                raise SourceError(f"Missing source-chain token in {source_path}: {token}")
            result.append({"token": token, "line": text.count("\n", 0, start) + 1})
        return {"source_path": source_path, "anchors": result}

    def clip(self, source_path):
        text = self.read(source_path)
        front = text.split("  m_EditorCurves:", 1)[0]
        tracks = []
        for source_section, (kind, attribute) in TRACK_SECTIONS.items():
            body, body_offset = section(front, source_section)
            starts = list(re.finditer(r"(?m)^  - curve:[ \t]*$", body))
            for i, match in enumerate(starts):
                item = body[match.end():starts[i + 1].start() if i + 1 < len(starts) else len(body)]
                key_starts = list(re.finditer(r"(?m)^      - serializedVersion:.*$", item))
                keys = []
                for j, key_start in enumerate(key_starts):
                    raw = item[key_start.end():key_starts[j + 1].start() if j + 1 < len(key_starts) else len(item)]
                    if field(raw, "time") is None or field(raw, "value") is None:
                        raise SourceError(f"Unparsed keyframe: {source_path}")
                    keys.append({"time": field(raw, "time"), "value": field(raw, "value"),
                                 "in_slope": field(raw, "inSlope"), "out_slope": field(raw, "outSlope"),
                                 "tangent_mode": field(raw, "tangentMode", 0),
                                 "weighted_mode": field(raw, "weightedMode", 0),
                                 "in_weight": field(raw, "inWeight"), "out_weight": field(raw, "outWeight")})
                if not keys:
                    raise SourceError(f"Empty/unsupported curve: {source_path}")
                tracks.append({"kind": kind, "attribute": attribute or field(item, "attribute"),
                               "path": field(item, "path", ""), "class_id": field(item, "classID"),
                               "source_line": front.count("\n", 0, body_offset + match.start()) + 1,
                               "pre_infinity": field(item, "m_PreInfinity"),
                               "post_infinity": field(item, "m_PostInfinity"), "keys": keys})
        if not tracks:
            raise SourceError(f"No supported animation tracks: {source_path}")
        events_text, _ = section(text, "m_Events")
        events = []
        for event in re.split(r"(?m)(?=^  - time:)", events_text):
            if event.strip():
                event_time = re.search(r"(?m)^  - time:\s*(\S+)", event)
                if not event_time:
                    raise SourceError(f"Unsupported animation event: {source_path}")
                events.append({"time": scalar(event_time[1]), "function": field(event, "functionName"),
                               "data": field(event, "data", ""), "float_parameter": field(event, "floatParameter"),
                               "int_parameter": field(event, "intParameter")})
        start = field(front, "m_StartTime", 0)
        stop = field(front, "m_StopTime")
        motion_end = max(key["time"] for track in tracks for key in track["keys"])
        return {"name": field(front, "m_Name"), "source_path": source_path,
                "source_guid": self.guid(source_path), "legacy": bool(field(front, "m_Legacy", 0)),
                "duration_seconds": stop - start if stop is not None else motion_end,
                "motion_end_seconds": motion_end, "start_time": start, "stop_time": stop,
                "sample_rate": field(front, "m_SampleRate"), "loop": bool(field(front, "m_LoopTime", 0)),
                "wrap_mode": field(front, "m_WrapMode"), "tracks": tracks, "events": events}

    def controller(self, source_path):
        text = self.read(source_path)
        blocks = unity_blocks(text)
        states = []
        defaults = []
        for block in blocks:
            body = block["text"]
            if block["class_id"] == 1102:
                motion = reference(body, "m_Motion")
                guid = motion.get("guid")
                states.append({"name": field(body, "m_Name"), "file_id": block["file_id"],
                               "source_line": block["line"], "speed": field(body, "m_Speed"),
                               "write_default_values": bool(field(body, "m_WriteDefaultValues", 0)),
                               "clip_guid": guid, "clip_path": self.resolve(guid) if guid else None,
                               "has_state_behaviours": field(body, "m_StateMachineBehaviours") != "[]"})
            elif block["class_id"] == 1107:
                defaults.append(referenced_id(body, "m_DefaultState"))
        return {"source_path": source_path, "source_guid": self.guid(source_path),
                "mode": CONTROLLER_PATHS.get(source_path, "custom"),
                "default_states": [state["name"] for state in states if state["file_id"] in defaults],
                "states": states}

    def prefab(self, source_path, controllers):
        text = self.read(source_path)
        blocks = unity_blocks(text)
        objects = {b["file_id"]: b for b in blocks if b["class_id"] == 1}
        transforms = {b["file_id"]: b for b in blocks if b["class_id"] in {4, 224}}
        object_transforms = {referenced_id(b["text"], "m_GameObject"): b for b in transforms.values()}

        def object_path(object_id, visited=None):
            visited = set() if visited is None else visited
            if object_id in visited:
                raise SourceError(f"Transform cycle in {source_path}: {object_id}")
            visited.add(object_id)
            name = field(objects.get(object_id, {}).get("text", ""), "m_Name", f"unresolved_{object_id}")
            transform = object_transforms.get(object_id)
            parent_id = referenced_id(transform["text"], "m_Father") if transform else "0"
            if parent_id == "0":
                return name
            parent = transforms.get(parent_id)
            if parent is None:
                return f"unresolved_parent_{parent_id}/{name}"
            return object_path(referenced_id(parent["text"], "m_GameObject"), visited) + "/" + name

        bindings, buttons, legacy = [], [], []
        for block in blocks:
            body = block["text"]
            object_id = referenced_id(body, "m_GameObject")
            path = object_path(object_id) if block["class_id"] in {95, 111, 114} else ""
            if block["class_id"] == 95:
                guid = reference(body, "m_Controller").get("guid")
                controller_path = self.resolve(guid) if guid else None
                known = controllers.get(guid)
                state_clips = {state["name"]: Path(state["clip_path"]).stem
                               for state in known["states"] if state["clip_path"]} if known else {}
                transform = object_transforms.get(object_id)
                hierarchy_resolved = "unresolved_" not in path
                is_root = bool(transform and hierarchy_resolved and referenced_id(transform["text"], "m_Father") == "0")
                bindings.append({"object_path": path, "is_root": is_root,
                                 "hierarchy_resolved": hierarchy_resolved,
                                 "source_line": block["line"], "enabled": bool(field(body, "m_Enabled", 0)),
                                 "controller_guid": guid, "controller_path": controller_path,
                                 "mode": known["mode"] if known else ("custom" if guid else "none"),
                                 "default_states": known["default_states"] if known else [],
                                 "state_clips": state_clips, "update_mode": field(body, "m_UpdateMode"),
                                 "keep_state_on_disable": bool(field(body, "m_KeepAnimatorControllerStateOnDisable", 0))})
            elif block["class_id"] == 114 and "22b70ef09447998499be6d52107d721b" in body:
                clips = {}
                for label in ("DownAnimation", "UpAnimation"):
                    guid = reference(body, label).get("guid")
                    clips[label] = self.resolve(guid) if guid else None
                buttons.append({"object_path": path, "source_line": block["line"],
                                "pass_event": bool(field(body, "IsPassEvent", 0)), **clips})
            elif block["class_id"] == 111:
                guid = reference(body, "m_Animation").get("guid")
                legacy.append({"object_path": path, "source_line": block["line"],
                               "clip_path": self.resolve(guid) if guid else None,
                               "play_automatically": bool(field(body, "m_PlayAutomatically", 0))})
        root = [binding for binding in bindings if binding["is_root"] and binding["controller_guid"]]
        mode = root[0]["mode"] if len(root) == 1 else ("custom" if root else "none")
        return {"source_path": source_path, "presentation_mode": mode,
                "presentation_mode_scope": "root Animator only; child bindings never promote a whole-window animation",
                "controller_bindings": bindings, "button_bindings": buttons, "legacy_bindings": legacy,
                "nested_prefab_instance_count": sum(b["class_id"] == 1001 for b in blocks),
                "unresolved_nested_binding_count": sum(not binding["hierarchy_resolved"] for binding in bindings)}

    def power_gain_effect(self):
        prefab_path = UI + "UIEffect/UI_10010.prefab"
        prefab = self.read(prefab_path)
        blocks = unity_blocks(prefab)
        particles = [b for b in blocks if b["class_id"] == 198]
        meshes = [b for b in blocks if b["class_id"] == 33]
        if len(particles) != 1 or len(meshes) != 1:
            raise SourceError("UI_10010 expected one particle system and one mesh filter")
        effect_root = ASSETS + "BundleAssets/Effect/"
        dependencies = []

        def binary_asset(path, expected_guid):
            if self.guid(path) != expected_guid:
                raise SourceError(f"UI_10010 dependency GUID mismatch: {path}")
            data = resolve(path).read_bytes()
            self.sources[path] = {"path": path, "sha256": hashlib.sha256(data).hexdigest(),
                                  "bytes": len(data), "binary": True}
            return {"path": path, "guid": expected_guid}

        mesh = binary_asset(effect_root + "Meshes/m_liupian001.FBX",
                            reference(meshes[0]["text"], "m_Mesh")["guid"])
        for name in ("tile003", "glow002"):
            path = effect_root + f"Materials/shared_effectmaterials/{name}.mat"
            material = self.read(path)
            guid = self.guid(path)
            if guid not in prefab:
                raise SourceError(f"UI_10010 no longer references material {path}")
            main_tex = material.split("    - _MainTex:", 1)[1].split("    - ", 1)[0]
            texture = binary_asset(effect_root + f"Textures/shared_effecttextures/{name}.png",
                                   reference(main_tex, "m_Texture")["guid"])
            dependencies.append({"material": path, "guid": guid, "texture": texture,
                                 "shader_reference": reference(material, "m_Shader"),
                                 "render_queue": field(material, "m_CustomRenderQueue")})
        lifetime = [field(b["text"], "time") for b in blocks if b["class_id"] == 114
                    and field(b["text"], "time") is not None]
        if len(lifetime) != 1:
            raise SourceError("UI_10010 expected one AutoDestory lifetime")
        particle = particles[0]["text"]
        return {"prefab": prefab_path, "mesh": mesh, "materials": dependencies,
                "auto_destroy_seconds": lifetime[0],
                "particle": {key: field(particle, key) for key in
                             ("lengthInSec", "looping", "playOnAwake", "useUnscaledTime", "autoRandomSeed")},
                "runtime_implemented": False,
                "shader_resolution": {"name": "Mobile/Particles/Additive", "blend": "SrcAlpha One",
                                      "source_url": "https://download.unity3d.com/download_unity/1381962e9d08/builtin_shaders-2019.4.28f1.zip",
                                      "source_file": "DefaultResourcesExtra/Mobile/Mobile-Particle-Add.shader",
                                      "probe_script": "Scripts/ProbeRokUnityPowerEffect.cs",
                                      "ue_material": "/Game/RokPrototype/UI/Materials/M_RokPowerGainUI"},
                "remaining": ["Visually validate UE additive UI material against Unity",
                              "Reproduce mesh UV animation and particle modules in UE",
                              "Bind positive visible power changes and verify real HUD capture"]}

    def task_reward_layout(self):
        """Read the actual task row, including its non-Arabic runtime mirror.

        This is a deliberately bounded RectTransform extraction, not a generic
        prefab flattener. Nested glow activation must come from its override.
        """
        text = self.read(TASK_PREFAB)
        blocks = unity_blocks(text)

        def unique(items, label):
            if len(items) != 1:
                raise SourceError(f"Expected one {label} in {TASK_PREFAB}, got {len(items)}")
            return items[0]

        def object_block(name):
            return unique([b for b in blocks if b["class_id"] == 1 and field(b["text"], "m_Name") == name], name)

        def component(name, class_id, field_name=None):
            object_id = object_block(name)["file_id"]
            return unique([b for b in blocks if b["class_id"] == class_id
                           and referenced_id(b["text"], "m_GameObject") == object_id
                           and (field_name is None or field(b["text"], field_name) is not None)],
                          f"{name}/{field_name or class_id}")

        def rect(name, parent_size, parent_top_left=None):
            block = component(name, 224)
            body = block["text"]
            values = {key: field(body, source) for key, source in (
                ("anchor_min", "m_AnchorMin"), ("anchor_max", "m_AnchorMax"),
                ("anchored_position", "m_AnchoredPosition"), ("size_delta", "m_SizeDelta"), ("pivot", "m_Pivot"))}
            raw = {key: dict(value) for key, value in values.items()}
            object_id = object_block(name)["file_id"]
            arab = [b for b in blocks if b["class_id"] == 114
                    and referenced_id(b["text"], "m_GameObject") == object_id
                    and field(b["text"], "_arab_Layout") is not None]
            flags = int(field(arab[0]["text"], "_arab_Layout", 0)) if arab else 0
            if flags & 2 and not flags & 1:  # Source Awake mirrors Preview=false for non-Arabic.
                values["anchor_min"]["x"] = 1 - raw["anchor_max"]["x"]
                values["anchor_max"]["x"] = 1 - raw["anchor_min"]["x"]
                values["pivot"]["x"] = 1 - raw["pivot"]["x"]
                values["anchored_position"]["x"] = -raw["anchored_position"]["x"]
            size, lower_left = {}, {}
            for axis in ("x", "y"):
                span = values["anchor_max"][axis] - values["anchor_min"][axis]
                size[axis] = parent_size[axis] * span + values["size_delta"][axis]
                lower_left[axis] = parent_size[axis] * (values["anchor_min"][axis] + span * values["pivot"][axis]) \
                    + values["anchored_position"][axis] - size[axis] * values["pivot"][axis]
            origin = parent_top_left or {"x": 0, "y": 0}
            return {"object_name": name, "source_line": block["line"], "raw_rect": raw,
                    "arab_layout_flags": flags, "size": size,
                    "top_left": {"x": round(origin["x"] + lower_left["x"], 6),
                                 "y": round(origin["y"] + parent_size["y"] - lower_left["y"] - size["y"], 6)},
                    "pivot": values["pivot"]}

        def sprite(name, source_path):
            body = component(name, 114, "m_Sprite")["text"]
            sprite_id = referenced_id(body, "m_Sprite")
            guid_match = re.search(r"m_Sprite:\s*\{[^}]*guid:\s*([0-9a-f]{32})", body)
            guid = guid_match[1] if guid_match else None
            if guid != self.guid(source_path) or not resolve(source_path).is_file():
                raise SourceError(f"Wrong/missing task sprite source: {source_path}")
            meta = self.read(source_path + ".meta")
            items = re.split(r"(?m)(?=^    - serializedVersion:)", meta)
            item = unique([item for item in items if re.search(rf"(?m)^      internalID: {re.escape(sprite_id)}$", item)],
                          f"sprite {sprite_id}")
            match = re.search(r"(?m)^      rect:\n([\s\S]*?)(?=^      [a-zA-Z])", item)
            if not match:
                raise SourceError(f"Missing sprite rect: {source_path}/{sprite_id}")
            return {"source_path": source_path, "source_guid": guid, "file_id": sprite_id,
                    "name": field(item, "name"), "rect_bottom_left": {key: field(match[1], key) for key in ("x", "y", "width", "height")},
                    "border_left_bottom_right_top": field(item, "border"), "pivot": field(item, "pivot"),
                    "meta_source_line": meta.count("\n", 0, meta.index(item)) + 1}

        row = component("UI_Item_HotTask", 224)
        row_size = field(row["text"], "m_SizeDelta")
        effect = rect("pl_effect", row_size)
        reward = rect("img", effect["size"], effect["top_left"])
        reward["sprite"] = sprite("img", UI + "UITextureStatic/ui_res_common/ui_mainmenu.png")
        reward["tint"] = field(component("img", 114, "m_Sprite")["text"], "m_Color")
        background = rect("img_bg", row_size)
        background["sprite"] = sprite("img_bg", UI + "UITextureStatic/ui_res_common/ui_common.png")
        background["tint"] = field(component("img_bg", 114, "m_Sprite")["text"], "m_Color")
        texts = {}
        for label, name in (("title", "lbl_title"), ("description", "lbl_desc")):
            entry = rect(name, row_size)
            body = component(name, 114, "m_FontData")["text"]
            entry.update({key: field(body, source) for key, source in (("font_size", "m_FontSize"),
                ("font_style", "m_FontStyle"), ("line_spacing", "m_LineSpacing"), ("prefab_tint", "m_Color"),
                ("best_fit", "m_BestFit"), ("raw_alignment", "m_Alignment"))})
            entry["non_arabic_alignment"] = "LowerLeft" if label == "title" else "UpperLeft"
            texts[label] = entry
        glow = unique([b for b in blocks if b["class_id"] == 1001 and "value: UI_10035\n" in b["text"]], "UI_10035 instance")
        active = re.search(r"propertyPath: m_IsActive\s+value: ([01])", glow["text"])
        if not active:
            raise SourceError("Task glow is missing its explicit active-state override")
        glow_path = UI + "UIEffect/UI_10035.prefab"
        self.read(glow_path)
        if reference(glow["text"], "m_SourcePrefab").get("guid") != self.guid(glow_path):
            raise SourceError("Task glow prefab source changed")
        list_block = unique([b for b in unity_blocks(self.read(MAIN_PREFAB)) if b["class_id"] == 114
                             and "  - UI_Item_HotTask\n" in b["text"]], "task ListView")
        return {"source_path": TASK_PREFAB, "locale": "non-Arabic runtime (including zh-CN)",
                "coordinates": "row-local screen top-left; X right, Y down",
                "row": {"size": row_size, "source_line": row["line"], "pivot": field(row["text"], "m_Pivot")},
                "background": background, **texts, "reward_icon": reward,
                "list_spacing": field(list_block["text"], "spacing"), "list_offset": field(list_block["text"], "offset"),
                "list_source_path": MAIN_PREFAB, "list_source_line": list_block["line"],
                "glow": {"source_path": glow_path, "active_override": active[1] == "1",
                         "override_source_line": glow["line"] + glow["text"].count("\n", 0, active.start()) + 1},
                "description_fit": "reset font to 18, decrement until preferredHeight <= rect height",
                "locale_evidence": self.evidence(ARAB_LAYOUT_SOURCE, ["if (isArabUI == Skyunion.LanguageUtils.IsArabic())",
                    "SetStyle(ArabLayoutStyle.ArabLayoutStyle_Preview, !isArabUI)", "pos.x = -pos.x;"])}


def build_manifest(coverage_path=COVERAGE_PATH):
    builder = Builder()
    builder.index_motion_sources()
    coverage_path = Path(coverage_path)
    if not coverage_path.is_file():
        raise SourceError(f"Missing runtime coverage input: {coverage_path}")
    coverage = json.loads(coverage_path.read_text(encoding="utf-8-sig"))
    mappings = coverage.get("mappings")
    if not isinstance(mappings, list) or not mappings:
        raise SourceError("Runtime coverage has no mappings")
    clips = {}
    for path in CLIP_PATHS:
        clip = builder.clip(path)
        if clip["name"] in clips:
            raise SourceError(f"Duplicate clip name: {clip['name']}")
        clips[clip["name"]] = clip
    controllers = {}
    for path in CONTROLLER_PATHS:
        controller = builder.controller(path)
        controllers[controller["source_guid"]] = controller
    prefabs = {}
    runtime_mappings = []
    seen_ids = set()
    for mapping in mappings:
        mapping_id = mapping.get("id")
        prefab_path = mapping.get("unity_source", {}).get("prefab")
        if not mapping_id or mapping_id in seen_ids or not prefab_path:
            raise SourceError(f"Invalid/duplicate coverage mapping: {mapping_id}")
        seen_ids.add(mapping_id)
        if prefab_path not in prefabs:
            prefabs[prefab_path] = builder.prefab(prefab_path, controllers)
        prefab = prefabs[prefab_path]
        runtime_mappings.append({"id": mapping_id, "display_name": mapping.get("display_name"),
                                 "unity_prefab": prefab_path,
                                 "runtime_class": mapping.get("runtime_widget", {}).get("class"),
                                 "runtime_open_method": mapping.get("runtime_widget", {}).get("open_method"),
                                 "presentation_mode": prefab["presentation_mode"],
                                 "controller_bindings": prefab["controller_bindings"]})
    for path in (MAIN_PREFAB, *SUPPLEMENTAL_PREFABS):
        if path not in prefabs:
            prefabs[path] = builder.prefab(path, controllers)
    redpoint_has_motion = any(block["class_id"] in {95, 111}
                              for block in unity_blocks(builder.read(REDPOINT_PREFAB)))
    source_chains = {
        "button": {"down_clip": "CommonBtnDownAni", "up_clip": "CommonBtnUpAni",
                   "evidence": builder.evidence(BUTTON_SOURCE, ["public void OnPointerDown(", "public void OnPointerUp(",
                       "mButton.interactable == false", 'this.ani.Play("ButtonDownAni")', 'this.ani.Play("ButtonUpAni")'])},
        "task_sidebar": {"initial_collapsed": False, "collapse_clip": "UA_MainIF_HotTask_On",
                         "expand_clip": "UA_MainIF_HotTask_Off",
                         "evidence": builder.evidence(MEDIATOR_SOURCE, ["private void OnTaskUIAnimation()",
                             'view.m_UI_Item_HotTask_Animator.Play("UA_MainIF_HotTask_Off")',
                             'view.m_UI_Item_HotTask_Animator.Play("UA_MainIF_HotTask_On")', "CmdConstant.QuestBtnChange"])},
        "more_menu": {"initial_open": True, "locale_scope": "non-Arabic source branch",
                      "open_clip": "UA_MainIF_Rotation_ROn", "close_clip": "UA_MainIF_Rotation_ROff",
                      "evidence": builder.evidence(MEDIATOR_SOURCE, ["private bool isFeatureBtnOn = true;",
                          "private void OnFeatureBtnAnimation()", "private void SetFeatureBtnOn()",
                          "private void SetFeatureBtnOff()", "CmdConstant.FeatureBtnChange",
                          '"UA_MainIF_Rotation_ROn"', '"UA_MainIF_Rotation_ROff"'])},
        "window_close": {"remove_after_animation_event": True,
                         "evidence": builder.evidence(MANAGER_SOURCE, ["public void CloseAnimatorUI(",
                             "if (IsForceClose)", "if (uiInfo.gameView.IsAniCloseing)",
                             'animator.Play("Close", -1, 0)', "uiInfo.gameView.onCloseAnimatorCallback = () =>"])},
        "task_reward": {"idle_clip": "UA_TaskBox", "condition": "finished task or completed chapter",
                        "layout": builder.task_reward_layout(),
                        "states": {"unfinished": {"reward_visible": False, "row_action": "GoScript"},
                                   "finished": {"reward_visible": True, "row_action": "Task_TaskFinish"},
                                   "chapter": {"reward_visible_when": "finished_count == task_count", "row_action": "open UI.s_Taskinfo"}},
                        "claim_guard": "finished and not taskAnimShow",
                        "title_colors": {"main_or_chapter": "#ECAE55", "side": "#FFFFFF"},
                        "rotation_conversion": "Unity positive Z is counter-clockwise on screen; negate for UMG screen angle",
                        "removal": {"main": "DOScale to 0.1 over 0.3 seconds then refresh; no source SetEase override",
                                    "side_clip": "MissionItemRemove", "side_layout": "row height times current localScale.y",
                                    "authority_note": "presentation only; this manifest does not provide task state or reward authority"},
                        "evidence": builder.evidence(MEDIATOR_SOURCE, ["case TaskState.finished:",
                            "itemView.m_pl_effect_ArabLayoutCompment.gameObject.SetActive(true)",
                            "case TaskState.unfinished:", "itemView.m_pl_effect_ArabLayoutCompment.gameObject.SetActive(false)",
                            "m_taskchapterFinishCount == m_taskchapterCount", "if (taskData.taskState != TaskState.finished)",
                            "Task_TaskFinish.request req", "CmdConstant.GoScript, taskData", "ShowUI(UI.s_Taskinfo)",
                            'ClientUtils.StringToHtmlColor("#ECAE55")', 'ClientUtils.StringToHtmlColor("#FFFFFF")',
                            "private const int fontsize = 18;", "while (text.preferredHeight > height)", "text.fontSize -= 1;",
                            "DOScale(new Vector3(0.1f, 0.1f, 0.1f), 0.3f)",
                            "UpdateItemSize(m_deleteItem.index, m_height * m_deleteItem.go.transform.localScale.y)"])},
        "chat_drawer": {"open_clip": "UA_Chat_On", "close_clip": "UA_Chat_Off", "controller_mode": "chat_drawer",
                        "animation_scope": "root anchored X and CanvasGroup alpha, not the separate contact-list pl_content tween",
                        "close_after_event": "CloseAnimatorCallback",
                        "evidence": builder.evidence(CHAT_MEDIATOR_SOURCE, ["CloseUI(UI.s_chat)", "private void CheckChatClose(UIInfo uiInfo)",
                            "private void OnChatShrink()", "DOLocalMoveX(m_contentLocalPos.x, 0.2f).SetEase(Ease.InCubic)"]),
                        "entry_evidence": builder.evidence(MEDIATOR_SOURCE, ["private void OnClickChatBtn()", "ShowUI(UI.s_chat)"]),
                        "layer_evidence": builder.evidence(ASSETS + "Scripts/Hotfix/MVC/UI.cs", [
                            "new UIViewInfo(UIViewType.Window, UILayer.ChatLayer, UIAddMode.Stack, UICloseMode.PopWin)",
                            "typeof(UI_Win_ChatView),s_chatview,EnumMaskStatus.kNone"])},
        "power_gain_effect": builder.power_gain_effect(),
        "power_counter": {
            "presentation_implemented": True,
            "authority": "UE local preview score; server combatPower not yet bound",
            "evidence": builder.evidence(ASSETS + "Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_PlayerPowerInfo_SubView.cs", [
                "private void SetSequence(long lastPower,long diff)", "i < 20", "AppendInterval(0.06f)",
                "lastPower + i * diff / 20", "ChangeText(lastPower + diff)", "SetEase(Ease.OutCirc)",
                "currentPower>m_lastPower&&!isEffectLoading&&NotInvisible()", 'Instantiate("UI_10010"'])},
        "hud_alliance_chat_unread": {
            "runtime_implemented": False,
            "presentation_implemented": True,
            "runtime_presentation_adapter": "URokMainHudWidget::SetChatChannelPresentation",
            "binding_gap": "Live alliance membership, active chat channel and unread-message authority are not yet bound to the presentation adapter",
            "evidence": builder.evidence(MEDIATOR_SOURCE, [
                "private void OnShowOrHideAlliancePage()", "m_allianceProxy.HasJionAlliance()",
                "private void RefreshChatPage()", "case EnumChatChannel.world:",
                "m_chatProxy.AllianceContact.GetUnreadCount() > 0",
                'view.m_img_chat2_Animator.Play("Flash")',
                "case EnumChatChannel.alliance:", "view.m_img_chat2_Animator.enabled = false;"])
        },
        "redpoint": {"generic_loop_animation": redpoint_has_motion, "prefab_path": REDPOINT_PREFAB,
                     "component_evidence": builder.evidence(ASSETS + "Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Common_Redpoint_SubView.cs",
                         ["public void ShowRedPoint(int count)", "public void ShowSmallRedPoint(int count)", "public void HideRedPoint()"]),
                     "evidence": builder.evidence(MEDIATOR_SOURCE, ["private void RefreshTaskRedPointView()",
                         "SetActive(redPoint != 0)", "SetActive(num > 0)"])},
    }
    builder.evidence(VIEW_SOURCE, ["pl_task_set/UI_Item_HotTask", "pl_menu/UI_Item_Features/pl_rect"])
    builder.evidence(BINDER_SOURCE, ["public void RunDefaultAni()", "public void CloseAnimatorCallback()"])
    return {"schema": "rok.unity_ui_animation_manifest.v1", "schema_version": 1,
            "inputs": {"runtime_coverage": coverage_path.resolve().as_posix(),
                       "runtime_mapping_ids": sorted(seen_ids)},
            "scope": {"prefab_scan": "direct serialized components; nested prefab instances are counted, not expanded",
                      "curve_interpolation": "Unity Hermite tangents; zero unweighted tangents mean piecewise smoothstep",
                      "duration_note": "Legacy serialized stop_time can exceed the last moving key; preserve both, do not assume clip.length",
                      "source_evidence_only": True, "runtime_visual_acceptance_claimed": False},
            "summary": {"runtime_mapping_count": len(runtime_mappings), "unique_runtime_prefab_count": len({x['unity_prefab'] for x in runtime_mappings}),
                        "clip_count": len(clips), "controller_count": len(controllers),
                        "presentation_mode_counts": dict(sorted(Counter(x["presentation_mode"] for x in runtime_mappings).items()))},
            "clips": clips, "controllers": controllers, "prefabs": prefabs,
            "supplemental_prefabs": SUPPLEMENTAL_PREFABS,
            "runtime_mappings": runtime_mappings, "source_chains": source_chains,
            "sources": [builder.sources[path] for path in sorted(builder.sources)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coverage", type=Path, default=COVERAGE_PATH)
    parser.add_argument("--output", type=Path, default=OUTPUT_PATH)
    args = parser.parse_args()
    try:
        manifest = build_manifest(args.coverage)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    except (SourceError, OSError, UnicodeError, ValueError) as error:
        parser.exit(1, f"UI animation extraction failed: {error}\n")
    print(json.dumps({"output": str(args.output), **manifest["summary"]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
