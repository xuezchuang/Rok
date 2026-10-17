"""Generate a bounded, source-only city BGM and UI SFX event catalog."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from ExtractRokUnityConfigTables import parse_table

ROOT = Path(__file__).resolve().parents[1]
WORKBOOK = ROOT / "ref/config-tables/s_AudioInfo.xlsm"
EXTRACTED = ROOT / "Saved/RokUnityMigration/ConfigTables/AudioInfo.json"
OUTPUT = ROOT / "Saved/RokUnityMigration/CityAudio/catalog.json"
EVENT_IDS = (1007, 1008, 4001, 4005, 4037, 4048, 4051)
BGM_IDS = (1007, 1008)
ASSET_ROOT = "/Game/RokPrototype/UI/Audio"
REF_CLIPS = ROOT / "ref/client-unity/Assets/BundleAssets/Souds"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def split_pipe(value: str, cast):
    return [cast(part.strip()) for part in value.split("|") if part.strip()]


def build() -> dict:
    source_table = parse_table(WORKBOOK, ROOT / "ref")
    extracted = json.loads(EXTRACTED.read_text(encoding="utf-8"))
    assert source_table["table_name"] == "AudioInfo" and source_table["row_count"] == 650
    assert source_table["rows"] == extracted["rows"], "AudioInfo extraction snapshot drift"
    by_id = {row["ID"]: row for row in source_table["rows"]}
    assert len(by_id) == 650
    source_files = [WORKBOOK, EXTRACTED, ROOT / "Scripts/ExtractRokUnityConfigTables.py",
                    ROOT / "ref/client-unity/Assets/Skyunion/RunTime/AudioService/AudioService.cs",
                    ROOT / "ref/client-unity/Assets/Skyunion/RunTime/AudioService/AudioConfig.cs",
                    ROOT / "ref/client-unity/Assets/Skyunion/RunTime/AudioService/EAudioMaxCountAction.cs",
                    ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Setting/GeneralSettingMediator.cs",
                    ROOT / "ref/client-unity/Assets/Scripts/Hotfix/MVC/RS.cs",
                    Path(__file__)]
    events = []
    for event_id in EVENT_IDS:
        row = by_id[event_id]
        clips = split_pipe(row["audios"], str)
        intervals = split_pipe(row["interval"], float)
        is_bgm = event_id in BGM_IDS
        assert row["group"] == (1 if is_bgm else 4)
        assert row["mode"] == (2 if is_bgm else 0)
        assert row["maxCount"] == 1 and row["maxCountAction"] == 1
        assert len(clips) == (2 if is_bgm else 1)
        assert len(intervals) == (2 if is_bgm else 1)
        assert all(value >= 0 for value in intervals)
        clip_rows = []
        for clip in clips:
            mp3 = REF_CLIPS / f"{clip}.mp3"
            meta = REF_CLIPS / f"{clip}.mp3.meta"
            assert mp3.is_file() and meta.is_file(), clip
            source_files.extend((mp3, meta))
            package = (f"{ASSET_ROOT}/CityBgm/{clip}" if is_bgm
                       else f"{ASSET_ROOT}/{clip}")
            clip_rows.append({"name": clip, "ue_object_path": f"{package}.{clip}",
                              "source_mp3": mp3.relative_to(ROOT).as_posix(),
                              "source_mp3_sha256": sha(mp3),
                              "source_meta": meta.relative_to(ROOT).as_posix(),
                              "source_meta_sha256": sha(meta)})
        events.append({
            "id": event_id, "name": row["name"], "channel": "music" if is_bgm else "sfx",
            "source_group_id": row["group"], "source_mode": row["mode"],
            "source_max_count": row["maxCount"],
            "source_max_count_action": row["maxCountAction"],
            "source_interval_seconds": intervals,
            "source_table_volume": row["volume"],
            "source_fade_time_seconds": row["fadeTime"],
            "clips": clip_rows,
        })
    assert len(source_files) == len(set(source_files))
    return {
        "schema": "rok.city_audio_source.v1",
        "policy": "local_audio_only_no_network_no_account_state",
        "gain_policy": "AudioService AudioConfig ignores AudioInfo.volume; multiply source voice once by channel slider, not by table volume",
        "source_table_row_count": 650,
        "event_count": len(events), "events": events,
        "sources": [{"path": path.relative_to(ROOT).as_posix(), "sha256": sha(path)}
                    for path in source_files],
    }


def main() -> None:
    result = build()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes((json.dumps(result, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(f"{OUTPUT.relative_to(ROOT)}: {result['event_count']} events, "
          f"{sum(len(row['clips']) for row in result['events'])} clips")


if __name__ == "__main__":
    main()
