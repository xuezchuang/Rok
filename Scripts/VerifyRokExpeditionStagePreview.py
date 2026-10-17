from __future__ import annotations

import hashlib
import json
from pathlib import Path

from BuildRokExpeditionStagePreview import OUTPUT, ROOT, build


def main() -> None:
    actual = OUTPUT.read_bytes()
    expected = (json.dumps(build(), ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    assert actual == expected, "Expedition preview does not match source; rebuild"
    payload = json.loads(actual)
    assert payload["schema"] == "rok.expedition_stage_preview.v1"
    assert payload["source_stage_count"] == len(payload["stages"]) == 80
    assert payload["account_state"] == "unsynced" and payload["preview_only"] is True
    assert [row["id"] for row in payload["stages"]] == list(range(1, 81))
    assert [row["battle_coordinate"] for row in payload["stages"][:2]] == [[466, 79], [601, 101]]
    for source, source_hash in payload["source_files_sha256"].items():
        assert hashlib.sha256((ROOT / source).read_bytes()).hexdigest() == source_hash, source
    print(f"passed: 80 source stages, account unsynced, source sha256={hashlib.sha256(actual).hexdigest()}")


if __name__ == "__main__":
    main()
