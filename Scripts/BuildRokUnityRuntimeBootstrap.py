import json
from pathlib import Path
from rok_data_paths import DATA_ROOT, label


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MIGRATION_ROOT = PROJECT_ROOT / "Saved" / "RokUnityMigration"
CATALOG_PATH = DATA_ROOT / "unity_gameplay_catalog.json"
INITIAL_CITY_PATH = DATA_ROOT / "unity_initial_city_state.json"
OUTPUT_JSON = DATA_ROOT / "unity_runtime_bootstrap_state.json"
OUTPUT_MD = MIGRATION_ROOT / "unity_runtime_bootstrap_state.md"

ACTIVE_MAIN_TASK_ID = 200900
REFERENCE_TASK_CHAIN_ID = 200001
STARTER_TROOP_COUNT_PER_STACK = 50
DEFAULT_PLAYER_HEAD_ID = 1000
DEFAULT_PLAYER_FRAME_ID = 2000


def load_json(path):
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def build_task_chain(tasks_by_id, start_id, limit=24):
    chain = []
    seen = set()
    task_id = start_id
    while task_id and task_id not in seen and len(chain) < limit:
        task = tasks_by_id.get(task_id)
        if not task:
            break
        seen.add(task_id)
        chain.append(task)
        task_id = int(task.get("next_id") or 0)
    return chain


def evaluate_task(task, city_buildings):
    task_type = int(task.get("type") or 0)
    param1 = int(task.get("param1") or 0)
    param2 = int(task.get("param2") or 0)
    require = int(task.get("require") or 1)

    if task_type == 8:
        current = max((int(item.get("level") or 0) for item in city_buildings if int(item.get("type_id") or 0) == param1), default=0)
        target = max(param2, 1)
        return {
            "rule": "building_level",
            "source": "TaskMain.type=8, param1=building type, param2=required level",
            "current": current,
            "target": target,
            "complete": current >= target,
        }

    if task_type == 13:
        current = sum(1 for item in city_buildings if int(item.get("type_id") or 0) == param1)
        target = max(require, 1)
        return {
            "rule": "building_count",
            "source": "TaskMain.type=13, param1=building type, require=count",
            "current": current,
            "target": target,
            "complete": current >= target,
        }

    return {
        "rule": "stubbed_runtime_adapter",
        "source": "TaskMain row migrated, task-specific runtime evaluator not implemented yet",
        "current": 0,
        "target": max(require, 1),
        "complete": False,
    }


def make_task_state(task, city_buildings):
    progress = evaluate_task(task, city_buildings)
    return {
        "task_id": int(task.get("id") or 0),
        "name": task.get("name", ""),
        "desc": task.get("desc", ""),
        "type": int(task.get("type") or 0),
        "param1": int(task.get("param1") or 0),
        "param2": int(task.get("param2") or 0),
        "require": int(task.get("require") or 1),
        "reward": int(task.get("reward") or 0),
        "next_id": int(task.get("next_id") or 0),
        "icon": task.get("icon", ""),
        "progress": progress,
    }


def select_starter_heroes(heroes, count=5):
    # HeroProxy skips listDisplay == 1 rows when building visible hero lists.
    # Never reintroduce those hidden rows as a fallback; a short eligible pool
    # must remain visible in generated verification evidence.
    display_heroes = [hero for hero in heroes if int(hero.get("list_display") or 0) != 1]
    selected = sorted(
        display_heroes,
        key=lambda item: (
            -int(item.get("rare") or 0),
            -int(item.get("score") or 0),
            int(item.get("id") or 0),
        ),
    )[:count]
    starter_heroes = [
        {
            "hero_id": int(hero.get("id") or 0),
            "level": 1,
            "star": int(hero.get("init_star") or 0),
            "max_star": int(hero.get("max_star") or 0),
            "rare": int(hero.get("rare") or 0),
            "score": int(hero.get("score") or 0),
            "skill_ids": hero.get("skill_ids", []),
            "talent_ids": hero.get("talent_ids", []),
            "list_display": int(hero.get("list_display") or 0),
            "icon": hero.get("icon", ""),
            "model": hero.get("model", ""),
            "source": "Hero table local-sim preview selected from HeroProxy-visible rows (listDisplay != 1); ownership is not server login state",
        }
        for hero in selected
    ]
    return starter_heroes, {
        "requested_count": count,
        "eligible_count": len(display_heroes),
        "selected_count": len(starter_heroes),
        "shortfall": max(0, count - len(starter_heroes)),
    }


def select_trainable_troops(troops):
    return [
        troop for troop in troops
        if int(troop.get("stats", {}).get("power") or 0) > 0 and troop.get("model")
    ]


def select_starter_troop_stacks(trainable_troops):
    stacks = []
    for arms_type in (1, 2, 3, 4):
        candidates = [
            troop for troop in trainable_troops
            if int(troop.get("arms_type") or 0) == arms_type and int(troop.get("level") or 0) == 1
        ]
        if not candidates:
            continue
        troop = sorted(candidates, key=lambda item: int(item.get("id") or 0))[0]
        power = int(troop.get("stats", {}).get("power") or 0)
        stacks.append({
            "troop_id": int(troop.get("id") or 0),
            "arms_type": arms_type,
            "level": int(troop.get("level") or 0),
            "count": STARTER_TROOP_COUNT_PER_STACK,
            "power_per_unit": power,
            "total_power": power * STARTER_TROOP_COUNT_PER_STACK,
            "train_seconds": int(troop.get("train_seconds") or 0),
            "icon": troop.get("icon", ""),
            "model": troop.get("model", ""),
            "source": "Arms table level-1 local-sim bootstrap count",
        })
    return stacks


def main():
    catalog = load_json(CATALOG_PATH)
    initial_city = load_json(INITIAL_CITY_PATH)
    tasks_by_id = {int(task["id"]): task for task in catalog.get("main_tasks", [])}
    city_buildings = initial_city.get("buildings", [])

    active_task = tasks_by_id.get(ACTIVE_MAIN_TASK_ID)
    if not active_task:
        active_task = sorted(tasks_by_id.values(), key=lambda item: int(item.get("id") or 0))[0]

    active_chain = build_task_chain(tasks_by_id, int(active_task["id"]), limit=12)
    reference_chain = build_task_chain(tasks_by_id, REFERENCE_TASK_CHAIN_ID, limit=12)
    starter_heroes, starter_hero_selection = select_starter_heroes(
        catalog.get("heroes", [])
    )
    trainable_troops = select_trainable_troops(catalog.get("troops", []))
    starter_troops = select_starter_troop_stacks(trainable_troops)

    task_states = [make_task_state(task, city_buildings) for task in active_chain]
    active_task_state = task_states[0] if task_states else {}
    total_troops = sum(stack["count"] for stack in starter_troops)
    total_troop_power = sum(stack["total_power"] for stack in starter_troops)
    total_hero_score = sum(hero["score"] for hero in starter_heroes)
    local_preview_score = total_hero_score + total_troop_power

    player_runtime = {
        "state_kind": "local_preview",
        "source": "UE local-simulation bootstrap; identity, economy, progression, and social fields require server-authoritative login state",
        "default_head_id": DEFAULT_PLAYER_HEAD_ID,
        "default_frame_id": DEFAULT_PLAYER_FRAME_ID,
        "has_role_id": False,
        "role_id": "",
        "has_display_name": False,
        "display_name": "",
        "has_civilization": False,
        "civilization_id": 0,
        "civilization_name": "",
        "has_alliance": False,
        "alliance_name": "",
        "has_kill_count": False,
        "kill_count": 0,
        "has_action_points": False,
        "action_points_current": 0,
        "action_points_max": 0,
        "has_vip_level": False,
        "vip_level": 0,
        "local_hero_score": total_hero_score,
        "local_troop_power": total_troop_power,
        "local_preview_score": local_preview_score,
        "local_preview_note": "LocalHeroScore + LocalTroopPower is a preview-only acceptance metric, not official combatPower",
    }

    manifest = {
        "schema_version": 1,
        "source_manifests": {
            "gameplay_catalog": label(CATALOG_PATH),
            "initial_city_state": label(INITIAL_CITY_PATH),
        },
        "notes": [
            "This is a UE local-simulation bootstrap derived from migrated Unity config tables.",
            "Starter hero ownership and troop counts are explicit bootstrap values, not accepted server login state.",
            "Player runtime identity and progression fields remain unknown until server-authoritative login state is connected.",
        ],
        "summary": {
            "active_task_id": active_task_state.get("task_id", 0),
            "active_task_current": active_task_state.get("progress", {}).get("current", 0),
            "active_task_target": active_task_state.get("progress", {}).get("target", 0),
            "active_task_complete": active_task_state.get("progress", {}).get("complete", False),
            "active_task_chain_count": len(task_states),
            "reference_task_chain_id": REFERENCE_TASK_CHAIN_ID,
            "reference_task_chain_count": len(reference_chain),
            "starter_hero_count": len(starter_heroes),
            "starter_hero_eligible_count": starter_hero_selection["eligible_count"],
            "starter_hero_selection_shortfall": starter_hero_selection["shortfall"],
            "starter_troop_stack_count": len(starter_troops),
            "total_troop_count": total_troops,
            "total_troop_power": total_troop_power,
            "total_hero_score": total_hero_score,
            "trainable_troop_count": len(trainable_troops),
        },
        "main_task_runtime": {
            "active_task": active_task_state,
            "active_chain": task_states,
            "reference_chain": [make_task_state(task, city_buildings) for task in reference_chain],
        },
        "hero_runtime": {
            "selection_rule": "Exclude Hero.listDisplay == 1; sort remaining rows by rarity, score, then ID; never reintroduce hidden rows as fallback",
            "source": "ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/HeroProxy.cs skips hero.config.listDisplay == 1",
            "selection": starter_hero_selection,
            "starter_heroes": starter_heroes,
        },
        "troop_runtime": {
            "selection_rule": "Level-1 positive-power Arms rows for armsType 1..4",
            "starter_stacks": starter_troops,
            "trainable_troop_ids": [int(troop.get("id") or 0) for troop in trainable_troops],
        },
        "player_runtime": player_runtime,
    }

    OUTPUT_JSON.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Unity Runtime Bootstrap State",
        "",
        f"- Active task: {active_task_state.get('task_id')} {active_task_state.get('name', '')}",
        f"- Active task progress: {active_task_state.get('progress', {}).get('current', 0)}/{active_task_state.get('progress', {}).get('target', 0)}",
        f"- Active chain rows: {len(task_states)}",
        f"- Starter heroes: {len(starter_heroes)}",
        f"- HeroProxy-visible hero rows: {starter_hero_selection['eligible_count']}",
        f"- Starter hero selection shortfall: {starter_hero_selection['shortfall']}",
        f"- Starter troop stacks: {len(starter_troops)}",
        f"- Starter troop count: {total_troops}",
        f"- Trainable troops: {len(trainable_troops)}",
        f"- Player runtime state: {player_runtime['state_kind']}",
        f"- Local preview score: {local_preview_score} (hero {total_hero_score} + troop {total_troop_power}; not official combatPower)",
        "",
        "## Player Runtime",
        "",
        f"- Default head/frame: {DEFAULT_PLAYER_HEAD_ID}/{DEFAULT_PLAYER_FRAME_ID}",
        "- Server role ID, name, civilization, alliance, kill count, action points, and VIP level: unknown",
        "- The local preview score is derived only from bootstrap hero and troop rows.",
        "",
        "## Starter Heroes",
        "",
        "| HeroId | Star | Rare | Score | Skills | Model |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for hero in starter_heroes:
        lines.append(f"| {hero['hero_id']} | {hero['star']} | {hero['rare']} | {hero['score']} | {len(hero['skill_ids'])} | {hero['model']} |")

    lines.extend([
        "",
        "## Starter Troops",
        "",
        "| TroopId | ArmsType | Level | Count | Power | Model |",
        "| --- | --- | --- | --- | --- | --- |",
    ])
    for troop in starter_troops:
        lines.append(f"| {troop['troop_id']} | {troop['arms_type']} | {troop['level']} | {troop['count']} | {troop['total_power']} | {troop['model']} |")

    lines.extend([
        "",
        "## Active Task Chain",
        "",
        "| TaskId | Name | Type | Progress | Next |",
        "| --- | --- | --- | --- | --- |",
    ])
    for task in task_states:
        progress = task["progress"]
        lines.append(f"| {task['task_id']} | {task['name']} | {task['type']} | {progress['current']}/{progress['target']} | {task['next_id']} |")
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(json.dumps({
        "output": str(OUTPUT_JSON),
        "summary": manifest["summary"],
    }, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
