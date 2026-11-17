"""Build offline server chapter snapshots using the shipped task definitions."""

import json
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
OUTPUT = PROJECT / "Saved/RokUi/Fixtures/TaskChapter"
SERVER_TIME_MS = 1794882600000


def main():
    role = {
        "rid": 1,
        "name": "Chapter Tasks UI Replay",
        "country": 101,
        "level": 5,
        "serverTime": SERVER_TIME_MS,
    }
    chapter = {
        **role,
        "chapterId": 2,
        "mainLineTaskId": 200903,
        "chapterTasks": [{"taskId": 102001, "status": 1}],
        "taskStatisticsSum": [{"type": 33, "statistics": [{"arg": 3, "num": 20}]}],
    }
    unknown = {key: value for key, value in chapter.items() if key not in ("chapterTasks", "taskStatisticsSum")}
    complete = {
        **chapter,
        "chapterTasks": [{"taskId": task_id, "status": 1} for task_id in range(102001, 102005)],
    }
    buildings = {"5": {"buildingIndex": 5, "type": 1, "level": 5, "pos": {"x": -5, "y": 5}, "finishTime": -1}}
    hospital = {"buildingIndex": 4, "type": 14, "level": 1, "pos": {"x": -1, "y": -1}, "finishTime": -1}
    cases = {
        "waiting": role,
        "no_chapter": {**role, "chapterId": 0, "mainLineTaskId": 0},
        "empty": {**chapter, "chapterId": 9999},
        "unknown": unknown,
        "mixed": chapter,
        "complete": complete,
        "navigation": {**chapter, "mainLineTaskId": 200904},
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, snapshot in cases.items():
        city = {**buildings, "4": hospital} if name == "navigation" else buildings
        protocols = {"Role_RoleInfo": {"roleInfo": snapshot}, "Build_BuildingInfo": {"buildingInfo": city}}
        if name == "mixed":
            protocols["Task_TaskInfo"] = {"taskInfo": {"401001": {"taskId": 401001, "taskSchedule": 10000}}}
        replay = {"rid": role["rid"], "replay_server_time_ms": SERVER_TIME_MS, "state_by_protocol": protocols}
        (OUTPUT / f"{name}.json").write_text(json.dumps(replay, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
