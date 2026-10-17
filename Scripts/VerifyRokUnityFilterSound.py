"""Check reproducible Unity GameToggle sound evidence (not an audible playback test)."""
from pathlib import Path
import array
import hashlib
import json
import wave

root = Path(__file__).resolve().parents[1]
evidence = json.loads((root / "Saved/RokUnityMigration/unity_filter_click_sound.json").read_text())
source = root / evidence["source"]
decoded = root / "Saved/RokUnityMigration/Sound_Ui_CommonSidePage_mono.wav"
assert hashlib.sha256(source.read_bytes()).hexdigest() == evidence["sha256"]
assert hashlib.sha256(decoded.read_bytes()).hexdigest() == evidence["decoded_sha256"]
meta = Path(str(source) + ".meta").read_text()
assert "forceToMono: 1" in meta and "normalize: 0" in meta
with wave.open(str(decoded), "rb") as audio:
    assert audio.getnchannels() == evidence["channels"] == 1
    assert audio.getsampwidth() == 2 and audio.getframerate() == 44100
    assert abs(audio.getnframes() / audio.getframerate() - evidence["duration"]) < 0.001
    samples = array.array("h", audio.readframes(audio.getnframes()))
    assert max(abs(sample) for sample in samples) > 0, "Decoded sound is silent"
assert (root / "Content/RokPrototype/UI/Audio/Sound_Ui_CommonSidePage.uasset").is_file()
print("PASS: source hash, mono import settings, WAV hash/format/duration, non-silent PCM, UE asset")
