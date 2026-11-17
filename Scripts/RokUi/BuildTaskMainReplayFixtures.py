"""Write local server snapshots for the main and side task screen captures."""

import json
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
OUTPUT = PROJECT / "Saved/RokUi/Fixtures/TaskMain"
SERVER_TIME_MS = 1794882600000


def building(index, kind, level, x, y):
    return {"buildingIndex": index, "type": kind, "level": level,
            "pos": {"x": x, "y": y}, "finishTime": -1}


def main():
    source = PROJECT / "Content/RokData/unity_gameplay_catalog.json"
    catalog = json.loads(source.read_text(encoding="utf-8-sig"))
    role = {
        "rid": 1,
        "name": "Main Tasks UI Replay",
        "country": 101,
        "level": 5,
        "serverTime": SERVER_TIME_MS,
    }
    complete_side_tasks = [{"taskId": task["id"], "status": 1} for task in catalog["side_tasks"]]
    empty_role = {**role, "mainLineTaskId": 0, "chapterId": 0, "finishSideTasks": complete_side_tasks}
    mixed_role = {**role, "mainLineTaskId": 200900, "chapterId": 1, "finishSideTasks": []}
    buildings = {
        "5": building(5, 1, 2, -5, 5),
        "6": building(6, 9, 1, -9, -3),
        "30": building(30, 11, 1, 5, -13),
    }
    cases = {
        "waiting": {"Role_RoleInfo": {"roleInfo": role}},
        "empty": {"Role_RoleInfo": {"roleInfo": empty_role}},
        "mixed": {
            "Role_RoleInfo": {"roleInfo": mixed_role},
            "Build_BuildingInfo": {"buildingInfo": buildings},
        },
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, protocols in cases.items():
        replay = {
            "rid": role["rid"],
            "replay_server_time_ms": SERVER_TIME_MS,
            "state_by_protocol": protocols,
        }
        (OUTPUT / f"{name}.json").write_text(json.dumps(replay, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
