#!/usr/bin/env python3
"""Offline Unity -> UE migration inventory for Rok.

Reads only files on disk (no Unreal, no Unity, no network):
  - Unity client Hotfix code: ref/client-unity/Assets/Scripts/Hotfix
      MVC/UI.cs registers every screen (UIInfo), each View class names its
      prefab (VIEW_NAME), mediators listen to `<Msg>.TagName`, send
      `new <Msg>.request`, and read config through `<Table>Define`.
  - Protocol: Content/RokData/Protocol/Protocol.sproto (the copy UE loads;
      same 237 message names/ids as the server and client copies).
  - Server: ref/server/data/work/server (proxy/<Module>.lua `response.X`
      handlers = client requests; quoted "Module_X" names = pushes/refs).
  - UE: Source/ (product code vs Source/**/Tests) and Scripts/.

Writes Saved/RokUnityMigration/MigrationInventory/{inventory.json,
screens.csv, modules.csv, systems.csv, protocol.csv, config_tables.csv,
ue_surfaces.csv}. Regenerate instead of editing the outputs.

"traced" means UE code names the Unity source (prefab, view, mediator) or a
UE screen surface matches it by name/alias. It is NOT screen parity: parity
still needs a side-by-side capture (see CLAUDE.md acceptance gates).

Run:  py -3.12 -B -X utf8 Scripts/BuildRokMigrationInventory.py
"""
from __future__ import annotations

import csv
import hashlib
import json
import re
import subprocess
import sys
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HOTFIX = ROOT / "ref/client-unity/Assets/Scripts/Hotfix"
UI_CS = HOTFIX / "MVC/UI.cs"
CONFIG_DIR = HOTFIX / "Config"
VIEW_MEDIATOR = HOTFIX / "MVC/View_Mediator"
SPROTO = ROOT / "Content/RokData/Protocol/Protocol.sproto"
SERVER = ROOT / "ref/server/data/work/server"
SERVER_PROXY = SERVER / "game_server/logic/service/proxy"
UE_CONFIG_TABLES = ROOT / "Content/RokData/ConfigTables"
UE_SOURCE = ROOT / "Source"
UE_SCRIPTS = ROOT / "Scripts"
FEATURE_SCREEN_H = UE_SOURCE / "Rok/RokUnityFeatureScreenWidget.h"
MODAL_WIDGET_H = UE_SOURCE / "Rok/RokUnityModalWidget.h"
OUT = ROOT / "Saved/RokUnityMigration/MigrationInventory"

# Unity folders that are developer tools, not product features.
DEV_ONLY_MODULES = {"TestUICompoment", "TestView", "GameTool"}

# UE surface -> Unity prefab names it implements, where the names differ.
# Equal normalized names are matched automatically. Entries are name-based
# candidates (the UE builder does not cite the prefab); a screen matched only
# here stays "name_match" until UE code names its Unity source. Sources:
# git 0f59fce:UnityLearning/MIGRATION_PLAN_2026-10-18.md (plan) and the Unity
# UI.cs s_* names. Unmapped on purpose (no confident Unity hub found):
# feature:ActivityCenter, Expedition, GemShop, Help, ArmyStatistics.
SURFACE_ALIASES: dict[str, list[str]] = {
    # Plan: "UI_Win_AddSpeed ... already exists as the feature screen".
    "feature:SpeedUp": ["UI_Win_AddSpeed"],
    # Plan Phase 5 / Phase 6 log.
    "modal:GainEffect": ["UI_Win_GainEffect"],
    "modal:VipPrivilege": ["UI_Win_Vip"],
    "modal:TavernBoxDesc": ["UI_Win_TavernBoxDesc"],
    # UI.cs s_Taskinfo; BuildServerTaskChapter cites UI_Win_Quest.
    "feature:Tasks": ["UI_Win_Quest"],
    # BuildCommander cites UI_IF_Captain / CaptainMediator.
    "feature:Commander": ["UI_IF_Captain"],
    # BuildItems follows BagProxy.Items.
    "feature:Items": ["UI_Win_Bag"],
    # UI.cs s_AllianceMain / s_AllianceJionList.
    "feature:Alliance": ["UI_Win_GuildMain", "UI_Win_GuildJoin"],
    "feature:WorldPositionSearch": ["UI_Pop_WorldPosSearch"],
    "feature:MapMarkers": ["UI_Win_Book"],
    "feature:ResourceSearch": ["UI_IF_SearchRes"],
    "feature:WorkerQueue": ["UI_Win_Worker"],
    # UI.cs s_Setting = PlayerSettingView (UI_Win_Setting).
    "feature:SettingsHub": ["UI_Win_Setting"],
    "feature:GeneralSettings": ["UI_Win_GeneralSetting"],
    "feature:NotificationSettings": ["UI_Win_SettingNotice"],
    "feature:LanguageSettings": ["UI_Win_Language"],
    "feature:AccountSettings": ["UI_Win_AccountMgr"],
    "feature:Research": ["UI_Win_ResearchMain"],
    "widget:Research": ["UI_Win_ResearchMain"],
    # BuildArmyComposer (GatherArmy) = troop creation.
    "feature:GatherArmy": ["UI_Win_CreateArmy"],
    "modal:BuildingInfo": ["UI_Win_BuildingDesc"],
    "modal:BuildingUpgrade": ["UI_Win_BuildingUpdate"],
    "modal:CivilizationChangeConfirmation": ["UI_Win_CivilizationChange"],
    "modal:PlayerProfile": ["UI_Win_PlayerData"],
    "modal:PlayerHeadSelection": ["UI_Win_PlayerHeadPic"],
    "modal:PlayerPowerDetails": ["UI_PlayerPowerView"],
    "modal:PlayerRanking": ["UI_Win_Ranking"],
    "modal:WarWarning": ["UI_Win_Warn"],
    "widget:MainHud": ["UI_IF_MainInterface"],
}

# UE surfaces that are developer/diagnostic tools with no Unity counterpart.
UE_DEV_SURFACES = {"modal:UiGallery", "modal:ResourceBuildingInfoReferencePreview"}

MSG_RE = re.compile(r"^([A-Z][A-Za-z0-9]*)_([A-Za-z0-9]+)\s+(\d+)\s*\{", re.M)
TAG_RE = re.compile(r"\b([A-Z][A-Za-z0-9]*_[A-Za-z0-9]+)\.TagName\b")
REQ_RE = re.compile(r"\bnew\s+([A-Z][A-Za-z0-9]*_[A-Za-z0-9]+)\.request\b")
DEFINE_RE = re.compile(r"\b([A-Z][A-Za-z0-9]*)Define\b")
CLASS_RE = re.compile(r"\b(?:class|struct|enum)\s+([A-Za-z_][A-Za-z0-9_]*)")
VIEW_NAME_RE = re.compile(r'const\s+string\s+VIEW_NAME\s*=\s*"([^"]+)"')
UIINFO_RE = re.compile(
    r"public\s+static\s+UIInfo\s+(\w+)\s*=\s*new\s+UIInfo\s*\(\s*([^,]+?)\s*,"
    r"\s*typeof\s*\(\s*([\w.]+)\s*\)\s*,\s*(\w+)",
    re.S,
)
IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
QUOTED_RE = re.compile(r"[\"']([A-Za-z0-9_./-]+)[\"']")
CSV_TOKEN_RE = re.compile(r"([A-Za-z0-9_]+)\.csv\b")
CS_COMMENT_OR_STRING_RE = re.compile(
    r'//[^\n]*|/\*.*?\*/|@"(?:[^"]|"")*"|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'',
    re.S,
)


def read_text(path: Path) -> str:
    data = path.read_bytes()
    for enc in ("utf-8-sig", "gb18030"):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def strip_cs_comments(text: str) -> str:
    """Drop // and /* */ comments, keep string literals."""
    return CS_COMMENT_OR_STRING_RE.sub(
        lambda m: m.group(0) if m.group(0)[0] in "\"@'" else " ", text
    )


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def norm(name: str) -> str:
    """Normalize a screen name for cross-engine matching."""
    s = re.sub(r"[^a-z0-9]", "", name.lower())
    for prefix in ("uiwin", "uiif", "uipop", "uitip", "uicommon", "ui"):
        if s.startswith(prefix) and len(s) > len(prefix) + 2:
            s = s[len(prefix):]
            break
    for suffix in ("mediator", "screenwidget", "widget", "screen", "view"):
        if s.endswith(suffix) and len(s) > len(suffix) + 2:
            s = s[: -len(suffix)]
            break
    return s


def git_head() -> str:
    try:
        return subprocess.run(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


# ---------------------------------------------------------------- protocol

def parse_sproto(path: Path) -> dict[str, dict]:
    text = "\n".join(line.split("#", 1)[0] for line in read_text(path).splitlines())
    messages: dict[str, dict] = {}
    for m in MSG_RE.finditer(text):
        name = f"{m.group(1)}_{m.group(2)}"
        depth, i = 0, m.end() - 1
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = text[m.end(): i]
        messages[name] = {
            "name": name,
            "module": m.group(1),
            "id": int(m.group(3)),
            "has_request": bool(re.search(r"\brequest\s*\{", body)),
            "has_response": bool(re.search(r"\bresponse\s*\{", body)),
        }
    return messages


def scan_server(messages: dict[str, dict]) -> None:
    handlers: set[str] = set()
    for lua in sorted(SERVER_PROXY.glob("*.lua")):
        module = lua.stem
        for m in re.finditer(r"function\s+(?:response|accept)\.(\w+)\s*\(", read_text(lua)):
            handlers.add(f"{module}_{m.group(1)}")
    quoted: dict[str, set[str]] = defaultdict(set)
    for lua in SERVER.rglob("*.lua"):
        for m in QUOTED_RE.finditer(read_text(lua)):
            if m.group(1) in messages:
                quoted[m.group(1)].add(rel(lua))
    for name, msg in messages.items():
        msg["server_handler"] = name in handlers
        msg["server_quoted_files"] = len(quoted.get(name, ()))
        if msg["server_handler"]:
            msg["direction"] = "c2s"
        elif quoted.get(name):
            msg["direction"] = "s2c_push"
        else:
            msg["direction"] = "not_named_on_server"


# ------------------------------------------------------------------- Unity

class UnityIndex:
    def __init__(self, messages: dict[str, dict], tables: set[str]):
        self.files: dict[Path, str] = {}
        self.class_file: dict[str, Path] = {}
        self.view_name: dict[str, str] = {}
        self.listens: dict[Path, set[str]] = {}
        self.sends: dict[Path, set[str]] = {}
        self.defines: dict[Path, set[str]] = {}
        self.loc: dict[Path, int] = {}
        for cs in sorted(HOTFIX.rglob("*.cs")):
            raw = read_text(cs)
            code = strip_cs_comments(raw)
            self.files[cs] = code
            self.loc[cs] = sum(1 for line in raw.splitlines() if line.strip())
            events = sorted(
                [(m.start(), "class", m.group(1)) for m in CLASS_RE.finditer(code)]
                + [(m.start(), "view", m.group(1)) for m in VIEW_NAME_RE.finditer(code)]
            )
            current = None
            for _, kind, value in events:
                if kind == "class":
                    current = value
                    self.class_file.setdefault(value, cs)
                elif current:
                    self.view_name.setdefault(current, value)
            self.listens[cs] = {t for t in TAG_RE.findall(code) if t in messages}
            self.sends[cs] = {t for t in REQ_RE.findall(code) if t in messages}
            self.defines[cs] = {t for t in DEFINE_RE.findall(code) if t in tables}

    def module_of(self, path: Path) -> str:
        parts = path.relative_to(HOTFIX).parts
        if parts[:2] == ("MVC", "View_Mediator") and len(parts) > 3:
            return parts[2]
        if parts[0] == "MVC" and len(parts) > 2:
            return f"MVC/{parts[1]}"
        return parts[0] if len(parts) > 1 else "(root)"

    def find_mediator(self, view_cls: str) -> str:
        view_file = self.class_file.get(view_cls)
        guesses = []
        if view_cls.endswith("View"):
            guesses.append(view_cls[:-4] + "Mediator")
        guesses.append(view_cls + "Mediator")
        for g in guesses:
            if g in self.class_file:
                return g
        token = re.compile(rf"\b{re.escape(view_cls)}\b")
        same_dir, anywhere = [], []
        for cls, path in self.class_file.items():
            if not cls.endswith("Mediator") or path == view_file:
                continue
            if token.search(self.files[path]):
                (same_dir if view_file and path.parent == view_file.parent else anywhere).append(cls)
        return (sorted(same_dir) or sorted(anywhere) or [""])[0]


def parse_config_tables() -> set[str]:
    names = set()
    for cs in CONFIG_DIR.glob("*.cs"):
        names.update(m.group(1) for m in re.finditer(r"class\s+(\w+)Define\b", read_text(cs)))
    return names


# ---------------------------------------------------------------------- UE

TABLE_CONTEXT_RE = re.compile(r"ConfigTables|config-tables|\.csv\b", re.I)


class TextIndex:
    """Token/quoted-string/csv-name index over a source tree."""

    def __init__(self, root: Path, patterns: tuple[str, ...]):
        self.tokens: dict[str, set[str]] = defaultdict(set)
        self.quoted: dict[str, set[str]] = defaultdict(set)
        self.csv: dict[str, set[str]] = defaultdict(set)
        # Quoted names in files that load config tables: catches
        # TEXT("Hero") / "troops": "Arms" without matching every "Config".
        self.table_quoted: dict[str, set[str]] = defaultdict(set)
        self.files: list[str] = []
        for pattern in patterns:
            for path in sorted(root.rglob(pattern)):
                if any(part in {"bin", "obj", "__pycache__"} for part in path.parts):
                    continue
                if path.resolve() == Path(__file__).resolve():
                    continue  # this script names tables in its own docs
                text = read_text(path)
                key = rel(path)
                self.files.append(key)
                for tok in set(IDENT_RE.findall(text)):
                    self.tokens[tok].add(key)
                quoted = set(QUOTED_RE.findall(text))
                for q in quoted:
                    self.quoted[q].add(key)
                for c in set(CSV_TOKEN_RE.findall(text)):
                    self.csv[c].add(key)
                if TABLE_CONTEXT_RE.search(text):
                    for q in quoted:
                        self.table_quoted[q].add(key)

    def table_files(self, table: str) -> set[str]:
        return self.csv.get(table, set()) | self.table_quoted.get(table, set())


def is_test(path: str) -> bool:
    return "/Tests/" in path or path.endswith("Tests.cpp") or path.endswith("Test.cpp")


def is_audit(path: str) -> bool:
    """Read-only audit/preview harnesses (CLAUDE.md): evidence, not product."""
    return "Audit" in Path(path).name


def split_files(files: set[str]) -> tuple[list[str], list[str]]:
    """(product, other) where other = tests + audit harnesses."""
    product = sorted(f for f in files if not is_test(f) and not is_audit(f))
    other = sorted(f for f in files if is_test(f) or is_audit(f))
    return product, other


def ue_surfaces() -> list[dict]:
    surfaces = []
    enum = re.search(r"enum class ERokUnityFeatureScreenMode[^{]*\{(.*?)\};", read_text(FEATURE_SCREEN_H), re.S)
    if enum:
        for name in re.findall(r"\b([A-Z][A-Za-z0-9]+)\b", enum.group(1)):
            if name != "None":
                surfaces.append({"id": f"feature:{name}", "kind": "feature_screen", "name": name,
                                 "source": rel(FEATURE_SCREEN_H)})
    for name in sorted(set(re.findall(r"\bvoid\s+Open([A-Z][A-Za-z0-9]+)\s*\(", read_text(MODAL_WIDGET_H)))):
        surfaces.append({"id": f"modal:{name}", "kind": "modal_window", "name": name,
                         "source": rel(MODAL_WIDGET_H)})
    for h in sorted((UE_SOURCE / "Rok").glob("*.h")):
        for m in re.finditer(r"class\s+ROK_API\s+URok([A-Za-z0-9]+?)(?:ScreenWidget|Widget)\b", read_text(h)):
            name = m.group(1)
            if name in {"UnityFeatureScreen", "UnityModal"}:
                continue
            surfaces.append({"id": f"widget:{name}", "kind": "widget_class", "name": name, "source": rel(h)})
    for s in surfaces:
        s["norm"] = norm(s["name"])
    return surfaces


# -------------------------------------------------------------------- main

def coverage(items: set[str], used: set[str]) -> str:
    return f"{len(items & used)}/{len(items)}" if items else ""


def main() -> int:
    for required in (HOTFIX, UI_CS, SPROTO, SERVER_PROXY, UE_SOURCE):
        if not required.exists():
            print(f"missing input: {required}", file=sys.stderr)
            return 2

    messages = parse_sproto(SPROTO)
    scan_server(messages)
    tables = parse_config_tables()
    unity = UnityIndex(messages, tables)
    ue = TextIndex(UE_SOURCE, ("*.h", "*.cpp", "*.inl", "*.cs"))
    scripts = TextIndex(UE_SCRIPTS, ("*.py", "*.cs", "*.ps1"))

    # Protocol usage in UE: a quoted name in product code means it is wired.
    ue_msg_used: set[str] = set()
    for name, msg in messages.items():
        quoted_prod, quoted_tests = split_files(ue.quoted.get(name, set()))
        token_prod, _ = split_files(ue.tokens.get(name, set()))
        msg["ue_product_files"] = quoted_prod
        msg["ue_test_or_audit_files"] = quoted_tests
        msg["ue_status"] = "wired" if quoted_prod else ("cited" if token_prod else ("test_or_audit_only" if quoted_tests else "none"))
        if quoted_prod:
            ue_msg_used.add(name)
        listen = sorted(rel(p) for p, s in unity.listens.items() if name in s)
        send = sorted(rel(p) for p, s in unity.sends.items() if name in s)
        msg["unity_listen_files"] = listen
        msg["unity_send_files"] = send
        msg["unity_modules"] = sorted({unity.module_of(ROOT / f) for f in listen + send})

    # Config tables.
    ue_table_used: set[str] = set()
    table_rows = []
    for t in sorted(tables):
        unity_files = sorted(rel(p) for p, s in unity.defines.items() if t in s)
        src_prod, src_tests = split_files(ue.table_files(t))
        script_files = sorted(scripts.table_files(t))
        csv_path = UE_CONFIG_TABLES / f"{t}.csv"
        if src_prod:
            status = "source"
        elif script_files:
            status = "scripts_pipeline"
        elif src_tests:
            status = "test_or_audit_only"
        else:
            status = "none"
        if status in {"source", "scripts_pipeline"}:
            ue_table_used.add(t)
        table_rows.append({
            "table": t, "in_rokdata": csv_path.exists(),
            "unity_files": len(unity_files),
            "unity_modules": sorted({unity.module_of(ROOT / f) for f in unity_files}),
            "ue_status": status, "ue_product_files": src_prod,
            "ue_test_or_audit_files": src_tests, "ue_script_files": script_files,
        })

    surfaces = ue_surfaces()
    surface_by_norm: dict[str, list[dict]] = defaultdict(list)
    for s in surfaces:
        surface_by_norm[s["norm"]].append(s)
    alias_by_prefab: dict[str, list[str]] = defaultdict(list)
    surface_ids = {s["id"] for s in surfaces}
    for sid, prefabs in SURFACE_ALIASES.items():
        if sid not in surface_ids:
            print(f"warning: alias key {sid} is not a UE surface", file=sys.stderr)
        for p in prefabs:
            alias_by_prefab[p].append(sid)

    # Screens from UI.cs.
    ui_code = strip_cs_comments(read_text(UI_CS))
    screens = []
    for m in UIINFO_RE.finditer(ui_code):
        s_name, first, view_cls, model = m.groups()
        view_cls = view_cls.split(".")[-1]
        if first.startswith('"'):
            prefab = first.strip('"')
        elif first.endswith(".VIEW_NAME"):
            prefab = unity.view_name.get(first.split(".")[0], first)
        else:
            prefab = first
        mediator = unity.find_mediator(view_cls)
        view_file = unity.class_file.get(view_cls)
        med_file = unity.class_file.get(mediator) if mediator else None
        files = [f for f in (view_file, med_file) if f]
        module = unity.module_of(files[0]) if files else "(unresolved)"
        listens = set().union(*(unity.listens[f] for f in files)) if files else set()
        sends = set().union(*(unity.sends[f] for f in files)) if files else set()
        defs = set().union(*(unity.defines[f] for f in files)) if files else set()
        msgs = listens | sends

        cite_tokens = {t for t in (prefab, view_cls, mediator, s_name) if t and len(t) > 4}
        cited = set()
        for t in cite_tokens:
            cited |= ue.tokens.get(t, set())
        cited_prod, cited_tests = split_files(cited)
        matched = [s["id"] for s in surface_by_norm.get(norm(prefab), [])]
        matched += [s["id"] for s in surface_by_norm.get(norm(view_cls), []) if s["id"] not in matched]
        matched += [sid for sid in alias_by_prefab.get(prefab, []) if sid not in matched]

        if module in DEV_ONLY_MODULES:
            status = "dev_only"
        elif cited_prod:
            status = "traced"
        elif matched:
            status = "name_match"
        elif cited_tests:
            status = "test_or_audit_only"
        else:
            status = "none"
        screens.append({
            "ui_info": s_name, "prefab": prefab, "view_class": view_cls,
            "mediator": mediator, "module": module, "view_model": model,
            "unity_files": [rel(f) for f in files],
            "unity_loc": sum(unity.loc[f] for f in files),
            "messages": sorted(msgs), "messages_ue_wired": sorted(msgs & ue_msg_used),
            "tables": sorted(defs), "tables_ue_used": sorted(defs & ue_table_used),
            "ue_surfaces": matched, "ue_product_files": cited_prod,
            "ue_test_or_audit_files": cited_tests, "status": status,
        })

    known_prefabs = {s["prefab"] for s in screens}
    for p in sorted(set(alias_by_prefab) - known_prefabs):
        print(f"warning: alias prefab {p} is not a Unity UIInfo screen", file=sys.stderr)

    # Modules (folders) and system classes (GlobalMediator/Proxy/CMD/Troop).
    module_files: dict[str, list[Path]] = defaultdict(list)
    for f in unity.files:
        module_files[unity.module_of(f)].append(f)
    modules = []
    systems = []
    for module, files in sorted(module_files.items()):
        msgs = set().union(*(unity.listens[f] | unity.sends[f] for f in files))
        defs = set().union(*(unity.defines[f] for f in files))
        classes = {c for c, p in unity.class_file.items() if p in set(files)}
        cited = set()
        for c in classes:
            if len(c) > 6:
                cited |= ue.tokens.get(c, set())
        cited_prod, _ = split_files(cited)
        mod_screens = [s for s in screens if s["module"] == module]
        server_backed = {m for m in msgs if messages[m]["server_handler"] or messages[m]["direction"] == "s2c_push"}
        modules.append({
            "module": module, "dev_only": module in DEV_ONLY_MODULES,
            "files": len(files), "loc": sum(unity.loc[f] for f in files),
            "screens": len(mod_screens),
            "screens_traced": sum(1 for s in mod_screens if s["status"] == "traced"),
            "screens_name_match": sum(1 for s in mod_screens if s["status"] == "name_match"),
            "messages": len(msgs), "messages_server_backed": len(server_backed),
            "messages_ue_wired": len(msgs & ue_msg_used),
            "tables": len(defs), "tables_ue_used": len(defs & ue_table_used),
            "ue_files_citing_module_classes": len(cited_prod),
        })
        if module.startswith("MVC/") and module.split("/")[1] in {"GlobalMediator", "Proxy", "CMD", "Troop"}:
            for f in sorted(files):
                cls = f.stem
                cprod, _ = split_files(ue.tokens.get(cls, set()))
                fm = unity.listens[f] | unity.sends[f]
                systems.append({
                    "module": module, "class": cls, "file": rel(f), "loc": unity.loc[f],
                    "messages": len(fm), "messages_ue_wired": len(fm & ue_msg_used),
                    "tables": len(unity.defines[f]),
                    "tables_ue_used": len(unity.defines[f] & ue_table_used),
                    "ue_product_files": cprod,
                })

    for s in surfaces:
        s["unity_screens"] = [x["prefab"] for x in screens if s["id"] in x["ue_surfaces"]]
        s["dev_only"] = s["id"] in UE_DEV_SURFACES

    # Summary.
    def count(rows, key, value):
        return sum(1 for r in rows if r[key] == value)

    product_screens = [s for s in screens if s["status"] != "dev_only"]
    msg_by_module: dict[str, list[int]] = defaultdict(lambda: [0, 0, 0])
    for msg in messages.values():
        row = msg_by_module[msg["module"]]
        row[0] += 1
        row[1] += msg["name"] in ue_msg_used
        row[2] += msg["direction"] != "not_named_on_server"
    summary = {
        "generated_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "git_head": git_head(),
        "inputs": {
            "protocol_sproto": {"path": rel(SPROTO), "sha256": sha256(SPROTO)},
            "unity_ui_cs": {"path": rel(UI_CS), "sha256": sha256(UI_CS)},
        },
        "screens": {
            "total": len(screens), "dev_only": count(screens, "status", "dev_only"),
            "product": len(product_screens),
            **{k: count(product_screens, "status", k)
               for k in ("traced", "name_match", "test_or_audit_only", "none")},
        },
        "protocol": {
            "messages": len(messages),
            "ue_wired": len(ue_msg_used),
            "ue_cited_only": sum(1 for m in messages.values() if m["ue_status"] == "cited"),
            "server_c2s": sum(1 for m in messages.values() if m["direction"] == "c2s"),
            "server_push": sum(1 for m in messages.values() if m["direction"] == "s2c_push"),
            "not_named_on_server": sum(1 for m in messages.values() if m["direction"] == "not_named_on_server"),
            "by_module": {k: {"total": v[0], "ue_wired": v[1], "server_backed": v[2]}
                          for k, v in sorted(msg_by_module.items())},
        },
        "config_tables": {
            "unity_defines": len(tables),
            "used_by_unity_code": sum(1 for r in table_rows if r["unity_files"]),
            "in_rokdata": sum(1 for r in table_rows if r["in_rokdata"]),
            **{k: count(table_rows, "ue_status", k) for k in ("source", "scripts_pipeline", "test_or_audit_only", "none")},
        },
        "unity_code": {
            "files": len(unity.files), "loc": sum(unity.loc.values()),
            "modules": len(modules),
        },
        "ue_surfaces": len(surfaces),
    }

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "inventory.json").write_text(json.dumps({
        "summary": summary, "screens": screens, "modules": modules, "systems": systems,
        "protocol": sorted(messages.values(), key=lambda m: m["id"]),
        "config_tables": table_rows, "ue_surfaces": surfaces,
        "surface_aliases": SURFACE_ALIASES,
    }, ensure_ascii=False, indent=1), encoding="utf-8", newline="\n")

    def write_csv(name: str, rows: list[dict], fields: list[str]) -> None:
        with (OUT / name).open("w", encoding="utf-8-sig", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=fields, extrasaction="ignore")
            w.writeheader()
            for r in rows:
                w.writerow({k: ("|".join(v) if isinstance(v, list) else v) for k, v in r.items()})

    write_csv("screens.csv", screens, ["status", "module", "prefab", "ui_info", "view_class", "mediator",
                                        "view_model", "unity_loc", "messages", "messages_ue_wired",
                                        "tables", "tables_ue_used", "ue_surfaces", "ue_product_files"])
    write_csv("modules.csv", modules, list(modules[0].keys()))
    write_csv("systems.csv", systems, list(systems[0].keys()) if systems else ["module"])
    write_csv("protocol.csv", sorted(messages.values(), key=lambda m: m["id"]),
              ["id", "name", "module", "direction", "has_request", "has_response", "server_handler",
               "ue_status", "unity_modules", "unity_listen_files", "unity_send_files", "ue_product_files"])
    write_csv("config_tables.csv", table_rows, ["table", "ue_status", "in_rokdata", "unity_files", "unity_modules",
                                                 "ue_product_files", "ue_script_files"])
    write_csv("ue_surfaces.csv", surfaces, ["id", "kind", "name", "norm", "dev_only", "unity_screens", "source"])

    print(json.dumps(summary, ensure_ascii=False, indent=1))
    print(f"wrote {rel(OUT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
