"""Write local server snapshots for the daily task screen captures."""

import json
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
OUTPUT = PROJECT / "Saved/RokUi/Fixtures/TaskDaily"
SERVER_TIME_MS = 1794882600000


def task_info(schedules):
    return {
        str(task_id): {"taskId": task_id, "taskSchedule": progress}
        for task_id, progress in schedules.items()
    }


def main():
    source = PROJECT / "Content/RokData/unity_gameplay_catalog.json"
    catalog = json.loads(source.read_text(encoding="utf-8-sig"))
    requirements = {task["id"]: task["require"] for task in catalog["daily_tasks"]}
    role = {
        "rid": 1,
        "name": "Daily Tasks UI Replay",
        "country": 101,
        "level": 5,
        "mainLineTaskId": 200900,
        "chapterId": 1,
        "serverTime": SERVER_TIME_MS,
    }
    mixed_role = {**role, "activePoint": 65, "activePointRewards": [20]}
    cases = {
        "waiting": (role, None),
        "empty": (role, {401007: -1}),
        "mixed": (mixed_role, {
            401001: requirements[401001],
            401003: 0,
            401005: 0,
            401006: requirements[401006] // 2,
            401007: -1,
        }),
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, (display_role, schedules) in cases.items():
        protocols = {"Role_RoleInfo": {"roleInfo": display_role}}
        if schedules is not None:
            protocols["Task_TaskInfo"] = {"taskInfo": task_info(schedules)}
        replay = {
            "rid": display_role["rid"],
            "replay_server_time_ms": SERVER_TIME_MS,
            "state_by_protocol": protocols,
        }
        (OUTPUT / f"{name}.json").write_text(json.dumps(replay, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
