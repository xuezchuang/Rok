"""Write synthetic, read-only server replays for the training UI captures."""

import json
from pathlib import Path


OUTPUT = Path(__file__).resolve().parents[2] / "Saved/RokUi/Fixtures/TrainArmy"
SERVER_SECONDS = 1794882600


def building(index, kind, level, x, y):
    return {"buildingIndex": index, "type": kind, "level": level,
            "pos": {"x": x, "y": y}, "finishTime": -1}


def main():
    role = {
        "rid": 1,
        "name": "Training UI Replay",
        "country": 101,
        "level": 5,
        "food": 100000,
        "wood": 100000,
        "stone": 100000,
        "gold": 100000,
        "denar": 1000,
        "serverTime": SERVER_SECONDS * 1000,
        "soldiers": [{"id": 10301, "type": 3, "level": 1, "num": 1080}],
        "armyQueue": [],
        "buildQueue": [],
    }
    queue = {"queueIndex": 3, "buildingIndex": 30, "armyType": 3, "armyNum": 40,
             "beginTime": SERVER_SECONDS - 120, "newArmyLevel": 1}
    buildings = {
        "5": building(5, 1, 5, -5, 5),
        "6": building(6, 9, 1, -9, -3),
        "30": building(30, 11, 1, 5, -13),
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, queues in (("idle", []), ("busy", [{**queue, "finishTime": SERVER_SECONDS + 180}]),
                         ("ready", [{**queue, "finishTime": SERVER_SECONDS - 20}])):
        replay_role = {**role, "armyQueue": queues}
        replay = {
            "rid": role["rid"],
            "replay_server_time_ms": role["serverTime"],
            "state_by_protocol": {
                "Role_RoleInfo": {"roleInfo": replay_role},
                "Build_BuildingInfo": {"buildingInfo": buildings},
            },
        }
        (OUTPUT / f"{name}.json").write_text(json.dumps(replay, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
