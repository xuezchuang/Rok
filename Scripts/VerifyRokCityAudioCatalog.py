"""Verify bounded CityAudio against the Unity workbook and exact source bytes."""

from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path

import BuildRokCityAudioCatalog as producer
from ExtractRokUnityConfigTables import parse_table
from rok_data_paths import DATA_ROOT, label, resolve

ROOT = Path(__file__).resolve().parents[1]
CATALOG = DATA_ROOT / "CityAudio/catalog.json"
REPORT = ROOT / "Saved/RokUnityMigration/CityAudio/verify.json"
WORKBOOK = ROOT / "ref/config-tables/s_AudioInfo.xlsm"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(doc: dict) -> None:
    assert doc["schema"] == "rok.city_audio_source.v1"
    assert doc["policy"] == "local_audio_only_no_network_no_account_state"
    assert doc["gain_policy"] == "AudioService AudioConfig ignores AudioInfo.volume; multiply source voice once by channel slider, not by table volume"
    assert doc["event_count"] == 7 and doc["source_table_row_count"] == 650
    source = parse_table(WORKBOOK, ROOT / "ref")
    assert source["row_count"] == 650
    rows = {row["ID"]: row for row in source["rows"]}
    assert len(rows) == 650
    assert [event["id"] for event in doc["events"]] == [1007, 1008, 4001, 4005, 4037, 4048, 4051]
    pins = {entry["path"]: entry["sha256"] for entry in doc["sources"]}
    assert len(pins) == len(doc["sources"])
    for path, digest in pins.items():
        assert sha(resolve(path)) == digest
    for required in (WORKBOOK, Path(producer.__file__),
                     ROOT / "ref/client-unity/Assets/Skyunion/RunTime/AudioService/AudioService.cs",
                     ROOT / "ref/client-unity/Assets/Skyunion/RunTime/AudioService/AudioConfig.cs",
                     ROOT / "ref/client-unity/Assets/Skyunion/RunTime/AudioService/EAudioMaxCountAction.cs",
                     ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Setting/GeneralSettingMediator.cs"):
        assert label(required) in pins
    seen_clips = set()
    for event in doc["events"]:
        row = rows[event["id"]]
        is_bgm = event["id"] in (1007, 1008)
        assert event["name"] == row["name"]
        assert event["channel"] == ("music" if is_bgm else "sfx")
        assert [event[field] for field in ("source_group_id", "source_mode", "source_max_count",
                                           "source_max_count_action", "source_table_volume",
                                           "source_fade_time_seconds")] == [
                                               row[field] for field in ("group", "mode", "maxCount",
                                                                       "maxCountAction", "volume", "fadeTime")]
        assert event["source_interval_seconds"] == [float(value) for value in row["interval"].split("|")]
        expected_names = row["audios"].split("|")
        assert [clip["name"] for clip in event["clips"]] == expected_names
        assert len(expected_names) == (2 if is_bgm else 1)
        for clip in event["clips"]:
            name = clip["name"]
            assert name not in seen_clips
            seen_clips.add(name)
            source_mp3 = f"ref/client-unity/Assets/BundleAssets/Souds/{name}.mp3"
            source_meta = source_mp3 + ".meta"
            package = (f"/Game/RokPrototype/UI/Audio/CityBgm/{name}" if is_bgm
                       else f"/Game/RokPrototype/UI/Audio/{name}")
            assert clip == {"name": name, "ue_object_path": package + "." + name,
                            "source_mp3": source_mp3,
                            "source_mp3_sha256": sha(resolve(source_mp3)),
                            "source_meta": source_meta,
                            "source_meta_sha256": sha(resolve(source_meta))}
            assert pins[source_mp3] == clip["source_mp3_sha256"]
            assert pins[source_meta] == clip["source_meta_sha256"]
    assert len(seen_clips) == 9
    assert rows[1007]["audios"] == "Bgm_PeaceDay01|Bgm_PeaceDay02"
    assert rows[1008]["audios"] == "Bgm_PeaceNight01|Bgm_PeaceNight02"
    assert rows[1007]["interval"] == "3|8" and rows[1008]["interval"] == "1|2"
    assert rows[4005]["name"] == "Sound_Ui_CommonScroll"


def main() -> None:
    doc = json.loads(CATALOG.read_text(encoding="utf-8"))
    verify(doc)
    assert CATALOG.read_bytes() == (json.dumps(producer.build(), ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    rejected = 0
    for mutate in (
        lambda d: d["events"][0]["source_interval_seconds"].__setitem__(0, 0.0),
        lambda d: d["events"][0]["clips"][1].__setitem__("name", "Bgm_PeaceDay01"),
        lambda d: d["events"][3].__setitem__("source_table_volume", 1.0),
        lambda d: d.__setitem__("policy", "server_audio"),
    ):
        damaged = copy.deepcopy(doc)
        mutate(damaged)
        try:
            verify(damaged)
        except (AssertionError, KeyError):
            rejected += 1
        else:
            raise AssertionError("Damaged source audio was accepted")
    result = {"schema": "rok.city_audio_verify.v1", "status": "passed",
              "catalog_path": label(CATALOG),
              "catalog_sha256": sha(CATALOG), "source_events": 7, "source_clips": 9,
              "negative_cases_rejected": rejected, "engine_started": False}
    REPORT.write_bytes((json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(f"PASS: 7 events, 9 clips, {rejected} negative cases; {sha(CATALOG)}")


if __name__ == "__main__":
    main()
